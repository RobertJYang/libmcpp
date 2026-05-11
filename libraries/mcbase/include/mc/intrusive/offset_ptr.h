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
#ifndef MC_INTRUSIVE_OFFSET_PTR_H
#define MC_INTRUSIVE_OFFSET_PTR_H

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace mc::intrusive {

template <typename T>
class offset_ptr {
public:
    using element_type    = T;
    using pointer         = T*;
    using reference       = T&;
    using difference_type = std::ptrdiff_t;

    static constexpr difference_type null_offset = 1;

    constexpr offset_ptr() noexcept = default;
    constexpr offset_ptr(std::nullptr_t) noexcept
    {}

    explicit offset_ptr(pointer ptr) noexcept
    {
        reset(ptr);
    }

    offset_ptr(const offset_ptr& other) noexcept
    {
        reset(other.get());
    }

    offset_ptr(offset_ptr&& other) noexcept
    {
        reset(other.get());
    }

    offset_ptr& operator=(std::nullptr_t) noexcept
    {
        reset();
        return *this;
    }

    offset_ptr& operator=(pointer ptr) noexcept
    {
        reset(ptr);
        return *this;
    }

    offset_ptr& operator=(const offset_ptr& other) noexcept
    {
        if (this != &other) {
            reset(other.get());
        }
        return *this;
    }

    offset_ptr& operator=(offset_ptr&& other) noexcept
    {
        if (this != &other) {
            reset(other.get());
        }
        return *this;
    }

    reference operator*() const noexcept
    {
        return *get();
    }

    pointer operator->() const noexcept
    {
        return get();
    }

    pointer get() const noexcept
    {
        if (is_null()) {
            return nullptr;
        }

        auto* self = reinterpret_cast<const std::byte*>(this);
        return reinterpret_cast<pointer>(const_cast<std::byte*>(self + m_offset));
    }

    constexpr bool is_null() const noexcept
    {
        return m_offset == null_offset;
    }

    constexpr bool is_offset() const noexcept
    {
        return !is_null();
    }

    constexpr explicit operator bool() const noexcept
    {
        return !is_null();
    }

    constexpr difference_type get_offset() const noexcept
    {
        return m_offset;
    }

    void reset() noexcept
    {
        m_offset = null_offset;
    }

    void reset(pointer ptr) noexcept
    {
        if (ptr == nullptr) {
            reset();
            return;
        }

        auto* self   = reinterpret_cast<const std::byte*>(this);
        auto* target = reinterpret_cast<const std::byte*>(ptr);
        m_offset     = static_cast<difference_type>(target - self);
    }

private:
    difference_type m_offset{null_offset};
};

template <typename T, typename U>
inline bool operator==(const offset_ptr<T>& lhs, const offset_ptr<U>& rhs) noexcept
{
    return lhs.get() == rhs.get();
}

template <typename T>
inline bool operator==(const offset_ptr<T>& lhs, std::nullptr_t) noexcept
{
    return lhs.is_null();
}

template <typename T>
inline bool operator==(std::nullptr_t, const offset_ptr<T>& rhs) noexcept
{
    return rhs.is_null();
}

template <typename T, typename U>
inline bool operator!=(const offset_ptr<T>& lhs, const offset_ptr<U>& rhs) noexcept
{
    return !(lhs == rhs);
}

template <typename T>
inline bool operator!=(const offset_ptr<T>& lhs, std::nullptr_t) noexcept
{
    return !lhs.is_null();
}

template <typename T>
inline bool operator!=(std::nullptr_t, const offset_ptr<T>& rhs) noexcept
{
    return !rhs.is_null();
}

} // namespace mc::intrusive

#endif // MC_INTRUSIVE_OFFSET_PTR_H
