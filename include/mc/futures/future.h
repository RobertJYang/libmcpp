//-- Copyright (c) 2025 Huawei Technologies Co., Ltd.
//-- openUBMC is licensed under Mulan PSL v2.
//-- You can use this software according to the terms and conditions of the Mulan PSL v2.
//-- You may obtain a copy of Mulan PSL v2 at:
//--         http://license.coscl.org.cn/MulanPSL2
//-- THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
//-- EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
//-- MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
//-- See the Mulan PSL v2 for more details.

#ifndef MC_FUTURES_FUTURE_H
#define MC_FUTURES_FUTURE_H

#include <mc/futures/promise.h>

namespace mc ::futures {

template <typename T>
class Promise;

namespace detail {

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

// 快速构造已完成的 future，直接设置结果和状态避免加锁
template <typename T, typename Executor>
auto make_resolved_state(T value, Executor executor) {
    auto state = make_pooled_state<T>(std::move(executor));
    state->set_value(std::move(value));
    state->set_ready();
    return state;
}

// 快速构造已完成的 future，直接设置结果和状态避免加锁（void 特化）
template <typename Executor>
auto make_resolved_state(Executor executor) {
    auto state = make_pooled_state<void>(std::move(executor));
    state->set_value();
    state->set_ready();
    return state;
}

// 快速构造已失败的 future，直接设置异常和状态，避免加锁
template <typename T, typename Executor>
auto make_rejected_state(std::exception_ptr error, Executor executor) {
    auto state = make_pooled_state<T>(std::move(executor));
    state->set_exception(error);
    state->set_ready();
    return state;
}

} // namespace detail

template <typename T>
class Future : public any_future {
public:
    using value_type    = T;
    using result_type   = std::conditional_t<std::is_void_v<T>, std::monostate, T>;
    using executor_type = mc::runtime::any_executor;
    using is_future     = std::true_type;
    using state_type    = futures::State<T>;
    using future_type   = Future<T>;

    Future() = default;
    explicit Future(state_ptr<state_type> state)
        : any_future(*reinterpret_cast<state_base_ptr*>(&state)) {
    }
    explicit Future(any_future&& future)
        : any_future(std::forward<any_future>(future)) {
    }
    ~Future() = default;

    Future(const Future&)                = default;
    Future& operator=(const Future&)     = default;
    Future(Future&&) noexcept            = default;
    Future& operator=(Future&&) noexcept = default;

    // 链式操作
    template <typename F>
    auto then(F&& func, launch policy = launch::async, std::optional<mc::any_executor> executor = std::nullopt)
        -> std::enable_if_t<
            detail::is_future_v<detail::result_t<T, F>>,
            Future<typename detail::result_t<T, F>::value_type>>;

    template <typename F>
    auto then(F&& func, launch policy = launch::async, std::optional<mc::any_executor> executor = std::nullopt)
        -> std::enable_if_t<!detail::is_future_v<detail::result_t<T, F>>,
                            Future<detail::result_t<T, F>>>;

    template <typename F>
    auto catch_error(F&& func, launch policy = launch::async, std::optional<mc::any_executor> executor = std::nullopt)
        -> std::enable_if_t<
            detail::is_future_v<std::invoke_result_t<F, const mc::exception&>> &&
                std::is_same_v<typename std::invoke_result_t<F, const mc::exception&>::value_type, T>,
            Future<T>>;

    template <typename F>
    auto catch_error(F&& func, launch policy = launch::async, std::optional<mc::any_executor> executor = std::nullopt)
        -> std::enable_if_t<
            !detail::is_future_v<std::invoke_result_t<F, const mc::exception&>> &&
                std::is_same_v<std::invoke_result_t<F, const mc::exception&>, T>,
            Future<T>>;

    // 清理操作（无论成功失败都会执行）
    template <typename F>
    auto finally(F&& cleanup, launch policy = launch::async, std::optional<mc::any_executor> executor = std::nullopt) -> future_type;

    // 查看值但不改变（用于调试/日志）
    template <typename F>
    auto tap(F&& inspector, launch policy = launch::async, std::optional<mc::any_executor> executor = std::nullopt) -> future_type;

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

    // 同步获取结果
    template <typename U = T>
    U get() const;

    // 带超时的获取结果
    template <typename Duration>
    T get_for(Duration duration) const;

    template <typename OtherT>
    auto as_future(std::optional<mc::any_executor> executor = std::nullopt) -> Future<OtherT>;

    const state_ptr<state_type>& get_state() const {
        auto& state = any_future::get_state();
        return *reinterpret_cast<const state_ptr<state_type>*>(&state);
    }

private:
    template <typename U>
    friend class Promise;

    template <typename U>
    friend class Future;
};

} // namespace mc::futures

#include <mc/futures/detail/future_impl.h>

#endif // MC_FUTURES_FUTURE_H
