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
 * @file set.h
 * @brief 侵入式有序集合容器（红黑树）
 */
#ifndef MC_INTRUSIVE_SET_H
#define MC_INTRUSIVE_SET_H

#include <mc/common.h>
#include <mc/intrusive/detail/set_core.h>
#include <mc/intrusive/hook.h>

#include <cassert>
#include <cstddef>
#include <functional>
#include <iterator>
#include <type_traits>
#include <utility>

namespace mc::intrusive {

template <typename Compare>
struct compare {
    using type = Compare;
};

namespace detail {

template <bool DefaultValue, typename... Options>
struct set_constant_time_size_selector : std::integral_constant<bool, DefaultValue> {};

template <bool DefaultValue, bool Value, typename... Rest>
struct set_constant_time_size_selector<DefaultValue, constant_time_size<Value>, Rest...>
    : std::integral_constant<bool, Value> {};

template <bool DefaultValue, typename First, typename... Rest>
struct set_constant_time_size_selector<DefaultValue, First, Rest...>
    : set_constant_time_size_selector<DefaultValue, Rest...> {};

template <typename DefaultCompare, typename... Options>
struct compare_selector {
    using type = DefaultCompare;
};

template <typename DefaultCompare, typename Compare, typename... Rest>
struct compare_selector<DefaultCompare, mc::intrusive::compare<Compare>, Rest...> {
    using type = Compare;
};

template <typename DefaultCompare, bool V, typename... Rest>
struct compare_selector<DefaultCompare, constant_time_size<V>, Rest...>
    : compare_selector<DefaultCompare, Rest...> {};

template <typename DefaultCompare, link_mode_type M, typename... Rest>
struct compare_selector<DefaultCompare, link_mode<M>, Rest...>
    : compare_selector<DefaultCompare, Rest...> {};

template <typename DefaultCompare, typename First, typename... Rest>
struct compare_selector<DefaultCompare, First, Rest...>
    : compare_selector<DefaultCompare, Rest...> {};

template <bool Enabled>
struct set_size_counter {};

template <>
struct set_size_counter<true> {
    std::size_t m_size{0};
};

template <typename T>
inline set_hook_state* set_hook_ptr(T& value) noexcept
{
    return static_cast<set_hook_state*>(&value);
}

template <typename T>
inline const set_hook_state* set_hook_ptr(const T& value) noexcept
{
    return static_cast<const set_hook_state*>(&value);
}

template <typename T>
inline T* set_value(set_hook_state* hook) noexcept
{
    return static_cast<T*>(hook);
}

template <typename T>
inline const T* set_value(const set_hook_state* hook) noexcept
{
    return static_cast<const T*>(hook);
}

} // namespace detail

template <typename T, typename... Options>
class set : private detail::set_size_counter<detail::set_constant_time_size_selector<true, Options...>::value> {
    static_assert(std::is_base_of_v<detail::set_hook_state, T>,
                  "T must inherit from mc::intrusive::set_base_hook");

    static constexpr bool has_constant_time_size =
        detail::set_constant_time_size_selector<true, Options...>::value;

public:
    using value_type  = T;
    using key_compare = typename detail::compare_selector<std::less<T>, Options...>::type;

private:
    template <bool IsConst>
    class iterator_impl {
        using container_type = std::conditional_t<IsConst, const set, set>;
        using hook_ptr       = std::conditional_t<IsConst, const detail::set_hook_state*, detail::set_hook_state*>;

    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = std::conditional_t<IsConst, const T*, T*>;
        using reference         = std::conditional_t<IsConst, const T&, T&>;

        iterator_impl() = default;

        // Const conversion from non-const
        template <bool OtherConst, typename = std::enable_if_t<IsConst && !OtherConst>>
        iterator_impl(const iterator_impl<OtherConst>& other) : m_set(other.m_set), m_hook(other.m_hook)
        {}

        reference operator*() const { return *value_from_hook(m_hook); }
        pointer   operator->() const { return value_from_hook(m_hook); }

        iterator_impl& operator++()
        {
            m_hook = detail::set_core::next(m_hook);
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
            if (m_hook == nullptr) {
                // --end() => last element (max node)
                m_hook = m_set->m_core.last();
            } else {
                m_hook = detail::set_core::prev(m_hook);
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
            return lhs.m_hook == rhs.m_hook;
        }

        friend bool operator!=(const iterator_impl& lhs, const iterator_impl& rhs)
        {
            return !(lhs == rhs);
        }

    private:
        friend class set;
        template <bool>
        friend class iterator_impl;

        static pointer value_from_hook(hook_ptr hook)
        {
            if constexpr (IsConst) {
                return detail::set_value<T>(hook);
            } else {
                return detail::set_value<T>(const_cast<detail::set_hook_state*>(hook));
            }
        }

        iterator_impl(container_type* s, hook_ptr hook) : m_set(s), m_hook(hook)
        {}

        container_type* m_set{nullptr};
        hook_ptr        m_hook{nullptr};
    };

public:
    using iterator               = iterator_impl<false>;
    using const_iterator         = iterator_impl<true>;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    set() = default;

    set(const set&)            = delete;
    set& operator=(const set&) = delete;
    set(set&&)                 = delete;
    set& operator=(set&&)      = delete;

    // --- Iterators ---

    iterator               begin()        { return iterator(this, m_core.begin()); }
    iterator               end()          { return iterator(this, nullptr); }
    const_iterator         begin()  const { return cbegin(); }
    const_iterator         end()    const { return cend(); }
    const_iterator         cbegin() const { return const_iterator(this, m_core.begin()); }
    const_iterator         cend()   const { return const_iterator(this, nullptr); }

    reverse_iterator       rbegin()       { return reverse_iterator(end()); }
    reverse_iterator       rend()         { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator rend()   const { return reverse_iterator(begin()); }
    const_reverse_iterator crbegin() const { return const_reverse_iterator(cend()); }
    const_reverse_iterator crend()   const { return reverse_iterator(cbegin()); }

    // --- Capacity ---

    bool empty() const noexcept { return size() == 0; }

    std::size_t size() const noexcept
    {
        if constexpr (has_constant_time_size) {
            return this->m_size;
        } else {
            return m_core.count_all();
        }
    }

    // --- Modifiers ---

    std::pair<iterator, bool> insert(T& value)
    {
        auto* existing = m_core.insert(detail::set_hook_ptr(value), &compare_dispatch, &m_compare);
        if (existing != nullptr) {
            return {iterator(this, existing), false};
        }
        on_insert();
        return {iterator(this, detail::set_hook_ptr(value)), true};
    }

    iterator erase(iterator pos)
    {
        if (pos == end()) {
            return end();
        }
        auto* hook = const_cast<detail::set_hook_state*>(pos.m_hook);
        auto* nxt  = detail::set_core::next(hook);
        m_core.erase(hook);
        on_erase();
        return iterator(this, nxt);
    }

    void erase(T& value)
    {
        auto* hook = detail::set_hook_ptr(value);
        if (hook->parent != nullptr || hook->left != nullptr || hook->right != nullptr || m_core.root() == hook) {
            m_core.erase(hook);
            on_erase();
        }
    }

    void clear()
    {
        m_core.clear();
        reset_size();
    }

    template <typename Disposer>
    void clear_and_dispose(Disposer d)
    {
        m_core.clear_and_dispose(
            [](detail::set_hook_state* node, void* ctx) {
                (*static_cast<Disposer*>(ctx))(detail::set_value<T>(node));
            },
            &d);
        reset_size();
    }

    template <typename Disposer>
    void erase_and_dispose(iterator pos, Disposer d)
    {
        if (pos == end()) {
            return;
        }
        auto* hook    = const_cast<detail::set_hook_state*>(pos.m_hook);
        auto* value_p = detail::set_value<T>(hook);
        m_core.erase(hook);
        on_erase();
        d(value_p);
    }

    template <typename Disposer>
    void erase_and_dispose(T& value, Disposer d)
    {
        auto* hook = detail::set_hook_ptr(value);
        if (hook->parent != nullptr || hook->left != nullptr || hook->right != nullptr || m_core.root() == hook) {
            m_core.erase(hook);
            on_erase();
        }
        d(&value);
    }

    void swap(set& other) noexcept
    {
        if (this == &other) {
            return;
        }
        m_core.swap(other.m_core);
        if constexpr (has_constant_time_size) {
            std::swap(this->m_size, other.m_size);
        }
    }

    // --- Lookup ---

    iterator find(const T& value)
    {
        auto* h = m_core.find(detail::set_hook_ptr(value), &compare_dispatch, &m_compare);
        return iterator(this, const_cast<detail::set_hook_state*>(h));
    }

    const_iterator find(const T& value) const
    {
        auto* h = m_core.find(detail::set_hook_ptr(value), &compare_dispatch, &m_compare);
        return const_iterator(this, h);
    }

    template <typename Key, typename Cmp>
    iterator find(const Key& key, Cmp cmp)
    {
        auto* h = m_core.find(&key, &compare_key_dispatch<Key, Cmp>, &cmp);
        return iterator(this, const_cast<detail::set_hook_state*>(h));
    }

    std::size_t count(const T& value) const { return find(value) != end() ? 1 : 0; }

    bool contains(const T& value) const { return find(value) != end(); }

    iterator lower_bound(const T& value)
    {
        auto* h = m_core.lower_bound(detail::set_hook_ptr(value), &compare_dispatch, &m_compare);
        return iterator(this, const_cast<detail::set_hook_state*>(h));
    }

    const_iterator lower_bound(const T& value) const
    {
        auto* h = m_core.lower_bound(detail::set_hook_ptr(value), &compare_dispatch, &m_compare);
        return const_iterator(this, h);
    }

    iterator upper_bound(const T& value)
    {
        auto* h = m_core.upper_bound(detail::set_hook_ptr(value), &compare_dispatch, &m_compare);
        return iterator(this, const_cast<detail::set_hook_state*>(h));
    }

    const_iterator upper_bound(const T& value) const
    {
        auto* h = m_core.upper_bound(detail::set_hook_ptr(value), &compare_dispatch, &m_compare);
        return const_iterator(this, h);
    }

    std::pair<iterator, iterator> equal_range(const T& value)
    {
        return {lower_bound(value), upper_bound(value)};
    }

    std::pair<const_iterator, const_iterator> equal_range(const T& value) const
    {
        return {lower_bound(value), upper_bound(value)};
    }

    // --- Iterator conversion ---

    iterator iterator_to(T& value)
    {
        return iterator(this, detail::set_hook_ptr(value));
    }

    const_iterator iterator_to(const T& value) const
    {
        return const_iterator(this, detail::set_hook_ptr(value));
    }

    // --- Accessor ---

    const key_compare& value_comp() const { return m_compare; }
    const key_compare& key_comp() const { return m_compare; }

private:
    static bool compare_dispatch(const void* a, const void* b, const void* ctx) noexcept
    {
        auto& cmp = *static_cast<const key_compare*>(ctx);
        return cmp(*static_cast<const T*>(a), *static_cast<const T*>(b));
    }

    template <typename Key, typename Cmp>
    static bool compare_key_dispatch(const void* a, const void* b, const void* ctx) noexcept
    {
        auto& cmp = *static_cast<const Cmp*>(ctx);
        // a is Key*, b is set_hook_state* (node)
        // First try key < node
        return cmp(*static_cast<const Key*>(a), *static_cast<const T*>(b));
    }

    void on_insert()
    {
        if constexpr (has_constant_time_size) {
            ++this->m_size;
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

    detail::set_core m_core;
    key_compare      m_compare;
};

} // namespace mc::intrusive

#endif // MC_INTRUSIVE_SET_H
