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

#ifndef MC_RUNTIME_ANY_EXECUTOR_H
#define MC_RUNTIME_ANY_EXECUTOR_H

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include <mc/common.h>
#include <mc/exception.h>
#include <mc/runtime/detail/task.h>
#include <mc/runtime/immediate_context.h>
#include <mc/runtime/runtime_executor.h>
#include <mc/runtime/runtime_strand.h>
#include <mc/runtime/thread_pool.h>

namespace mc::runtime {
namespace detail {
enum class any_executor_backend_kind : uint8_t { immediate, thread_pool, runtime_strand, runtime_executor };

using any_executor_task = task;

struct any_executor_access;
} // namespace detail

/**
 * @brief 轻量级执行器包装器
 *
 * 当前只支持 mc runtime 自己的内建执行器类型，使用固定大小内联存储和 ops table
 * 避免 variant 和通用堆分配包装带来的额外开销。
 */
class MC_API any_executor {
public:
    using function = detail::any_executor_task;

    /**
     * @brief 默认构造函数
     */
    any_executor();
    any_executor(immediate_executor executor);

    /**
     * @brief 从 thread_pool 执行器构造
     */
    any_executor(thread_pool::executor_type executor);

    /**
     * @brief 从 runtime_strand 执行器构造
     */
    any_executor(runtime_strand executor);

    /**
     * @brief 从 runtime_executor 构造
     */
    any_executor(runtime_executor executor);

    /**
     * @brief 拷贝构造函数
     */
    any_executor(const any_executor& other);

    /**
     * @brief 移动构造函数
     */
    any_executor(any_executor&& other) noexcept;

    /**
     * @brief 拷贝赋值运算符
     */
    any_executor& operator=(const any_executor& other);

    /**
     * @brief 移动赋值运算符
     */
    any_executor& operator=(any_executor&& other) noexcept;

    /**
     * @brief 析构函数
     */
    ~any_executor();

    /**
     * @brief 提交任务到队列末尾执行
     */
    template <typename Function, typename Allocator = std::allocator<void>>
    auto post(Function&& f, const Allocator& a = Allocator()) const
    {
        MC_UNUSED(a);
        post_impl(function(std::forward<Function>(f)));
    }

    /**
     * @brief 延迟执行任务
     */
    template <typename Function, typename Allocator = std::allocator<void>>
    auto defer(Function&& f, const Allocator& a = Allocator()) const
    {
        MC_UNUSED(a);
        defer_impl(function(std::forward<Function>(f)));
    }

    /**
     * @brief 分发任务（可能立即执行）
     */
    template <typename Function, typename Allocator = std::allocator<void>>
    void dispatch(Function&& f, const Allocator& a = Allocator()) const
    {
        MC_UNUSED(a);
        dispatch_impl(function(std::forward<Function>(f)));
    }

    /**
     * @brief 检查执行器是否有效
     */
    bool valid() const noexcept;

    /**
     * @brief 比较两个执行器是否相等
     */
    bool operator==(const any_executor& other) const noexcept;

    /**
     * @brief 比较两个执行器是否不等
     */
    bool operator!=(const any_executor& other) const noexcept;

    void on_work_started() const noexcept;
    void on_work_finished() const noexcept;

    /**
     * @brief 检查当前线程是否在此 executor 上执行
     */
    bool running_in_this_thread() const noexcept;

    any_executor& bound_pool(thread_pool* pool) noexcept;
    thread_pool*  get_bound_pool() const noexcept;

private:
    struct executor_ops {
        detail::any_executor_backend_kind kind;
        void (*destroy)(void*) noexcept;
        void (*copy)(const void*, void*);
        void (*move)(void*, void*) noexcept;
        bool (*equal)(const void*, const void*) noexcept;
        void (*post)(const void*, function&&);
        void (*defer)(const void*, function&&);
        void (*dispatch)(const void*, function&&);
        void (*on_work_started)(const void*) noexcept;
        void (*on_work_finished)(const void*) noexcept;
        bool (*running_in_this_thread)(const void*) noexcept;
        void (*bound_pool)(void*, thread_pool*) noexcept;
        thread_pool* (*get_bound_pool)(const void*) noexcept;
    };

    static constexpr std::size_t storage_size  = 16;
    static constexpr std::size_t storage_align = alignof(std::max_align_t);

    template <typename Executor>
    static constexpr bool fits_inline_v = sizeof(Executor) <= storage_size && alignof(Executor) <= storage_align;

    const executor_ops*                                              m_ops{nullptr};
    typename std::aligned_storage<storage_size, storage_align>::type m_storage;

    void* storage() noexcept
    {
        return &m_storage;
    }

    const void* storage() const noexcept
    {
        return &m_storage;
    }

    template <typename Executor>
    void emplace(Executor executor);

    template <typename Executor>
    static const executor_ops& ops();

    void post_impl(function&& f) const;
    void defer_impl(function&& f) const;
    void dispatch_impl(function&& f) const;

    friend struct detail::any_executor_access;
};

} // namespace mc::runtime

#endif // MC_RUNTIME_ANY_EXECUTOR_H
