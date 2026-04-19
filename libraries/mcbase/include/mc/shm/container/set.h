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

#ifndef MC_SHM_CONTAINER_SET_H
#define MC_SHM_CONTAINER_SET_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iterator>
#include <type_traits>
#include <utility>

#include <mc/common.h>
#include <mc/shm/allocator.h>
#include <mc/shm/container/guard.h>
#include <mc/shm/container/journal.h>
#include <mc/shm/container/node_header.h>
#include <mc/shm/ipc_mutex.h>

// mc::shm::container::set<Key, Compare>
//
// 崩溃安全的跨进程有序集合（skip list 实现）。
// 设计详见 docs/plans/2026-04-17-shm-crash-safe-containers-design.md §6.3。
//
// 支持两类 Key：
//   1) trivially_copyable + trivially_destructible 的 POD（int / struct 等）
//   2) move_constructible 的 owning 类型，例如 mc::shm::string
//      - Key 对齐须 <=16；move 后必须仍然自洽（self-rel 指针等）
//      - erase / clear 会显式调用 ~Key() 释放二级分配
// Compare 必须是 stateless、default-constructible、noexcept 的严格弱序比较

namespace mc::shm::container {

// ---- 常量 ----
// max_level 固定 12；期望支持 ~ 65536 元素仍保持 O(log N) 查询
constexpr std::uint32_t set_max_level = 12;

namespace detail {

// ==========================================================================
// set_node_common —— 变长节点公共前缀
// ==========================================================================
//
// 节点内存布局（L = node.level；k_prefix = sizeof(set_node_common) = 32，含编译器插入
// 的 8B 尾部 padding 使得结构体 16B 对齐）：
//   [0   .. 16)              node_header (16B)
//   [16  .. 20)              level        (uint32, 1..set_max_level)
//   [20  .. 24)              _pad         (uint32, reserved)
//   [24  .. 32)              _trailing_pad (因 alignas(16) 自然产生)
//   [32  .. 32+8L)           forward_offset[L]  : int64[]  (self-rel to set_control，0=end)
//   [32+8L .. 32+8L+8)       prev_offset        : int64    (仅 layer 0 语义)
//   [32+8L+8 .. 32+16L+8)    anchor_backup[L]   : int64[]
//   [32+16L+8 .. 32+16L+8+key_size)  key_blob
//
// 所有 offset 均为 self-relative to set_control 基址（int64，0 表示 null）。
struct alignas(16) set_node_common {
    node_header   header;       // 16B；owner_offset 存 set_control.self_tag
    std::uint32_t level;        // 层高 [1, set_max_level]
    std::uint32_t _pad;         // reserved (=0)
    // 之后 8B 尾部 padding（alignas(16) 产生）；再之后是变长区域
};

static_assert(sizeof(set_node_common) == 32, "set_node_common 前缀必须是 32 字节");

// 计算节点总字节数（含 forward / prev / anchor_backup / key_blob）
MC_API std::size_t set_node_bytes(std::uint32_t level, std::size_t key_size) noexcept;

// 按 level + key_size 返回节点对齐（max(16, alignof(Key))；Key 对齐由调用方保证 <= 16）
MC_API std::size_t set_node_align(std::size_t key_align) noexcept;

// ---- 访问器（读写均以 self-rel offset 为单位，调用者需自行 from_offset） ----

// forward_offset[i] 的地址（i ∈ [0, node.level)）
MC_API std::int64_t* set_node_forward_ptr(set_node_common* node, std::uint32_t i) noexcept;
MC_API const std::int64_t* set_node_forward_ptr(const set_node_common* node,
                                                std::uint32_t          i) noexcept;

// 第 0 层 prev_offset（双向链，仅最底层）
MC_API std::int64_t* set_node_prev_ptr(set_node_common* node) noexcept;
MC_API const std::int64_t* set_node_prev_ptr(const set_node_common* node) noexcept;

// anchor_backup[i] 的地址
MC_API std::int64_t* set_node_anchor_ptr(set_node_common* node, std::uint32_t i) noexcept;
MC_API const std::int64_t* set_node_anchor_ptr(const set_node_common* node,
                                               std::uint32_t          i) noexcept;

// key_blob 的地址
MC_API void* set_node_key_ptr(set_node_common* node, std::uint32_t level_for_layout) noexcept;
MC_API const void* set_node_key_ptr(const set_node_common* node,
                                    std::uint32_t          level_for_layout) noexcept;

// self-rel offset 工具（与 list/map 的对应工具同语义；不共用符号是为了在同一 TU
// 内同时包含多个容器头时避免重定义冲突）
inline std::int64_t set_self_offset_from(const void* control, const void* node) noexcept
{
    return static_cast<std::int64_t>(reinterpret_cast<const std::byte*>(node)
                                     - reinterpret_cast<const std::byte*>(control));
}

inline void* set_from_offset(void* control, std::int64_t offset) noexcept
{
    return reinterpret_cast<std::byte*>(control) + offset;
}

inline const void* set_from_offset(const void* control, std::int64_t offset) noexcept
{
    return reinterpret_cast<const std::byte*>(control) + offset;
}

} // namespace detail

// ==========================================================================
// set_control —— 容器控制块（固定大小）
// ==========================================================================
struct MC_API set_control {
    ipc_mutex                  mutex;
    container_journal          journal;
    // 头哨兵 forward 数组：index i 表示最底第 i 层的 head.next；0 = 空层
    std::int64_t               head_forward[set_max_level];
    std::uint32_t              self_tag;       // 反向归属校验
    std::uint32_t              key_size;       // Key 字节数
    std::uint32_t              key_align;      // Key 对齐（<=16）
    std::uint32_t              max_level;      // 运行期最大层（初始 set_max_level）
    std::atomic<std::uint64_t> size;           // 元素数量 hint
    std::atomic<std::uint64_t> degraded_count; // recover 累计
    std::atomic<std::uint64_t> rng_state;      // random_level 的 xorshift 状态
    std::uint8_t               _reserved[16];
};

// 初始化控制块：清 0 + journal/mutex 初始化 + 写入 key_size/align/self_tag
MC_API void set_control_init(set_control& ctrl, std::uint32_t self_tag,
                             std::size_t key_size, std::size_t key_align) noexcept;

// 崩溃恢复入口（持 ctrl.mutex 时调用；container_guard 自动触发）
// 需提供 Compare（运行期函数指针版本）用于 re-find predecessors
// core 层不引入模板；由 set<K, Compare> 的 static 回调桥接
using set_compare_fn = int (*)(const void* a, const void* b, std::size_t key_size);
MC_API void set_recover(set_control& ctrl, set_compare_fn cmp) noexcept;

MC_API std::size_t set_degraded_count(const set_control& ctrl) noexcept;

// ==========================================================================
// 非模板 core API（供 set<K, Compare> 模板 wrapper 调用；避免二进制膨胀）
// ==========================================================================
namespace detail {

// find_predecessors：对给定 key 找每层的 predecessor，结果写入 update[max_level]
// 返回值：layer-0 上如果找到相同 key，返回该节点指针；否则 nullptr
// 调用者持 ctrl.mutex
MC_API set_node_common* set_find_predecessors(set_control& ctrl, const void* key,
                                              set_compare_fn cmp,
                                              std::int64_t*  update_out) noexcept;

MC_API const set_node_common* set_find_const(const set_control& ctrl, const void* key,
                                             set_compare_fn cmp) noexcept;

// lower_bound / upper_bound 非模板实现；模板 wrapper 通过 Compare 桥接 set_compare_fn
MC_API const set_node_common* set_lower_bound_impl(const set_control& ctrl, const void* key,
                                                   set_compare_fn cmp) noexcept;
MC_API const set_node_common* set_upper_bound_impl(const set_control& ctrl, const void* key,
                                                   set_compare_fn cmp) noexcept;

// 给定 layer-0 上已链入的节点，返回下一节点（nullptr 表示已到尾）
MC_API const set_node_common* set_node_next_impl(const set_control& ctrl,
                                                 const set_node_common* node) noexcept;

// random_level：使用 ctrl.rng_state 推进的 xorshift，返回 [1, max_level] 的层高
MC_API std::uint32_t set_random_level(set_control& ctrl) noexcept;

// 在 update[] 所标识的位置插入 new_node
//   - node 必须已 allocate + header_init + level/forward/anchor/key 写好
//   - 按 top-down 顺序 splice（layer 0 最后提交，作为可见性 commit 点）
//   - journal_begin / journal_end 由本函数内部管理
MC_API void set_insert_commit(set_control& ctrl, set_node_common* new_node,
                              const std::int64_t* update_in) noexcept;

// 删除指定节点：先 find_predecessors，然后按 top-down 从每层切除
// 返回 true 成功；false 表示节点不存在或归属错误
MC_API bool set_erase_commit(set_control& ctrl, set_node_common* node,
                             set_compare_fn cmp) noexcept;

// container_guard recover 回调桥（数据传 set_compare_fn_data*）
struct set_recover_cb_data {
    set_control*   control;
    set_compare_fn cmp;
};
MC_API void set_recover_cb(void* data) noexcept;

// 持锁态下 prepare 一个新节点（allocate + header_init + 字段置 0），返回 nullptr 表示 OOM
// level 由调用者提前决定；key 数据由调用者写入
MC_API set_node_common* set_prepare_node(set_control& ctrl, shm_allocator& alloc,
                                         std::uint32_t level) noexcept;

// 校验节点 header 归属本 ctrl
MC_API bool set_node_belongs_to(const set_control& ctrl, const set_node_common* node) noexcept;

} // namespace detail

// ==========================================================================
// set<Key, Compare> 模板 wrapper
// ==========================================================================
template <typename Key, typename Compare = std::less<Key>>
class set {
public:
    static_assert(std::is_move_constructible_v<Key>,
                  "mc::shm::container::set<Key>: Key 必须可 move 构造");
    static_assert(alignof(Key) <= 16,
                  "mc::shm::container::set<Key>: Key 对齐不能超过 16 字节");
    static_assert(std::is_empty_v<Compare> && std::is_default_constructible_v<Compare>
                      && std::is_invocable_r_v<bool, Compare, const Key&, const Key&>,
                  "mc::shm::container::set<Key, Compare>: Compare 必须是 stateless + "
                  "default-constructible + 可调用 bool(const Key&, const Key&)");

    class iterator;
    class const_iterator;

    set(set_control& control, shm_allocator& alloc) noexcept
        : m_control(&control), m_alloc(&alloc)
    {
    }

    set(const set&)            = delete;
    set& operator=(const set&) = delete;
    set(set&&)                 = default;
    set& operator=(set&&)      = default;

    static void init(set_control& control, std::uint32_t self_tag) noexcept
    {
        set_control_init(control, self_tag, sizeof(Key), alignof(Key));
    }

    std::size_t size() const noexcept
    {
        return m_control->size.load(std::memory_order_acquire);
    }
    bool empty() const noexcept { return size() == 0; }
    std::size_t degraded_count() const noexcept
    {
        return m_control->degraded_count.load(std::memory_order_acquire);
    }

    // 插入 key；若已存在则返回 {existing_key_ptr, false}；成功返回 {new_key_ptr, true}
    // OOM 返回 {nullptr, false}。两个重载：const-ref 走 memcpy/copy-construct，
    // rvalue-ref 走 move-construct（owning Key 如 shm::string 需用此形式转移所有权）
    std::pair<const Key*, bool> insert(const Key& key) noexcept;
    std::pair<const Key*, bool> insert(Key&& key) noexcept;

    // 查找；不持锁仅做一致读，调用者需自行保证容器不在并发修改
    // （典型调用路径：在用户已持相应 region 锁或调用方读取时期）
    const Key* find(const Key& key) const noexcept;

    // Heterogeneous find：调用方提供 Probe 类型（例如 std::string_view），
    // Compare 必须支持 Compare{}(const Key&, const Probe&) 和
    // Compare{}(const Probe&, const Key&)。用于 shm::string key 场景。
    template <typename Probe>
    const Key* find(const Probe& probe) const noexcept;

    // lower_bound：返回第一个 >= key 的迭代器；无则 end()
    const_iterator lower_bound(const Key& key) const noexcept;
    template <typename Probe>
    const_iterator lower_bound(const Probe& probe) const noexcept;

    // upper_bound：返回第一个 > key 的迭代器；无则 end()
    const_iterator upper_bound(const Key& key) const noexcept;
    template <typename Probe>
    const_iterator upper_bound(const Probe& probe) const noexcept;

    // equal_range：[lower_bound(key), upper_bound(key))；key 不存在时两端相等
    std::pair<const_iterator, const_iterator> equal_range(const Key& key) const noexcept;
    template <typename Probe>
    std::pair<const_iterator, const_iterator> equal_range(const Probe& probe) const noexcept;

    // 删除；返回 true 成功删除，false 表示不存在
    bool erase(const Key& key) noexcept;

    template <typename Probe>
    bool erase(const Probe& probe) noexcept;

    // 清空：持锁析构所有 Key + free 所有 node（单次事务，中途崩溃 recover 继续）
    void clear() noexcept;

    iterator       begin() noexcept;
    iterator       end() noexcept { return iterator{m_control, nullptr}; }
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept { return const_iterator{m_control, nullptr}; }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }

    set_control*       control() noexcept { return m_control; }
    const set_control* control() const noexcept { return m_control; }
    shm_allocator*     allocator() noexcept { return m_alloc; }

    static const Key* key_ptr(const detail::set_node_common* node) noexcept
    {
        return static_cast<const Key*>(detail::set_node_key_ptr(node, node->level));
    }

private:
    // 运行期比较函数桥：Compare{}(a, b) → -1 / 0 / 1
    static int compare_bridge(const void* a, const void* b, std::size_t /*size*/) noexcept
    {
        const Key& ka = *static_cast<const Key*>(a);
        const Key& kb = *static_cast<const Key*>(b);
        Compare    c{};
        if (c(ka, kb)) {
            return -1;
        }
        if (c(kb, ka)) {
            return 1;
        }
        return 0;
    }

    // Heterogeneous 查找桥：a = Key*, b = Probe*
    template <typename Probe>
    static int compare_probe_bridge(const void* a, const void* b, std::size_t /*size*/) noexcept
    {
        const Key&   ka = *static_cast<const Key*>(a);
        const Probe& kp = *static_cast<const Probe*>(b);
        Compare      c{};
        if (c(ka, kp)) {
            return -1;
        }
        if (c(kp, ka)) {
            return 1;
        }
        return 0;
    }

    // 统一 insert 核心：以 ConstructFn 在 key_blob 处构造 Key
    template <typename ConstructFn>
    std::pair<const Key*, bool> insert_impl(const Key&        probe,
                                            ConstructFn&&     construct) noexcept;

    set_control*   m_control;
    shm_allocator* m_alloc;
};

// ==========================================================================
// iterator（forward + prev 双向，但 ++ / -- 都通过第 0 层 forward/prev）
// ==========================================================================
template <typename Key, typename Compare>
class set<Key, Compare>::iterator {
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type        = const Key;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const Key*;
    using reference         = const Key&;

    iterator() noexcept = default;
    iterator(set_control* ctrl, detail::set_node_common* node) noexcept
        : m_control(ctrl), m_node(node)
    {
    }

    reference operator*() const noexcept { return *set<Key, Compare>::key_ptr(m_node); }
    pointer   operator->() const noexcept { return set<Key, Compare>::key_ptr(m_node); }

    iterator& operator++() noexcept
    {
        if (m_node != nullptr) {
            const auto off = *detail::set_node_forward_ptr(m_node, 0);
            m_node = off == 0 ? nullptr
                              : reinterpret_cast<detail::set_node_common*>(
                                    detail::set_from_offset(m_control, off));
        }
        return *this;
    }

    iterator operator++(int) noexcept
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    bool operator==(const iterator& other) const noexcept { return m_node == other.m_node; }
    bool operator!=(const iterator& other) const noexcept { return m_node != other.m_node; }

    detail::set_node_common* node() const noexcept { return m_node; }

private:
    friend class set<Key, Compare>::const_iterator;
    set_control*             m_control = nullptr;
    detail::set_node_common* m_node    = nullptr;
};

template <typename Key, typename Compare>
class set<Key, Compare>::const_iterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type        = const Key;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const Key*;
    using reference         = const Key&;

    const_iterator() noexcept = default;
    const_iterator(const set_control* ctrl, const detail::set_node_common* node) noexcept
        : m_control(ctrl), m_node(node)
    {
    }
    const_iterator(const iterator& it) noexcept : m_control(it.m_control), m_node(it.m_node) {}

    reference operator*() const noexcept { return *set<Key, Compare>::key_ptr(m_node); }
    pointer   operator->() const noexcept { return set<Key, Compare>::key_ptr(m_node); }

    const_iterator& operator++() noexcept
    {
        if (m_node != nullptr) {
            const auto off = *detail::set_node_forward_ptr(m_node, 0);
            m_node = off == 0 ? nullptr
                              : reinterpret_cast<const detail::set_node_common*>(
                                    detail::set_from_offset(m_control, off));
        }
        return *this;
    }

    const_iterator operator++(int) noexcept
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    bool operator==(const const_iterator& other) const noexcept { return m_node == other.m_node; }
    bool operator!=(const const_iterator& other) const noexcept { return m_node != other.m_node; }

private:
    const set_control*             m_control = nullptr;
    const detail::set_node_common* m_node    = nullptr;
};

// ==========================================================================
// 模板实现
// ==========================================================================

template <typename Key, typename Compare>
template <typename ConstructFn>
std::pair<const Key*, bool>
set<Key, Compare>::insert_impl(const Key& probe, ConstructFn&& construct) noexcept
{
    detail::set_recover_cb_data cb{m_control, &compare_bridge};
    container_guard             guard(m_control->mutex, detail::set_recover_cb, &cb);

    std::int64_t update[set_max_level] = {0};
    if (auto* existing = detail::set_find_predecessors(*m_control, &probe, &compare_bridge, update);
        existing != nullptr) {
        return {key_ptr(existing), false};
    }

    const std::uint32_t level = detail::set_random_level(*m_control);
    auto*               node  = detail::set_prepare_node(*m_control, *m_alloc, level);
    if (node == nullptr) {
        return {nullptr, false};
    }

    // 在 key_blob 处构造 Key（memcpy or placement new + move）
    void* key_slot = detail::set_node_key_ptr(node, level);
    construct(key_slot);

    for (std::uint32_t i = 0; i < level; ++i) {
        std::int64_t next_off = 0;
        if (update[i] != 0) {
            auto* pred = reinterpret_cast<detail::set_node_common*>(
                detail::set_from_offset(m_control, update[i]));
            next_off = *detail::set_node_forward_ptr(pred, i);
        } else {
            next_off = m_control->head_forward[i];
        }
        *detail::set_node_forward_ptr(node, i) = next_off;
        *detail::set_node_anchor_ptr(node, i)  = next_off;
    }
    *detail::set_node_prev_ptr(node) = update[0];

    detail::set_insert_commit(*m_control, node, update);
    return {key_ptr(node), true};
}

template <typename Key, typename Compare>
std::pair<const Key*, bool> set<Key, Compare>::insert(const Key& key) noexcept
{
    return insert_impl(key, [&](void* slot) noexcept {
        if constexpr (std::is_trivially_copyable_v<Key>) {
            std::memcpy(slot, &key, sizeof(Key));
        } else {
            new (slot) Key(key);
        }
    });
}

template <typename Key, typename Compare>
std::pair<const Key*, bool> set<Key, Compare>::insert(Key&& key) noexcept
{
    return insert_impl(key, [&](void* slot) noexcept {
        new (slot) Key(std::move(key));
    });
}

template <typename Key, typename Compare>
const Key* set<Key, Compare>::find(const Key& key) const noexcept
{
    auto* node = detail::set_find_const(*m_control, &key, &compare_bridge);
    return node == nullptr ? nullptr : key_ptr(node);
}

template <typename Key, typename Compare>
template <typename Probe>
const Key* set<Key, Compare>::find(const Probe& probe) const noexcept
{
    auto* node = detail::set_find_const(*m_control, &probe, &compare_probe_bridge<Probe>);
    return node == nullptr ? nullptr : key_ptr(node);
}

template <typename Key, typename Compare>
typename set<Key, Compare>::const_iterator
set<Key, Compare>::lower_bound(const Key& key) const noexcept
{
    const auto* node = detail::set_lower_bound_impl(*m_control, &key, &compare_bridge);
    return const_iterator{m_control, node};
}

template <typename Key, typename Compare>
template <typename Probe>
typename set<Key, Compare>::const_iterator
set<Key, Compare>::lower_bound(const Probe& probe) const noexcept
{
    const auto* node = detail::set_lower_bound_impl(*m_control, &probe, &compare_probe_bridge<Probe>);
    return const_iterator{m_control, node};
}

template <typename Key, typename Compare>
typename set<Key, Compare>::const_iterator
set<Key, Compare>::upper_bound(const Key& key) const noexcept
{
    const auto* node = detail::set_upper_bound_impl(*m_control, &key, &compare_bridge);
    return const_iterator{m_control, node};
}

template <typename Key, typename Compare>
template <typename Probe>
typename set<Key, Compare>::const_iterator
set<Key, Compare>::upper_bound(const Probe& probe) const noexcept
{
    const auto* node = detail::set_upper_bound_impl(*m_control, &probe, &compare_probe_bridge<Probe>);
    return const_iterator{m_control, node};
}

template <typename Key, typename Compare>
std::pair<typename set<Key, Compare>::const_iterator, typename set<Key, Compare>::const_iterator>
set<Key, Compare>::equal_range(const Key& key) const noexcept
{
    return {lower_bound(key), upper_bound(key)};
}

template <typename Key, typename Compare>
template <typename Probe>
std::pair<typename set<Key, Compare>::const_iterator, typename set<Key, Compare>::const_iterator>
set<Key, Compare>::equal_range(const Probe& probe) const noexcept
{
    return {lower_bound(probe), upper_bound(probe)};
}

template <typename Key, typename Compare>
bool set<Key, Compare>::erase(const Key& key) noexcept
{
    detail::set_recover_cb_data cb{m_control, &compare_bridge};
    container_guard             guard(m_control->mutex, detail::set_recover_cb, &cb);

    std::int64_t update[set_max_level] = {0};
    auto* node = detail::set_find_predecessors(*m_control, &key, &compare_bridge, update);
    if (node == nullptr) {
        return false;
    }
    // 先 unlink + 标记 free（erase_commit 内部还要以 key 做 re-find，不能先析构）；
    // commit 成功后再析构 Key 以释放二级分配
    Key*       kp = const_cast<Key*>(key_ptr(node));
    const bool ok = detail::set_erase_commit(*m_control, node, &compare_bridge);
    if (ok) {
        if constexpr (!std::is_trivially_destructible_v<Key>) {
            kp->~Key();
        }
    }
    return ok;
}

template <typename Key, typename Compare>
template <typename Probe>
bool set<Key, Compare>::erase(const Probe& probe) noexcept
{
    detail::set_recover_cb_data cb{m_control, &compare_bridge};
    container_guard             guard(m_control->mutex, detail::set_recover_cb, &cb);

    std::int64_t update[set_max_level] = {0};
    auto* node = detail::set_find_predecessors(*m_control, &probe,
                                               &compare_probe_bridge<Probe>, update);
    if (node == nullptr) {
        return false;
    }
    Key*       kp = const_cast<Key*>(key_ptr(node));
    const bool ok = detail::set_erase_commit(*m_control, node, &compare_bridge);
    if (ok) {
        if constexpr (!std::is_trivially_destructible_v<Key>) {
            kp->~Key();
        }
    }
    return ok;
}

template <typename Key, typename Compare>
void set<Key, Compare>::clear() noexcept
{
    detail::set_recover_cb_data cb{m_control, &compare_bridge};
    container_guard             guard(m_control->mutex, detail::set_recover_cb, &cb);

    // 从第 0 层头指针开始，逐个 unlink（erase_commit）然后析构 Key
    while (m_control->head_forward[0] != 0) {
        auto* node = reinterpret_cast<detail::set_node_common*>(
            detail::set_from_offset(m_control, m_control->head_forward[0]));
        if (node == nullptr) {
            break;
        }
        Key* kp = const_cast<Key*>(key_ptr(node));
        if (!detail::set_erase_commit(*m_control, node, &compare_bridge)) {
            break; // 损坏节点：交给 fsck 处理
        }
        if constexpr (!std::is_trivially_destructible_v<Key>) {
            kp->~Key();
        }
    }
}

template <typename Key, typename Compare>
typename set<Key, Compare>::iterator set<Key, Compare>::begin() noexcept
{
    const auto off = m_control->head_forward[0];
    return iterator{m_control, off == 0 ? nullptr
                                        : reinterpret_cast<detail::set_node_common*>(
                                              detail::set_from_offset(m_control, off))};
}

template <typename Key, typename Compare>
typename set<Key, Compare>::const_iterator set<Key, Compare>::begin() const noexcept
{
    const auto off = m_control->head_forward[0];
    return const_iterator{m_control,
                          off == 0 ? nullptr
                                   : reinterpret_cast<const detail::set_node_common*>(
                                         detail::set_from_offset(m_control, off))};
}

} // namespace mc::shm::container

#endif // MC_SHM_CONTAINER_SET_H
