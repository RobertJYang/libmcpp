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

#ifndef MC_SHM_CONTAINER_LIST_H
#define MC_SHM_CONTAINER_LIST_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <type_traits>
#include <utility>

#include <mc/common.h>
#include <mc/shm/allocator.h>
#include <mc/shm/container/guard.h>
#include <mc/shm/container/journal.h>
#include <mc/shm/container/node_header.h>
#include <mc/shm/ipc_mutex.h>

// mc::shm::container::list<T>
//
// 崩溃安全的跨进程双向链表。节点来自 shm_allocator，容器自身放在
// arena 内某个位置（可由 region::register_root 托管）。所有偏移均为
// **self-relative**（相对 list_control 基址的有符号 int64，0 表示 null），
// 无需外部基址即可跨进程重 attach。
//
// 使用模式：
//   1) 分配 list_control 并调 list<T>::init(control, self_tag) 完成
//      journal / mutex 初始化；self_tag 由调用者提供（一般为 control 的
//      arena-relative offset），用于节点反向归属校验
//   2) push_back / push_front / erase / pop_* 等修改 API 内部持
//      container_lock 并维护 journal
//   3) 任一持锁者崩溃后，下一个持锁者通过 container_guard 自动触发 recover()

namespace mc::shm::container {

namespace detail {

// 节点公共前缀；用户节点内存布局 = list_node_common + T
struct list_node_common {
    node_header  header;       // 16B；owner_offset 存 list_control.self_tag
    std::int64_t prev_offset;  // self-relative to owning list_control
    std::int64_t next_offset;  // self-relative to owning list_control
};

static_assert(sizeof(list_node_common) == 32, "list_node_common 必须是 32 字节");

inline std::int64_t self_offset_from(const void* control, const void* node) noexcept
{
    return static_cast<std::int64_t>(reinterpret_cast<const std::byte*>(node)
                                     - reinterpret_cast<const std::byte*>(control));
}

inline void* from_offset(void* control, std::int64_t offset) noexcept
{
    return reinterpret_cast<std::byte*>(control) + offset;
}

inline const void* from_offset(const void* control, std::int64_t offset) noexcept
{
    return reinterpret_cast<const std::byte*>(control) + offset;
}

} // namespace detail

// ==========================================================================
// list_control —— 容器控制块
// ==========================================================================
//
// 布局固定，可直接放置在 arena 任意位置。所有偏移字段为 self-relative
// 0 表示 null
struct MC_API list_control {
    ipc_mutex                  mutex;           // 修改 API 持有的 lock
    container_journal          journal;         // 64B 事务 journal
    std::int64_t               head_offset;     // head 节点 self-relative offset
    std::int64_t               tail_offset;     // tail 节点 self-relative offset
    std::uint64_t              node_size;       // 一个节点的总字节数
    std::uint32_t              self_tag;        // 反向归属校验 tag（node.header.owner_offset 保存此值）
    std::uint32_t              _pad0;
    std::atomic<std::uint64_t> size;            // hint
    std::atomic<std::uint64_t> degraded_count;  // recover 累计
    std::uint8_t               _reserved[24];
};

// 初始化 list_control。调用者通常一次性完成：
//   1) 从 shm_allocator 分配 sizeof(list_control) 字节
//   2) placement new list_control{}
//   3) 调用 list_control_init 填充 journal、size 等字段
//
// self_tag：跨进程保持一致的唯一 id（推荐用 control 的 arena-relative offset 低 32 bit）
// element_size / element_align：用户数据的大小与对齐
MC_API void list_control_init(list_control& ctrl, std::uint32_t self_tag,
                              std::size_t element_size, std::size_t element_align) noexcept;

// 崩溃恢复入口（持 ctrl.mutex 时调用，container_guard 会自动触发）
MC_API void list_recover(list_control& ctrl) noexcept;

// 遍历统计 degraded 节点数（不修改结构）
MC_API std::size_t list_degraded_count(const list_control& ctrl) noexcept;

// ==========================================================================
// 非模板 core API
// ==========================================================================

namespace detail {

// 在 anchor_prev_offset 之后插入 new_node
//   anchor_prev_offset == 0 → 插入 head 位置（new_node 成为新 head）
//   anchor_prev_offset == ctrl.tail_offset → 插入 tail 位置
//   其他值：在 offset 对应节点之后插入
// 调用者持 ctrl.mutex，new_node 必须已 allocate 且 header 已初始化、value 已构造
MC_API void list_insert_after(list_control& ctrl, std::int64_t anchor_prev_offset,
                              list_node_common* new_node) noexcept;

// 从容器中摘除 node 的链接（不 free allocator slot）；调用者持 ctrl.mutex
// 返回 true 成功摘除，false 表示节点损坏 / 不属于本容器
MC_API bool list_unlink(list_control& ctrl, list_node_common* node) noexcept;

// ---- 非模板 helper（供 list<T> 模板 wrapper 复用，避免二进制膨胀） ----

// container_guard recover 回调（无类型依赖，可复用）
MC_API void list_recover_cb(void* data) noexcept;

// 持锁状态下分配一个新节点并初始化 header，prev/next 置 0
// 返回 nullptr 表示 allocator OOM；调用者随后在 value_ptr 位置 placement-new T
MC_API list_node_common* list_prepare_node(list_control& ctrl, shm_allocator& alloc,
                                           std::size_t node_size,
                                           std::size_t node_align) noexcept;

// 持锁状态下完成插入：根据 at_front 选择 head/tail 为 anchor，调 list_insert_after
MC_API void list_finish_push(list_control& ctrl, bool at_front, list_node_common* node) noexcept;

// 持锁状态下摘除 head；返回被摘下的节点指针，空链表返回 nullptr
// 调用者负责析构 value 并 deallocate
MC_API list_node_common* list_take_head(list_control& ctrl) noexcept;
MC_API list_node_common* list_take_tail(list_control& ctrl) noexcept;

// 持锁状态下校验 node 归属本 ctrl，然后 unlink；成功返回 true
MC_API bool list_validate_and_unlink(list_control& ctrl, list_node_common* node) noexcept;

} // namespace detail

// ==========================================================================
// list<T> 模板 wrapper
// ==========================================================================

template <typename T>
class list {
public:
    static_assert(std::is_trivially_destructible_v<T>,
                  "list<T> 当前要求 T 为 trivially destructible");
    static_assert(alignof(T) <= 16, "list<T> 暂不支持 T 对齐大于 16 字节");

    static constexpr std::size_t node_size  = sizeof(detail::list_node_common) + sizeof(T);
    static constexpr std::size_t node_align = alignof(detail::list_node_common) > alignof(T)
                                                  ? alignof(detail::list_node_common)
                                                  : alignof(T);

    class iterator;
    class const_iterator;

    // 绑定一个已初始化的 list_control；不拥有所有权
    list(list_control& control, shm_allocator& alloc) noexcept
        : m_control(&control), m_alloc(&alloc)
    {
    }

    list(const list&)            = delete;
    list& operator=(const list&) = delete;
    list(list&&)                 = default;
    list& operator=(list&&)      = default;

    // 首次构造时调用（持 nothing，对 idle control 执行）
    static void init(list_control& control, std::uint32_t self_tag) noexcept
    {
        list_control_init(control, self_tag, sizeof(T), alignof(T));
    }

    std::size_t size() const noexcept
    {
        return m_control->size.load(std::memory_order_acquire);
    }
    bool        empty() const noexcept { return size() == 0; }
    std::size_t degraded_count() const noexcept
    {
        return m_control->degraded_count.load(std::memory_order_acquire);
    }

    T* push_back(const T& value) noexcept
    {
        T tmp = value;
        return do_push(/*at_front=*/false, std::move(tmp));
    }
    T* push_back(T&& value) noexcept { return do_push(/*at_front=*/false, std::move(value)); }

    T* push_front(const T& value) noexcept
    {
        T tmp = value;
        return do_push(/*at_front=*/true, std::move(tmp));
    }
    T* push_front(T&& value) noexcept { return do_push(/*at_front=*/true, std::move(value)); }

    bool erase(T* value) noexcept;
    bool pop_front() noexcept;
    bool pop_back() noexcept;

    iterator       begin() noexcept;
    iterator       end() noexcept { return iterator{m_control, nullptr}; }
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept { return const_iterator{m_control, nullptr}; }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }

    list_control*       control() noexcept { return m_control; }
    const list_control* control() const noexcept { return m_control; }
    shm_allocator*      allocator() noexcept { return m_alloc; }

    // 内部 helper（供 iterator 使用）
    static T* value_ptr(detail::list_node_common* node) noexcept
    {
        return reinterpret_cast<T*>(reinterpret_cast<std::byte*>(node)
                                    + sizeof(detail::list_node_common));
    }

    static const T* value_ptr(const detail::list_node_common* node) noexcept
    {
        return reinterpret_cast<const T*>(reinterpret_cast<const std::byte*>(node)
                                          + sizeof(detail::list_node_common));
    }

private:
    static detail::list_node_common* node_of(T* value) noexcept
    {
        return reinterpret_cast<detail::list_node_common*>(
            reinterpret_cast<std::byte*>(value) - sizeof(detail::list_node_common));
    }

    T* do_push(bool at_front, T&& value) noexcept;

    list_control*  m_control;
    shm_allocator* m_alloc;
};

// ==========================================================================
// iterator
// ==========================================================================

template <typename T>
class list<T>::iterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type        = T;
    using difference_type   = std::ptrdiff_t;
    using pointer           = T*;
    using reference         = T&;

    iterator() noexcept = default;
    iterator(list_control* ctrl, detail::list_node_common* node) noexcept
        : m_control(ctrl), m_node(node)
    {
    }

    reference operator*() const noexcept { return *list<T>::value_ptr(m_node); }
    pointer   operator->() const noexcept { return list<T>::value_ptr(m_node); }

    iterator& operator++() noexcept
    {
        if (m_node != nullptr) {
            const auto off = m_node->next_offset;
            m_node         = off == 0
                                 ? nullptr
                                 : reinterpret_cast<detail::list_node_common*>(
                                     detail::from_offset(m_control, off));
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

    // 暴露 node 指针，供容器内部 erase 等使用
    detail::list_node_common* node() const noexcept { return m_node; }

private:
    friend class list<T>::const_iterator;

    list_control*             m_control = nullptr;
    detail::list_node_common* m_node    = nullptr;
};

template <typename T>
class list<T>::const_iterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type        = T;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const T*;
    using reference         = const T&;

    const_iterator() noexcept = default;
    const_iterator(const list_control* ctrl, const detail::list_node_common* node) noexcept
        : m_control(ctrl), m_node(node)
    {
    }

    const_iterator(const iterator& it) noexcept : m_control(it.m_control), m_node(it.m_node) {}

    reference operator*() const noexcept { return *list<T>::value_ptr(m_node); }
    pointer   operator->() const noexcept { return list<T>::value_ptr(m_node); }

    const_iterator& operator++() noexcept
    {
        if (m_node != nullptr) {
            const auto off = m_node->next_offset;
            m_node         = off == 0
                                 ? nullptr
                                 : reinterpret_cast<const detail::list_node_common*>(
                                     detail::from_offset(m_control, off));
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
    const list_control*             m_control = nullptr;
    const detail::list_node_common* m_node    = nullptr;
};

// ==========================================================================
// list<T> 模板实现
// ==========================================================================

template <typename T>
T* list<T>::do_push(bool at_front, T&& value) noexcept
{
    container_guard guard(m_control->mutex, detail::list_recover_cb, m_control);

    auto* node = detail::list_prepare_node(*m_control, *m_alloc, node_size, node_align);
    if (node == nullptr) {
        return nullptr;
    }

    new (value_ptr(node)) T(std::move(value));

    detail::list_finish_push(*m_control, at_front, node);
    return value_ptr(node);
}

template <typename T>
bool list<T>::erase(T* value) noexcept
{
    if (value == nullptr) {
        return false;
    }

    container_guard guard(m_control->mutex, detail::list_recover_cb, m_control);

    auto* node = node_of(value);
    if (!detail::list_validate_and_unlink(*m_control, node)) {
        return false;
    }

    value->~T();
    m_alloc->deallocate(node);
    return true;
}

template <typename T>
bool list<T>::pop_front() noexcept
{
    container_guard guard(m_control->mutex, detail::list_recover_cb, m_control);

    auto* head = detail::list_take_head(*m_control);
    if (head == nullptr) {
        return false;
    }
    value_ptr(head)->~T();
    m_alloc->deallocate(head);
    return true;
}

template <typename T>
bool list<T>::pop_back() noexcept
{
    container_guard guard(m_control->mutex, detail::list_recover_cb, m_control);

    auto* tail = detail::list_take_tail(*m_control);
    if (tail == nullptr) {
        return false;
    }
    value_ptr(tail)->~T();
    m_alloc->deallocate(tail);
    return true;
}

template <typename T>
typename list<T>::iterator list<T>::begin() noexcept
{
    const auto off = m_control->head_offset;
    return iterator{m_control, off == 0
                                   ? nullptr
                                   : reinterpret_cast<detail::list_node_common*>(
                                       detail::from_offset(m_control, off))};
}

template <typename T>
typename list<T>::const_iterator list<T>::begin() const noexcept
{
    const auto off = m_control->head_offset;
    return const_iterator{m_control, off == 0
                                         ? nullptr
                                         : reinterpret_cast<const detail::list_node_common*>(
                                             detail::from_offset(m_control, off))};
}

} // namespace mc::shm::container

#endif // MC_SHM_CONTAINER_LIST_H
