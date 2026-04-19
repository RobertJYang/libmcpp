//-- Copyright (c) 2025 Huawei Technologies Co., Ltd.
//-- openUBMC is licensed under Mulan PSL v2.
//-- You can use this software according to the terms and conditions of the Mulan PSL v2.
//-- You may obtain a copy of Mulan PSL v2 at:
//--         http://license.coscl.org.cn/MulanPSL2
//-- THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
//-- EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
//-- MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
//-- See the Mulan PSL v2 for more details.

#ifndef MC_FUTURE_H
#define MC_FUTURE_H

#include <mc/futures/combinator.h>
#include <mc/futures/future.h>
#include <mc/futures/promise.h>

namespace mc ::futures {
// 集合操作
template <typename... Futures>
auto all(Futures&&... futures) -> Future<std::tuple<typename std::decay_t<Futures>::result_type...>>;

template <typename... Futures>
auto any(Futures&&... futures) -> Future<std::pair<
    std::size_t,
    mc::traits::apply_type_t<std::variant, mc::traits::type_set_t<typename std::decay_t<Futures>::result_type...>>>>;

// 容器版本的 all 函数
template <typename Iterator>
auto all(Iterator begin, Iterator end)
    -> Future<std::vector<typename std::iterator_traits<Iterator>::value_type::result_type>>;

// 容器版本的 any 函数
template <typename Iterator>
auto any(Iterator begin, Iterator end)
    -> Future<std::pair<std::size_t, typename std::iterator_traits<Iterator>::value_type::result_type>>;

} // namespace mc::futures

namespace mc {
// 从执行器创建 promise
template <typename T, typename Executor = mc::runtime::any_executor>
auto make_promise(Executor executor = mc::get_io_executor())
    -> std::enable_if_t<futures::detail::is_executor_v<Executor>,
                        mc::futures::Promise<mc::futures::detail::state_tt<T>>>
{
    return mc::futures::make_promise<T>(std::move(executor));
}

template <typename T, typename Execution>
auto make_promise(Execution& execution) -> std::enable_if_t<futures::detail::is_execution_context_v<Execution>,
                                                            mc::futures::Promise<mc::futures::detail::state_tt<T>>>
{
    return mc::futures::make_promise<T>(execution.get_executor());
}

template <typename T, typename Executor, typename Task>
auto make_deferred_future(Executor executor, Task&& task)
    -> std::enable_if_t<futures::detail::is_executor_v<Executor>,
                        decltype(mc::futures::make_deferred_future<T>(std::move(executor), std::forward<Task>(task)))>
{
    return mc::futures::make_deferred_future<T>(std::move(executor), std::forward<Task>(task));
}

template <typename T, typename Execution, typename Task>
auto make_deferred_future(Execution& execution, Task&& task)
    -> std::enable_if_t<futures::detail::is_execution_context_v<Execution>,
                        decltype(mc::futures::make_deferred_future<T>(execution.get_executor(),
                                                                      std::forward<Task>(task)))>
{
    return mc::futures::make_deferred_future<T>(execution.get_executor(), std::forward<Task>(task));
}

template <typename T>
using future = mc::futures::Future<T>;

template <typename T>
using promise = mc::futures::Promise<T>;

namespace futures {

// 创建已完成的 future
template <typename T, typename Executor = runtime::any_executor>
auto resolve(T&& value, Executor executor = runtime::any_executor())
    -> std::enable_if_t<detail::is_executor_v<Executor>, mc::futures::Future<mc::futures::detail::state_tt<T>>>
{
    auto state =
        detail::make_resolved_state<mc::futures::detail::state_tt<T>>(std::forward<T>(value), std::move(executor));
    return Future<mc::futures::detail::state_tt<T>>(std::move(state));
}

template <typename T, typename Execution = runtime::immediate_context>
auto resolve(T&& value, Execution& execution) -> std::enable_if_t<detail::is_execution_context_v<Execution>,
                                                                  mc::futures::Future<mc::futures::detail::state_tt<T>>>
{
    return resolve<T>(std::forward<T>(value), execution.get_executor());
}

// 创建已完成的 future (void 版本)
template <typename Executor = runtime::any_executor>
auto resolve(Executor executor = runtime::any_executor())
    -> std::enable_if_t<detail::is_executor_v<Executor>, mc::futures::Future<void>>
{
    auto state = detail::make_resolved_state(std::move(executor));
    return Future<void>(std::move(state));
}
template <typename Execution = runtime::immediate_context>
auto resolve(Execution& execution)
    -> std::enable_if_t<detail::is_execution_context_v<Execution>, mc::futures::Future<void>>
{
    return resolve(execution.get_executor());
}

// 创建已失败的 future
template <typename T, typename Executor = runtime::any_executor>
auto reject(std::exception_ptr eptr, Executor executor = runtime::any_executor())
    -> std::enable_if_t<detail::is_executor_v<Executor>, mc::futures::Future<mc::futures::detail::state_tt<T>>>
{
    auto state = detail::make_rejected_state<mc::futures::detail::state_tt<T>>(std::move(eptr), std::move(executor));
    return Future<mc::futures::detail::state_tt<T>>(std::move(state));
}

template <typename T, typename Execution = runtime::immediate_context>
auto reject(std::exception_ptr eptr, Execution& execution)
    -> std::enable_if_t<detail::is_execution_context_v<Execution>,
                        mc::futures::Future<mc::futures::detail::state_tt<T>>>
{
    return reject<T>(std::move(eptr), execution.get_executor());
}

// 便利函数：直接从异常创建
template <typename T, typename Exception, typename Execution = runtime::immediate_context>
auto reject(Exception&& ex, Execution& execution)
    -> std::enable_if_t<detail::is_execution_context_v<Execution>,
                        mc::futures::Future<mc::futures::detail::state_tt<T>>>
{
    return reject<T>(std::make_exception_ptr(std::forward<Exception>(ex)), execution);
}

template <typename T, typename Exception, typename Executor = runtime::any_executor>
auto reject(Exception&& ex, Executor executor = mc::any_executor())
    -> std::enable_if_t<detail::is_executor_v<Executor>, mc::futures::Future<mc::futures::detail::state_tt<T>>>
{
    return reject<T>(std::make_exception_ptr(std::forward<Exception>(ex)), std::move(executor));
}

// 超时函数：给 future 添加超时功能
template <typename FutureType, typename Duration, typename Executor = mc::runtime::any_executor>
auto timeout(FutureType future, Duration timeout_duration, Executor executor = mc::get_io_executor())
    -> std::enable_if_t<detail::is_future_v<FutureType> && detail::is_executor_v<Executor>,
                        Future<mc::futures::detail::state_tt<typename FutureType::value_type>>>
{
    using value_type = typename FutureType::value_type;

    // 优化：如果传入的future已经cancelled或ready，则直接返回
    if (future.is_cancelled() || future.is_ready()) {
        return future;
    }

    auto promise       = mc::make_promise<value_type>(executor);
    auto result_future = promise.get_future().on_cancel(future);
    result_future.timeout(future, timeout_duration,
                          [promise = std::move(promise), state = future.get_state()]() mutable {
        promise.set_state_value(*state);
    });
    return result_future;
}

template <typename FutureType, typename Duration, typename Execution>
auto timeout(FutureType future, Duration timeout_duration, Execution& execution)
{
    return timeout(std::move(future), timeout_duration, execution.get_executor());
}

// 延迟执行函数：创建一个在指定时间后完成的 future
template <typename Duration, typename Executor = mc::runtime::any_executor>
auto delay(Duration delay_duration, Executor executor = mc::get_io_executor())
    -> std::enable_if_t<std::is_constructible_v<std::chrono::steady_clock::duration, Duration> &&
                            detail::is_executor_v<Executor>,
                        Future<void>>
{
    return Future<void>{any_future::delay(static_cast<any_future::duration_type>(delay_duration), executor)};
}

// 时间为 0 的延时函数，目的是延迟执行链式调用
template <typename Executor = mc::runtime::any_executor>
auto delay(Executor executor = mc::get_io_executor()) -> std::enable_if_t<detail::is_executor_v<Executor>, Future<void>>
{
    return delay(std::chrono::steady_clock::duration::zero(), executor);
}

template <typename Duration, typename Execution>
auto delay(Duration delay_duration, Execution& execution)
    -> std::enable_if_t<detail::is_execution_context_v<Execution>, Future<void>>
{
    return delay(delay_duration, execution.get_executor());
}

} // namespace futures
} // namespace mc

namespace mc {
// 导出到 mc 命名空间方便使用
using futures::all;
using futures::any;
using futures::delay;
using futures::future_status;
using futures::launch;
using futures::reject;
using futures::resolve;
using futures::timeout;
using futures::detail::is_future_v;
} // namespace mc

#endif // MC_FUTURE_H
