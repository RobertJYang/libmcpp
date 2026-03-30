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

#ifndef MC_RUNTIME_RUNTIME_EXECUTOR_H
#define MC_RUNTIME_RUNTIME_EXECUTOR_H

#include <mc/common.h>
#include <mc/runtime/thread_pool.h>

namespace mc::runtime {

class runtime_context;

/**
 * @brief runtime_context 级别的 executor
 */
class MC_API runtime_executor {
public:
    /**
     * @brief 默认构造函数，使用全局 runtime_context
     */
    runtime_executor();

    /**
     * @brief 从指定的 runtime_context 构造
     */
    explicit runtime_executor(runtime_context& ctx);

    runtime_executor(const runtime_executor&)            = default;
    runtime_executor& operator=(const runtime_executor&) = default;
    runtime_executor(runtime_executor&&)                 = default;
    runtime_executor& operator=(runtime_executor&&)      = default;

    /**
     * @brief 提交任务到队列末尾执行
     */
    template <typename Function, typename Allocator = std::allocator<void>>
    void post(Function&& f, const Allocator& a = Allocator()) const;

    /**
     * @brief 分发任务（可能立即执行）
     */
    template <typename Function, typename Allocator = std::allocator<void>>
    void dispatch(Function&& f, const Allocator& a = Allocator()) const;

    /**
     * @brief 延迟执行任务
     */
    template <typename Function, typename Allocator = std::allocator<void>>
    void defer(Function&& f, const Allocator& a = Allocator()) const;

    bool operator==(const runtime_executor& other) const noexcept;
    bool operator!=(const runtime_executor& other) const noexcept;

    void             on_work_started() const noexcept;
    void             on_work_finished() const noexcept;
    runtime_context& get_runtime_context() const noexcept;
    bool             running_in_this_thread() const noexcept;

    runtime_executor& bound_pool(thread_pool* pool) noexcept
    {
        m_bound_pool = pool;
        return *this;
    }

    thread_pool* get_bound_pool() const noexcept
    {
        return m_bound_pool;
    }

private:
    thread_pool& select_pool() const;

    runtime_context* m_context;
    thread_pool*     m_bound_pool = nullptr;
};

// 模板实现
template <typename Function, typename Allocator>
void runtime_executor::post(Function&& f, const Allocator& a) const
{
    select_pool().get_executor().post(std::forward<Function>(f), a);
}

template <typename Function, typename Allocator>
void runtime_executor::dispatch(Function&& f, const Allocator& a) const
{
    select_pool().get_executor().dispatch(std::forward<Function>(f), a);
}

template <typename Function, typename Allocator>
void runtime_executor::defer(Function&& f, const Allocator& a) const
{
    select_pool().get_executor().defer(std::forward<Function>(f), a);
}

} // namespace mc::runtime

#endif // MC_RUNTIME_RUNTIME_EXECUTOR_H
