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

#ifndef MC_FUTURES_DETAIL_FUTURE_IMPL_H
#define MC_FUTURES_DETAIL_FUTURE_IMPL_H

namespace mc::futures {

inline std::exception_ptr make_invalid_future_exception() {
    return std::make_exception_ptr(MC_MAKE_EXCEPTION(invalid_future_exception, "Future 无效"));
}

template <typename T>
inline auto make_invalid_future(any_executor ex = any_executor()) {
    using value_type = std::conditional_t<std::is_same_v<T, std::monostate>, void, T>;
    return Future<value_type>(detail::make_rejected_state<value_type>(
        make_invalid_future_exception(), std::move(ex)));
}

// 调用 handler 并根据返回值类型设置 promise 结果
template <typename T, typename Promise, typename Handler, typename... Args>
void set_promise_value(Promise& promise, Handler&& handler, Args&&... args) {
    try {
        if constexpr (std::is_same_v<detail::result_t<T, Handler, Args...>, void> ||
                      std::is_void_v<typename Promise::value_type>) {
            detail::invoke_func<T>(std::forward<Handler>(handler),
                                   std::forward<Args>(args)...);
            promise.set_value();
        } else {
            promise.set_value(detail::invoke_func<T>(std::forward<Handler>(handler),
                                                     std::forward<Args>(args)...));
        }
    } catch (...) {
        promise.set_exception(std::current_exception());
    }
}

template <typename T, typename Promise, typename Handler, typename State>
void handle_exception_case(Promise& promise, Handler&& handler, State& state) {
    try {
        std::rethrow_exception(state.get_exception());
    } catch (const mc::exception& mc_ex) {
        if (mc_ex.code() == mc::canceled_exception_code) {
            promise.cancel();
        } else {
            set_promise_value<T>(promise, std::forward<Handler>(handler), mc_ex);
        }
    } catch (const std::exception& std_ex) {
        auto wrapped_ex = mc::std_exception_wrapper::from_current_exception(std_ex);
        set_promise_value<T>(promise, std::forward<Handler>(handler), wrapped_ex);
    } catch (...) {
        auto unknown_ex = MC_MAKE_EXCEPTION(mc::exception, "Unknown Future Exception");
        set_promise_value<T>(promise, std::forward<Handler>(handler), unknown_ex);
    }
}

template <typename T, typename Promise, typename F, typename State>
void handle_result_impl(Promise& promise, F&& func, State& state) {
    if (state.is_cancelled()) {
        promise.cancel();
        return;
    }

    if (state.is_rejected()) {
        promise.set_exception(state.get_exception());
        return;
    }

    if constexpr (std::is_void_v<T>) {
        set_promise_value<T>(promise, std::forward<F>(func));
    } else {
        set_promise_value<T>(promise, std::forward<F>(func), state.get_value());
    }
}

template <typename T, typename Promise, typename Handler, typename State>
void handle_error_impl(Promise& promise, Handler&& handler, State& state) {
    if (state.is_cancelled()) {
        promise.cancel();
        return;
    }

    if (state.is_rejected()) {
        handle_exception_case<T>(promise, std::forward<Handler>(handler), state);
        return;
    }

    if constexpr (std::is_void_v<T>) {
        promise.set_value();
    } else {
        promise.set_value(state.get_value());
    }
}

template <typename T, typename Promise, typename F>
callback_type make_continuation(Promise promise, F&& func, state_ptr<State<T>> state) {
    return [promise = std::move(promise), func = std::forward<F>(func), state = std::move(state)]() mutable {
        try {
            handle_result_impl<T>(promise, std::forward<F>(func), *state);
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    };
}

template <typename T, typename Promise, typename F>
callback_type make_error_continuation(Promise promise, F&& func, state_ptr<State<T>> state) {
    return [promise = std::move(promise), func = std::forward<F>(func), state = std::move(state)]() mutable {
        try {
            handle_error_impl<T>(promise, std::forward<F>(func), *state);
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    };
}

template <typename T>
template <typename U>
U Future<T>::get() const {
    return any_future::get<T>();
}

template <typename T>
template <typename Duration>
T Future<T>::get_for(Duration duration) const {
    auto status = any_future::wait_for(duration);
    if (status == future_status::timeout) {
        MC_THROW(mc::timeout_exception, "Future 超时");
    }
    return get();
}

template <typename T>
template <typename CompletionToken>
void Future<T>::async_get(CompletionToken&& token, launch policy, std::optional<mc::any_executor> executor) {
    if (!m_state) {
        return;
    }

    auto continuation = [token = std::forward<CompletionToken>(token), state = get_state()]() mutable {
        if (state->is_ready()) {
            if constexpr (std::is_void_v<T>) {
                token();
            } else {
                token(state->get_value());
            }
        }
    };
    any_executor ex(executor ? *executor : any_future::get_executor());
    add_continuation(std::move(continuation), policy, ex);
}

template <typename T>
template <typename F>
auto Future<T>::then(F&& func, launch policy, std::optional<mc::any_executor> executor)
    -> std::enable_if_t<
        detail::is_future_v<detail::result_t<T, F>>,
        Future<typename detail::result_t<T, F>::value_type>> {
    using ResultType = typename detail::result_t<T, F>::value_type;

    any_executor ex(executor ? *executor : any_future::get_executor());
    if (!m_state) {
        return Future<ResultType>(make_invalid_future<ResultType>(std::move(ex)));
    }

    auto promise = make_promise<ResultType>(ex);
    auto future  = promise.get_future().on_cancel(*this);
    promise.set_policy(policy);
    add_continuation(make_continuation<T>(std::move(promise), std::forward<F>(func), get_state()), policy, ex);
    return future;
}

template <typename T>
template <typename F>
auto Future<T>::then(F&& func, launch policy, std::optional<mc::any_executor> executor)
    -> std::enable_if_t<!detail::is_future_v<detail::result_t<T, F>>,
                        Future<detail::result_t<T, F>>> {
    using ResultType = detail::result_t<T, F>;

    any_executor ex(executor ? *executor : any_future::get_executor());
    if (!m_state) {
        return Future<ResultType>(make_invalid_future<ResultType>(std::move(ex)));
    }

    auto promise = make_promise<ResultType>(ex);
    auto future  = promise.get_future().on_cancel(*this);
    promise.set_policy(policy);
    add_continuation(make_continuation<T>(std::move(promise), std::forward<F>(func), get_state()), policy, ex);
    return future;
}

template <typename T>
template <typename OtherT>
auto Future<T>::as_future(std::optional<mc::any_executor> executor)
    -> Future<detail::state_tt<OtherT>> {
    using other_type = detail::state_tt<OtherT>;
    if constexpr (std::is_same_v<other_type, T>) {
        return std::move(*this);
    } else if constexpr (std::is_same_v<other_type, void>) {
        return then([](auto&&) {
        }, launch::async, executor);
    } else if constexpr (std::is_same_v<T, void>) {
        return then([]() -> other_type {
            return other_type{};
        }, launch::async, executor);
    } else {
        return then([](auto&& value) -> other_type {
            return other_type(value);
        }, launch::async, executor);
    }
}

template <typename T>
template <typename F>
auto Future<T>::catch_error(F&& func, launch policy, std::optional<mc::any_executor> executor)
    -> std::enable_if_t<
        detail::is_future_v<std::invoke_result_t<F, const mc::exception&>> &&
            std::is_same_v<typename std::invoke_result_t<F, const mc::exception&>::value_type, T>,
        Future<T>> {
    any_executor ex(executor ? *executor : any_future::get_executor());
    if (!m_state) {
        auto future = Future<T>(make_invalid_future<T>(std::move(ex)));
        return future.catch_error(std::forward<F>(func), policy, executor);
    }

    auto promise = make_promise<T>(ex);
    auto future  = promise.get_future().on_cancel(*this);
    add_continuation(make_error_continuation<T>(std::move(promise), std::forward<F>(func), get_state()), policy, ex);
    return future;
}

template <typename T>
template <typename F>
auto Future<T>::catch_error(F&& func, launch policy, std::optional<mc::any_executor> executor)
    -> std::enable_if_t<
        !detail::is_future_v<std::invoke_result_t<F, const mc::exception&>> &&
            std::is_same_v<std::invoke_result_t<F, const mc::exception&>, T>,
        Future<T>> {
    any_executor ex(executor ? *executor : any_future::get_executor());
    if (!m_state) {
        auto future = Future<T>(make_invalid_future<T>(std::move(ex)));
        return future.catch_error(std::forward<F>(func), policy, executor);
    }

    auto promise = make_promise<T>(ex);
    auto future  = promise.get_future().on_cancel(*this);
    add_continuation(make_error_continuation<T>(std::move(promise), std::forward<F>(func), get_state()), policy, ex);
    return future;
}

template <typename T>
template <typename F>
auto Future<T>::finally(F&& cleanup, launch policy, std::optional<mc::any_executor> executor) -> future_type {
    any_executor ex(executor ? *executor : any_future::get_executor());
    if (!m_state) {
        auto future = Future<T>(make_invalid_future<T>(std::move(ex)));
        return future.finally(std::forward<F>(cleanup), policy, executor);
    }

    auto promise = make_promise<T>(ex);
    auto future  = promise.get_future().on_cancel(*this);
    any_future::finally(promise, [promise, cleanup = std::forward<F>(cleanup), state = get_state()]() mutable {
        cleanup();
        promise.set_state_value(*state);
    }, policy, ex);
    return future;
}

template <typename T>
template <typename F>
auto Future<T>::tap(F&& inspector, launch policy) -> future_type {
    if (!m_state) {
        return Future<T>(detail::make_rejected_state<T>(
            make_invalid_future_exception(),
            mc::any_executor(mc::runtime::immediate_executor())));
    }

    any_future::add_continuation([inspector = std::forward<F>(inspector), state = get_state()]() mutable {
        if (state->is_ready()) {
            if constexpr (std::is_void_v<T>) {
                inspector();
            } else {
                inspector(state->get_value());
            }
        }
    }, policy, mc::any_executor(mc::runtime::immediate_executor())); // tap 回调立即执行
    return *this;
}

template <typename T>
template <typename F>
auto Future<T>::tap_error(F&& inspector, launch policy)
    -> std::enable_if_t<std::is_invocable_v<F, const mc::exception&>, future_type> {
    if (!m_state) {
        auto future = Future<T>(detail::make_rejected_state<T>(
            make_invalid_future_exception(), mc::any_executor(mc::runtime::immediate_executor())));
        return future.tap_error(std::forward<F>(inspector), policy);
    }

    any_future::tap_error(std::forward<F>(inspector), policy); // tap_error 回调立即执行
    return *this;
}

template <typename T>
template <typename F>
auto Future<T>::on_cancel(F&& callback) & -> std::enable_if_t<
    detail::is_cancel_callback_v<F>, future_type&> {
    any_future::on_cancel(std::forward<F>(callback));
    return *this;
}

template <typename T>
template <typename F>
auto Future<T>::on_cancel(F&& callback) && -> std::enable_if_t<
    detail::is_cancel_callback_v<F>, future_type> {
    any_future::on_cancel(std::forward<F>(callback));
    return std::move(*this);
}

} // namespace mc::futures

#endif // MC_FUTURES_DETAIL_FUTURE_IMPL_H
