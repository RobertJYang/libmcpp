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

#ifndef MC_IM_NODE_PTR_H
#define MC_IM_NODE_PTR_H

#include <cstddef>
#include <mc/im/ref_base.h>
#include <memory>
#include <type_traits>
#include <utility>

namespace mc::im {

template <typename T, typename Alloc = std::allocator<T>, typename... Args>
T* allocate(const Alloc& alloc, Args&&... args) {
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
void destroy(const Alloc& alloc, T* ptr) {
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
} // namespace detail

/**
 * ref_ptr 智能指针，类似 std::shared_ptr 但专门用于管理 ref_base 对象
 */
template <typename T, typename PointerType = T*>
class ref_ptr {
public:
    using element_type = T;
    using pointer_type = PointerType;

    // 默认构造函数
    constexpr ref_ptr() noexcept : m_ptr(nullptr) {
    }

    // nullptr 构造函数
    constexpr ref_ptr(std::nullptr_t) noexcept : m_ptr(nullptr) {
    }

    // 接受裸指针的构造函数
    explicit ref_ptr(pointer_type ptr, bool add_ref = true) noexcept : m_ptr(ptr) {
        if (m_ptr && add_ref) {
            m_ptr->add_ref();
        }
    }

    // 拷贝构造函数
    ref_ptr(const ref_ptr& other) noexcept : m_ptr(other.m_ptr) {
        if (m_ptr) {
            m_ptr->add_ref();
        }
    }

    // 移动构造函数
    ref_ptr(ref_ptr&& other) noexcept : m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }

    // 拷贝赋值运算符
    ref_ptr& operator=(const ref_ptr& other) noexcept {
        ref_ptr(other).swap(*this);
        return *this;
    }

    // 移动赋值运算符
    ref_ptr& operator=(ref_ptr&& other) noexcept {
        ref_ptr(std::move(other)).swap(*this);
        return *this;
    }

    // 析构函数
    ~ref_ptr() {
        if (m_ptr && m_ptr->release_ref()) {
            detail::destroy_ptr(m_ptr);
        }
    }

    // 重置指针
    void reset() noexcept {
        ref_ptr().swap(*this);
    }

    // 重置为新指针
    void reset(pointer_type ptr) noexcept {
        ref_ptr(ptr).swap(*this);
    }

    // 交换两个指针
    void swap(ref_ptr& other) noexcept {
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

private:
    pointer_type m_ptr;
};

// 相等运算符
template <typename T, typename U>
bool operator==(const ref_ptr<T>& lhs, const ref_ptr<U>& rhs) noexcept {
    return lhs.get() == rhs.get();
}

template <typename T>
bool operator==(const ref_ptr<T>& lhs, std::nullptr_t) noexcept {
    return lhs.get() == nullptr;
}

template <typename T>
bool operator==(std::nullptr_t, const ref_ptr<T>& rhs) noexcept {
    return nullptr == rhs.get();
}

template <typename T>
bool operator==(T* lhs, const ref_ptr<T>& rhs) noexcept {
    return lhs == rhs.get();
}

template <typename T>
bool operator==(const ref_ptr<T>& lhs, T* rhs) noexcept {
    return lhs.get() == rhs;
}

// 不等运算符
template <typename T, typename U>
bool operator!=(const ref_ptr<T>& lhs, const ref_ptr<U>& rhs) noexcept {
    return lhs.get() != rhs.get();
}

template <typename T>
bool operator!=(const ref_ptr<T>& lhs, std::nullptr_t) noexcept {
    return lhs.get() != nullptr;
}

template <typename T>
bool operator!=(std::nullptr_t, const ref_ptr<T>& rhs) noexcept {
    return nullptr != rhs.get();
}

template <typename T>
bool operator!=(T* lhs, const ref_ptr<T>& rhs) noexcept {
    return lhs != rhs.get();
}

template <typename T>
bool operator!=(const ref_ptr<T>& lhs, T* rhs) noexcept {
    return lhs.get() != rhs;
}

// 小于运算符，用于排序容器
template <typename T, typename U>
bool operator<(const ref_ptr<T>& lhs, const ref_ptr<U>& rhs) noexcept {
    return lhs.get() < rhs.get();
}

template <typename T>
bool operator<(T* lhs, const ref_ptr<T>& rhs) noexcept {
    return lhs < rhs.get();
}

template <typename T>
bool operator<(const ref_ptr<T>& lhs, T* rhs) noexcept {
    return lhs.get() < rhs;
}

template <typename T, typename Alloc = std::allocator<T>, typename... Args>
ref_ptr<T> allocate_ref(const Alloc& alloc, Args&&... args) {
    return ref_ptr<T>(allocate<T, Alloc>(alloc, std::forward<Args>(args)...));
}

/**
 * 创建一个新的引用计数对象指针
 * 类似于std::make_shared，使用默认分配器
 * @tparam T 对象类型
 * @tparam Args 构造函数参数类型
 * @param args 传递给构造函数的参数
 * @return 指向新创建对象的ref_ptr
 */
template <typename T, typename... Args>
ref_ptr<T> make_ref(Args&&... args) {
    return allocate_ref<T>(std::allocator<T>(), std::forward<Args>(args)...);
}

template <typename ObjectType>
mc::im::ref_ptr<ObjectType> cast(ref_base* ptr) {
    auto* p = dynamic_cast<ObjectType*>(ptr);
    return mc::im::ref_ptr<ObjectType>(p);
}

template <typename ObjectType>
mc::im::ref_ptr<const ObjectType> cast(const ref_base* ptr) {
    auto* p = dynamic_cast<const ObjectType*>(ptr);
    return mc::im::ref_ptr<const ObjectType>(p);
}

} // namespace mc::im

#endif // MC_IM_NODE_PTR_H