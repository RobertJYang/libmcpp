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

    // 错误处理
    template <typename F>
    auto catch_error(F&& handler)
        -> Future<typename std::invoke_result_t<F, std::exception&>, Executor, Allocator>;

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

private:
    template <typename U, typename E, typename A>
    friend class Promise;

    template <typename U, typename E, typename A>
    friend class Future;
    using State = futures::State<T, Executor, Allocator>;
    std::shared_ptr<State> state_;
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

template <typename T, typename Executor, typename Allocator>
auto ok(T value, Executor& executor, Allocator alloc) {
    auto promise = Promise<T, Executor, Allocator>(executor, alloc).set_value(value);
    return promise.get_future();
}

} // namespace mc::futures

#include "mc/futures/detail/future_impl.h"
#include "mc/futures/detail/promise_impl.h"

namespace mc {

namespace detail {
template <typename T, typename = void>
struct is_execution_context : std::false_type {};

template <typename T>
struct is_execution_context<T, std::void_t<typename T::executor_type>> : std::true_type {};

template <typename T>
constexpr bool is_execution_context_v = is_execution_context<T>::value;

template <typename T>
constexpr bool is_executor_v = boost::asio::is_executor<T>::value;
} // namespace detail

template <typename T, typename Execution = boost::asio::io_context,
          typename Allocator = std::allocator<void>>
auto make_promise(Execution& execution, Allocator alloc = Allocator())
    -> std::enable_if_t<detail::is_execution_context_v<Execution>,
                        mc::futures::Promise<T, typename Execution::executor_type, Allocator>> {
    return mc::futures::make_promise<T>(execution.get_executor(), alloc);
}

template <typename T, typename Executor, typename Allocator = std::allocator<void>>
auto make_promise(Executor executor, Allocator alloc = Allocator())
    -> std::enable_if_t<detail::is_executor_v<Executor>,
                        mc::futures::Promise<T, Executor, Allocator>> {
    return mc::futures::make_promise<T>(std::move(executor), alloc);
}

template <typename T, typename Executor = boost::asio::io_context::executor_type,
          typename Allocator = std::allocator<void>>
using future = mc::futures::Future<T, Executor, Allocator>;

template <typename T, typename Executor = boost::asio::io_context::executor_type,
          typename Allocator = std::allocator<void>>
using promise = mc::futures::Promise<T, Executor, Allocator>;

} // namespace mc

#endif // MC_FUTURE_H
