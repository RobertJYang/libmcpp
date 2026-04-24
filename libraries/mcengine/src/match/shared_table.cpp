/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "shared_table.h"

#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM

#include "shared_trie_layout.h"

#include <mc/engine/match/path_pattern.h>
#include <mc/engine/match/rule.h>
#include <mc/exception.h>
#include <mc/shm/allocator.h>
#include <mc/shm/byte_string.h>
#include <mc/shm/container/map.h>
#include <mc/shm/ipc_shared_mutex.h>
#include <mc/shm/region.h>
#include <mc/shm/shm_runtime.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <new>
#include <utility>
#include <vector>

namespace mc::engine::match {

namespace {

using detail::path_end_sentinel;
using detail::shared_root;
using detail::shared_root_magic;
using detail::shared_root_name;
using detail::shared_root_version;
using detail::shared_lock_name;
using detail::subscription_entry;
using detail::trie_level;
using detail::trie_node;
using detail::wildcard_multi;
using detail::wildcard_single;

// 把 message_type 字符串化，与 condition_backend 那一份保持一致语义。
mc::string_view _message_type_token(message_type t) noexcept
{
    switch (t) {
        case message_type::method_call:   return "method_call";
        case message_type::method_return: return "method_return";
        case message_type::error:         return "error";
        case message_type::signal:        return "signal";
        default:                          return "unknown";
    }
}

// 子字典的 self_tag：用 trie_node 自身在 SHM region 内的 offset 低 32bit。
std::uint32_t _self_tag_for(const mc::shm::shm_region& region, const void* node) noexcept
{
    return static_cast<std::uint32_t>(region.offset_of(node));
}

// region 基址 / 偏移转换的便利包装；shared_root 自身 offset 必须落在 region 内。
template <typename T>
T* _ptr_from_root(shared_root* root, std::int64_t self_relative_offset) noexcept
{
    if (self_relative_offset == 0) {
        return nullptr;
    }
    return reinterpret_cast<T*>(reinterpret_cast<std::byte*>(root)
                                + self_relative_offset);
}

template <typename T>
const T* _ptr_from_root(const shared_root* root, std::int64_t self_relative_offset) noexcept
{
    if (self_relative_offset == 0) {
        return nullptr;
    }
    return reinterpret_cast<const T*>(reinterpret_cast<const std::byte*>(root)
                                      + self_relative_offset);
}

template <typename T>
std::int64_t _offset_from_root(const shared_root* root, const T* ptr) noexcept
{
    if (ptr == nullptr) {
        return 0;
    }
    return reinterpret_cast<const std::byte*>(ptr)
           - reinterpret_cast<const std::byte*>(root);
}

// ---- trie 子字典访问 helper ----
//
// children map 的 Key=byte_string, Value=int64_t（child trie_node 相对 shared_root 的 offset）
using children_map = mc::shm::container::map<mc::shm::byte_string,
                                             std::int64_t,
                                             mc::shm::byte_string_less>;

// 包装：从 trie_node 拿一个非 const map wrapper（map ctor 需要非 const）。
// const-cast 安全：map_control 内部已 thread-safe（atomic + mutex），map 的 find 不
// 修改可见状态。
inline children_map _wrap_children(const trie_node& node, mc::shm::shm_allocator& alloc) noexcept
{
    return children_map(const_cast<mc::shm::container::map_control&>(node.children), alloc);
}

// 给指定 trie_node 创建/复用 key 对应的子 trie_node。返回新（或已有）子 node 的 self-relative offset。
// 失败返回 0。
std::int64_t _children_emplace_node(shared_root&            root,
                                    mc::shm::shm_region&    region,
                                    mc::shm::shm_allocator& alloc,
                                    trie_node&              parent,
                                    mc::string_view         key,
                                    trie_level              child_level)
{
    children_map cm(parent.children, alloc);

    auto existing = cm.find(key);
    if (existing) {
        return *existing.value;
    }

    // 1) 先 alloc 子 trie_node 并 init 它的 children map。
    auto* child_mem = alloc.allocate(sizeof(trie_node), alignof(trie_node));
    if (child_mem == nullptr) {
        return 0;
    }
    auto* child = new (child_mem) trie_node{};
    child->leaf_list_head = 0;
    child->level          = static_cast<std::uint8_t>(child_level);

    children_map::init(child->children, _self_tag_for(region, child));

    // 2) byte_string key —— 拷一份到 SHM。
    auto key_blob = mc::shm::byte_string::create(alloc, key);
    if (key_blob.empty() && !key.empty()) {
        // alloc 失败，回滚
        alloc.deallocate(child_mem);
        return 0;
    }

    std::int64_t child_offset = _offset_from_root(&root, child);

    // 3) try_emplace：把 (key_blob, child_offset) 插入 parent.children；插入是 journaled。
    //    必须用 (Key&&, Value&&) 的 rvalue overload —— byte_string 是 move-only，
    //    第二参也强制 rvalue 以避免 overload 解析挑到 (const&,const&) 的 copy 路径。
    auto inserted = cm.try_emplace(std::move(key_blob), std::int64_t{child_offset});
    if (!inserted.second) {
        // 另一个进程并发先插入了 —— 回滚我们这份子节点
        alloc.deallocate(child_mem);
        return *inserted.first.value;
    }
    return child_offset;
}

// 把 match_rule 编码成 trie 下行段序列：
//   [type, sender, dest, path_seg1, path_seg2, ..., (PATH_END or stays empty if rule.path 末尾 ** ),
//    iface, member]
//
// 通配规则：
//   - 6 个文本字段，空 → "*"。
//   - rule.path 走 path_pattern：
//       full_wildcard ("")           → 单段 "**"，再 PATH_END
//       prefix_doublestar ("/a/b/")  → segments + "**" + PATH_END
//       exact ("/a/b")               → segments + PATH_END
//
// type 字段例外：rule.type 已经是 "signal" 这种文本，原样使用；空 → "*"。
struct rule_encoding {
    mc::string                     fixed_type;        // type
    mc::string                     fixed_sender;      // sender
    mc::string                     fixed_destination; // destination
    std::vector<mc::string>        path_segments;     // 含末尾 "**" 与 PATH_END
    mc::string                     fixed_interface;   // interface_name
    mc::string                     fixed_member;      // member_name
};

mc::string _to_segment_or_star(const mc::string& v)
{
    if (v.empty()) {
        return mc::string(wildcard_single);
    }
    return v;
}

rule_encoding _encode_rule(const match_rule& rule)
{
    rule_encoding enc;
    enc.fixed_type        = _to_segment_or_star(rule.type);
    enc.fixed_sender      = _to_segment_or_star(rule.sender);
    enc.fixed_destination = _to_segment_or_star(rule.destination);
    enc.fixed_interface   = _to_segment_or_star(rule.interface_name);
    enc.fixed_member      = _to_segment_or_star(rule.member_name);

    path_pattern pp(rule.path);
    switch (pp.get_kind()) {
        case path_pattern::kind::full_wildcard:
            // 任意 path：直接 "**" + PATH_END
            enc.path_segments.emplace_back(wildcard_multi);
            enc.path_segments.emplace_back(path_end_sentinel);
            break;
        case path_pattern::kind::exact: {
            for (const auto& seg : pp.segments()) {
                enc.path_segments.push_back(seg);
            }
            enc.path_segments.emplace_back(path_end_sentinel);
            break;
        }
        case path_pattern::kind::prefix_doublestar: {
            for (const auto& seg : pp.segments()) {
                enc.path_segments.push_back(seg);
            }
            enc.path_segments.emplace_back(wildcard_multi);
            enc.path_segments.emplace_back(path_end_sentinel);
            break;
        }
    }
    return enc;
}

// 把 message_header 编码成 trie 下行的"实际值序列"（不含 PATH_END/通配，
// 由 find_targets 在 path 层动态枚举）。
struct message_encoding {
    mc::string              type_token;
    mc::string_view         sender;
    mc::string_view         destination;
    std::vector<mc::string> path_segments;
    mc::string_view         interface_name;
    mc::string_view         member_name;
};

message_encoding _encode_message(const message_header& h)
{
    message_encoding e;
    e.type_token     = mc::string(_message_type_token(h.type));
    e.sender         = h.sender;
    e.destination    = h.destination;
    e.interface_name = h.interface_name;
    e.member_name    = h.member_name;
    auto segs = split_path_segments(h.path);
    e.path_segments.reserve(segs.size());
    for (auto sv : segs) {
        e.path_segments.emplace_back(sv);
    }
    return e;
}

} // namespace (anon)

// ============================================================================
// shared_table 实现
// ============================================================================

shared_table::shared_table(mc::shm::shm_runtime& runtime,
                           std::uint16_t         owner_endpoint_id,
                           std::uint32_t         owner_instance_id)
    : m_runtime(runtime),
      m_endpoint_id(owner_endpoint_id),
      m_instance_id(owner_instance_id)
{
    _attach_or_create();
}

shared_table::~shared_table()
{
    // 析构：把本进程拥有的 SHM 订阅条目摘掉，避免 leak / dangling。
    _drop_owned_entries();
}

void shared_table::_attach_or_create()
{
    auto& region = m_runtime.region();
    MC_ASSERT_THROW(region.is_valid(), mc::invalid_arg_exception,
                    "shared_table: shm_region 不可用");

    m_lock = m_runtime.get_or_create_shared_mutex(shared_lock_name);
    MC_ASSERT_THROW(m_lock != nullptr, mc::invalid_arg_exception,
                    "shared_table: 无法 lazy 创建命名锁 ${n}",
                    ("n", mc::string(shared_lock_name)));

    auto alloc = m_runtime.user_arena();

    mc::shm::ipc_unique_lock_guard guard(*m_lock);

    auto existing = region.find_root(shared_root_name);
    if (existing) {
        m_root = region.ptr_from_offset<shared_root>(existing->offset);
        MC_ASSERT_THROW(m_root != nullptr && m_root->magic == shared_root_magic
                            && m_root->version == shared_root_version,
                        mc::invalid_arg_exception,
                        "shared_table: shared_root 校验失败");
        return;
    }

    // lazy 创建 root + type 层 trie_node。
    void* root_mem = alloc.allocate(sizeof(shared_root), alignof(shared_root));
    MC_ASSERT_THROW(root_mem != nullptr, mc::invalid_arg_exception,
                    "shared_table: 分配 shared_root 失败");
    auto* root = new (root_mem) shared_root{};
    root->magic   = shared_root_magic;
    root->version = shared_root_version;
    root->next_match_id.store(0, std::memory_order_relaxed);

    void* type_node_mem = alloc.allocate(sizeof(trie_node), alignof(trie_node));
    if (type_node_mem == nullptr) {
        alloc.deallocate(root_mem);
        MC_THROW(mc::invalid_arg_exception, "shared_table: 分配 type 层 trie_node 失败");
    }
    auto* type_node = new (type_node_mem) trie_node{};
    type_node->leaf_list_head = 0;
    type_node->level          = static_cast<std::uint8_t>(trie_level::type);
    children_map::init(type_node->children, _self_tag_for(region, type_node));

    root->root_node_offset = _offset_from_root(root, type_node);

    bool ok = region.upsert_root(shared_root_name, region.offset_of(root),
                                 sizeof(shared_root),
                                 mc::shm::root_kind::opaque, shared_root_version);
    MC_ASSERT_THROW(ok, mc::invalid_arg_exception,
                    "shared_table: shm_region.upsert_root 失败");
    m_root = root;
}

std::int64_t shared_table::_ensure_path_for_rule(const match_rule& rule)
{
    auto  enc    = _encode_rule(rule);
    auto& region = m_runtime.region();
    auto  alloc  = m_runtime.user_arena();

    trie_node* node = _ptr_from_root<trie_node>(m_root, m_root->root_node_offset);
    MC_ASSERT_THROW(node != nullptr, mc::invalid_arg_exception,
                    "shared_table: shared_root.root_node_offset 为 0");

    auto descend = [&](mc::string_view key, trie_level child_level) {
        auto child_offset = _children_emplace_node(*m_root, region, alloc, *node, key, child_level);
        MC_ASSERT_THROW(child_offset != 0, mc::invalid_arg_exception,
                        "shared_table: 创建 trie 子节点失败 (level=${lvl} key=${k})",
                        ("lvl", static_cast<int>(child_level))("k", mc::string(key)));
        node = _ptr_from_root<trie_node>(m_root, child_offset);
    };

    descend(enc.fixed_type,        trie_level::sender);
    descend(enc.fixed_sender,      trie_level::destination);
    descend(enc.fixed_destination, trie_level::path);

    // path 段（含 PATH_END）：所有中间节点都仍是 path 层，最后 PATH_END 转去 iface 层。
    for (std::size_t i = 0; i < enc.path_segments.size(); ++i) {
        bool is_path_end = (enc.path_segments[i] == path_end_sentinel);
        descend(enc.path_segments[i],
                is_path_end ? trie_level::interface_name : trie_level::path);
    }

    descend(enc.fixed_interface, trie_level::member_name);
    descend(enc.fixed_member,    trie_level::member_name); // 末端节点 level 也算 member（叶子标记）

    return _offset_from_root(m_root, node);
}

std::int64_t shared_table::_push_subscription_entry(std::int64_t       node_offset,
                                                    std::uint64_t      id,
                                                    mc::string_view    service_name,
                                                    const filter_spec& spec)
{
    auto  alloc = m_runtime.user_arena();
    auto* node  = _ptr_from_root<trie_node>(m_root, node_offset);
    MC_ASSERT_THROW(node != nullptr, mc::invalid_arg_exception,
                    "shared_table: leaf trie_node 偏移为空");

    void* entry_mem = alloc.allocate(sizeof(subscription_entry), alignof(subscription_entry));
    MC_ASSERT_THROW(entry_mem != nullptr, mc::invalid_arg_exception,
                    "shared_table: 分配 subscription_entry 失败");

    auto* entry = new (entry_mem) subscription_entry{};
    entry->match_id            = id;
    entry->owner_endpoint_id   = m_endpoint_id;
    entry->owner_instance_id   = m_instance_id;
    entry->filter_backend_type = spec.backend_type;
    entry->service_name        = mc::shm::byte_string::create(alloc, service_name);
    entry->filter_text         = mc::shm::byte_string::create(alloc, spec.text);
    entry->leaf_node_offset    = node_offset;

    std::int64_t entry_offset = _offset_from_root(m_root, entry);

    // 头插 leaf 链：新 entry 成为新 head；旧 head 的 prev 指向新 entry。
    entry->prev_in_leaf = 0;
    entry->next_in_leaf = node->leaf_list_head;
    if (entry->next_in_leaf != 0) {
        auto* old_head = _ptr_from_root<subscription_entry>(m_root, entry->next_in_leaf);
        old_head->prev_in_leaf = entry_offset;
    }
    node->leaf_list_head = entry_offset;

    // 头插 endpoint 链（仅当 endpoint_id 落在 [1, shared_max_endpoints) 内：
    //   - 0 表示"本地默认 / 不挂 shm endpoint"，不参与 sweep；
    //   - 超出 shared_max_endpoints 暂时不支持，留给 M2-C 扩展。
    entry->prev_in_endpoint = 0;
    entry->next_in_endpoint = 0;
    if (m_endpoint_id > 0 && m_endpoint_id < detail::shared_max_endpoints) {
        auto& head = m_root->endpoint_lists[m_endpoint_id];
        entry->next_in_endpoint = head;
        if (head != 0) {
            auto* old_head = _ptr_from_root<subscription_entry>(m_root, head);
            old_head->prev_in_endpoint = entry_offset;
        }
        head = entry_offset;
    }

    return entry_offset;
}

namespace {

// 把指定 entry 从两条链上 O(1) 摘掉并 free arena。调用方持锁。
void _detach_and_free_entry(detail::shared_root*    root,
                            mc::shm::shm_allocator& alloc,
                            subscription_entry*     entry) noexcept
{
    if (entry == nullptr) {
        return;
    }

    // 1) leaf 链
    auto* leaf_node =
        reinterpret_cast<detail::trie_node*>(reinterpret_cast<std::byte*>(root)
                                             + entry->leaf_node_offset);
    if (entry->prev_in_leaf == 0) {
        leaf_node->leaf_list_head = entry->next_in_leaf;
    } else {
        auto* prev = reinterpret_cast<subscription_entry*>(
            reinterpret_cast<std::byte*>(root) + entry->prev_in_leaf);
        prev->next_in_leaf = entry->next_in_leaf;
    }
    if (entry->next_in_leaf != 0) {
        auto* next = reinterpret_cast<subscription_entry*>(
            reinterpret_cast<std::byte*>(root) + entry->next_in_leaf);
        next->prev_in_leaf = entry->prev_in_leaf;
    }

    // 2) endpoint 链（仅当 entry 当初挂上去了；endpoint_id == 0 跳过）
    if (entry->owner_endpoint_id > 0
        && entry->owner_endpoint_id < detail::shared_max_endpoints) {
        auto& head = root->endpoint_lists[entry->owner_endpoint_id];
        if (entry->prev_in_endpoint == 0) {
            head = entry->next_in_endpoint;
        } else {
            auto* prev = reinterpret_cast<subscription_entry*>(
                reinterpret_cast<std::byte*>(root) + entry->prev_in_endpoint);
            prev->next_in_endpoint = entry->next_in_endpoint;
        }
        if (entry->next_in_endpoint != 0) {
            auto* next = reinterpret_cast<subscription_entry*>(
                reinterpret_cast<std::byte*>(root) + entry->next_in_endpoint);
            next->prev_in_endpoint = entry->prev_in_endpoint;
        }
    }

    // 3) free arena
    entry->service_name.destroy(alloc);
    entry->filter_text.destroy(alloc);
    entry->~subscription_entry();
    alloc.deallocate(entry);
}

} // namespace (anon)

match_id shared_table::subscribe(mc::string_view service_name,
                                 match_rule      rule,
                                 filter_spec     spec,
                                 match_callback  callback)
{
    MC_ASSERT_THROW(callback, mc::invalid_arg_exception,
                    "match::table::subscribe 需要非空 callback");

    if (spec.backend_type != no_filter) {
        auto compiled = filter_backend_registry::instance().get_or_compile(spec.backend_type, spec.text);
        MC_ASSERT_THROW(compiled, mc::invalid_arg_exception,
                        "filter backend 未注册或表达式编译失败: backend_type=${t}",
                        ("t", spec.backend_type));
    }

    std::uint64_t id = 0;
    std::int64_t  entry_offset = 0;
    {
        mc::shm::ipc_unique_lock_guard guard(*m_lock);
        id = m_root->next_match_id.fetch_add(1, std::memory_order_relaxed) + 1;
        // 0 不分配
        if (id == 0) {
            id = m_root->next_match_id.fetch_add(1, std::memory_order_relaxed) + 1;
        }
        auto leaf_offset = _ensure_path_for_rule(rule);
        entry_offset     = _push_subscription_entry(leaf_offset, id, service_name, spec);
    }

    {
        std::lock_guard lock(m_local_mutex);
        local_entry e;
        e.service_name = mc::string(service_name);
        e.callback     = std::move(callback);
        m_local_callbacks.emplace(id, std::move(e));
        m_owned_entry_offsets.emplace(id, entry_offset);
    }

    return id;
}

void shared_table::_drop_entry_locked(std::uint64_t id) noexcept
{
    auto it = m_owned_entry_offsets.find(id);
    if (it == m_owned_entry_offsets.end()) {
        return;
    }
    auto* entry = _ptr_from_root<subscription_entry>(m_root, it->second);
    auto  alloc = m_runtime.user_arena();
    _detach_and_free_entry(m_root, alloc, entry);
    m_owned_entry_offsets.erase(it);
}

std::size_t shared_table::sweep_dead_endpoint(std::uint16_t endpoint_id,
                                              std::uint32_t min_alive_instance_id) noexcept
{
    if (endpoint_id == 0U || endpoint_id >= detail::shared_max_endpoints) {
        return 0U;
    }

    if (m_root == nullptr || m_lock == nullptr) {
        return 0U;
    }

    std::size_t                    swept = 0U;
    mc::shm::ipc_unique_lock_guard guard(*m_lock);
    auto                           alloc = m_runtime.user_arena();

    std::int64_t cur = m_root->endpoint_lists[endpoint_id];
    while (cur != 0) {
        auto* entry = _ptr_from_root<subscription_entry>(m_root, cur);
        std::int64_t next = entry->next_in_endpoint;

        if (entry->owner_endpoint_id == endpoint_id
            && entry->owner_instance_id < min_alive_instance_id) {
            // 若 sweep 的是本进程归属 entry（少见，但理论可发生），同步清掉
            // local 缓存，避免后续 lookup_callback 拿到失效记录。
            if (entry->owner_endpoint_id == m_endpoint_id
                && entry->owner_instance_id == m_instance_id) {
                std::lock_guard local_lock(m_local_mutex);
                m_local_callbacks.erase(entry->match_id);
                m_owned_entry_offsets.erase(entry->match_id);
            }
            _detach_and_free_entry(m_root, alloc, entry);
            ++swept;
        }
        cur = next;
    }
    return swept;
}

void shared_table::unsubscribe(match_id id) noexcept
{
    if (id == 0) {
        return;
    }

    {
        std::lock_guard lock(m_local_mutex);
        m_local_callbacks.erase(id);
    }

    mc::shm::ipc_unique_lock_guard guard(*m_lock);
    std::lock_guard                local_lock(m_local_mutex);
    _drop_entry_locked(id);
}

void shared_table::_drop_owned_entries() noexcept
{
    if (m_root == nullptr || m_lock == nullptr) {
        return;
    }
    mc::shm::ipc_unique_lock_guard guard(*m_lock);
    std::lock_guard                local_lock(m_local_mutex);
    // 拷一份再删，避免迭代器失效。
    std::vector<std::uint64_t> ids;
    ids.reserve(m_owned_entry_offsets.size());
    for (const auto& kv : m_owned_entry_offsets) {
        ids.push_back(kv.first);
    }
    for (auto id : ids) {
        _drop_entry_locked(id);
    }
    m_local_callbacks.clear();
}

void shared_table::clear() noexcept
{
    _drop_owned_entries();
}

std::size_t shared_table::size() const noexcept
{
    std::lock_guard lock(m_local_mutex);
    return m_owned_entry_offsets.size();
}

// ============================================================================
// find_targets：trie 并行下行
// ============================================================================
namespace {

void _emit_leaf(const shared_root*           root,
                mc::shm::shm_allocator&      alloc,
                const trie_node*             node,
                std::vector<const subscription_entry*>& out)
{
    if (node == nullptr) {
        return;
    }
    std::int64_t cur = node->leaf_list_head;
    while (cur != 0) {
        const auto* e = reinterpret_cast<const subscription_entry*>(
            reinterpret_cast<const std::byte*>(root) + cur);
        out.push_back(e);
        cur = e->next_in_leaf;
    }
}

// 下行 type/sender/dest/iface/member 这种"固定值层"。同时尝试 exact 与 "*"。
template <typename Continuation>
void _descend_fixed(const shared_root*      root,
                    mc::shm::shm_allocator& alloc,
                    const trie_node*        node,
                    mc::string_view         actual_value,
                    Continuation&&          continuation)
{
    if (node == nullptr) {
        return;
    }
    auto cm = _wrap_children(*node, alloc);

    auto try_branch = [&](mc::string_view key) {
        auto p = cm.find(key);
        if (!p) {
            return;
        }
        const auto* child = reinterpret_cast<const trie_node*>(
            reinterpret_cast<const std::byte*>(root) + *p.value);
        continuation(child);
    };

    try_branch(actual_value);
    if (actual_value != wildcard_single) {
        try_branch(wildcard_single);
    }
}

// path 层下行：递归吃光 message 的 path segments 后再按 PATH_END 转去 iface。
void _descend_path(const shared_root*               root,
                   mc::shm::shm_allocator&          alloc,
                   const trie_node*                 node,
                   const std::vector<mc::string>&   path_segments,
                   std::size_t                      idx,
                   const message_encoding&          msg_enc,
                   std::vector<const subscription_entry*>& out);

void _descend_member(const shared_root*      root,
                     mc::shm::shm_allocator& alloc,
                     const trie_node*        node,
                     mc::string_view         actual_member,
                     std::vector<const subscription_entry*>& out)
{
    _descend_fixed(root, alloc, node, actual_member, [&](const trie_node* child) {
        _emit_leaf(root, alloc, child, out);
    });
}

void _descend_interface(const shared_root*      root,
                        mc::shm::shm_allocator& alloc,
                        const trie_node*        node,
                        const message_encoding& msg_enc,
                        std::vector<const subscription_entry*>& out)
{
    _descend_fixed(root, alloc, node, msg_enc.interface_name, [&](const trie_node* child) {
        _descend_member(root, alloc, child, msg_enc.member_name, out);
    });
}

void _descend_path(const shared_root*               root,
                   mc::shm::shm_allocator&          alloc,
                   const trie_node*                 node,
                   const std::vector<mc::string>&   path_segments,
                   std::size_t                      idx,
                   const message_encoding&          msg_enc,
                   std::vector<const subscription_entry*>& out)
{
    if (node == nullptr) {
        return;
    }
    auto cm = _wrap_children(*node, alloc);

    auto find_child = [&](mc::string_view key) -> const trie_node* {
        auto p = cm.find(key);
        if (!p) {
            return nullptr;
        }
        return reinterpret_cast<const trie_node*>(
            reinterpret_cast<const std::byte*>(root) + *p.value);
    };

    bool path_done = (idx >= path_segments.size());

    if (!path_done) {
        // 1) exact 段
        if (auto* child = find_child(path_segments[idx])) {
            _descend_path(root, alloc, child, path_segments, idx + 1, msg_enc, out);
        }
        // 2) "*" 单段通配
        if (auto* child = find_child(wildcard_single)) {
            _descend_path(root, alloc, child, path_segments, idx + 1, msg_enc, out);
        }
    }

    // 3) "**" 多段通配（消耗 0+ 剩余段，必须接 PATH_END 才能转 iface）
    if (auto* dstar = find_child(wildcard_multi)) {
        // 通常 ** 后挂的就是 PATH_END
        auto         dcm   = _wrap_children(*dstar, alloc);
        auto         p     = dcm.find(path_end_sentinel);
        if (p) {
            const auto* end_node = reinterpret_cast<const trie_node*>(
                reinterpret_cast<const std::byte*>(root) + *p.value);
            _descend_interface(root, alloc, end_node, msg_enc, out);
        }
    }

    // 4) PATH_END：当前消息 path 段已用完时才能走
    if (path_done) {
        if (auto* end_node = find_child(path_end_sentinel)) {
            _descend_interface(root, alloc, end_node, msg_enc, out);
        }
    }
}

} // namespace (anon)

std::vector<target> shared_table::find_targets(const message& msg) const
{
    auto enc   = _encode_message(msg.header);
    auto alloc = m_runtime.user_arena();

    std::vector<const subscription_entry*> hits;

    {
        mc::shm::ipc_shared_lock_guard guard(*m_lock);
        if (m_root == nullptr) {
            return {};
        }
        const auto* type_node = _ptr_from_root<trie_node>(m_root, m_root->root_node_offset);
        // type → sender → dest → path-trie → iface → member
        _descend_fixed(m_root, alloc, type_node, enc.type_token, [&](const trie_node* sender_node) {
            _descend_fixed(m_root, alloc, sender_node, enc.sender, [&](const trie_node* dest_node) {
                _descend_fixed(m_root, alloc, dest_node, enc.destination, [&](const trie_node* path_node) {
                    _descend_path(m_root, alloc, path_node, enc.path_segments, 0, enc, hits);
                });
            });
        });
    }

    // 锁外 evaluate filter，避免在 SHM 锁内长时间 CPU。
    auto&               registry = filter_backend_registry::instance();
    std::vector<target> result;
    result.reserve(hits.size());
    for (const auto* e : hits) {
        // filter 终判
        if (e->filter_backend_type != no_filter) {
            mc::string_view text(reinterpret_cast<const char*>(e->filter_text.data()),
                                 e->filter_text.size());
            if (!registry.evaluate(e->filter_backend_type, text, msg)) {
                continue;
            }
        }
        target t;
        t.endpoint_id  = e->owner_endpoint_id;
        t.instance_id  = e->owner_instance_id;
        t.service_name = mc::string(e->service_name.as_string_view());
        t.id           = e->match_id;
        result.push_back(std::move(t));
    }
    return result;
}

match_callback shared_table::lookup_callback(mc::string_view service_name, match_id id) const
{
    if (id == 0) {
        return {};
    }
    std::lock_guard lock(m_local_mutex);
    auto            it = m_local_callbacks.find(id);
    if (it == m_local_callbacks.end()) {
        return {};
    }
    if (!service_name.empty() && it->second.service_name != service_name) {
        return {};
    }
    return it->second.callback;
}

} // namespace mc::engine::match

#endif // MCENGINE_USE_SHM
