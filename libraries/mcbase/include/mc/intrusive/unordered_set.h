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
 * @file unordered_set.h
 * @brief 侵入式哈希集合容器
 */
#ifndef MC_INTRUSIVE_UNORDERED_SET_H
#define MC_INTRUSIVE_UNORDERED_SET_H

#include <mc/common.h>
#include <mc/intrusive/detail/hash_core.h>

#include <cassert>
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>

namespace mc::intrusive {

template <typename Hash>
struct hash {
    using type = Hash;
};

template <typename Equal>
struct equal {
    using type = Equal;
};

namespace detail {

template <bool DefaultValue, typename... Options>
struct unordered_constant_time_size_selector : std::integral_constant<bool, DefaultValue> {};

template <bool DefaultValue, bool Value, typename... Rest>
struct unordered_constant_time_size_selector<DefaultValue, constant_time_size<Value>, Rest...>
    : std::integral_constant<bool, Value> {};

template <bool DefaultValue, typename First, typename... Rest>
struct unordered_constant_time_size_selector<DefaultValue, First, Rest...>
    : unordered_constant_time_size_selector<DefaultValue, Rest...> {};

template <typename DefaultHash, typename... Options>
struct hash_selector {
    using type = DefaultHash;
};

template <typename DefaultHash, typename Hash, typename... Rest>
struct hash_selector<DefaultHash, mc::intrusive::hash<Hash>, Rest...> {
    using type = Hash;
};

template <typename DefaultHash, typename First, typename... Rest>
struct hash_selector<DefaultHash, First, Rest...> : hash_selector<DefaultHash, Rest...> {};

template <typename DefaultEqual, typename... Options>
struct equal_selector {
    using type = DefaultEqual;
};

template <typename DefaultEqual, typename Equal, typename... Rest>
struct equal_selector<DefaultEqual, mc::intrusive::equal<Equal>, Rest...> {
    using type = Equal;
};

template <typename DefaultEqual, typename First, typename... Rest>
struct equal_selector<DefaultEqual, First, Rest...> : equal_selector<DefaultEqual, Rest...> {};

template <typename T>
inline unordered_hook_state* unordered_hook_ptr(T& value)
{
    return static_cast<unordered_hook_state*>(&value);
}

template <typename T>
inline const unordered_hook_state* unordered_hook_ptr(const T& value)
{
    return static_cast<const unordered_hook_state*>(&value);
}

template <typename T>
inline T* unordered_value(unordered_hook_state* hook)
{
    return static_cast<T*>(hook);
}

template <typename T>
inline const T* unordered_value(const unordered_hook_state* hook)
{
    return static_cast<const T*>(hook);
}

template <bool Enabled>
struct unordered_size_counter {};

template <>
struct unordered_size_counter<true> {
    std::size_t m_size{0};
};

} // namespace detail

template <typename T, typename... Options>
class unordered_set
    : private detail::unordered_size_counter<detail::unordered_constant_time_size_selector<true, Options...>::value> {
    static_assert(std::is_base_of_v<detail::unordered_hook_state, T>,
                  "T must inherit from mc::intrusive::unordered_set_base_hook");

    static constexpr bool has_constant_time_size =
        detail::unordered_constant_time_size_selector<true, Options...>::value;

public:
    using value_type  = T;
    using hasher      = typename detail::hash_selector<std::hash<T>, Options...>::type;
    using key_equal   = typename detail::equal_selector<std::equal_to<T>, Options...>::type;
    using bucket_type = detail::unordered_bucket;

    struct bucket_traits {
        bucket_type* buckets{nullptr};
        std::size_t  count{0};

        bucket_traits() = default;
        bucket_traits(bucket_type* buckets_ptr, std::size_t bucket_count) : buckets(buckets_ptr), count(bucket_count)
        {}
    };

    template <bool IsConst>
    class iterator_impl {
        using container_type = std::conditional_t<IsConst, const unordered_set, unordered_set>;
        using hook_ptr       = std::conditional_t<IsConst, const detail::unordered_hook_state*, detail::unordered_hook_state*>;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = std::conditional_t<IsConst, const T*, T*>;
        using reference         = std::conditional_t<IsConst, const T&, T&>;

        iterator_impl() = default;

        // Const conversion from non-const
        template <bool OtherConst, typename = std::enable_if_t<IsConst && !OtherConst>>
        iterator_impl(const iterator_impl<OtherConst>& other) : m_set(other.m_set), m_state(other.m_state)
        {}

        reference operator*() const { return *value_from_hook(m_state.current); }
        pointer   operator->() const { return value_from_hook(m_state.current); }

        iterator_impl& operator++()
        {
            m_state = m_set->m_core.advance(m_state);
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
            return lhs.m_state.current == rhs.m_state.current;
        }

        friend bool operator!=(const iterator_impl& lhs, const iterator_impl& rhs)
        {
            return !(lhs == rhs);
        }

    private:
        friend class unordered_set;
        template <bool>
        friend class iterator_impl;

        static pointer value_from_hook(hook_ptr hook)
        {
            if constexpr (IsConst) {
                return detail::unordered_value<T>(hook);
            } else {
                return detail::unordered_value<T>(const_cast<detail::unordered_hook_state*>(hook));
            }
        }

        iterator_impl(container_type* set, detail::unordered_iterator_state state) : m_set(set), m_state(state)
        {}

        container_type*                  m_set{nullptr};
        detail::unordered_iterator_state m_state{};
    };

    using iterator       = iterator_impl<false>;
    using const_iterator = iterator_impl<true>;

    unordered_set(bucket_traits traits, const hasher& hash_fn = hasher(), const key_equal& equal_fn = key_equal())
        : m_core(traits.buckets, traits.count), m_hasher(hash_fn), m_key_equal(equal_fn)
    {
        assert((traits.count & (traits.count - 1)) == 0 && "bucket_count must be power of 2");
    }

    unordered_set(const unordered_set&)            = delete;
    unordered_set& operator=(const unordered_set&) = delete;
    unordered_set(unordered_set&&)                 = delete;
    unordered_set& operator=(unordered_set&&)      = delete;

    iterator       begin()        { return iterator(this, m_core.begin()); }
    iterator       end()          { return iterator(this, {}); }
    const_iterator begin()  const { return cbegin(); }
    const_iterator end()    const { return cend(); }
    const_iterator cbegin() const { return const_iterator(this, m_core.begin()); }
    const_iterator cend()   const { return const_iterator(this, {}); }

    const hasher& hash_function() const
    {
        return m_hasher;
    }

    const key_equal& key_eq() const
    {
        return m_key_equal;
    }

private:
    template <typename Key, typename SearchEqual>
    static bool match_key(const void* key, const detail::unordered_hook_state* node, const void* ctx) noexcept
    {
        return (*static_cast<const SearchEqual*>(ctx))(*static_cast<const Key*>(key),
                                                       *detail::unordered_value<T>(node));
    }

public:
    // Non-const find
    template <typename Key, typename SearchHasher, typename SearchEqual>
    iterator find(const Key& key, const SearchHasher& hash_fn, const SearchEqual& equal_fn)
    {
        if (m_core.bucket_count() == 0) { return end(); }
        const auto h   = hash_fn(key);
        const auto idx = h & (m_core.bucket_count() - 1);
        auto* current  = const_cast<detail::unordered_hook_state*>(m_core.find(idx, h, &key, &match_key<Key, SearchEqual>, &equal_fn));
        if (current == nullptr) { return end(); }
        return iterator(this, detail::unordered_iterator_state{idx, current});
    }

    template <typename Key>
    iterator find(const Key& key) { return find(key, m_hasher, m_key_equal); }

    // Const find
    template <typename Key, typename SearchHasher, typename SearchEqual>
    const_iterator find(const Key& key, const SearchHasher& hash_fn, const SearchEqual& equal_fn) const
    {
        if (m_core.bucket_count() == 0) { return end(); }
        const auto h   = hash_fn(key);
        const auto idx = h & (m_core.bucket_count() - 1);
        auto* current  = const_cast<detail::unordered_hook_state*>(m_core.find(idx, h, &key, &match_key<Key, SearchEqual>, &equal_fn));
        if (current == nullptr) { return end(); }
        return const_iterator(this, detail::unordered_iterator_state{idx, current});
    }

    template <typename Key>
    const_iterator find(const Key& key) const { return find(key, m_hasher, m_key_equal); }

    template <typename Key>
    std::size_t count(const Key& key) const
    {
        return find(key) != end() ? 1 : 0;
    }

    template <typename Key>
    bool contains(const Key& key) const
    {
        return find(key) != end();
    }

    std::pair<iterator, bool> insert(T& value)
    {
        const auto h             = m_hasher(value);
        const auto bucket_index = h & (m_core.bucket_count() - 1);
        // Check for duplicate using core find with hash comparison + key_equal
        auto* existing = const_cast<detail::unordered_hook_state*>(
            m_core.find(bucket_index, h, &value, &match_key<T, key_equal>, &m_key_equal));
        if (existing != nullptr) {
            return {iterator(this, detail::unordered_iterator_state{bucket_index, existing}), false};
        }
        auto* hook = detail::unordered_hook_ptr(value);
        m_core.insert_front(bucket_index, h, hook);
        on_insert();
        return {iterator(this, detail::unordered_iterator_state{bucket_index, hook}), true};
    }

    iterator iterator_to(T& value)
    {
        auto*      hook = detail::unordered_hook_ptr(value);
        const auto idx  = hook->hash_value & (m_core.bucket_count() - 1);
        return iterator(this, detail::unordered_iterator_state{idx, hook});
    }

    const_iterator iterator_to(const T& value) const
    {
        auto*      hook = detail::unordered_hook_ptr(const_cast<T&>(value));
        const auto idx  = hook->hash_value & (m_core.bucket_count() - 1);
        return const_iterator(this, detail::unordered_iterator_state{idx, hook});
    }

    void erase(iterator it)
    {
        if (it == end()) { return; }
        if (m_core.erase(it.m_state.bucket_idx, it.m_state.current)) {
            on_erase();
        }
    }

    void erase(const_iterator it)
    {
        if (it == end()) { return; }
        if (m_core.erase(it.m_state.bucket_idx, const_cast<detail::unordered_hook_state*>(it.m_state.current))) {
            on_erase();
        }
    }

    void erase(T& value)
    {
        auto*      hook = detail::unordered_hook_ptr(value);
        const auto idx  = hook->hash_value & (m_core.bucket_count() - 1);
        if (m_core.erase(idx, hook)) {
            on_erase();
        }
    }

    void clear()
    {
        m_core.clear();
        reset_size();
    }

    template <typename Disposer>
    void clear_and_dispose(Disposer disposer)
    {
        m_core.clear_and_dispose(
            [](detail::unordered_hook_state* node, void* ctx) {
                (*static_cast<Disposer*>(ctx))(detail::unordered_value<T>(node));
            },
            &disposer);
        reset_size();
    }

    template <typename Disposer>
    void erase_and_dispose(iterator it, Disposer disposer)
    {
        if (it == end()) { return; }
        auto* hook    = const_cast<detail::unordered_hook_state*>(it.m_state.current);
        auto* value_p = detail::unordered_value<T>(hook);
        m_core.erase(it.m_state.bucket_idx, hook);
        on_erase();
        disposer(value_p);
    }

    template <typename Disposer>
    void erase_and_dispose(T& value, Disposer disposer)
    {
        auto*      hook = detail::unordered_hook_ptr(value);
        const auto idx  = hook->hash_value & (m_core.bucket_count() - 1);
        if (m_core.erase(idx, hook)) {
            on_erase();
        }
        disposer(&value);
    }

    void swap(unordered_set& other)
    {
        if (this == &other) { return; }
        m_core.swap_rehash(other.m_core);
        if constexpr (has_constant_time_size) {
            std::swap(this->m_size, other.m_size);
        }
    }

    void rehash(bucket_traits traits)
    {
        assert((traits.count & (traits.count - 1)) == 0 && "bucket_count must be power of 2");
        m_core.rehash(traits.buckets, traits.count);
    }

    bool empty() const
    {
        return size() == 0;
    }

    std::size_t size() const
    {
        if constexpr (has_constant_time_size) {
            return this->m_size;
        } else {
            return m_core.count_all();
        }
    }

    std::size_t bucket_count() const
    {
        return m_core.bucket_count();
    }

    float load_factor() const
    {
        auto bc = bucket_count();
        return bc == 0 ? 0.0f : static_cast<float>(size()) / static_cast<float>(bc);
    }

private:
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

    detail::unordered_core m_core;
    hasher                 m_hasher;
    key_equal              m_key_equal;
};

} // namespace mc::intrusive

#endif // MC_INTRUSIVE_UNORDERED_SET_H