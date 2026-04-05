/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/**
 * @file list.h
 * @brief 侵入式链表容器
 */
#ifndef MC_INTRUSIVE_LIST_H
#define MC_INTRUSIVE_LIST_H

#include <mc/common.h>
#include <mc/intrusive/hook.h>

#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>

namespace mc::intrusive {

namespace detail {

template <bool DefaultValue, typename... Options>
struct constant_time_size_selector : std::integral_constant<bool, DefaultValue> {};

template <bool DefaultValue, bool Value, typename... Rest>
struct constant_time_size_selector<DefaultValue, constant_time_size<Value>, Rest...>
    : std::integral_constant<bool, Value> {};

template <bool DefaultValue, typename First, typename... Rest>
struct constant_time_size_selector<DefaultValue, First, Rest...> : constant_time_size_selector<DefaultValue, Rest...> {
};

template <bool Enabled>
struct size_counter {};

template <>
struct size_counter<true> {
    std::size_t m_size{0};
};

template <typename T>
inline list_hook_state& list_hook(T& value)
{
    return static_cast<list_hook_state&>(value);
}

template <typename T>
inline const list_hook_state& list_hook(const T& value)
{
    return static_cast<const list_hook_state&>(value);
}

template <typename T>
inline slist_hook_state& slist_hook(T& value)
{
    return static_cast<slist_hook_state&>(value);
}

template <typename T>
inline const slist_hook_state& slist_hook(const T& value)
{
    return static_cast<const slist_hook_state&>(value);
}

template <typename T>
inline list_hook_state* list_hook_ptr(T& value)
{
    return &list_hook(value);
}

template <typename T>
inline const list_hook_state* list_hook_ptr(const T& value)
{
    return &list_hook(value);
}

template <typename T>
inline slist_hook_state* slist_hook_ptr(T& value)
{
    return &slist_hook(value);
}

template <typename T>
inline const slist_hook_state* slist_hook_ptr(const T& value)
{
    return &slist_hook(value);
}

template <typename T>
inline T* list_value(list_hook_state* hook)
{
    return static_cast<T*>(hook);
}

template <typename T>
inline const T* list_value(const list_hook_state* hook)
{
    return static_cast<const T*>(hook);
}

template <typename T>
inline T* slist_value(slist_hook_state* hook)
{
    return static_cast<T*>(hook);
}

template <typename T>
inline const T* slist_value(const slist_hook_state* hook)
{
    return static_cast<const T*>(hook);
}

class MC_API list_core {
public:
    list_core() = default;

    list_core(const list_core&)            = delete;
    list_core& operator=(const list_core&) = delete;
    list_core(list_core&&)                 = delete;
    list_core& operator=(list_core&&)      = delete;

    list_hook_state*       head() noexcept;
    const list_hook_state* head() const noexcept;
    list_hook_state*       tail() noexcept;
    const list_hook_state* tail() const noexcept;

    static list_hook_state*       next(list_hook_state* node) noexcept;
    static const list_hook_state* next(const list_hook_state* node) noexcept;
    static list_hook_state*       prev(list_hook_state* node) noexcept;
    static const list_hook_state* prev(const list_hook_state* node) noexcept;

    void             push_front(list_hook_state* node) noexcept;
    void             push_back(list_hook_state* node) noexcept;
    void             pop_front() noexcept;
    void             pop_back() noexcept;
    void             insert_before(list_hook_state* pos, list_hook_state* node) noexcept;
    list_hook_state* erase(list_hook_state* node) noexcept;
    void             splice(list_hook_state* pos, list_core& other) noexcept;
    void             clear() noexcept;
    bool             empty() const noexcept;

private:
    static void reset_hook(list_hook_state* node) noexcept;

    list_hook_state* m_head{nullptr};
    list_hook_state* m_tail{nullptr};
};

class MC_API slist_core {
public:
    slist_core() = default;

    slist_core(const slist_core&)            = delete;
    slist_core& operator=(const slist_core&) = delete;
    slist_core(slist_core&&)                 = delete;
    slist_core& operator=(slist_core&&)      = delete;

    slist_hook_state*       head() noexcept;
    const slist_hook_state* head() const noexcept;

    static slist_hook_state*       next(slist_hook_state* node) noexcept;
    static const slist_hook_state* next(const slist_hook_state* node) noexcept;

    void push_front(slist_hook_state* node) noexcept;
    void pop_front() noexcept;
    void erase(slist_hook_state* node) noexcept;
    void clear() noexcept;
    using slist_dispose_fn = void (*)(slist_hook_state* node, void* ctx);
    void clear_and_dispose(slist_dispose_fn fn, void* ctx);
    void reverse() noexcept;
    bool empty() const noexcept;

private:
    static void reset_hook(slist_hook_state* node) noexcept;

    slist_hook_state* m_head{nullptr};
};

} // namespace detail

template <typename T, typename... Options>
class list : private detail::size_counter<detail::constant_time_size_selector<true, Options...>::value> {
    static_assert(std::is_base_of_v<detail::list_hook_state, T>, "T must inherit from mc::intrusive::list_base_hook");

    static constexpr bool has_constant_time_size = detail::constant_time_size_selector<true, Options...>::value;

    template <bool IsConst>
    class iterator_impl {
        using container_type = std::conditional_t<IsConst, const list, list>;
        using hook_ptr       = std::conditional_t<IsConst, const detail::list_hook_state*, detail::list_hook_state*>;

    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = std::conditional_t<IsConst, const T*, T*>;
        using reference         = std::conditional_t<IsConst, const T&, T&>;

        iterator_impl() = default;

        template <bool OtherConst, typename = std::enable_if_t<IsConst && !OtherConst>>
        iterator_impl(const iterator_impl<OtherConst>& other) : m_owner(other.m_owner), m_current(other.m_current)
        {}

        template <bool OtherConst, typename = std::enable_if_t<IsConst && !OtherConst>>
        iterator_impl(iterator_impl<OtherConst>&& other) : m_owner(other.m_owner), m_current(other.m_current)
        {}

        reference operator*() const
        {
            return *value_from_hook(m_current);
        }

        pointer operator->() const
        {
            return value_from_hook(m_current);
        }

        iterator_impl& operator++()
        {
            m_current = m_owner->next_hook(m_current);
            return *this;
        }

        iterator_impl operator++(int)
        {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        iterator_impl& operator--()
        {
            if (m_current == nullptr) {
                m_current = m_owner->tail_hook();
            } else {
                m_current = m_owner->prev_hook(m_current);
            }
            return *this;
        }

        iterator_impl operator--(int)
        {
            auto tmp = *this;
            --(*this);
            return tmp;
        }

        friend bool operator==(const iterator_impl& lhs, const iterator_impl& rhs)
        {
            return lhs.m_current == rhs.m_current;
        }

        friend bool operator!=(const iterator_impl& lhs, const iterator_impl& rhs)
        {
            return !(lhs == rhs);
        }

    private:
        friend class list;
        template <bool>
        friend class iterator_impl;

        static pointer value_from_hook(hook_ptr hook)
        {
            if constexpr (IsConst) {
                return detail::list_value<T>(hook);
            } else {
                return detail::list_value<T>(const_cast<detail::list_hook_state*>(hook));
            }
        }

        iterator_impl(container_type* owner, hook_ptr current) : m_owner(owner), m_current(current)
        {}

        container_type* m_owner{nullptr};
        hook_ptr        m_current{nullptr};
    };

public:
    using value_type             = T;
    using iterator               = iterator_impl<false>;
    using const_iterator         = iterator_impl<true>;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    list() = default;

    list(const list&)            = delete;
    list& operator=(const list&) = delete;
    list(list&&)                 = delete;
    list& operator=(list&&)      = delete;

    iterator begin()
    {
        return iterator(this, m_core.head());
    }

    iterator end()
    {
        return iterator(this, nullptr);
    }

    const_iterator begin() const
    {
        return cbegin();
    }

    const_iterator end() const
    {
        return cend();
    }

    const_iterator cbegin() const
    {
        return const_iterator(this, m_core.head());
    }

    const_iterator cend() const
    {
        return const_iterator(this, nullptr);
    }

    reverse_iterator rbegin()
    {
        return reverse_iterator(end());
    }

    reverse_iterator rend()
    {
        return reverse_iterator(begin());
    }

    const_reverse_iterator rbegin() const
    {
        return const_reverse_iterator(end());
    }

    const_reverse_iterator rend() const
    {
        return const_reverse_iterator(begin());
    }

    const_reverse_iterator crbegin() const
    {
        return const_reverse_iterator(cend());
    }

    const_reverse_iterator crend() const
    {
        return const_reverse_iterator(cbegin());
    }

    void push_front(T& value)
    {
        m_core.push_front(detail::list_hook_ptr(value));
        on_insert();
    }

    void push_back(T& value)
    {
        m_core.push_back(detail::list_hook_ptr(value));
        on_insert();
    }

    void pop_front()
    {
        if (m_core.head() == nullptr) {
            return;
        }
        m_core.pop_front();
        on_erase();
    }

    void pop_back()
    {
        if (m_core.tail() == nullptr) {
            return;
        }
        m_core.pop_back();
        on_erase();
    }

    iterator insert(iterator pos, T& value)
    {
        auto* hook = detail::list_hook_ptr(value);
        m_core.insert_before(pos.m_current, hook);
        on_insert();
        return iterator(this, hook);
    }

    iterator erase(iterator pos)
    {
        if (pos.m_current == nullptr) {
            return end();
        }

        auto* next = m_core.erase(pos.m_current);
        on_erase();
        return iterator(this, next);
    }

    // Splice all nodes from other list before pos
    void splice(iterator pos, list& other)
    {
        if (other.empty()) {
            return;
        }
        std::size_t other_size = 0;
        if constexpr (has_constant_time_size) {
            other_size = other.size();
        } else {
            for (auto* cur = other.m_core.head(); cur != nullptr; cur = detail::list_core::next(cur)) {
                ++other_size;
            }
        }
        m_core.splice(pos.m_current, other.m_core);
        on_insert_n(other_size);
        other.reset_size();
    }

    // Splice all nodes from other list to the end
    void splice(list& other)
    {
        splice(end(), other);
    }

    iterator iterator_to(T& value)
    {
        return iterator(this, detail::list_hook_ptr(value));
    }

    const_iterator iterator_to(const T& value) const
    {
        return const_iterator(this, detail::list_hook_ptr(value));
    }

    void clear()
    {
        m_core.clear();
        reset_size();
    }

    template <typename Disposer>
    void clear_and_dispose(Disposer disposer)
    {
        auto* current = m_core.head();
        while (current != nullptr) {
            auto* next = m_core.erase(current);
            disposer(detail::list_value<T>(current));
            current = next;
        }
        reset_size();
    }

    bool empty() const
    {
        return m_core.empty();
    }

    std::size_t size() const
    {
        if constexpr (has_constant_time_size) {
            return this->m_size;
        } else {
            std::size_t count = 0;
            for (auto* current = m_core.head(); current != nullptr; current = detail::list_core::next(current)) {
                ++count;
            }
            return count;
        }
    }

private:
    using hook_ptr       = detail::list_hook_state*;
    using const_hook_ptr = const detail::list_hook_state*;

    hook_ptr next_hook(hook_ptr hook)
    {
        return detail::list_core::next(hook);
    }

    const_hook_ptr next_hook(const_hook_ptr hook) const
    {
        return detail::list_core::next(hook);
    }

    hook_ptr prev_hook(hook_ptr hook)
    {
        return detail::list_core::prev(hook);
    }

    const_hook_ptr prev_hook(const_hook_ptr hook) const
    {
        return detail::list_core::prev(hook);
    }

    hook_ptr tail_hook()
    {
        return m_core.tail();
    }

    const_hook_ptr tail_hook() const
    {
        return m_core.tail();
    }

    void on_insert()
    {
        if constexpr (has_constant_time_size) {
            ++this->m_size;
        }
    }

    void on_insert_n(std::size_t n)
    {
        if constexpr (has_constant_time_size) {
            this->m_size += n;
        }
    }

    void on_erase()
    {
        if constexpr (has_constant_time_size) {
            --this->m_size;
        }
    }

    void reset_size()
    {
        if constexpr (has_constant_time_size) {
            this->m_size = 0;
        }
    }

    detail::list_core m_core;
};

template <typename T, typename... Options>
class slist : private detail::size_counter<detail::constant_time_size_selector<true, Options...>::value> {
    static_assert(std::is_base_of_v<detail::slist_hook_state, T>, "T must inherit from mc::intrusive::slist_base_hook");

    static constexpr bool has_constant_time_size = detail::constant_time_size_selector<true, Options...>::value;

    template <bool IsConst>
    class iterator_impl {
        using container_type = std::conditional_t<IsConst, const slist, slist>;
        using hook_ptr       = std::conditional_t<IsConst, const detail::slist_hook_state*, detail::slist_hook_state*>;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = std::conditional_t<IsConst, const T*, T*>;
        using reference         = std::conditional_t<IsConst, const T&, T&>;

        iterator_impl() = default;

        template <bool OtherConst, typename = std::enable_if_t<IsConst && !OtherConst>>
        iterator_impl(const iterator_impl<OtherConst>& other) : m_owner(other.m_owner), m_current(other.m_current)
        {}

        template <bool OtherConst, typename = std::enable_if_t<IsConst && !OtherConst>>
        iterator_impl(iterator_impl<OtherConst>&& other) : m_owner(other.m_owner), m_current(other.m_current)
        {}

        reference operator*() const
        {
            return *value_from_hook(m_current);
        }

        pointer operator->() const
        {
            return value_from_hook(m_current);
        }

        iterator_impl& operator++()
        {
            m_current = m_owner->next_hook(m_current);
            return *this;
        }

        iterator_impl operator++(int)
        {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(const iterator_impl& lhs, const iterator_impl& rhs)
        {
            return lhs.m_current == rhs.m_current;
        }

        friend bool operator!=(const iterator_impl& lhs, const iterator_impl& rhs)
        {
            return !(lhs == rhs);
        }

    private:
        friend class slist;
        template <bool>
        friend class iterator_impl;

        static pointer value_from_hook(hook_ptr hook)
        {
            if constexpr (IsConst) {
                return detail::slist_value<T>(hook);
            } else {
                return detail::slist_value<T>(const_cast<detail::slist_hook_state*>(hook));
            }
        }

        iterator_impl(container_type* owner, hook_ptr current) : m_owner(owner), m_current(current)
        {}

        container_type* m_owner{nullptr};
        hook_ptr        m_current{nullptr};
    };

public:
    using value_type     = T;
    using iterator       = iterator_impl<false>;
    using const_iterator = iterator_impl<true>;

    slist() = default;

    slist(const slist&)            = delete;
    slist& operator=(const slist&) = delete;
    slist(slist&&)                 = delete;
    slist& operator=(slist&&)      = delete;

    iterator begin()
    {
        return iterator(this, m_core.head());
    }

    iterator end()
    {
        return iterator(this, nullptr);
    }

    const_iterator begin() const
    {
        return cbegin();
    }

    const_iterator end() const
    {
        return cend();
    }

    const_iterator cbegin() const
    {
        return const_iterator(this, m_core.head());
    }

    const_iterator cend() const
    {
        return const_iterator(this, nullptr);
    }

    void push_front(T& value)
    {
        m_core.push_front(detail::slist_hook_ptr(value));
        on_insert();
    }

    void pop_front()
    {
        if (m_core.head() == nullptr) {
            return;
        }
        m_core.pop_front();
        on_erase();
    }

    iterator erase(iterator pos)
    {
        if (pos == end()) {
            return end();
        }
        auto* hook    = pos.m_current;
        auto* next_hk = detail::slist_core::next(hook);
        m_core.erase(hook);
        on_erase();
        return iterator(this, next_hk);
    }

    void erase(T& value)
    {
        m_core.erase(detail::slist_hook_ptr(value));
        on_erase();
    }

    void clear()
    {
        m_core.clear();
        reset_size();
    }

    template <typename Disposer>
    void clear_and_dispose(Disposer d)
    {
        m_core.clear_and_dispose([](detail::slist_hook_state* hook, void* ctx) {
            auto* val = detail::slist_value<T>(hook);
            (*static_cast<Disposer*>(ctx))(val);
        }, &d);
        reset_size();
    }

    template <typename Disposer>
    void erase_and_dispose(iterator pos, Disposer d)
    {
        if (pos == end()) {
            return;
        }
        auto* hook = pos.m_current;
        m_core.erase(hook);
        on_erase();
        d(detail::slist_value<T>(hook));
    }

    template <typename Disposer>
    void erase_and_dispose(T& value, Disposer d)
    {
        m_core.erase(detail::slist_hook_ptr(value));
        on_erase();
        d(&value);
    }

    void reverse()
    {
        m_core.reverse();
    }

    bool empty() const
    {
        return m_core.empty();
    }

    std::size_t size() const
    {
        if constexpr (has_constant_time_size) {
            return this->m_size;
        } else {
            std::size_t count = 0;
            for (auto* current = m_core.head(); current != nullptr; current = detail::slist_core::next(current)) {
                ++count;
            }
            return count;
        }
    }

private:
    using hook_ptr       = detail::slist_hook_state*;
    using const_hook_ptr = const detail::slist_hook_state*;

    hook_ptr next_hook(hook_ptr hook)
    {
        return detail::slist_core::next(hook);
    }

    const_hook_ptr next_hook(const_hook_ptr hook) const
    {
        return detail::slist_core::next(hook);
    }

    void on_insert()
    {
        if constexpr (has_constant_time_size) {
            ++this->m_size;
        }
    }

    void reset_size()
    {
        if constexpr (has_constant_time_size) {
            this->m_size = 0;
        }
    }

    void on_erase()
    {
        if constexpr (has_constant_time_size) {
            --this->m_size;
        }
    }

    detail::slist_core m_core;
};

} // namespace mc::intrusive

#endif // MC_INTRUSIVE_LIST_H