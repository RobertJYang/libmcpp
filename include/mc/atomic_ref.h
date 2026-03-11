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

#ifndef MC_ATOMIC_REF_H
#define MC_ATOMIC_REF_H

#include <atomic>
#include <type_traits>

namespace mc {

/**
 * atomic_ref 为 C++17 实现类似 C++20 std::atomic_ref 的功能
 * 提供对非原子对象的原子操作引用
 */
template <typename T>
class atomic_ref {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    static_assert(sizeof(T) <= sizeof(std::atomic<T>), "T size must be compatible with atomic");

public:
    using value_type = T;

    // 构造函数
    explicit atomic_ref(T& obj) noexcept
        : m_ptr(&obj)
    {
        static_assert(alignof(T) >= alignof(std::atomic<T>), "Alignment requirement not met");
    }

    // 拷贝构造函数
    atomic_ref(const atomic_ref& other) noexcept = default;

    // 赋值运算符
    atomic_ref& operator=(const atomic_ref&) = delete;

    // 加载值
    T load(std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        return reinterpret_cast<const volatile std::atomic<T>*>(m_ptr)->load(order);
    }

    // 存储值
    void store(T desired, std::memory_order order = std::memory_order_seq_cst) noexcept
    {
        reinterpret_cast<volatile std::atomic<T>*>(m_ptr)->store(desired, order);
    }

    // 比较并交换（弱版本）
    bool compare_exchange_weak(T& expected, T desired,
                               std::memory_order success = std::memory_order_seq_cst,
                               std::memory_order failure = std::memory_order_seq_cst) noexcept
    {
        return reinterpret_cast<volatile std::atomic<T>*>(m_ptr)->compare_exchange_weak(
            expected, desired, success, failure);
    }

    // 比较并交换（强版本）
    bool compare_exchange_strong(T& expected, T desired,
                                 std::memory_order success = std::memory_order_seq_cst,
                                 std::memory_order failure = std::memory_order_seq_cst) noexcept
    {
        return reinterpret_cast<volatile std::atomic<T>*>(m_ptr)->compare_exchange_strong(
            expected, desired, success, failure);
    }

    // 原子加法（仅限整数类型）
    template <typename U = T>
    typename std::enable_if_t<std::is_integral_v<U>, T>
    fetch_add(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept
    {
        return reinterpret_cast<volatile std::atomic<T>*>(m_ptr)->fetch_add(arg, order);
    }

    // 原子减法（仅限整数类型）
    template <typename U = T>
    typename std::enable_if_t<std::is_integral_v<U>, T>
    fetch_sub(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept
    {
        return reinterpret_cast<volatile std::atomic<T>*>(m_ptr)->fetch_sub(arg, order);
    }

    // 隐式转换为 T
    operator T() const noexcept
    {
        return load();
    }

    // 原子位或操作（仅限整数类型）
    template <typename U = T>
    typename std::enable_if_t<std::is_integral_v<U>, T>
    fetch_or(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept
    {
        return reinterpret_cast<volatile std::atomic<T>*>(m_ptr)->fetch_or(arg, order);
    }

    // 原子位与操作（仅限整数类型）
    template <typename U = T>
    typename std::enable_if_t<std::is_integral_v<U>, T>
    fetch_and(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept
    {
        return reinterpret_cast<volatile std::atomic<T>*>(m_ptr)->fetch_and(arg, order);
    }

    // 原子位异或操作（仅限整数类型）
    template <typename U = T>
    typename std::enable_if_t<std::is_integral_v<U>, T>
    fetch_xor(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept
    {
        return reinterpret_cast<volatile std::atomic<T>*>(m_ptr)->fetch_xor(arg, order);
    }

private:
    T* m_ptr;
};

// 特化 std::size_t 类型
using atomic_size_ref = atomic_ref<std::size_t>;

// 特化 uint8_t 类型
using atomic_uint8_ref = atomic_ref<uint8_t>;

// 特化 uint32_t 类型
using atomic_uint32_ref = atomic_ref<uint32_t>;

} // namespace mc

#endif // MC_ATOMIC_REF_H