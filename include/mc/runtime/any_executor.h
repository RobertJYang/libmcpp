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

#include <mc/common.h>
#include <mc/exception.h>
#include <mc/runtime/executor.h>
#include <mc/runtime/immediate_context.h>
#include <mc/runtime/runtime_strand.h>
#include <mc/runtime/thread_pool.h>

#include <boost/asio.hpp>

#include <functional>
#include <type_traits>
#include <variant>

namespace mc::runtime {

namespace detail {
using executor_variant = std::variant<::mc::runtime::immediate_executor, ::mc::runtime::thread_pool::executor_type,
                                      ::mc::runtime::runtime_strand, ::mc::runtime::executor>;
}

/**
 * @brief 轻量级执行器包装器
 *
 * 使用 std::variant 来避免不必要的虚函数开销：
 * - 对于轻量级的 boost::asio 执行器（如 io_context::executor_type），直接存储
 * - 对于重量级执行器（如 strand、thread_pool），使用 mc::runtime::executor 包装
 */
class MC_API any_executor {
public:
    /**
     * @brief 默认构造函数
     */
    any_executor();

    /**
     * @brief 从 thread_pool 执行器构造
     */
    any_executor(thread_pool::executor_type executor);

    /**
     * @brief 从 runtime_strand 执行器构造
     */
    any_executor(runtime_strand executor);

    /**
     * @brief 从 mc::runtime::executor 构造
     */
    any_executor(runtime::executor executor);

    /**
     * @brief 从任意执行器构造（会被包装到 mc::runtime::executor 中）
     */
    template <typename Executor,
              typename = std::enable_if_t<!std::is_same_v<std::decay_t<Executor>, any_executor> &&
                                          !std::is_same_v<std::decay_t<Executor>, thread_pool::executor_type> &&
                                          !std::is_same_v<std::decay_t<Executor>, runtime_strand> &&
                                          !std::is_same_v<std::decay_t<Executor>, runtime::executor>>>
    any_executor(Executor&& executor);

    /**
     * @brief 拷贝构造函数
     */
    any_executor(const any_executor&) = default;

    /**
     * @brief 移动构造函数
     */
    any_executor(any_executor&&) = default;

    /**
     * @brief 拷贝赋值运算符
     */
    any_executor& operator=(const any_executor&) = default;

    /**
     * @brief 移动赋值运算符
     */
    any_executor& operator=(any_executor&&) = default;

    /**
     * @brief 析构函数
     */
    ~any_executor() = default;

    /**
     * @brief 提交任务到队列末尾执行
     */
    template <typename Function, typename Allocator = std::allocator<void>>
    auto post(Function&& f, const Allocator& a = Allocator()) const;

    /**
     * @brief 延迟执行任务
     */
    template <typename Function, typename Allocator = std::allocator<void>>
    auto defer(Function&& f, const Allocator& a = Allocator()) const;

    /**
     * @brief 分发任务（可能立即执行）
     */
    template <typename Function, typename Allocator = std::allocator<void>>
    void dispatch(Function&& f, const Allocator& a = Allocator()) const;

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

    void               on_work_started() const noexcept;
    void               on_work_finished() const noexcept;
    execution_context& context() const;

    /**
     * @brief 检查当前线程是否在此 executor 上执行
     */
    bool running_in_this_thread() const noexcept;

    detail::executor_variant& get_executor() noexcept
    {
        return m_executor;
    }
    const detail::executor_variant& get_executor() const noexcept
    {
        return m_executor;
    }

    any_executor& bound_pool(thread_pool* pool) noexcept;
    thread_pool*  get_bound_pool() const noexcept;

    operator boost::asio::any_io_executor() const;
    operator boost::asio::io_context::executor_type() const;

private:
    detail::executor_variant m_executor;
};

// 模板实现
template <typename Executor, typename>
any_executor::any_executor(Executor&& executor) : m_executor(runtime::executor(std::forward<Executor>(executor)))
{}

template <typename Function, typename Allocator>
auto any_executor::post(Function&& f, const Allocator& a) const
{
    return std::visit([&f, a = std::move(a)](const auto& exec) {
        return exec.post(std::forward<Function>(f), std::move(a));
    }, m_executor);
}

template <typename Function, typename Allocator>
auto any_executor::defer(Function&& f, const Allocator& a) const
{
    return std::visit([&f, a = std::move(a)](const auto& exec) {
        return exec.defer(std::forward<Function>(f), std::move(a));
    }, m_executor);
}

template <typename Function, typename Allocator>
void any_executor::dispatch(Function&& f, const Allocator& a) const
{
    std::visit([&f, a = std::move(a)](const auto& exec) {
        exec.dispatch(std::forward<Function>(f), std::move(a));
    }, m_executor);
}

} // namespace mc::runtime

namespace boost::asio {
template <>
struct is_executor<mc::runtime::any_executor> : std::true_type {};
} // namespace boost::asio

#endif // MC_RUNTIME_ANY_EXECUTOR_H
