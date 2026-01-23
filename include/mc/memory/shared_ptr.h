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

#include <mc/memory/allocator.h>
#include <mc/memory/shared_base.h>

#include <memory>
#include <type_traits>
#include <utility>

namespace mc::memory {

/**
 * shared_ptr 智能指针，类似 std::shared_ptr 但专门用于管理 enable_shared_from_this 对象
 */
template <typename T, typename Deleter, typename PointerType>
class shared_ptr {
public:
    // 类型别名
    using element_type = T;
    using pointer_type = PointerType;

    // 默认构造函数
    constexpr shared_ptr() noexcept : m_ptr(nullptr) {
    }

    // nullptr 构造函数
    constexpr shared_ptr(std::nullptr_t) noexcept : m_ptr(nullptr) {
    }

    // 接受裸指针的构造函数
    explicit shared_ptr(pointer_type ptr) noexcept : m_ptr(ptr) {
        if (m_ptr) {
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
    ~shared_ptr() noexcept {
        release_noexcept(m_ptr);
    }

    // 重置指针
    void reset() {
        auto* old_ptr = m_ptr;
        m_ptr         = nullptr;
        release(old_ptr);
    }

    // 重置为新指针
    void reset(pointer_type ptr) {
        auto* old_ptr = m_ptr;
        m_ptr         = nullptr;

        shared_ptr new_ptr(ptr);
        m_ptr = new_ptr.detach();

        release(old_ptr);
    }

    // 重置为新指针
    template <typename U>
    void reset(U* ptr) {
        static_assert(std::is_convertible_v<U*, T*>, "U* must be convertible to T*");
        reset(static_cast<pointer_type>(ptr));
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

    // 布尔转换运算符
    operator bool() const noexcept {
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
    auto static_pointer_cast() const noexcept {
        using UDeleter = typename Deleter::template rebind<U>;
        return shared_ptr<U, UDeleter, UP>(static_cast<UP>(m_ptr));
    }

    // 动态类型转换
    template <typename U, typename UP = U*>
    auto dynamic_pointer_cast() const noexcept {
        using UDeleter = typename Deleter::template rebind<U>;
        if (auto* ptr = dynamic_cast<UP>(m_ptr)) {
            return shared_ptr<U, UDeleter, UP>(ptr);
        }
        return shared_ptr<U, UDeleter, UP>();
    }

    // 常量类型转换
    template <typename U, typename UP = U*>
    auto const_pointer_cast() const noexcept {
        using UDeleter = typename Deleter::template rebind<U>;
        return shared_ptr<U, UDeleter, UP>(const_cast<UP>(m_ptr));
    }

    // 重新解释类型转换
    template <typename U, typename UP = U*>
    auto reinterpret_pointer_cast() const noexcept {
        using UDeleter = typename Deleter::template rebind<U>;
        return shared_ptr<U, UDeleter, UP>(reinterpret_cast<UP>(m_ptr));
    }

    // 相等运算符
    template <typename U, typename UDeleter, typename UPointerType>
    bool operator==(const shared_ptr<U, UDeleter, UPointerType>& rhs) const noexcept {
        return this->get() == rhs.get();
    }

    bool operator==(std::nullptr_t) const noexcept {
        return this->get() == nullptr;
    }

    bool operator==(T* rhs) const noexcept {
        return this->get() == rhs;
    }

    // 不等运算符
    template <typename U, typename UDeleter, typename UPointerType>
    bool operator!=(const shared_ptr<U, UDeleter, UPointerType>& rhs) const noexcept {
        return this->get() != rhs.get();
    }

    bool operator!=(std::nullptr_t) const noexcept {
        return this->get() != nullptr;
    }

    bool operator!=(T* rhs) const noexcept {
        return this->get() != rhs;
    }

    // 小于运算符，用于排序容器
    template <typename U, typename UDeleter, typename UPointerType>
    bool operator<(const shared_ptr<U, UDeleter, UPointerType>& rhs) const noexcept {
        return this->get() < rhs.get();
    }

    bool operator<(T* rhs) const noexcept {
        return this->get() < rhs;
    }

private:
    pointer_type m_ptr;

    static void release(pointer_type ptr) {
        if (!ptr) {
            return;
        }

        if (!ptr->release_ref()) {
            return;
        }

        using deleter_traits = mc::deleter_traits<Deleter, element_type>;

        deleter_traits::destroy(static_cast<element_type*>(ptr));
        if (ptr->release_weak_ref()) {
            deleter_traits::deallocate(ptr);
        }
    }

    static void release_noexcept(pointer_type ptr) noexcept {
        try {
            release(ptr);
        } catch (...) {
        }
    }

    // 内部构造函数，用于已经增加引用计数的情况
    struct already_referenced_tag {};
    explicit shared_ptr(pointer_type ptr, already_referenced_tag) noexcept : m_ptr(ptr) {
        // 不增加引用计数，因为调用者已经增加了
    }

    // 允许 weak_ptr 访问私有构造函数
    template <typename U, typename DeleterU, typename UP>
    friend class weak_ptr;

    // 允许 enable_shared_from_this 访问私有构造函数
    template <typename U, typename DeleterU, typename UP, typename CounterType>
    friend class enable_shared_from_this;
};

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
template <typename T, typename U, typename UDeleter, typename UPointerType>
auto static_pointer_cast(const shared_ptr<U, UDeleter, UPointerType>& r) noexcept {
    return r.template static_pointer_cast<T>();
}

/**
 * 动态类型转换 shared_ptr
 */
template <typename T, typename U, typename UDeleter, typename UPointerType>
auto dynamic_pointer_cast(const shared_ptr<U, UDeleter, UPointerType>& r) noexcept {
    return r.template dynamic_pointer_cast<T>();
}

/**
 * 常量类型转换 shared_ptr
 */
template <typename T, typename U, typename UDeleter, typename UPointerType>
auto const_pointer_cast(const shared_ptr<U, UDeleter, UPointerType>& r) noexcept {
    return r.template const_pointer_cast<T>();
}

/**
 * 重新解释类型转换 shared_ptr
 */
template <typename T, typename U, typename UDeleter, typename UPointerType>
auto reinterpret_pointer_cast(const shared_ptr<U, UDeleter, UPointerType>& r) noexcept {
    return r.template reinterpret_pointer_cast<T>();
}

} // namespace mc::memory

// 全局比较运算符重载，支持与 std::nullptr_t 的比较
namespace mc::memory {
template <typename T, typename Deleter, typename PointerType>
bool operator==(std::nullptr_t, const shared_ptr<T, Deleter, PointerType>& rhs) noexcept {
    return nullptr == rhs.get();
}

template <typename T, typename Deleter, typename PointerType>
bool operator!=(std::nullptr_t, const shared_ptr<T, Deleter, PointerType>& rhs) noexcept {
    return nullptr != rhs.get();
}

template <typename T, typename Deleter, typename PointerType>
bool operator==(T* lhs, const shared_ptr<T, Deleter, PointerType>& rhs) noexcept {
    return lhs == rhs.get();
}

template <typename T, typename Deleter, typename PointerType>
bool operator!=(T* lhs, const shared_ptr<T, Deleter, PointerType>& rhs) noexcept {
    return lhs != rhs.get();
}

template <typename T, typename Deleter, typename PointerType>
bool operator<(T* lhs, const shared_ptr<T, Deleter, PointerType>& rhs) noexcept {
    return lhs < rhs.get();
}
} // namespace mc::memory

// 为 std::hash 提供特化支持
namespace std {
template <typename T, typename Deleter, typename PointerType>
struct hash<mc::memory::shared_ptr<T, Deleter, PointerType>> {
    size_t operator()(const mc::memory::shared_ptr<T, Deleter, PointerType>& p) const noexcept {
        return hash<PointerType>{}(p.get());
    }
};
} // namespace std

#endif // MC_REF_PTR_H
