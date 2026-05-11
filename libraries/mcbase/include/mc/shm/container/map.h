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

#ifndef MC_SHM_CONTAINER_MAP_H
#define MC_SHM_CONTAINER_MAP_H

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

// mc::shm::container::map<Key, Value, Compare>
//
// 崩溃安全的跨进程有序映射（skip list 实现，与 set 同源）。
//
// 支持的 Key / Value：
//   - move_constructible + destructible；align <= 16
//   - Value 支持 shm::string 等 owning 类型；erase / clear 会显式析构 Value
// Compare：stateless、default-constructible、noexcept，只比较 Key
//
// API 风格：
//   - try_emplace(K, V) → {mapped_ptr, inserted}：key 已存在则 V 不被消费（rvalue 尚未 move）
//   - erase(K) / erase(Probe) / clear()
//   - find(K) / find(Probe) → mapped_ptr（写）/ const_mapped_ptr（读）
//
// 价值变更：不提供原地 update API（即不扩展 journal）。业务侧要改 V 必须 erase + try_emplace
// 两次独立事务。这样 journal 不引入 update 语义，可靠性零新增风险。

namespace mc::shm::container {

constexpr std::uint32_t map_max_level = 12;

namespace detail {

// ==========================================================================
// map_node_common —— 变长节点公共前缀（32B，与 set_node_common 对称）
// ==========================================================================
// 节点内存布局（L = node.level）：
//   [0   .. 16)                      node_header (16B)
//   [16  .. 20)                      level
//   [20  .. 24)                      _pad
//   [24  .. 32)                      _trailing_pad
//   [32  .. 32+8L)                   forward_offset[L]
//   [32+8L .. 32+8L+8)               prev_offset
//   [32+8L+8 .. 32+16L+8)            anchor_backup[L]
//   [align_to(key_align))            key_blob（key_size 字节）
//   [align_to(value_align))          value_blob（value_size 字节）
// 节点整体对齐 = max(16, alignof(K), alignof(V))，最大 16
struct alignas(16) map_node_common {
    node_header   header;
    std::uint32_t level;
    std::uint32_t _pad;
};

static_assert(sizeof(map_node_common) == 32, "map_node_common 前缀必须是 32 字节");

// 计算节点总字节数
MC_API std::size_t map_node_bytes(std::uint32_t level, std::size_t key_size,
                                  std::size_t key_align, std::size_t value_size,
                                  std::size_t value_align) noexcept;

// 节点整体对齐 = max(16, key_align, value_align)
MC_API std::size_t map_node_align(std::size_t key_align, std::size_t value_align) noexcept;

// offset 访问器
MC_API std::int64_t*       map_node_forward_ptr(map_node_common* node, std::uint32_t i) noexcept;
MC_API const std::int64_t* map_node_forward_ptr(const map_node_common* node,
                                                std::uint32_t i) noexcept;
MC_API std::int64_t*       map_node_prev_ptr(map_node_common* node) noexcept;
MC_API const std::int64_t* map_node_prev_ptr(const map_node_common* node) noexcept;
MC_API std::int64_t*       map_node_anchor_ptr(map_node_common* node, std::uint32_t i) noexcept;
MC_API const std::int64_t* map_node_anchor_ptr(const map_node_common* node,
                                               std::uint32_t i) noexcept;

// key_ptr / value_ptr —— 依赖 ctrl.key_align / ctrl.value_align，不是纯 level 函数
MC_API void*       map_node_key_ptr(map_node_common* node, std::uint32_t level,
                                    std::size_t key_align) noexcept;
MC_API const void* map_node_key_ptr(const map_node_common* node, std::uint32_t level,
                                    std::size_t key_align) noexcept;
MC_API void*       map_node_value_ptr(map_node_common* node, std::uint32_t level,
                                      std::size_t key_size, std::size_t key_align,
                                      std::size_t value_align) noexcept;
MC_API const void* map_node_value_ptr(const map_node_common* node, std::uint32_t level,
                                      std::size_t key_size, std::size_t key_align,
                                      std::size_t value_align) noexcept;

inline std::int64_t map_self_offset_from(const void* control, const void* node) noexcept
{
    return static_cast<std::int64_t>(reinterpret_cast<const std::byte*>(node)
                                     - reinterpret_cast<const std::byte*>(control));
}

inline void* map_from_offset(void* control, std::int64_t offset) noexcept
{
    return reinterpret_cast<std::byte*>(control) + offset;
}

inline const void* map_from_offset(const void* control, std::int64_t offset) noexcept
{
    return reinterpret_cast<const std::byte*>(control) + offset;
}

} // namespace detail

// ==========================================================================
// map_control —— 控制块
// ==========================================================================
struct MC_API map_control {
    ipc_mutex                  mutex;
    container_journal          journal;
    std::int64_t               head_forward[map_max_level];
    std::uint32_t              self_tag;
    std::uint32_t              key_size;
    std::uint32_t              key_align;
    std::uint32_t              value_size;
    std::uint32_t              value_align;
    std::uint32_t              max_level;
    std::atomic<std::uint64_t> size;
    std::atomic<std::uint64_t> degraded_count;
    std::atomic<std::uint64_t> rng_state;
    std::uint8_t               _reserved[16];
};

MC_API void map_control_init(map_control& ctrl, std::uint32_t self_tag,
                             std::size_t key_size, std::size_t key_align,
                             std::size_t value_size, std::size_t value_align) noexcept;

// recover/compare：同 set，运行期函数指针桥
using map_compare_fn = int (*)(const void* a, const void* b, std::size_t key_size);
MC_API void        map_recover(map_control& ctrl, map_compare_fn cmp) noexcept;
MC_API std::size_t map_degraded_count(const map_control& ctrl) noexcept;

// ==========================================================================
// 非模板 core API
// ==========================================================================
namespace detail {

MC_API map_node_common* map_find_predecessors(map_control& ctrl, const void* key,
                                              map_compare_fn cmp,
                                              std::int64_t*  update_out) noexcept;

MC_API const map_node_common* map_find_const(const map_control& ctrl, const void* key,
                                             map_compare_fn cmp) noexcept;

// lower_bound / upper_bound / node_next 非模板实现
MC_API const map_node_common* map_lower_bound_impl(const map_control& ctrl, const void* key,
                                                   map_compare_fn cmp) noexcept;
MC_API const map_node_common* map_upper_bound_impl(const map_control& ctrl, const void* key,
                                                   map_compare_fn cmp) noexcept;
MC_API const map_node_common* map_node_next_impl(const map_control& ctrl,
                                                 const map_node_common* node) noexcept;

MC_API std::uint32_t map_random_level(map_control& ctrl) noexcept;

MC_API void map_insert_commit(map_control& ctrl, map_node_common* new_node,
                              const std::int64_t* update_in) noexcept;

MC_API bool map_erase_commit(map_control& ctrl, map_node_common* node,
                             map_compare_fn cmp) noexcept;

struct map_recover_cb_data {
    map_control*   control;
    map_compare_fn cmp;
};
MC_API void map_recover_cb(void* data) noexcept;

MC_API map_node_common* map_prepare_node(map_control& ctrl, shm_allocator& alloc,
                                         std::uint32_t level) noexcept;

MC_API bool map_node_belongs_to(const map_control& ctrl,
                                const map_node_common* node) noexcept;

} // namespace detail

// ==========================================================================
// map<Key, Value, Compare>
// ==========================================================================
template <typename Key, typename Value, typename Compare = std::less<Key>>
class map {
public:
    static_assert(std::is_move_constructible_v<Key>,
                  "mc::shm::container::map<Key, Value>: Key 必须可 move 构造");
    static_assert(std::is_move_constructible_v<Value>,
                  "mc::shm::container::map<Key, Value>: Value 必须可 move 构造");
    static_assert(alignof(Key) <= 16 && alignof(Value) <= 16,
                  "mc::shm::container::map: Key / Value 对齐不能超过 16 字节");
    static_assert(std::is_empty_v<Compare> && std::is_default_constructible_v<Compare>
                      && std::is_invocable_r_v<bool, Compare, const Key&, const Key&>,
                  "mc::shm::container::map<Key, Value, Compare>: Compare 必须是 stateless "
                  "+ default-constructible");

    struct mapped_ptr {
        const Key* key;
        Value*     value;
        explicit operator bool() const noexcept { return key != nullptr; }
    };
    struct const_mapped_ptr {
        const Key*   key;
        const Value* value;
        explicit operator bool() const noexcept { return key != nullptr; }
    };

    class iterator;
    class const_iterator;

    map(map_control& control, shm_allocator& alloc) noexcept
        : m_control(&control), m_alloc(&alloc)
    {
    }

    map(const map&)            = delete;
    map& operator=(const map&) = delete;
    map(map&&)                 = default;
    map& operator=(map&&)      = default;

    static void init(map_control& control, std::uint32_t self_tag) noexcept
    {
        map_control_init(control, self_tag, sizeof(Key), alignof(Key), sizeof(Value),
                         alignof(Value));
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

    // 插入（key 已存在则返回 {existing, false}，rvalue 的 k/v 均未被 move）
    std::pair<mapped_ptr, bool> try_emplace(const Key& k, const Value& v) noexcept;
    std::pair<mapped_ptr, bool> try_emplace(Key&& k, Value&& v) noexcept;

    mapped_ptr        find(const Key& k) noexcept;
    const_mapped_ptr  find(const Key& k) const noexcept;
    template <typename Probe> mapped_ptr       find(const Probe& p) noexcept;
    template <typename Probe> const_mapped_ptr find(const Probe& p) const noexcept;

    // lower_bound / upper_bound / equal_range（只读 const_iterator；与 set 对称）
    const_iterator lower_bound(const Key& k) const noexcept;
    template <typename Probe> const_iterator lower_bound(const Probe& p) const noexcept;
    const_iterator upper_bound(const Key& k) const noexcept;
    template <typename Probe> const_iterator upper_bound(const Probe& p) const noexcept;
    std::pair<const_iterator, const_iterator> equal_range(const Key& k) const noexcept;
    template <typename Probe>
    std::pair<const_iterator, const_iterator> equal_range(const Probe& p) const noexcept;

    bool erase(const Key& k) noexcept;
    template <typename Probe> bool erase(const Probe& p) noexcept;

    void clear() noexcept;

    iterator       begin() noexcept;
    iterator       end() noexcept { return iterator{m_control, nullptr}; }
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept { return const_iterator{m_control, nullptr}; }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }

    map_control*       control() noexcept { return m_control; }
    const map_control* control() const noexcept { return m_control; }
    shm_allocator*     allocator() noexcept { return m_alloc; }

    // 辅助：从节点取 K / V 指针
    const Key* key_ptr(const detail::map_node_common* node) const noexcept
    {
        return static_cast<const Key*>(detail::map_node_key_ptr(node, node->level, alignof(Key)));
    }
    Value* value_ptr(detail::map_node_common* node) noexcept
    {
        return static_cast<Value*>(detail::map_node_value_ptr(node, node->level, sizeof(Key),
                                                              alignof(Key), alignof(Value)));
    }
    const Value* value_ptr(const detail::map_node_common* node) const noexcept
    {
        return static_cast<const Value*>(detail::map_node_value_ptr(node, node->level,
                                                                    sizeof(Key), alignof(Key),
                                                                    alignof(Value)));
    }

private:
    static int compare_bridge(const void* a, const void* b, std::size_t /*size*/) noexcept
    {
        const Key& ka = *static_cast<const Key*>(a);
        const Key& kb = *static_cast<const Key*>(b);
        Compare    c{};
        if (c(ka, kb)) return -1;
        if (c(kb, ka)) return 1;
        return 0;
    }

    template <typename Probe>
    static int compare_probe_bridge(const void* a, const void* b, std::size_t /*size*/) noexcept
    {
        const Key&   ka = *static_cast<const Key*>(a);
        const Probe& kp = *static_cast<const Probe*>(b);
        Compare      c{};
        if (c(ka, kp)) return -1;
        if (c(kp, ka)) return 1;
        return 0;
    }

    template <typename KeyFn, typename ValueFn>
    std::pair<mapped_ptr, bool> emplace_impl(const Key& probe, KeyFn&& key_fn,
                                              ValueFn&& value_fn) noexcept;

    map_control*   m_control;
    shm_allocator* m_alloc;
};

// ==========================================================================
// iterator
// ==========================================================================
template <typename Key, typename Value, typename Compare>
class map<Key, Value, Compare>::iterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type        = mapped_ptr;
    using difference_type   = std::ptrdiff_t;
    using pointer           = mapped_ptr*;
    using reference         = mapped_ptr;

    iterator() noexcept = default;
    iterator(map_control* ctrl, detail::map_node_common* node) noexcept
        : m_control(ctrl), m_node(node)
    {
    }

    mapped_ptr operator*() const noexcept
    {
        if (m_node == nullptr) {
            return {nullptr, nullptr};
        }
        auto* kp = static_cast<const Key*>(
            detail::map_node_key_ptr(m_node, m_node->level, alignof(Key)));
        auto* vp = static_cast<Value*>(detail::map_node_value_ptr(
            m_node, m_node->level, sizeof(Key), alignof(Key), alignof(Value)));
        return {kp, vp};
    }

    iterator& operator++() noexcept
    {
        if (m_node != nullptr) {
            const auto off = *detail::map_node_forward_ptr(m_node, 0);
            m_node         = off == 0 ? nullptr
                                      : reinterpret_cast<detail::map_node_common*>(
                                            detail::map_from_offset(m_control, off));
        }
        return *this;
    }

    iterator operator++(int) noexcept { auto t = *this; ++*this; return t; }

    bool operator==(const iterator& o) const noexcept { return m_node == o.m_node; }
    bool operator!=(const iterator& o) const noexcept { return m_node != o.m_node; }

    detail::map_node_common* node() const noexcept { return m_node; }

private:
    friend class map<Key, Value, Compare>::const_iterator;
    map_control*             m_control = nullptr;
    detail::map_node_common* m_node    = nullptr;
};

template <typename Key, typename Value, typename Compare>
class map<Key, Value, Compare>::const_iterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type        = const_mapped_ptr;
    using difference_type   = std::ptrdiff_t;

    const_iterator() noexcept = default;
    const_iterator(const map_control* ctrl, const detail::map_node_common* node) noexcept
        : m_control(ctrl), m_node(node)
    {
    }
    const_iterator(const iterator& it) noexcept : m_control(it.m_control), m_node(it.m_node) {}

    const_mapped_ptr operator*() const noexcept
    {
        if (m_node == nullptr) {
            return {nullptr, nullptr};
        }
        auto* kp = static_cast<const Key*>(
            detail::map_node_key_ptr(m_node, m_node->level, alignof(Key)));
        auto* vp = static_cast<const Value*>(detail::map_node_value_ptr(
            m_node, m_node->level, sizeof(Key), alignof(Key), alignof(Value)));
        return {kp, vp};
    }

    const_iterator& operator++() noexcept
    {
        if (m_node != nullptr) {
            const auto off = *detail::map_node_forward_ptr(m_node, 0);
            m_node         = off == 0 ? nullptr
                                      : reinterpret_cast<const detail::map_node_common*>(
                                            detail::map_from_offset(m_control, off));
        }
        return *this;
    }

    const_iterator operator++(int) noexcept { auto t = *this; ++*this; return t; }

    bool operator==(const const_iterator& o) const noexcept { return m_node == o.m_node; }
    bool operator!=(const const_iterator& o) const noexcept { return m_node != o.m_node; }

private:
    const map_control*             m_control = nullptr;
    const detail::map_node_common* m_node    = nullptr;
};

// ==========================================================================
// 模板实现
// ==========================================================================

template <typename Key, typename Value, typename Compare>
template <typename KeyFn, typename ValueFn>
std::pair<typename map<Key, Value, Compare>::mapped_ptr, bool>
map<Key, Value, Compare>::emplace_impl(const Key& probe, KeyFn&& key_fn,
                                        ValueFn&& value_fn) noexcept
{
    detail::map_recover_cb_data cb{m_control, &compare_bridge};
    container_guard             guard(m_control->mutex, detail::map_recover_cb, &cb);

    std::int64_t update[map_max_level] = {0};
    if (auto* existing =
            detail::map_find_predecessors(*m_control, &probe, &compare_bridge, update);
        existing != nullptr) {
        auto* kp = static_cast<const Key*>(
            detail::map_node_key_ptr(existing, existing->level, alignof(Key)));
        auto* vp = static_cast<Value*>(detail::map_node_value_ptr(
            existing, existing->level, sizeof(Key), alignof(Key), alignof(Value)));
        return {{kp, vp}, false};
    }

    const std::uint32_t level = detail::map_random_level(*m_control);
    auto*               node  = detail::map_prepare_node(*m_control, *m_alloc, level);
    if (node == nullptr) {
        return {{nullptr, nullptr}, false};
    }

    void* key_slot = detail::map_node_key_ptr(node, level, alignof(Key));
    void* val_slot =
        detail::map_node_value_ptr(node, level, sizeof(Key), alignof(Key), alignof(Value));
    key_fn(key_slot);
    value_fn(val_slot);

    for (std::uint32_t i = 0; i < level; ++i) {
        std::int64_t next_off = 0;
        if (update[i] != 0) {
            auto* pred = reinterpret_cast<detail::map_node_common*>(
                detail::map_from_offset(m_control, update[i]));
            next_off = *detail::map_node_forward_ptr(pred, i);
        } else {
            next_off = m_control->head_forward[i];
        }
        *detail::map_node_forward_ptr(node, i) = next_off;
        *detail::map_node_anchor_ptr(node, i)  = next_off;
    }
    *detail::map_node_prev_ptr(node) = update[0];

    detail::map_insert_commit(*m_control, node, update);

    auto* kp = static_cast<const Key*>(detail::map_node_key_ptr(node, level, alignof(Key)));
    auto* vp = static_cast<Value*>(detail::map_node_value_ptr(
        node, level, sizeof(Key), alignof(Key), alignof(Value)));
    return {{kp, vp}, true};
}

template <typename Key, typename Value, typename Compare>
std::pair<typename map<Key, Value, Compare>::mapped_ptr, bool>
map<Key, Value, Compare>::try_emplace(const Key& k, const Value& v) noexcept
{
    return emplace_impl(
        k,
        [&](void* slot) noexcept {
            if constexpr (std::is_trivially_copyable_v<Key>) {
                std::memcpy(slot, &k, sizeof(Key));
            } else {
                new (slot) Key(k);
            }
        },
        [&](void* slot) noexcept {
            if constexpr (std::is_trivially_copyable_v<Value>) {
                std::memcpy(slot, &v, sizeof(Value));
            } else {
                new (slot) Value(v);
            }
        });
}

template <typename Key, typename Value, typename Compare>
std::pair<typename map<Key, Value, Compare>::mapped_ptr, bool>
map<Key, Value, Compare>::try_emplace(Key&& k, Value&& v) noexcept
{
    // 注意：key 已存在时返回 false，k / v 不能被消费（让调用者回退）
    // 实现技巧：先 find 判重（probe=k），未找到再执行 move
    // emplace_impl 传递 const Key& 作为 probe，只有 insert 路径才调用 construct
    const Key& probe = k;
    return emplace_impl(
        probe, [&](void* slot) noexcept { new (slot) Key(std::move(k)); },
        [&](void* slot) noexcept { new (slot) Value(std::move(v)); });
}

template <typename Key, typename Value, typename Compare>
typename map<Key, Value, Compare>::mapped_ptr
map<Key, Value, Compare>::find(const Key& k) noexcept
{
    auto* node = const_cast<detail::map_node_common*>(
        detail::map_find_const(*m_control, &k, &compare_bridge));
    if (node == nullptr) return {nullptr, nullptr};
    return {key_ptr(node), value_ptr(node)};
}

template <typename Key, typename Value, typename Compare>
typename map<Key, Value, Compare>::const_mapped_ptr
map<Key, Value, Compare>::find(const Key& k) const noexcept
{
    auto* node = detail::map_find_const(*m_control, &k, &compare_bridge);
    if (node == nullptr) return {nullptr, nullptr};
    return {key_ptr(node), value_ptr(node)};
}

template <typename Key, typename Value, typename Compare>
template <typename Probe>
typename map<Key, Value, Compare>::mapped_ptr
map<Key, Value, Compare>::find(const Probe& p) noexcept
{
    auto* node = const_cast<detail::map_node_common*>(
        detail::map_find_const(*m_control, &p, &compare_probe_bridge<Probe>));
    if (node == nullptr) return {nullptr, nullptr};
    return {key_ptr(node), value_ptr(node)};
}

template <typename Key, typename Value, typename Compare>
template <typename Probe>
typename map<Key, Value, Compare>::const_mapped_ptr
map<Key, Value, Compare>::find(const Probe& p) const noexcept
{
    auto* node = detail::map_find_const(*m_control, &p, &compare_probe_bridge<Probe>);
    if (node == nullptr) return {nullptr, nullptr};
    return {key_ptr(node), value_ptr(node)};
}

template <typename Key, typename Value, typename Compare>
typename map<Key, Value, Compare>::const_iterator
map<Key, Value, Compare>::lower_bound(const Key& k) const noexcept
{
    const auto* node = detail::map_lower_bound_impl(*m_control, &k, &compare_bridge);
    return const_iterator{m_control, node};
}

template <typename Key, typename Value, typename Compare>
template <typename Probe>
typename map<Key, Value, Compare>::const_iterator
map<Key, Value, Compare>::lower_bound(const Probe& p) const noexcept
{
    const auto* node = detail::map_lower_bound_impl(*m_control, &p, &compare_probe_bridge<Probe>);
    return const_iterator{m_control, node};
}

template <typename Key, typename Value, typename Compare>
typename map<Key, Value, Compare>::const_iterator
map<Key, Value, Compare>::upper_bound(const Key& k) const noexcept
{
    const auto* node = detail::map_upper_bound_impl(*m_control, &k, &compare_bridge);
    return const_iterator{m_control, node};
}

template <typename Key, typename Value, typename Compare>
template <typename Probe>
typename map<Key, Value, Compare>::const_iterator
map<Key, Value, Compare>::upper_bound(const Probe& p) const noexcept
{
    const auto* node = detail::map_upper_bound_impl(*m_control, &p, &compare_probe_bridge<Probe>);
    return const_iterator{m_control, node};
}

template <typename Key, typename Value, typename Compare>
std::pair<typename map<Key, Value, Compare>::const_iterator,
          typename map<Key, Value, Compare>::const_iterator>
map<Key, Value, Compare>::equal_range(const Key& k) const noexcept
{
    return {lower_bound(k), upper_bound(k)};
}

template <typename Key, typename Value, typename Compare>
template <typename Probe>
std::pair<typename map<Key, Value, Compare>::const_iterator,
          typename map<Key, Value, Compare>::const_iterator>
map<Key, Value, Compare>::equal_range(const Probe& p) const noexcept
{
    return {lower_bound(p), upper_bound(p)};
}

template <typename Key, typename Value, typename Compare>
bool map<Key, Value, Compare>::erase(const Key& k) noexcept
{
    detail::map_recover_cb_data cb{m_control, &compare_bridge};
    container_guard             guard(m_control->mutex, detail::map_recover_cb, &cb);

    std::int64_t update[map_max_level] = {0};
    auto* node = detail::map_find_predecessors(*m_control, &k, &compare_bridge, update);
    if (node == nullptr) return false;

    Key*       kp = const_cast<Key*>(key_ptr(node));
    Value*     vp = value_ptr(node);
    const bool ok = detail::map_erase_commit(*m_control, node, &compare_bridge);
    if (ok) {
        if constexpr (!std::is_trivially_destructible_v<Value>) vp->~Value();
        if constexpr (!std::is_trivially_destructible_v<Key>) kp->~Key();
        // 节点已从 skiplist 解链，归还内存给 allocator 防止 arena 增长。
        if (m_alloc != nullptr) {
            m_alloc->deallocate(node);
        }
    }
    return ok;
}

template <typename Key, typename Value, typename Compare>
template <typename Probe>
bool map<Key, Value, Compare>::erase(const Probe& p) noexcept
{
    detail::map_recover_cb_data cb{m_control, &compare_bridge};
    container_guard             guard(m_control->mutex, detail::map_recover_cb, &cb);

    std::int64_t update[map_max_level] = {0};
    auto* node = detail::map_find_predecessors(*m_control, &p,
                                               &compare_probe_bridge<Probe>, update);
    if (node == nullptr) return false;

    Key*       kp = const_cast<Key*>(key_ptr(node));
    Value*     vp = value_ptr(node);
    const bool ok = detail::map_erase_commit(*m_control, node, &compare_bridge);
    if (ok) {
        if constexpr (!std::is_trivially_destructible_v<Value>) vp->~Value();
        if constexpr (!std::is_trivially_destructible_v<Key>) kp->~Key();
        if (m_alloc != nullptr) {
            m_alloc->deallocate(node);
        }
    }
    return ok;
}

template <typename Key, typename Value, typename Compare>
void map<Key, Value, Compare>::clear() noexcept
{
    detail::map_recover_cb_data cb{m_control, &compare_bridge};
    container_guard             guard(m_control->mutex, detail::map_recover_cb, &cb);

    while (m_control->head_forward[0] != 0) {
        auto* node = reinterpret_cast<detail::map_node_common*>(
            detail::map_from_offset(m_control, m_control->head_forward[0]));
        if (node == nullptr) break;
        Key*   kp = const_cast<Key*>(key_ptr(node));
        Value* vp = value_ptr(node);
        if (!detail::map_erase_commit(*m_control, node, &compare_bridge)) break;
        if constexpr (!std::is_trivially_destructible_v<Value>) vp->~Value();
        if constexpr (!std::is_trivially_destructible_v<Key>) kp->~Key();
        if (m_alloc != nullptr) {
            m_alloc->deallocate(node);
        }
    }
}

template <typename Key, typename Value, typename Compare>
typename map<Key, Value, Compare>::iterator map<Key, Value, Compare>::begin() noexcept
{
    const auto off = m_control->head_forward[0];
    return iterator{m_control, off == 0 ? nullptr
                                        : reinterpret_cast<detail::map_node_common*>(
                                              detail::map_from_offset(m_control, off))};
}

template <typename Key, typename Value, typename Compare>
typename map<Key, Value, Compare>::const_iterator
map<Key, Value, Compare>::begin() const noexcept
{
    const auto off = m_control->head_forward[0];
    return const_iterator{m_control,
                          off == 0 ? nullptr
                                   : reinterpret_cast<const detail::map_node_common*>(
                                         detail::map_from_offset(m_control, off))};
}

} // namespace mc::shm::container

#endif // MC_SHM_CONTAINER_MAP_H
