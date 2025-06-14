/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_WEAK_PTR_H
#define MC_WEAK_PTR_H

#include <mc/memory/shared_base.h>
#include <mc/memory/shared_ptr.h>

#include <memory>
#include <type_traits>

namespace mc::memory {

/**
 * weak_ptr 智能指针，类似 std::weak_ptr 但专门用于管理 shared_base 对象
 * 支持共享内存中的 offset_ptr
 */
template <typename T, typename PointerType>
class weak_ptr {
public:
    using element_type = T;
    using pointer_type = PointerType;
    using ref_ptr_type = shared_ptr<element_type, pointer_type>;

    // 默认构造函数
    constexpr weak_ptr() noexcept : m_ptr(nullptr) {
    }

    // nullptr 构造函数
    constexpr weak_ptr(std::nullptr_t) noexcept : m_ptr(nullptr) {
    }

    // 从 shared_ptr 构造
    weak_ptr(const ref_ptr_type& ref) noexcept : m_ptr(ref.get()) {
        if (m_ptr) {
            m_ptr->add_weak_ref();
        }
    }

    // 接受裸指针的构造函数
    explicit weak_ptr(pointer_type ptr) noexcept : m_ptr(ptr) {
        if (m_ptr) {
            m_ptr->add_weak_ref();
        }
    }

    // 拷贝构造函数
    weak_ptr(const weak_ptr& other) noexcept : m_ptr(other.m_ptr) {
        if (m_ptr) {
            m_ptr->add_weak_ref();
        }
    }

    // 类型转换拷贝构造函数
    template <typename U, typename UP>
    weak_ptr(const weak_ptr<U, UP>& other) noexcept : m_ptr(other.get()) {
        static_assert(std::is_convertible_v<U*, T*>, "U* must be convertible to T*");
        if (m_ptr) {
            m_ptr->add_weak_ref();
        }
    }

    // 移动构造函数
    weak_ptr(weak_ptr&& other) noexcept : m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }

    // 类型转换移动构造函数
    template <typename U, typename UP>
    weak_ptr(weak_ptr<U, UP>&& other) noexcept : m_ptr(other.get()) {
        static_assert(std::is_convertible_v<U*, T*>, "U* must be convertible to T*");
        other.reset();
    }

    // 拷贝赋值运算符
    weak_ptr& operator=(const weak_ptr& other) noexcept {
        weak_ptr(other).swap(*this);
        return *this;
    }

    // 移动赋值运算符
    weak_ptr& operator=(weak_ptr&& other) noexcept {
        weak_ptr(std::move(other)).swap(*this);
        return *this;
    }

    // 从 shared_ptr 赋值
    weak_ptr& operator=(const ref_ptr_type& ref) noexcept {
        weak_ptr(ref).swap(*this);
        return *this;
    }

    // 类型转换拷贝赋值运算符
    template <typename U, typename UP>
    weak_ptr& operator=(const weak_ptr<U, UP>& other) noexcept {
        static_assert(std::is_convertible_v<U*, T*>, "U* must be convertible to T*");
        weak_ptr(other).swap(*this);
        return *this;
    }

    // 类型转换移动赋值运算符
    template <typename U, typename UP>
    weak_ptr& operator=(weak_ptr<U, UP>&& other) noexcept {
        static_assert(std::is_convertible_v<U*, T*>, "U* must be convertible to T*");
        weak_ptr(std::move(other)).swap(*this);
        return *this;
    }

    // 从不同类型的 shared_ptr 赋值
    template <typename U, typename UP>
    weak_ptr& operator=(const shared_ptr<U, UP>& ref) noexcept {
        static_assert(std::is_convertible_v<U*, T*>, "U* must be convertible to T*");
        weak_ptr(ref).swap(*this);
        return *this;
    }

    // 析构函数
    ~weak_ptr() {
        if (m_ptr && m_ptr->release_weak_ref()) {
            // 弱引用计数为0且强引用计数也为0时，释放内存
            // 由于引用计数使用普通size_t，不会被析构函数破坏
            detail::deallocate_ptr(m_ptr);
        }
    }

    // 重置指针
    void reset() noexcept {
        weak_ptr().swap(*this);
    }

    // 交换两个指针
    void swap(weak_ptr& other) noexcept {
        std::swap(m_ptr, other.m_ptr);
    }

    // 获取弱引用计数
    size_t use_count() const noexcept {
        return m_ptr ? m_ptr->ref_count() : 0;
    }

    // 检查对象是否已过期（强引用计数为0）
    bool expired() const noexcept {
        return !m_ptr || m_ptr->ref_count() == 0;
    }

    // 尝试获取强引用，如果对象已过期则返回空的 shared_ptr
    ref_ptr_type lock() const noexcept {
        if (!m_ptr || !m_ptr->try_add_ref()) {
            return ref_ptr_type();
        }
        return ref_ptr_type(m_ptr, false); // 已经增加了引用计数，不需要再次增加
    }

    // 获取原始指针
    pointer_type get() const noexcept {
        return m_ptr;
    }

    // 检查是否为空
    bool empty() const noexcept {
        return m_ptr == nullptr;
    }

    // 布尔转换运算符
    explicit operator bool() const noexcept {
        return m_ptr != nullptr;
    }

private:
    pointer_type m_ptr;
};

// 相等运算符
template <typename T, typename U>
bool operator==(const weak_ptr<T>& lhs, const weak_ptr<U>& rhs) noexcept {
    return lhs.get() == rhs.get();
}

template <typename T>
bool operator==(const weak_ptr<T>& lhs, std::nullptr_t) noexcept {
    return lhs.get() == nullptr;
}

template <typename T>
bool operator==(std::nullptr_t, const weak_ptr<T>& rhs) noexcept {
    return nullptr == rhs.get();
}

// 不等运算符
template <typename T, typename U>
bool operator!=(const weak_ptr<T>& lhs, const weak_ptr<U>& rhs) noexcept {
    return lhs.get() != rhs.get();
}

template <typename T>
bool operator!=(const weak_ptr<T>& lhs, std::nullptr_t) noexcept {
    return lhs.get() != nullptr;
}

template <typename T>
bool operator!=(std::nullptr_t, const weak_ptr<T>& rhs) noexcept {
    return nullptr != rhs.get();
}

// 小于运算符，用于排序容器
template <typename T, typename U>
bool operator<(const weak_ptr<T>& lhs, const weak_ptr<U>& rhs) noexcept {
    return lhs.get() < rhs.get();
}

} // namespace mc::memory

// 为 std::hash 提供特化支持
namespace std {
template <typename T, typename PointerType>
struct hash<mc::memory::weak_ptr<T, PointerType>> {
    size_t operator()(const mc::memory::weak_ptr<T, PointerType>& p) const noexcept {
        return hash<PointerType>{}(p.get());
    }
};
} // namespace std

#endif // MC_WEAK_PTR_H