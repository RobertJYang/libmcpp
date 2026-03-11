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

#ifndef MC_ALLOCATOR_H
#define MC_ALLOCATOR_H

#include <memory>
#include <type_traits>
#include <utility>

namespace mc::memory {

/**
 * @brief 使用分配器分配并构造对象
 * @tparam T 对象类型
 * @tparam Alloc 分配器类型
 * @tparam Args 构造函数参数类型
 * @param alloc 分配器
 * @param args 传递给构造函数的参数
 * @return 指向新创建对象的指针
 */
template <typename T, typename Alloc = std::allocator<T>, typename... Args>
T* allocate_ptr(const Alloc& alloc, Args&&... args)
{
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
void destroy_ptr(const Alloc& alloc, T* ptr)
{
    using AllocTraits  = std::allocator_traits<Alloc>;
    using ReboundAlloc = typename AllocTraits::template rebind_alloc<T>;

    ReboundAlloc rebound_alloc(alloc);
    using ReboundAllocTraits = std::allocator_traits<ReboundAlloc>;

    ReboundAllocTraits::destroy(rebound_alloc, ptr);
    ReboundAllocTraits::deallocate(rebound_alloc, ptr, 1);
}

namespace detail {
template <typename T>
static constexpr auto has_m_alloc(int) -> decltype(std::declval<T>().m_alloc, bool())
{
    return true;
}

template <typename T>
static constexpr bool has_m_alloc(...)
{
    return false;
}

template <typename T>
void destroy_ptr(T* ptr)
{
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
void deallocate_ptr(T* ptr)
{
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

template <typename T>
struct default_deleter {
    template <typename U>
    using rebind = default_deleter<U>;

    // 对象析构
    void destroy(T* ptr)
    {
        if constexpr (detail::has_m_alloc<T>(0)) {
            using alloc_type   = typename std::remove_const_t<T>::alloc_type;
            using alloc_traits = std::allocator_traits<alloc_type>;
            alloc_type alloc   = ptr->m_alloc;
            alloc_traits::destroy(alloc, ptr);
        } else {
            ptr->~T();
        }
    }

    // 内存释放
    void deallocate(const void* ptr)
    {
        detail::deallocate_ptr<T>(static_cast<T*>(const_cast<void*>(ptr)));
    }
};

/**
 * @brief Deleter traits 用于检测和调用 Deleter 的方法
 * 支持检测 Deleter 是否具有 destroy 和 deallocate 方法，并提供统一的调用接口
 */
template <typename Deleter, typename T>
struct deleter_traits {
private:
    template <typename D, typename = void>
    struct has_destroy_method_impl : std::false_type {};
    template <typename D>
    struct has_destroy_method_impl<
        D, std::void_t<decltype(std::declval<D&>().destroy(std::declval<T*>()))>>
        : std::true_type {};

    template <typename D, typename = void>
    struct has_deallocate_method_impl : std::false_type {};
    template <typename D>
    struct has_deallocate_method_impl<
        D, std::void_t<decltype(std::declval<D&>().deallocate(std::declval<const void*>()))>>
        : std::true_type {};

public:
    static constexpr bool has_destroy_method    = has_destroy_method_impl<Deleter>::value;
    static constexpr bool has_deallocate_method = has_deallocate_method_impl<Deleter>::value;

    /**
     * @brief 调用对象析构
     * 如果 Deleter 有 destroy 方法则调用它，否则直接调用析构函数
     * @param ptr 要析构的对象指针
     */
    static void destroy(T* ptr)
    {
        if constexpr (has_destroy_method) {
            Deleter{}.destroy(ptr);
        } else {
            ptr->~T();
        }
    }

    /**
     * @brief 调用内存释放
     * 如果 Deleter 有 deallocate 方法则调用它，否则调用 operator()
     * @param ptr 要释放的内存指针
     */
    static void deallocate(const void* ptr)
    {
        if constexpr (has_deallocate_method) {
            Deleter{}.deallocate(ptr);
        } else {
            operator delete(const_cast<void*>(ptr));
        }
    }
};
} // namespace mc::memory

namespace mc {
using mc::memory::allocate_ptr;
using mc::memory::default_deleter;
using mc::memory::deleter_traits;
using mc::memory::destroy_ptr;
} // namespace mc

#endif // MC_ALLOCATOR_H