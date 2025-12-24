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

#include <mc/futures/exceptions.h>
#include <mc/futures/state.h>
#include <mc/futures/state_pool.h>
#include <mc/futures/status.h>
#include <mc/runtime.h>
#include <mc/time.h>
#include <mc/traits.h>

namespace mc ::futures {

template <typename T, typename Executor, typename Allocator = std::allocator<void>>
class Promise;

namespace detail {
template <typename ResultType, typename Executor, typename Allocator>
struct CombinatorState;
template <typename T, typename = void>
struct is_future : std::false_type {};

template <typename T>
struct is_future<T,
                 std::void_t<typename mc::traits::remove_cvref_t<T>::is_future>>
    : std::true_type {};

template <typename T>
constexpr bool is_future_v = is_future<std::remove_cv_t<T>>::value;

template <typename T, typename = void>
struct is_promise : std::false_type {};

template <typename T>
struct is_promise<T,
                  std::void_t<typename mc::traits::remove_cvref_t<T>::is_promise>>
    : std::true_type {};

template <typename T>
constexpr bool is_promise_v = is_promise<std::remove_cv_t<T>>::value;

template <typename T>
constexpr bool is_cancel_callback_v = is_promise_v<T> || is_future_v<T> || std::is_invocable_v<T>;

template <typename T, typename F, typename... Args>
struct invoke_func_convert_arg;

template <typename T, typename F, typename... Args>
struct invoke_func_convert_arg<T, F, std::tuple<Args...>> {
    template <typename... InputArgs>
    static auto invoke(F&& func, InputArgs&&... args) {
        return func(mc::detail::convert_arg<Args>("future_arg", std::forward<InputArgs>(args))...);
    }
};

template <typename T, typename F, typename... Args>
auto invoke_func(F&& func, Args&&... args) {
    if constexpr (std::is_invocable_v<F>) {
        // 1、不管有没有参数的 future，都支持在链式调用时提供没有参数的 func，
        return func(); // 丢弃参数直接调用 func
    } else if constexpr (std::is_invocable_v<F, Args...>) {
        // 2、判断是否可以直接调用，减少不必要的转换
        return func(std::forward<Args>(args)...);
    } else if constexpr (std::is_invocable_v<F, std::monostate>) {
        // 3、针对 void 类型的 future 做个特殊处理：
        //
        // 正常情况下没有参数的 future 这样使用：
        // mc::delay(100ms).then([]() { // mc::delay 会返回 mc::future<void>
        //    do_some_process();
        // });
        //
        // 有参数的 future 这样使用：
        // mc::resolve<int>(100).then([](auto&& val) {
        //    do_some_process(val);
        // });
        //
        // 但有些场景下，比如在 mc::any 和 mc::all 中，我们希望保持一致的调用方式，
        // 不用每次判断 future 的类型区别处理，所以允许针对 void 类型的 future 这样使用：
        //
        // mc::resolve<void>().then([](auto&&) { // 支持传入一个参数的 lamda 函数
        //    do_some_process();
        // });
        //
        // 所以我们补充一个 std::monostate 参数来兼容这种场景
        return func(std::monostate{});
    } else if constexpr (mc::is_variant_v<T>) {
        // 4、不能直接调用，如果是 mc::variant 类型，尝试转换参数类型后调用
        using function_traits = mc::traits::function_traits<F>;
        using args_type       = typename function_traits::args_type;
        return invoke_func_convert_arg<T, F, args_type>::invoke(
            std::forward<F>(func), std::forward<Args>(args)...);
    } else {
        // 5、还是不行就直接调用，让编译器去报错吧
        return func(std::forward<Args>(args)...);
    }
}

template <typename StatePtr, typename F>
void on_cancel(StatePtr& state, F&& callback) {
    if constexpr (std::is_invocable_v<F>) {
        state->add_cancel_callback(std::forward<F>(callback));
    } else if constexpr (detail::is_future_v<F> || detail::is_promise_v<F>) {
        // 如果是 future 或 promise，则双向 on_cancel 取消对方
        auto other_state = callback.get_state();
        state->add_cancel_callback([state = mc::weak_ptr(other_state)]() {
            if (auto state_ptr = state.lock()) {
                state_ptr->cancel();
            }
        });
        other_state->add_cancel_callback([state = mc::weak_ptr(state)]() {
            if (auto state_ptr = state.lock()) {
                state_ptr->cancel();
            }
        });
    } else {
        MC_THROW(mc::invalid_arg_exception, "Invalid cancel callback type");
    }
}

template <typename T, typename F, typename P, typename = void>
struct continuation_result : std::false_type {
    using return_type = void;
};

template <typename T, typename F, typename P>
struct continuation_result<
    T, F, P,
    std::void_t<decltype(invoke_func<T>(std::declval<F>(), std::declval<P>()))>> : std::true_type {
    using call_result_type = decltype(invoke_func<T>(std::declval<F>(), std::declval<P>()));

    static constexpr bool is_future = is_future_v<call_result_type>;
    using return_type               = call_result_type;
};

template <typename T, typename F, typename Args, typename = void>
struct result_test;

template <typename T, typename F>
struct result_test<T, F, std::tuple<>,
                   std::enable_if_t<std::is_same_v<T, void>>> {
    using type = typename continuation_result<T, F, int>::return_type;
};

template <typename T, typename F, typename Arg, typename... Args>
struct result_test<T, F, std::tuple<Arg, Args...>,
                   std::enable_if_t<std::is_same_v<T, void>>> {
    using type = typename continuation_result<T, F, Arg>::return_type;
};

template <typename T, typename F>
struct result_test<T, F, std::tuple<>,
                   std::enable_if_t<!std::is_same_v<T, void>>> {
    using type = typename continuation_result<T, F, T>::return_type;
};

template <typename T, typename F, typename Arg, typename... Args>
struct result_test<T, F, std::tuple<Arg, Args...>,
                   std::enable_if_t<!std::is_same_v<T, void>>> {
    using type = typename continuation_result<T, F, Arg>::return_type;
};

template <typename T, typename F, typename... Args>
using result_t = typename result_test<T, F, std::tuple<Args...>>::type;

template <typename T>
constexpr bool is_executor_v = boost::asio::is_executor<T>::value ||
                               std::is_same_v<T, boost::asio::any_io_executor>;

template <typename T, typename = void>
struct is_execution_context : std::false_type {};

template <typename T>
struct is_execution_context<T, std::void_t<typename T::executor_type>>
    : std::bool_constant<!std::is_same_v<T, typename T::executor_type>> {};

template <typename T>
constexpr bool is_execution_context_v = is_execution_context<T>::value;

// 快速构造已完成的 future，直接设置结果和状态避免加锁
template <typename T, typename Executor, typename Allocator>
auto make_resolved_state(T value, Executor executor, Allocator alloc) {
    auto state = make_pooled_state<T>(std::move(executor), alloc);
    state->result.template emplace<T>(std::move(value));
    state->ready = true;
    return state;
}

// 快速构造已完成的 future，直接设置结果和状态避免加锁（void 特化）
template <typename Executor, typename Allocator>
auto make_resolved_state(Executor executor, Allocator alloc) {
    auto state    = make_pooled_state<void>(std::move(executor), alloc);
    state->result = std::monostate{};
    state->ready  = true;
    return state;
}

// 快速构造已失败的 future，直接设置异常和状态，避免加锁
template <typename T, typename Executor, typename Allocator>
auto make_rejected_state(std::exception_ptr error, Executor executor, Allocator alloc) {
    auto state    = make_pooled_state<T>(std::move(executor), alloc);
    state->result = error;
    state->ready  = true;
    return state;
}

} // namespace detail

template <typename T, typename Executor, typename Allocator = std::allocator<void>>
class Future {
public:
    using value_type     = T;
    using result_type    = std::conditional_t<std::is_void_v<T>, std::monostate, T>;
    using executor_type  = Executor;
    using allocator_type = Allocator;
    using is_future      = std::true_type;
    using state_type     = futures::State<T, Executor, Allocator>;
    using future_type    = Future<T, Executor, Allocator>;

    Future() = default;
    explicit Future(state_ptr<state_type> state) : state_(state) {
    }
    ~Future() = default;

    Future(const Future&)                = default;
    Future& operator=(const Future&)     = default;
    Future(Future&&) noexcept            = default;
    Future& operator=(Future&&) noexcept = default;

    // 链式操作
    template <typename F>
    auto then(F&& func, launch policy = launch::async)
        -> std::enable_if_t<
            detail::is_future_v<detail::result_t<T, F>>,
            Future<typename detail::result_t<T, F>::value_type, Executor, Allocator>>;

    template <typename F>
    auto then(F&& func, launch policy = launch::async)
        -> std::enable_if_t<!detail::is_future_v<detail::result_t<T, F>>,
                            Future<detail::result_t<T, F>, Executor, Allocator>>;

    // 错误处理 - 统一接受 mc::exception，标准异常自动包装
    template <typename F>
    auto catch_error(F&& handler)
        -> Future<std::invoke_result_t<F, const mc::exception&>, Executor, Allocator>;

    // 清理操作（无论成功失败都会执行）
    template <typename F>
    auto finally(F&& cleanup) -> future_type;

    // 查看值但不改变（用于调试/日志）
    template <typename F>
    auto tap(F&& inspector) -> future_type;

    // 添加取消回调（用于清理资源）- 左值引用版本
    template <typename F>
    auto on_cancel(F&& callback) & -> std::enable_if_t<
        detail::is_cancel_callback_v<F>, future_type&>;

    // 添加取消回调（用于清理资源）- 右值引用版本
    template <typename F>
    auto on_cancel(F&& callback) && -> std::enable_if_t<
        detail::is_cancel_callback_v<F>, future_type>;

    // 获取结果（异步等待）
    template <typename CompletionToken>
    void async_get(CompletionToken&& token, launch policy = launch::async);

    // 检查是否就绪
    bool is_ready() const;

    // 检查是否错误
    bool is_rejected() const;

    std::exception_ptr get_exception() const;

    template <typename ResultType>
    ResultType* get_if() const;

    // 等待策略
    template <typename Duration>
    future_status wait_for(Duration timeout_duration) const;

    template <typename TimePoint>
    future_status wait_until(TimePoint timeout_time) const;

    void wait() const;

    // 同步获取结果
    template <typename U = T>
    U get() const;

    // 带超时的获取结果
    template <typename Duration>
    T get_for(Duration duration) const;

    // 取消操作
    void cancel();

    // 检查是否被取消
    bool is_cancelled() const;

    template <typename OtherT>
    auto as_future() -> Future<OtherT, Executor, Allocator>;

    template <typename OtherT, typename OtherExecutor>
    auto as_future(OtherExecutor&& executor) -> Future<OtherT, OtherExecutor, Allocator>;

    // 获取 State 状态
    auto get_state() const {
        return state_;
    }

private:
    template <typename U, typename E, typename A>
    friend class Promise;

    template <typename U, typename E, typename A>
    friend class Future;

    template <typename R, typename E, typename A>
    friend struct detail::CombinatorState;

    state_ptr<state_type> state_;

    // 内部方法：从State创建新的Future（仅供内部使用）
    static Future from_state(const state_ptr<state_type>& state) {
        return Future(state);
    }
};

template <typename T, typename Executor, typename Allocator>
class Promise {
public:
    using allocator_type = Allocator;
    using future_type    = Future<T, Executor, Allocator>;
    using state_type     = typename future_type::state_type;
    using is_promise     = std::true_type;

    Promise() = default;
    Promise(Executor executor, const Allocator& alloc);

    ~Promise();

    Promise(const Promise&)                = default;
    Promise& operator=(const Promise&)     = default;
    Promise(Promise&&) noexcept            = default;
    Promise& operator=(Promise&&) noexcept = default;

    // 设置值
    template <typename U = T, typename... Args>
    void set_value(Args&&... args);

    // 设置异常
    void set_exception(std::exception_ptr e);

    // 设置 mc 异常
    void set_exception(const mc::exception& e);

    // 取消Promise（设置canceled_exception）
    void cancel();

    // 获取关联的Future
    future_type get_future();

    template <typename F>
    auto on_cancel(F&& callback)
        -> std::enable_if_t<detail::is_cancel_callback_v<F>, void>;

    operator bool() const {
        return state_ != nullptr;
    }

    auto get_state() const {
        return state_;
    }

private:
    state_ptr<state_type> state_;
    bool                  future_retrieved_ = false;
    Allocator             allocator_;
};

template <typename T, typename Executor, typename Allocator>
auto make_promise(Executor executor, Allocator alloc) {
    return Promise<T, Executor, Allocator>(std::move(executor), alloc);
}

// 集合操作
template <typename... Futures>
auto all(Futures&&... futures)
    -> Future<std::tuple<typename std::decay_t<Futures>::result_type...>,
              typename std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>::executor_type,
              typename std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>::allocator_type>;

template <typename... Futures>
auto any(Futures&&... futures)
    -> Future<std::pair<std::size_t,
                        mc::traits::apply_type_t<
                            std::variant,
                            mc::traits::type_set_t<typename std::decay_t<Futures>::result_type...>>>,
              typename std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>::executor_type,
              typename std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>::allocator_type>;

// 容器版本的 all 函数
template <typename Iterator>
auto all(Iterator begin, Iterator end)
    -> Future<std::vector<typename std::iterator_traits<Iterator>::value_type::result_type>,
              typename std::iterator_traits<Iterator>::value_type::executor_type,
              typename std::iterator_traits<Iterator>::value_type::allocator_type>;

// 容器版本的 any 函数
template <typename Iterator>
auto any(Iterator begin, Iterator end)
    -> Future<std::pair<std::size_t, typename std::iterator_traits<Iterator>::value_type::result_type>,
              typename std::iterator_traits<Iterator>::value_type::executor_type,
              typename std::iterator_traits<Iterator>::value_type::allocator_type>;

} // namespace mc::futures

namespace mc {
// 从执行器创建 promise
template <typename T, typename Executor = mc::work_context::executor_type,
          typename Allocator = std::allocator<void>>
auto make_promise(Executor executor = mc::get_work_executor(), Allocator alloc = Allocator())
    -> std::enable_if_t<futures::detail::is_executor_v<Executor>,
                        mc::futures::Promise<T, Executor, Allocator>> {
    return mc::futures::make_promise<T>(std::move(executor), alloc);
}

template <typename T, typename Execution,
          typename Allocator = std::allocator<void>>
auto make_promise(Execution& execution, Allocator alloc = Allocator())
    -> std::enable_if_t<futures::detail::is_execution_context_v<Execution>,
                        mc::futures::Promise<T, typename Execution::executor_type, Allocator>> {
    return mc::futures::make_promise<T>(execution.get_executor(), alloc);
}

template <typename T, typename Executor = mc::work_context::executor_type,
          typename Allocator = std::allocator<void>>
using future = mc::futures::Future<T, Executor, Allocator>;

template <typename T, typename Executor = mc::work_context::executor_type,
          typename Allocator = std::allocator<void>>
using promise = mc::futures::Promise<T, Executor, Allocator>;

namespace futures {

// 创建已完成的 future
template <typename T, typename Executor = runtime::any_executor,
          typename Allocator = std::allocator<void>>
auto resolve(T&& value, Executor executor = runtime::any_executor(),
             Allocator alloc = Allocator())
    -> std::enable_if_t<detail::is_executor_v<Executor>,
                        mc::futures::Future<mc::traits::remove_cvref_t<T>, Executor, Allocator>> {
    auto state = detail::make_resolved_state<mc::traits::remove_cvref_t<T>>(
        std::forward<T>(value), std::move(executor), alloc);
    return Future<mc::traits::remove_cvref_t<T>, Executor, Allocator>(std::move(state));
}

template <typename T, typename Execution = runtime::immediate_context,
          typename Allocator = std::allocator<void>>
auto resolve(T&& value, Execution& execution,
             Allocator alloc = Allocator())
    -> std::enable_if_t<detail::is_execution_context_v<Execution>,
                        mc::futures::Future<mc::traits::remove_cvref_t<T>,
                                            typename Execution::executor_type, Allocator>> {
    return resolve<T>(std::forward<T>(value), execution.get_executor(), alloc);
}

// 创建已完成的 future (void 版本)
template <typename Executor  = runtime::any_executor,
          typename Allocator = std::allocator<void>>
auto resolve(Executor  executor = runtime::any_executor(),
             Allocator alloc    = Allocator())
    -> std::enable_if_t<detail::is_executor_v<Executor>,
                        mc::futures::Future<void, Executor, Allocator>> {
    auto state = detail::make_resolved_state(
        std::move(executor), alloc);
    return Future<void, Executor, Allocator>(std::move(state));
}

template <typename Execution = runtime::immediate_context,
          typename Allocator = std::allocator<void>>
auto resolve(Execution& execution,
             Allocator  alloc = Allocator())
    -> std::enable_if_t<detail::is_execution_context_v<Execution>,
                        mc::futures::Future<void,
                                            typename Execution::executor_type, Allocator>> {
    return resolve<void>(execution.get_executor(), alloc);
}

// 创建已失败的 future
template <typename T, typename Executor = runtime::any_executor,
          typename Allocator = std::allocator<void>>
auto reject(std::exception_ptr eptr,
            Executor           executor = runtime::any_executor(),
            Allocator          alloc    = Allocator())
    -> std::enable_if_t<detail::is_executor_v<Executor>,
                        mc::futures::Future<T, Executor, Allocator>> {
    auto state = detail::make_rejected_state<T>(
        std::move(eptr), std::move(executor), alloc);
    return Future<T, Executor, Allocator>(std::move(state));
}

template <typename T, typename Execution = runtime::immediate_context,
          typename Allocator = std::allocator<void>>
auto reject(std::exception_ptr eptr, Execution& execution,
            Allocator alloc = Allocator())
    -> std::enable_if_t<detail::is_execution_context_v<Execution>,
                        mc::futures::Future<T, typename Execution::executor_type, Allocator>> {
    return reject<T>(std::move(eptr), execution.get_executor(), alloc);
}

// 便利函数：直接从异常创建
template <typename T, typename Exception,
          typename Execution = runtime::immediate_context,
          typename Allocator = std::allocator<void>>
auto reject(Exception&& ex, Execution& execution,
            Allocator alloc = Allocator())
    -> std::enable_if_t<detail::is_execution_context_v<Execution>,
                        mc::futures::Future<T, typename Execution::executor_type, Allocator>> {
    return reject<T>(std::make_exception_ptr(std::forward<Exception>(ex)), execution, alloc);
}

template <typename T, typename Exception,
          typename Executor  = mc::any_executor,
          typename Allocator = std::allocator<void>>
auto reject(Exception&& ex, Executor executor = mc::any_executor(),
            Allocator alloc = Allocator())
    -> std::enable_if_t<detail::is_executor_v<Executor>,
                        mc::futures::Future<T, Executor, Allocator>> {
    return reject<T>(std::make_exception_ptr(std::forward<Exception>(ex)), std::move(executor), alloc);
}

// 超时函数：给 future 添加超时功能
template <typename FutureType, typename Duration,
          typename Executor  = mc::work_context::executor_type,
          typename Allocator = std::allocator<void>>
auto timeout(FutureType future, Duration timeout_duration,
             Executor  executor = mc::get_work_executor(),
             Allocator alloc    = Allocator())
    -> std::enable_if_t<detail::is_future_v<FutureType> && detail::is_executor_v<Executor>,
                        mc::futures::Future<typename FutureType::value_type, Executor, Allocator>> {
    using value_type    = typename FutureType::value_type;
    using duration_type = std::chrono::steady_clock::duration;

    // 优化：如果传入的future已经cancelled
    if (future.is_cancelled()) {
        return mc::futures::reject<value_type>(mc::canceled_exception(), executor, alloc);
    }

    // 优化：如果传入的future已经ready
    if (future.is_ready()) {
        if constexpr (std::is_same_v<value_type, void>) {
            return mc::futures::resolve(executor, alloc);
        } else {
            return mc::futures::resolve(future.get(), executor, alloc);
        }
    }

    auto promise       = mc::make_promise<value_type>(executor, alloc);
    auto result_future = promise.get_future().on_cancel(future);

    struct timer_data {
        using timer_type = mc::runtime::basic_timer<Executor>;

        timer_data(Executor executor, Duration duration)
            : timer(executor, static_cast<duration_type>(duration)),
              completed(false) {
        }

        timer_type        timer;
        std::atomic<bool> completed;
    };
    auto data = std::make_shared<timer_data>(executor, timeout_duration);

    std::move(future).then([promise, data](auto&& value) mutable {
        if (!data->completed.exchange(true)) {
            data->timer.cancel();

            if constexpr (std::is_same_v<std::decay_t<decltype(value)>, void>) {
                promise.set_value();
            } else {
                promise.set_value(std::forward<decltype(value)>(value));
            }
        }
    }).catch_error([promise, data](const std::exception& e) mutable {
        if (!data->completed.exchange(true)) {
            data->timer.cancel();
            promise.set_exception(std::current_exception());
        }
    });

    data->timer.async_wait([promise, data](const auto& ec) mutable {
        if (!ec && !data->completed.exchange(true)) {
            promise.set_exception(std::make_exception_ptr(
                MC_MAKE_EXCEPTION(mc::timeout_exception, "Future 操作超时")));
        }
    });

    result_future.on_cancel([data]() {
        if (!data->completed.exchange(true)) {
            data->timer.cancel();
        }
    });

    return result_future;
}

template <typename FutureType, typename Duration, typename Execution,
          typename Allocator = std::allocator<void>>
auto timeout(FutureType future, Duration timeout_duration,
             Execution& execution,
             Allocator  alloc = Allocator()) {
    return timeout(std::move(future), timeout_duration, execution.get_executor(), alloc);
}

// 延迟执行函数：创建一个在指定时间后完成的 future
template <typename Duration, typename Executor = mc::work_context::executor_type,
          typename Allocator = std::allocator<void>>
auto delay(Duration  delay_duration,
           Executor  executor = mc::get_work_executor(),
           Allocator alloc    = Allocator())
    -> std::enable_if_t<detail::is_executor_v<Executor>,
                        mc::futures::Future<void, Executor, Allocator>> {
    using duration_type = std::chrono::steady_clock::duration;

    auto promise = mc::make_promise<void>(executor, alloc);
    auto future  = promise.get_future();

    using timer_type = mc::runtime::basic_timer<Executor>;

    auto timer = std::make_shared<timer_type>(
        executor,
        static_cast<duration_type>(delay_duration));

    future.on_cancel([timer]() {
        timer->cancel();
    });

    timer->async_wait([promise = std::move(promise), timer](const auto& ec) mutable {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }

        if (!ec) {
            promise.set_value();
        } else {
            promise.set_exception(std::make_exception_ptr(
                MC_MAKE_EXCEPTION(mc::timeout_exception, "定时器错误: ${error}", ("error", ec.message()))));
        }
    });

    return future;
}

template <typename Duration, typename Execution,
          typename Allocator = std::allocator<void>>
auto delay(Duration   delay_duration,
           Execution& execution,
           Allocator  alloc = Allocator())
    -> std::enable_if_t<detail::is_execution_context_v<Execution>,
                        mc::futures::Future<void, typename Execution::executor_type, Allocator>> {
    return delay(delay_duration, execution.get_executor(), alloc);
}

} // namespace futures

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

#include <mc/futures/detail/combinator_future.h>
#include <mc/futures/detail/future_impl.h>
#include <mc/futures/detail/promise_impl.h>

#endif // MC_FUTURE_H
