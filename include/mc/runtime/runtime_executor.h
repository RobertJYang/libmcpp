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

#include <boost/asio.hpp>

namespace mc::runtime {

class runtime_context;

/**
 * @brief 工具类用于绑定目标 thread_pool
 *
 * 在作用域内设置 TLS 变量，使 runtime_executor::select_pool() 返回绑定的 pool。
 *
 * 使用方式：
 * @code
 * {
 *     scoped_pool_binding binding(&ctx.work());
 *     boost::asio::post(executor, [...] { ... });
 * }
 * @endcode
 */
class MC_API scoped_pool_binding {
public:
    explicit scoped_pool_binding(thread_pool* pool);
    ~scoped_pool_binding();

    // 禁用拷贝和移动
    scoped_pool_binding(const scoped_pool_binding&)            = delete;
    scoped_pool_binding& operator=(const scoped_pool_binding&) = delete;
    scoped_pool_binding(scoped_pool_binding&&)                 = delete;
    scoped_pool_binding& operator=(scoped_pool_binding&&)      = delete;

    /**
     * @brief 获取当前绑定的 pool（可能为 nullptr）
     */
    static thread_pool* current_bound_pool();

private:
    thread_pool* m_old_pool;
};

/**
 * @brief runtime_context 级别的 executor
 *
 * 任务执行策略（优先级从高到低）：
 * 1. 如果有 scoped_pool_binding 绑定的 pool，则使用该 pool
 * 2. 如果当前线程在某个 thread_pool 上，则在该 pool 上执行
 * 3. 否则，在默认的 io pool 上执行
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

    /**
     * @brief 比较两个 executor 是否相等
     */
    bool operator==(const runtime_executor& other) const noexcept;

    /**
     * @brief 比较两个 executor 是否不等
     */
    bool operator!=(const runtime_executor& other) const noexcept;

    /**
     * @brief 获取关联的 execution_context
     */
    boost::asio::execution_context& context() const noexcept;

    /**
     * @brief 通知开始工作
     */
    void on_work_started() const noexcept;

    /**
     * @brief 通知完成工作
     */
    void on_work_finished() const noexcept;

    /**
     * @brief 获取关联的 runtime_context
     */
    runtime_context& get_runtime_context() const noexcept;

    /**
     * @brief 检查当前线程是否在此 executor 上执行
     */
    bool running_in_this_thread() const noexcept;

    /**
     * @brief 隐式转换为 any_io_executor
     *
     * 立即选定一个 thread_pool 切片并返回其 executor。
     * 用于需要真正 io_context 的异步操作。
     */
    operator boost::asio::any_io_executor() const;

    /**
     * @brief 隐式转换为 io_context::executor_type
     *
     * 立即选定一个 thread_pool 切片并返回其 executor。
     * 用于需要真正 io_context 的异步操作。
     */
    operator boost::asio::io_context::executor_type() const;

private:
    thread_pool& select_pool() const;

    /**
     * @brief 选择一个 io_context 并返回其 executor
     *
     * 用于类型转换操作，立即绑定到一个具体的 io_context。
     */
    boost::asio::io_context::executor_type select_io_executor() const;

    runtime_context* m_context;
};

// 模板实现
template <typename Function, typename Allocator>
void runtime_executor::post(Function&& f, const Allocator& a) const {
    select_pool().get_executor().post(std::forward<Function>(f), a);
}

template <typename Function, typename Allocator>
void runtime_executor::dispatch(Function&& f, const Allocator& a) const {
    select_pool().get_executor().dispatch(std::forward<Function>(f), a);
}

template <typename Function, typename Allocator>
void runtime_executor::defer(Function&& f, const Allocator& a) const {
    select_pool().get_executor().defer(std::forward<Function>(f), a);
}

} // namespace mc::runtime

namespace boost::asio {
template <>
struct is_executor<mc::runtime::runtime_executor> : std::true_type {};
} // namespace boost::asio

#endif // MC_RUNTIME_RUNTIME_EXECUTOR_H
