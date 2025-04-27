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

template <typename PromisePtr, typename State>
void set_promise_exception(PromisePtr& promise, State& state) {
    promise->set_exception(std::get<std::exception_ptr>(state->result));
}

template <typename PromisePtr, typename F, typename... Args>
void set_promise_value(PromisePtr& promise, F&& func, Args&&... args) {
    if constexpr (std::is_same_v<std::invoke_result_t<F, Args...>, void>) {
        func(std::forward<Args>(args)...);
        promise->set_value();
    } else if constexpr (detail::is_future_v<std::invoke_result_t<F, Args...>>) {
        auto future = func(std::forward<Args>(args)...);
        future
            .then([promise](auto&& value) {
                promise->set_value(std::forward<decltype(value)>(value));
            })
            .catch_error([promise](const std::exception& ec) {
                promise->set_exception(std::current_exception());
                return 0;
            });
    } else {
        promise->set_value(func(std::forward<Args>(args)...));
    }
}

template <bool IsVoid, typename T, typename PromisePtr, typename F, typename State>
void handle_result_impl(PromisePtr& promise, F&& func, State& state) {
    if constexpr (IsVoid) {
        if (std::holds_alternative<std::monostate>(state->result)) {
            set_promise_value(promise, func);
        } else {
            set_promise_exception(promise, state);
        }
    } else {
        if (auto* value = std::get_if<T>(&state->result)) {
            set_promise_value(promise, func, std::move(*value));
        } else {
            set_promise_exception(promise, state);
        }
    }
}

template <typename T, typename ResultType, typename Executor, typename Allocator, typename F>
auto make_continuation(std::shared_ptr<Promise<ResultType, Executor, Allocator>>&& promise,
                       F&& func, std::shared_ptr<State<T, Executor, Allocator>> state) {
    return [promise = std::move(promise), func = std::forward<F>(func),
            state = std::move(state)]() mutable {
        try {
            handle_result_impl<std::is_void_v<T>, T>(promise, func, state);
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    };
}

template <typename T, typename Executor, typename Allocator>
template <typename U>
U Future<T, Executor, Allocator>::get() {
    wait();
    std::unique_lock<std::mutex> lock(state_->mutex);
    if constexpr (std::is_same_v<U, void>) {
        if (std::holds_alternative<std::monostate>(state_->result)) {
            return;
        }
    } else {
        if (auto* value = std::get_if<U>(&state_->result)) {
            return std::move(*value);
        }
    }
    std::rethrow_exception(std::get<std::exception_ptr>(state_->result));
}

template <typename T, typename Executor, typename Allocator>
template <typename Rep, typename Period>
T Future<T, Executor, Allocator>::get_for(
    const std::chrono::duration<Rep, Period>& timeout_duration) {
    auto status = wait_for(timeout_duration);
    if (status == future_status::timeout) {
        throw timeout_error();
    }
    return get();
}

template <typename T, typename Executor, typename Allocator>
bool Future<T, Executor, Allocator>::is_ready() const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->ready;
}

template <typename T, typename Executor, typename Allocator>
void Future<T, Executor, Allocator>::wait() const {
    std::unique_lock<std::mutex> lock(state_->mutex);
    if (state_->deferred) {
        return;
    }
    state_->cv.wait(lock, [this] {
        return state_->ready;
    });
}

template <typename T, typename Executor, typename Allocator>
template <typename Rep, typename Period>
future_status Future<T, Executor, Allocator>::wait_for(
    const std::chrono::duration<Rep, Period>& timeout_duration) const {
    std::unique_lock<std::mutex> lock(state_->mutex);
    if (state_->deferred) {
        return future_status::deferred;
    }
    if (state_->cv.wait_for(lock, timeout_duration, [this] {
            return state_->ready;
        })) {
        return future_status::ready;
    }
    return future_status::timeout;
}

template <typename T, typename Executor, typename Allocator>
template <typename Clock, typename Duration>
future_status Future<T, Executor, Allocator>::wait_until(
    const std::chrono::time_point<Clock, Duration>& timeout_time) const {
    std::unique_lock<std::mutex> lock(state_->mutex);
    if (state_->deferred) {
        return future_status::deferred;
    }
    if (state_->cv.wait_until(lock, timeout_time, [this] {
            return state_->ready;
        })) {
        return future_status::ready;
    }
    return future_status::timeout;
}

template <typename T, typename Executor, typename Allocator>
template <typename CompletionToken>
void Future<T, Executor, Allocator>::async_get(CompletionToken&& token, launch policy) {
    auto handle_result = [token = std::forward<CompletionToken>(token)](auto& state) mutable {
        if (auto* value = std::get_if<T>(&state->result)) {
            token(std::move(*value));
        } else {
            std::rethrow_exception(std::get<std::exception_ptr>(state->result));
        }
    };

    std::unique_lock<std::mutex> lock(state_->mutex);
    if (state_->ready) {
        if (policy == launch::deferred) {
            state_->continuations.push_back(
                [state = state_, handler = std::move(handle_result)]() mutable {
                    handler(state);
                });
            state_->deferred = true;
            state_->policy   = policy;
        } else if (policy == launch::dispatch) {
            state_->executor.dispatch(
                [state = state_, handler = std::move(handle_result)]() mutable {
                    handler(state);
                },
                state_->allocator);
        } else {
            state_->executor.post(
                [state = state_, handler = std::move(handle_result)]() mutable {
                    handler(state);
                },
                state_->allocator);
        }
    } else {
        state_->continuations.push_back(
            [state = state_, handler = std::move(handle_result)]() mutable {
                handler(state);
            });
    }
}

template <typename T, typename Executor, typename Allocator>
template <typename F>
auto Future<T, Executor, Allocator>::then(F&& func, launch policy)
    -> std::enable_if_t<detail::is_future_v<std::invoke_result_t<F, T>>,
                        std::invoke_result_t<F, T>> {
    using ResultFutureType = std::invoke_result_t<F, T>;
    using ResultType       = typename ResultFutureType::value_type;
    using PromiseType      = Promise<ResultType, Executor, Allocator>;
    auto promise           = std::make_shared<PromiseType>(state_->executor, state_->allocator);
    auto future            = promise->get_future();

    auto continuation = make_continuation<T, ResultType, Executor, Allocator, F>(
        std::move(promise), std::forward<F>(func), state_);

    std::unique_lock<std::mutex> lock(state_->mutex);
    if (state_->ready) {
        if (policy == launch::deferred) {
            future.state_->continuations.push_back(continuation);
            future.state_->deferred = true;
            future.state_->policy   = policy;
        } else if (policy == launch::dispatch) {
            state_->executor.dispatch(continuation, state_->allocator);
        } else {
            state_->executor.post(continuation, state_->allocator);
        }
    } else {
        state_->continuations.push_back(continuation);
    }

    return future;
}

template <typename T, typename Executor, typename Allocator>
template <typename F>
auto Future<T, Executor, Allocator>::then(F&& func, launch policy)
    -> std::enable_if_t<!detail::is_future_v<std::invoke_result_t<F, T>>,
                        Future<std::invoke_result_t<F, T>, Executor, Allocator>> {
    using ResultType  = std::invoke_result_t<F, T>;
    using PromiseType = Promise<ResultType, Executor, Allocator>;
    auto promise      = std::make_shared<PromiseType>(state_->executor, state_->allocator);
    auto future       = promise->get_future();

    auto continuation = make_continuation<T, ResultType, Executor, Allocator, F>(
        std::move(promise), std::forward<F>(func), state_);

    std::unique_lock<std::mutex> lock(state_->mutex);
    if (state_->ready) {
        if (policy == launch::deferred) {
            future.state_->continuations.push_back(continuation);
            future.state_->deferred = true;
            future.state_->policy   = policy;
        } else if (policy == launch::dispatch) {
            state_->executor.dispatch(continuation, state_->allocator);
        } else {
            state_->executor.post(continuation, state_->allocator);
        }
    } else {
        state_->continuations.push_back(continuation);
    }

    return future;
}

template <typename T, typename Executor, typename Allocator>
template <typename F>
auto Future<T, Executor, Allocator>::catch_error(F&& handler)
    -> Future<typename std::invoke_result_t<F, std::exception&>, Executor, Allocator> {
    using ResultType = typename std::invoke_result_t<F, std::exception&>;
    auto promise     = std::make_shared<Promise<ResultType, Executor, Allocator>>(state_->executor,
                                                                                  state_->allocator);
    auto future      = promise->get_future();

    auto handle_result = [promise, handler = std::forward<F>(handler), state = state_]() mutable {
        try {
            if constexpr (std::is_same_v<T, void>) {
                if (std::holds_alternative<std::monostate>(state->result)) {
                    promise->set_exception(
                        std::make_exception_ptr(std::runtime_error("No error occurred")));
                } else {
                    try {
                        std::exception_ptr eptr = std::get<std::exception_ptr>(state->result);
                        try {
                            std::rethrow_exception(eptr);
                        } catch (std::exception& e) {
                            promise->set_value(handler(e));
                        }
                    } catch (...) {
                        promise->set_exception(std::current_exception());
                    }
                }
            } else {
                if (auto* value = std::get_if<T>(&state->result)) {
                    promise->set_exception(
                        std::make_exception_ptr(std::runtime_error("No error occurred")));
                } else {
                    try {
                        std::exception_ptr eptr = std::get<std::exception_ptr>(state->result);
                        try {
                            std::rethrow_exception(eptr);
                        } catch (std::exception& e) {
                            promise->set_value(handler(e));
                        }
                    } catch (...) {
                        promise->set_exception(std::current_exception());
                    }
                }
            }
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    };

    std::unique_lock<std::mutex> lock(state_->mutex);
    if (state_->ready) {
        state_->executor.post(handle_result, state_->allocator);
    } else {
        state_->continuations.push_back(handle_result);
    }

    return future;
}

} // namespace mc::futures

#endif // MC_FUTURES_DETAIL_FUTURE_IMPL_H