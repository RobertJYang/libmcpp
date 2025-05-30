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

#include <list>
#include <thread>
#include <variant>

#include <boost/asio.hpp>

#include "mc/futures/exceptions.h"
#include "mc/futures/state.h"
#include "mc/futures/status.h"
#include "mc/traits.h"

namespace mc ::futures {

template <typename T, typename Executor, typename Allocator = std::allocator<void>>
class Promise;

namespace detail {

template <typename T, typename = void>
struct is_future : std::false_type {};

template <typename T>
struct is_future<T, std::void_t<typename T::is_future>> : std::true_type {};

template <typename T>
constexpr bool is_future_v = is_future<T>::value;

template <typename T, typename = void>
struct is_execution_context : std::false_type {};

template <typename T>
struct is_execution_context<T, std::void_t<typename T::executor_type>> : std::true_type {};

template <typename T>
constexpr bool is_execution_context_v = is_execution_context<T>::value;

template <typename T>
constexpr bool is_executor_v = boost::asio::is_executor<T>::value;
} // namespace detail

template <typename T, typename Executor, typename Allocator = std::allocator<void>>
class Future {
public:
    using value_type     = T;
    using executor_type  = Executor;
    using allocator_type = Allocator;
    using is_future      = std::true_type;

    explicit Future(std::shared_ptr<futures::State<T, Executor, Allocator>> state) : state_(state) {
    }
    ~Future() = default;

    // 禁止拷贝
    Future(const Future&)            = delete;
    Future& operator=(const Future&) = delete;

    // 允许移动
    Future(Future&&) noexcept            = default;
    Future& operator=(Future&&) noexcept = default;

    // 链式操作
    template <typename F>
    auto then(F&& func, launch policy = launch::async)
        -> std::enable_if_t<detail::is_future_v<std::invoke_result_t<F, T>>,
                            std::invoke_result_t<F, T>>;

    template <typename F>
    auto then(F&& func, launch policy = launch::async)
        -> std::enable_if_t<!detail::is_future_v<std::invoke_result_t<F, T>>,
                            Future<std::invoke_result_t<F, T>, Executor, Allocator>>;

    // 错误处理 - 统一接受 mc::exception，标准异常自动包装
    template <typename F>
    auto catch_error(F&& handler)
        -> Future<std::invoke_result_t<F, const mc::exception&>, Executor, Allocator>;

    // 清理操作（无论成功失败都会执行）
    template <typename F>
    auto finally(F&& cleanup) -> Future<T, Executor, Allocator>;

    // 查看值但不改变（用于调试/日志）
    template <typename F>
    auto tap(F&& inspector) -> Future<T, Executor, Allocator>;

    // 获取结果（异步等待）
    template <typename CompletionToken>
    void async_get(CompletionToken&& token, launch policy = launch::async);

    // 检查是否就绪
    bool is_ready() const;

    // 等待策略
    template <typename Rep, typename Period>
    future_status wait_for(const std::chrono::duration<Rep, Period>& timeout_duration) const;

    template <typename Clock, typename Duration>
    future_status wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time) const;

    void wait() const;

    // 同步获取结果
    template <typename U = T>
    U get();

    // 带超时的获取结果
    template <typename Rep, typename Period>
    T get_for(const std::chrono::duration<Rep, Period>& timeout_duration);

    // 取消操作
    void cancel();

    // 检查是否被取消
    bool is_cancelled() const;

    // 添加取消回调（用于清理资源）
    template <typename F>
    void on_cancel(F&& callback);

private:
    template <typename U, typename E, typename A>
    friend class Promise;

    template <typename U, typename E, typename A>
    friend class Future;

    // 添加 all 和 any 的友元声明
    template <typename... F>
    friend auto all(F&&... futures)
        -> Future<std::tuple<typename std::decay_t<F>::value_type...>,
                  typename std::decay_t<std::tuple_element_t<0, std::tuple<F...>>>::executor_type,
                  typename std::decay_t<std::tuple_element_t<0, std::tuple<F...>>>::allocator_type>;

    template <typename... F>
    friend auto any(F&&... futures)
        -> Future<std::pair<std::size_t, std::variant<typename std::decay_t<F>::value_type...>>,
                  typename std::decay_t<std::tuple_element_t<0, std::tuple<F...>>>::executor_type,
                  typename std::decay_t<std::tuple_element_t<0, std::tuple<F...>>>::allocator_type>;

    using State = futures::State<T, Executor, Allocator>;
    std::shared_ptr<State> state_;

    // 内部方法：获取State智能指针的拷贝（仅供内部使用）
    std::shared_ptr<State> get_state() const {
        return state_;
    }

    // 内部方法：从State创建新的Future（仅供内部使用）
    static Future from_state(const std::shared_ptr<State>& state) {
        return Future(state);
    }
};

template <typename T, typename Executor, typename Allocator>
class Promise {
public:
    using allocator_type = Allocator;
    using future_type    = Future<T, Executor, Allocator>;
    using state_type     = typename future_type::State;

    explicit Promise(Executor executor, const Allocator& alloc);

    ~Promise();

    // 禁止拷贝
    Promise(const Promise&)            = default;
    Promise& operator=(const Promise&) = default;

    // 允许移动
    Promise(Promise&&) noexcept            = default;
    Promise& operator=(Promise&&) noexcept = default;

    // 设置值
    template <typename U = T, typename... Args>
    void set_value(Args&&... args);

    // 设置异常
    void set_exception(std::exception_ptr e);

    // 取消Promise（设置canceled_exception）
    void cancel();

    // 获取关联的Future
    future_type get_future();

    operator bool() const {
        return state_ != nullptr;
    }

private:
    std::shared_ptr<state_type> state_;
    bool                        future_retrieved_ = false;
    Allocator                   allocator_;
};

template <typename T, typename Executor, typename Allocator>
auto make_promise(Executor executor, Allocator alloc) {
    return Promise<T, Executor, Allocator>(std::move(executor), alloc);
}

// 集合操作
template <typename... Futures>
auto all(Futures&&... futures)
    -> Future<std::tuple<typename std::decay_t<Futures>::value_type...>,
              typename std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>::executor_type,
              typename std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>::allocator_type>;

template <typename... Futures>
auto any(Futures&&... futures)
    -> Future<std::pair<std::size_t, std::variant<typename std::decay_t<Futures>::value_type...>>,
              typename std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>::executor_type,
              typename std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>::allocator_type>;

} // namespace mc::futures

#include "mc/futures/detail/future_impl.h"
#include "mc/futures/detail/promise_impl.h"

namespace mc {

template <typename T, typename Execution = boost::asio::io_context,
          typename Allocator = std::allocator<void>>
auto make_promise(Execution& execution, Allocator alloc = Allocator())
    -> std::enable_if_t<futures::detail::is_execution_context_v<Execution>,
                        mc::futures::Promise<T, typename Execution::executor_type, Allocator>> {
    return mc::futures::make_promise<T>(execution.get_executor(), alloc);
}

template <typename T, typename Executor, typename Allocator = std::allocator<void>>
auto make_promise(Executor executor, Allocator alloc = Allocator())
    -> std::enable_if_t<futures::detail::is_executor_v<Executor>,
                        mc::futures::Promise<T, Executor, Allocator>> {
    return mc::futures::make_promise<T>(std::move(executor), alloc);
}

template <typename T, typename Executor = boost::asio::io_context::executor_type,
          typename Allocator = std::allocator<void>>
using future = mc::futures::Future<T, Executor, Allocator>;

template <typename T, typename Executor = boost::asio::io_context::executor_type,
          typename Allocator = std::allocator<void>>
using promise = mc::futures::Promise<T, Executor, Allocator>;

namespace futures {

// 创建已完成的 future（成功值）
template <typename T, typename Execution = boost::asio::io_context,
          typename Allocator = std::allocator<void>>
auto resolve(T&& value, Execution& execution, Allocator alloc = Allocator())
    -> std::enable_if_t<detail::is_execution_context_v<Execution>,
                        mc::futures::Future<std::decay_t<T>, typename Execution::executor_type, Allocator>> {
    auto promise = mc::make_promise<std::decay_t<T>>(execution, alloc);
    auto future  = promise.get_future();
    promise.set_value(std::forward<T>(value));
    return future;
}

template <typename T, typename Executor, typename Allocator = std::allocator<void>>
auto resolve(T&& value, Executor executor, Allocator alloc = Allocator())
    -> std::enable_if_t<detail::is_executor_v<Executor>,
                        mc::futures::Future<std::decay_t<T>, Executor, Allocator>> {
    auto promise = mc::make_promise<std::decay_t<T>>(std::move(executor), alloc);
    auto future  = promise.get_future();
    promise.set_value(std::forward<T>(value));
    return future;
}

// 创建已完成的 future（异常）
template <typename T, typename Execution = boost::asio::io_context,
          typename Allocator = std::allocator<void>>
auto reject(std::exception_ptr eptr, Execution& execution, Allocator alloc = Allocator())
    -> std::enable_if_t<detail::is_execution_context_v<Execution>,
                        mc::futures::Future<T, typename Execution::executor_type, Allocator>> {
    auto promise = mc::make_promise<T>(execution, alloc);
    auto future  = promise.get_future();
    promise.set_exception(eptr);
    return future;
}

template <typename T, typename Executor, typename Allocator = std::allocator<void>>
auto reject(std::exception_ptr eptr, Executor executor, Allocator alloc = Allocator())
    -> std::enable_if_t<detail::is_executor_v<Executor>,
                        mc::futures::Future<T, Executor, Allocator>> {
    auto promise = mc::make_promise<T>(std::move(executor), alloc);
    auto future  = promise.get_future();
    promise.set_exception(eptr);
    return future;
}

// 便利函数：直接从异常创建
template <typename T, typename Exception, typename Execution = boost::asio::io_context,
          typename Allocator = std::allocator<void>>
auto reject(Exception&& ex, Execution& execution, Allocator alloc = Allocator())
    -> std::enable_if_t<detail::is_execution_context_v<Execution>,
                        mc::futures::Future<T, typename Execution::executor_type, Allocator>> {
    return reject<T>(std::make_exception_ptr(std::forward<Exception>(ex)), execution, alloc);
}

template <typename T, typename Exception, typename Executor, typename Allocator = std::allocator<void>>
auto reject(Exception&& ex, Executor executor, Allocator alloc = Allocator())
    -> std::enable_if_t<detail::is_executor_v<Executor>,
                        mc::futures::Future<T, Executor, Allocator>> {
    return reject<T>(std::make_exception_ptr(std::forward<Exception>(ex)), std::move(executor), alloc);
}

// 超时函数：给 future 添加超时功能
template <typename T, typename Executor, typename Allocator, typename Rep, typename Period>
auto timeout(mc::futures::Future<T, Executor, Allocator> future,
             const std::chrono::duration<Rep, Period>&   timeout_duration,
             Executor executor, Allocator alloc = Allocator())
    -> mc::futures::Future<T, Executor, Allocator> {
    // 优化：如果传入的future已经ready，直接返回它
    if (future.is_ready()) {
        return std::move(future);
    }

    // 优化：如果传入的future已经cancelled，创建一个已取消的future返回
    if (future.is_cancelled()) {
        return mc::futures::reject<T>(mc::canceled_exception(), executor, alloc);
    }

    auto promise = std::make_shared<mc::futures::Promise<T, Executor, Allocator>>(
        executor, alloc);
    auto result_future = promise->get_future();

    auto timer     = std::make_shared<boost::asio::steady_timer>(executor, timeout_duration);
    auto completed = std::make_shared<std::atomic<bool>>(false);

    std::move(future).then([promise, completed, timer](auto&& value) {
        if (!completed->exchange(true)) {
            timer->cancel();

            if constexpr (std::is_same_v<std::decay_t<decltype(value)>, void>) {
                promise->set_value();
            } else {
                promise->set_value(std::forward<decltype(value)>(value));
            }
        }
    }).catch_error([promise, completed, timer](const std::exception& e) {
        if (!completed->exchange(true)) {
            timer->cancel();
            promise->set_exception(std::current_exception());
        }
    });

    timer->async_wait([promise, completed, timer](const boost::system::error_code& ec) {
        if (!ec && !completed->exchange(true)) {
            promise->set_exception(std::make_exception_ptr(
                MC_MAKE_EXCEPTION(mc::timeout_exception, "Future 操作超时")));
        }
    });

    result_future.on_cancel([timer, completed]() {
        if (!completed->exchange(true)) {
            timer->cancel();
        }
    });

    return result_future;
}

template <typename T, typename Executor, typename Allocator, typename Rep, typename Period, typename Execution>
auto timeout(mc::futures::Future<T, Executor, Allocator> future,
             const std::chrono::duration<Rep, Period>&   timeout_duration,
             Execution&                                  execution)
    -> std::enable_if_t<detail::is_execution_context_v<Execution>,
                        mc::futures::Future<T, Executor, Allocator>> {
    return timeout(std::move(future), timeout_duration, execution.get_executor());
}

// 延迟执行函数：创建一个在指定时间后完成的 future
template <typename Rep, typename Period, typename Executor, typename Allocator = std::allocator<void>>
auto delay(const std::chrono::duration<Rep, Period>& delay_duration,
           Executor executor, Allocator alloc = Allocator())
    -> std::enable_if_t<detail::is_executor_v<Executor>,
                        mc::futures::Future<void, Executor, Allocator>> {
    auto promise = std::make_shared<mc::futures::Promise<void, Executor, Allocator>>(
        executor, alloc);
    auto future = promise->get_future();

    auto timer = std::make_shared<boost::asio::steady_timer>(executor, delay_duration);

    future.on_cancel([timer]() {
        timer->cancel();
    });

    timer->async_wait([promise, timer](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }

        if (!ec) {
            promise->set_value();
        } else {
            promise->set_exception(std::make_exception_ptr(
                MC_MAKE_EXCEPTION(mc::timeout_exception, "定时器错误: ${error}", ("error", ec.message()))));
        }
    });

    return future;
}

// 便利函数：从 execution context 创建延迟 future
template <typename Rep, typename Period, typename Execution>
auto delay(const std::chrono::duration<Rep, Period>& delay_duration,
           Execution&                                execution)
    -> std::enable_if_t<detail::is_execution_context_v<Execution>,
                        mc::futures::Future<void, typename Execution::executor_type, std::allocator<void>>> {
    return delay(delay_duration, execution.get_executor());
}

} // namespace futures

// 导出到 mc 命名空间方便使用

using futures::all;
using futures::any;
using futures::delay;
using futures::launch;
using futures::reject;
using futures::resolve;
using futures::timeout;

} // namespace mc

#endif // MC_FUTURE_H
