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

} // namespace mc::memory

namespace mc {
using mc::memory::allocate_ptr;
using mc::memory::destroy_ptr;
} // namespace mc

#endif // MC_ALLOCATOR_H