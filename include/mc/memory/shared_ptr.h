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

#ifndef MC_REF_PTR_H
#define MC_REF_PTR_H

#include <mc/memory/shared_base.h>

#include <memory>
#include <type_traits>
#include <utility>

namespace mc::memory {
template <typename T, typename Alloc = std::allocator<T>, typename... Args>
T* allocate_ptr(const Alloc& alloc, Args&&... args) {
    using AllocTraits  = std::allocator_traits<Alloc>;
    using ReboundAlloc = typename AllocTraits::template rebind_alloc<T>;

    ReboundAlloc rebound_alloc(alloc);
    using ReboundAllocTraits = std::allocator_traits<ReboundAlloc>;

    // 分配内存
    T* ptr = ReboundAllocTraits::allocate(rebound_alloc, 1);

    try {
        // 构造对象
        ReboundAllocTraits::construct(rebound_alloc, ptr, std::forward<Args>(args)...);
        return ptr;
    } catch (...) {
        ReboundAllocTraits::deallocate(rebound_alloc, ptr, 1);
        throw;
    }
}

/**
 * @brief 销毁并释放内存
 * @tparam T 对象类型
 * @tparam Alloc 分配器类型
 * @param alloc 分配器
 * @param ptr 指向要销毁的对象的指针
 */
template <typename T, typename Alloc = std::allocator<T>>
void destroy_ptr(const Alloc& alloc, T* ptr) {
    using AllocTraits  = std::allocator_traits<Alloc>;
    using ReboundAlloc = typename AllocTraits::template rebind_alloc<T>;

    ReboundAlloc rebound_alloc(alloc);
    using ReboundAllocTraits = std::allocator_traits<ReboundAlloc>;

    ReboundAllocTraits::destroy(rebound_alloc, ptr);
    ReboundAllocTraits::deallocate(rebound_alloc, ptr, 1);
}

namespace detail {
template <typename T>
static constexpr auto has_m_alloc(int) -> decltype(std::declval<T>().m_alloc, bool()) {
    return true;
}

template <typename T>
static constexpr bool has_m_alloc(...) {
    return false;
}

template <typename T>
void destroy_ptr(T* ptr) {
    using non_const_t = std::remove_const_t<T>;
    if constexpr (has_m_alloc<T>(0)) {
        using alloc_type   = typename non_const_t::alloc_type;
        using alloc_traits = std::allocator_traits<alloc_type>;
        alloc_type alloc   = ptr->m_alloc;
        alloc_traits::destroy(alloc, ptr);
        alloc_traits::deallocate(alloc, const_cast<non_const_t*>(ptr), 1);
    } else {
        std::default_delete<T>()(const_cast<non_const_t*>(ptr));
    }
}

template <typename T>
void deallocate_ptr(T* ptr) {
    using non_const_t = std::remove_const_t<T>;
    if constexpr (has_m_alloc<T>(0)) {
        using alloc_type   = typename non_const_t::alloc_type;
        using alloc_traits = std::allocator_traits<alloc_type>;
        alloc_type alloc   = ptr->m_alloc;
        alloc_traits::deallocate(alloc, const_cast<non_const_t*>(ptr), 1);
    } else {
        operator delete(const_cast<non_const_t*>(ptr));
    }
}
} // namespace detail

/**
 * shared_ptr 智能指针，类似 std::shared_ptr 但专门用于管理 shared_base 对象
 */
template <typename T, typename PointerType>
class shared_ptr {
public:
    using element_type = T;
    using pointer_type = PointerType;

    // 默认构造函数
    constexpr shared_ptr() noexcept : m_ptr(nullptr) {
    }

    // nullptr 构造函数
    constexpr shared_ptr(std::nullptr_t) noexcept : m_ptr(nullptr) {
    }

    // 接受裸指针的构造函数
    explicit shared_ptr(pointer_type ptr, bool add_ref = true) noexcept : m_ptr(ptr) {
        if (m_ptr && add_ref) {
            m_ptr->add_ref();
        }
    }

    // 拷贝构造函数
    shared_ptr(const shared_ptr& other) noexcept : m_ptr(other.m_ptr) {
        if (m_ptr) {
            m_ptr->add_ref();
        }
    }

    // 类型转换拷贝构造函数
    template <typename U, typename UP>
    shared_ptr(const shared_ptr<U, UP>& other) noexcept
        : m_ptr(other.get()) {
        static_assert(std::is_convertible_v<U*, T*>, "U* must be convertible to T*");
        if (m_ptr) {
            m_ptr->add_ref();
        }
    }

    // 移动构造函数
    shared_ptr(shared_ptr&& other) noexcept : m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }

    // 类型转换移动构造函数
    template <typename U, typename UP>
    shared_ptr(shared_ptr<U, UP>&& other) noexcept
        : m_ptr(other.detach()) {
        static_assert(std::is_convertible_v<U*, T*>, "U* must be convertible to T*");
    }

    // 拷贝赋值运算符
    shared_ptr& operator=(const shared_ptr& other) noexcept {
        shared_ptr(other).swap(*this);
        return *this;
    }

    // 移动赋值运算符
    shared_ptr& operator=(shared_ptr&& other) noexcept {
        shared_ptr(std::move(other)).swap(*this);
        return *this;
    }

    // 类型转换拷贝赋值运算符
    template <typename U, typename UP>
    shared_ptr& operator=(const shared_ptr<U, UP>& other) noexcept {
        static_assert(std::is_convertible_v<U*, T*>, "U* must be convertible to T*");
        shared_ptr(other).swap(*this);
        return *this;
    }

    // 类型转换移动赋值运算符
    template <typename U, typename UP>
    shared_ptr& operator=(shared_ptr<U, UP>&& other) noexcept {
        static_assert(std::is_convertible_v<U*, T*>, "U* must be convertible to T*");
        shared_ptr(std::move(other)).swap(*this);
        return *this;
    }

    // 析构函数
    ~shared_ptr() {
        if (m_ptr && m_ptr->release_ref()) {
            // 强引用计数为0时，立即调用对象析构函数
            static_cast<element_type*>(m_ptr)->~element_type();

            // 如果没有弱引用，则立即释放内存
            if (m_ptr->weak_count() == 0) {
                detail::deallocate_ptr(m_ptr);
            }
        }
    }

    // 重置指针
    void reset() noexcept {
        shared_ptr().swap(*this);
    }

    // 重置为新指针
    void reset(pointer_type ptr) noexcept {
        shared_ptr(ptr).swap(*this);
    }

    // 重置为新指针（类型转换版本）
    template <typename U>
    void reset(U* ptr) noexcept {
        static_assert(std::is_convertible_v<U*, T*>, "U* must be convertible to T*");
        shared_ptr(static_cast<pointer_type>(ptr)).swap(*this);
    }

    // 交换两个指针
    void swap(shared_ptr& other) noexcept {
        std::swap(m_ptr, other.m_ptr);
    }

    // 获取原始指针
    pointer_type get() const noexcept {
        return m_ptr;
    }

    // 解引用运算符
    T& operator*() const noexcept {
        return *m_ptr;
    }

    // 箭头运算符
    pointer_type operator->() const noexcept {
        return m_ptr;
    }

    operator pointer_type() const noexcept {
        return m_ptr;
    }

    // 布尔转换运算符
    explicit operator bool() const noexcept {
        return m_ptr != nullptr;
    }

    // 获取引用计数
    size_t use_count() const noexcept {
        return m_ptr ? m_ptr->ref_count() : 0;
    }

    // 检查是否唯一引用
    bool unique() const noexcept {
        return use_count() == 1;
    }

    pointer_type detach() noexcept {
        pointer_type ptr = m_ptr;
        m_ptr            = nullptr;
        return ptr;
    }

    // 静态类型转换
    template <typename U, typename UP = U*>
    shared_ptr<U, UP> static_pointer_cast() const noexcept {
        return shared_ptr<U, UP>(static_cast<UP>(m_ptr));
    }

    // 动态类型转换
    template <typename U, typename UP = U*>
    shared_ptr<U, UP> dynamic_pointer_cast() const noexcept {
        if (auto* ptr = dynamic_cast<UP>(m_ptr)) {
            return shared_ptr<U, UP>(ptr);
        }
        return shared_ptr<U, UP>();
    }

    // 常量类型转换
    template <typename U, typename UP = U*>
    shared_ptr<U, UP> const_pointer_cast() const noexcept {
        return shared_ptr<U, UP>(const_cast<UP>(m_ptr));
    }

    // 重新解释类型转换
    template <typename U, typename UP = U*>
    shared_ptr<U, UP> reinterpret_pointer_cast() const noexcept {
        return shared_ptr<U, UP>(reinterpret_cast<UP>(m_ptr));
    }

private:
    pointer_type m_ptr;
};

// 相等运算符
template <typename T, typename U>
bool operator==(const shared_ptr<T>& lhs, const shared_ptr<U>& rhs) noexcept {
    return lhs.get() == rhs.get();
}

template <typename T>
bool operator==(const shared_ptr<T>& lhs, std::nullptr_t) noexcept {
    return lhs.get() == nullptr;
}

template <typename T>
bool operator==(std::nullptr_t, const shared_ptr<T>& rhs) noexcept {
    return nullptr == rhs.get();
}

template <typename T>
bool operator==(T* lhs, const shared_ptr<T>& rhs) noexcept {
    return lhs == rhs.get();
}

template <typename T>
bool operator==(const shared_ptr<T>& lhs, T* rhs) noexcept {
    return lhs.get() == rhs;
}

// 不等运算符
template <typename T, typename U>
bool operator!=(const shared_ptr<T>& lhs, const shared_ptr<U>& rhs) noexcept {
    return lhs.get() != rhs.get();
}

template <typename T>
bool operator!=(const shared_ptr<T>& lhs, std::nullptr_t) noexcept {
    return lhs.get() != nullptr;
}

template <typename T>
bool operator!=(std::nullptr_t, const shared_ptr<T>& rhs) noexcept {
    return nullptr != rhs.get();
}

template <typename T>
bool operator!=(T* lhs, const shared_ptr<T>& rhs) noexcept {
    return lhs != rhs.get();
}

template <typename T>
bool operator!=(const shared_ptr<T>& lhs, T* rhs) noexcept {
    return lhs.get() != rhs;
}

// 小于运算符，用于排序容器
template <typename T, typename U>
bool operator<(const shared_ptr<T>& lhs, const shared_ptr<U>& rhs) noexcept {
    return lhs.get() < rhs.get();
}

template <typename T>
bool operator<(T* lhs, const shared_ptr<T>& rhs) noexcept {
    return lhs < rhs.get();
}

template <typename T>
bool operator<(const shared_ptr<T>& lhs, T* rhs) noexcept {
    return lhs.get() < rhs;
}

template <typename T, typename Alloc = std::allocator<T>, typename... Args>
shared_ptr<T> allocate_shared(const Alloc& alloc, Args&&... args) {
    return shared_ptr<T>(allocate_ptr<T, Alloc>(alloc, std::forward<Args>(args)...));
}

/**
 * 创建一个新的智能指针对象
 * 类似于std::make_shared，使用默认分配器
 * @tparam T 对象类型
 * @tparam Args 构造函数参数类型
 * @param args 传递给构造函数的参数
 * @return 指向新创建对象的shared_ptr
 */
template <typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) {
    return shared_ptr<T>(allocate_ptr<T>(std::allocator<T>(), std::forward<Args>(args)...));
}

// 全局类型转换函数，类似于标准库的 static_pointer_cast 等

/**
 * 静态类型转换 shared_ptr
 */
template <typename T, typename U, typename UP>
shared_ptr<T> static_pointer_cast(const shared_ptr<U, UP>& r) noexcept {
    return r.template static_pointer_cast<T>();
}

/**
 * 动态类型转换 shared_ptr
 */
template <typename T, typename U, typename UP>
shared_ptr<T> dynamic_pointer_cast(const shared_ptr<U, UP>& r) noexcept {
    return r.template dynamic_pointer_cast<T>();
}

/**
 * 常量类型转换 shared_ptr
 */
template <typename T, typename U, typename UP>
shared_ptr<T> const_pointer_cast(const shared_ptr<U, UP>& r) noexcept {
    return r.template const_pointer_cast<T>();
}

/**
 * 重新解释类型转换 shared_ptr
 */
template <typename T, typename U, typename UP>
shared_ptr<T> reinterpret_pointer_cast(const shared_ptr<U, UP>& r) noexcept {
    return r.template reinterpret_pointer_cast<T>();
}

} // namespace mc::memory

// 为 std::hash 提供特化支持
namespace std {
template <typename T, typename PointerType>
struct hash<mc::memory::shared_ptr<T, PointerType>> {
    size_t operator()(const mc::memory::shared_ptr<T, PointerType>& p) const noexcept {
        return hash<PointerType>{}(p.get());
    }
};
} // namespace std

#endif // MC_REF_PTR_H