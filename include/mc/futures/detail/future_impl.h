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

namespace detail {

// CombinatorState - 包含 all 和 any 的公共部分
template <typename ResultType, typename Executor, typename Allocator>
struct CombinatorState {
    using promise_type = Promise<ResultType, Executor, Allocator>;
    using promise_ptr  = std::shared_ptr<promise_type>;

    std::mutex    mutex;
    std::size_t   total_count;
    promise_ptr   promise;
    Executor      executor;
    Allocator     allocator;
    callback_list cancel_callbacks;

    template <typename Future>
    CombinatorState(std::size_t total_count, const Future& future)
        : total_count(total_count), promise(make_promise(future)), executor(future.state_->executor), allocator(future.state_->allocator) {
    }

    template <typename Futures>
    void add_cancel_callback(Futures& futures) {
        mc::traits::tuple_for_each(futures, [&](auto& fut) {
            cancel_callbacks.push_back([state = fut.get_state()]() {
                state->cancel();
            });
        });
    }

    void execute_cancel_callbacks() {
        cancel_callbacks.execute_and_clear();
    }

    template <typename Future>
    static auto make_promise(const Future& future) {
        return std::make_shared<promise_type>(
            future.state_->executor, future.state_->allocator);
    }
};

// all 操作专用的 CombinatorState
template <typename ResultType, typename Executor, typename Allocator>
struct AllState : public CombinatorState<ResultType, Executor, Allocator> {
    std::size_t        completed_count = 0;
    ResultType         results;
    std::exception_ptr first_exception = nullptr;

    template <typename Future>
    AllState(std::size_t total_count, const Future& future)
        : CombinatorState<ResultType, Executor, Allocator>(total_count, future) {
    }
};

// any 操作专用的 CombinatorState
template <typename ResultType, typename Executor, typename Allocator>
struct AnyState : public CombinatorState<ResultType, Executor, Allocator> {
    bool        completed      = false;
    std::size_t canceled_count = 0;

    template <typename Future>
    AnyState(std::size_t total_count, const Future& future)
        : CombinatorState<ResultType, Executor, Allocator>(total_count, future) {
    }
};

} // namespace detail

// 调用 handler 并根据返回值类型设置 promise 结果
template <typename PromisePtr, typename Handler, typename... Args>
void set_promise_value(PromisePtr& promise, Handler&& handler, Args&&... args) {
    try {
        if constexpr (std::is_same_v<std::invoke_result_t<Handler, Args...>, void>) {
            handler(std::forward<Args>(args)...);
            promise->set_value();
        } else if constexpr (detail::is_future_v<std::invoke_result_t<Handler, Args...>>) {
            auto future = handler(std::forward<Args>(args)...);
            future.then([promise](auto&& value) {
                promise->set_value(std::forward<decltype(value)>(value));
            }).catch_error([promise](const mc::exception& ec) {
                promise->set_exception(std::current_exception());
            });
        } else {
            promise->set_value(handler(std::forward<Args>(args)...));
        }
    } catch (...) {
        promise->set_exception(std::current_exception());
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

// 可以处理值传递或异常传递的情况
template <typename T, typename PromisePtr, typename F, typename State>
void handle_state_result(PromisePtr& promise, F&& func, State& state) {
    handle_result_impl<std::is_void_v<T>, T>(promise, std::forward<F>(func), state);
}

// 直接传递状态结果（不调用任何函数，仅传递值或异常）
template <typename T, typename PromisePtr, typename State>
void forward_state_result(PromisePtr& promise, State& state) {
    if constexpr (std::is_void_v<T>) {
        if (std::holds_alternative<std::monostate>(state->result)) {
            promise->set_value();
        } else {
            promise->set_exception(std::get<std::exception_ptr>(state->result));
        }
    } else {
        if (auto* value = std::get_if<T>(&state->result)) {
            promise->set_value(std::move(*value));
        } else {
            promise->set_exception(std::get<std::exception_ptr>(state->result));
        }
    }
}

// 在传递状态结果前先调用检查函数（用于 tap）
template <typename T, typename PromisePtr, typename F, typename State>
void inspect_and_forward_state_result(PromisePtr& promise, F&& inspector, State& state) {
    if constexpr (std::is_void_v<T>) {
        if (std::holds_alternative<std::monostate>(state->result)) {
            inspector(); // 查看成功状态
            promise->set_value();
        } else {
            // 对于异常情况，直接传递异常
            promise->set_exception(std::get<std::exception_ptr>(state->result));
        }
    } else {
        if (auto* value = std::get_if<T>(&state->result)) {
            inspector(*value);          // 查看值但不改变
            promise->set_value(*value); // 传递原值
        } else {
            promise->set_exception(std::get<std::exception_ptr>(state->result));
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
    std::lock_guard<std::mutex> lock(state_->m_mutex);
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
        MC_THROW(mc::timeout_exception, "Future 超时");
    }
    return get();
}

template <typename T, typename Executor, typename Allocator>
bool Future<T, Executor, Allocator>::is_ready() const {
    std::lock_guard<std::mutex> lock(state_->m_mutex);
    return state_->ready;
}

template <typename T, typename Executor, typename Allocator>
void Future<T, Executor, Allocator>::wait() const {
    std::unique_lock<std::mutex> lock(state_->m_mutex);
    if (state_->deferred) {
        return;
    }
    state_->m_cv.wait(lock, [&] {
        return state_->ready.load();
    });
}

template <typename T, typename Executor, typename Allocator>
template <typename Rep, typename Period>
future_status Future<T, Executor, Allocator>::wait_for(
    const std::chrono::duration<Rep, Period>& timeout_duration) const {
    std::unique_lock<std::mutex> lock(state_->m_mutex);
    if (state_->deferred) {
        return future_status::deferred;
    }
    if (state_->m_cv.wait_for(lock, timeout_duration, [&] {
        return state_->ready.load();
    })) {
        return future_status::ready;
    }
    return future_status::timeout;
}

template <typename T, typename Executor, typename Allocator>
template <typename Clock, typename Duration>
future_status Future<T, Executor, Allocator>::wait_until(
    const std::chrono::time_point<Clock, Duration>& timeout_time) const {
    std::unique_lock<std::mutex> lock(state_->m_mutex);
    if (state_->deferred) {
        return future_status::deferred;
    }
    if (state_->m_cv.wait_until(lock, timeout_time, [&] {
        return state_->ready.load();
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

    std::unique_lock<std::mutex> lock(state_->m_mutex);
    if (state_->ready) {
        if (policy == launch::deferred) {
            state_->m_continuations.push_back(
                [state = state_, handler = std::move(handle_result)]() mutable {
                handler(state);
            });
            state_->deferred = true;
            state_->policy   = policy;
        } else if (policy == launch::dispatch) {
            state_->executor.dispatch(
                [state = state_, handler = std::move(handle_result)]() mutable {
                handler(state);
            }, state_->allocator);
        } else {
            state_->executor.post([state = state_, handler = std::move(handle_result)]() mutable {
                handler(state);
            }, state_->allocator);
        }
    } else {
        state_->m_continuations.push_back(
            [state = state_, handler = std::move(handle_result)]() mutable {
            handler(state);
        });
    }
}

template <typename T, typename Executor, typename Allocator>
template <typename F>
auto Future<T, Executor, Allocator>::then(F&& func, launch policy)
    -> std::enable_if_t<detail::is_future_v<detail::result_t<T, F>>,
                        detail::result_t<T, F>> {
    using ResultFutureType = detail::result_t<T, F>;
    using ResultType       = typename ResultFutureType::value_type;
    using PromiseType      = Promise<ResultType, Executor, Allocator>;
    auto promise           = std::make_shared<PromiseType>(state_->executor, state_->allocator);
    auto future            = promise->get_future();

    auto continuation = make_continuation<T, ResultType, Executor, Allocator, F>(
        std::move(promise), std::forward<F>(func), state_);

    std::unique_lock<std::mutex> lock(state_->m_mutex);
    if (state_->ready) {
        if (policy == launch::deferred) {
            future.state_->m_continuations.push_back(continuation);
            future.state_->deferred = true;
            future.state_->policy   = policy;
        } else if (policy == launch::dispatch) {
            state_->executor.dispatch(continuation, state_->allocator);
        } else {
            state_->executor.post(continuation, state_->allocator);
        }
    } else {
        state_->m_continuations.push_back(continuation);
    }

    return future;
}

template <typename T, typename Executor, typename Allocator>
template <typename F>
auto Future<T, Executor, Allocator>::then(F&& func, launch policy)
    -> std::enable_if_t<!detail::is_future_v<detail::result_t<T, F>>,
                        Future<detail::result_t<T, F>, Executor, Allocator>> {
    using ResultType  = detail::result_t<T, F>;
    using PromiseType = Promise<ResultType, Executor, Allocator>;
    auto promise      = std::make_shared<PromiseType>(state_->executor, state_->allocator);
    auto future       = promise->get_future();

    auto continuation = make_continuation<T, ResultType, Executor, Allocator, F>(
        std::move(promise), std::forward<F>(func), state_);

    std::unique_lock<std::mutex> lock(state_->m_mutex);
    if (state_->ready) {
        if (policy == launch::deferred) {
            future.state_->m_continuations.push_back(continuation);
            future.state_->deferred = true;
            future.state_->policy   = policy;
        } else if (policy == launch::dispatch) {
            state_->executor.dispatch(continuation, state_->allocator);
        } else {
            state_->executor.post(continuation, state_->allocator);
        }
    } else {
        state_->m_continuations.push_back(continuation);
    }

    return future;
}

template <typename PromisePtr>
void handle_no_error_case(PromisePtr& promise) {
    using ValueType = typename PromisePtr::element_type::future_type::value_type;
    if constexpr (std::is_same_v<ValueType, void>) {
        promise->set_value();
    } else {
        promise->set_exception(std::make_exception_ptr(std::runtime_error("No error occurred")));
    }
}

template <typename PromisePtr, typename Handler, typename State>
void handle_exception_case(PromisePtr& promise, Handler&& handler, State& state) {
    try {
        std::exception_ptr eptr = std::get<std::exception_ptr>(state->result);
        std::rethrow_exception(eptr);

    } catch (const mc::exception& mc_ex) {
        set_promise_value(promise, std::forward<Handler>(handler), mc_ex);
    } catch (const std::exception& std_ex) {
        auto wrapped_ex = mc::std_exception_wrapper::from_current_exception(std_ex);
        set_promise_value(promise, std::forward<Handler>(handler), wrapped_ex);
    } catch (...) {
        auto unknown_ex = MC_MAKE_EXCEPTION(mc::exception, "Unknown Future Exception");
        set_promise_value(promise, std::forward<Handler>(handler), unknown_ex);
    }
}

template <typename T, typename PromisePtr, typename Handler, typename State>
void process_error_result(PromisePtr& promise, Handler&& handler, State& state) {
    using HandlerResultType = std::invoke_result_t<Handler, const mc::exception&>;

    if constexpr (std::is_same_v<T, void>) {
        if (std::holds_alternative<std::monostate>(state->result)) {
            if constexpr (std::is_same_v<HandlerResultType, void>) {
                promise->set_value();
            } else {
                promise->set_exception(std::make_exception_ptr(std::runtime_error("Cannot change return type from void in catch_error")));
            }
        } else {
            handle_exception_case(promise, std::forward<Handler>(handler), state);
        }
    } else {
        if (auto* value = std::get_if<T>(&state->result)) {
            if constexpr (std::is_same_v<HandlerResultType, T>) {
                promise->set_value(std::move(*value));
            } else {
                promise->set_exception(std::make_exception_ptr(std::runtime_error("catch_error handler return type must match previous chain type when no error occurs")));
            }
        } else {
            handle_exception_case(promise, std::forward<Handler>(handler), state);
        }
    }
}

template <typename T, typename Executor, typename Allocator>
template <typename F>
auto Future<T, Executor, Allocator>::catch_error(F&& handler)
    -> Future<std::invoke_result_t<F, const mc::exception&>, Executor, Allocator> {
    using ResultType = std::invoke_result_t<F, const mc::exception&>;

    auto promise = std::make_shared<Promise<ResultType, Executor, Allocator>>(state_->executor,
                                                                              state_->allocator);
    auto future  = promise->get_future();

    auto handle_result = [promise, handler = std::forward<F>(handler), state = state_]() mutable {
        try {
            process_error_result<T>(promise, std::move(handler), state);
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    };

    std::unique_lock<std::mutex> lock(state_->m_mutex);
    if (state_->ready) {
        state_->executor.post(handle_result, state_->allocator);
    } else {
        state_->m_continuations.push_back(handle_result);
    }

    return future;
}

template <typename T, typename Executor, typename Allocator>
template <typename F>
auto Future<T, Executor, Allocator>::finally(F&& cleanup) -> Future<T, Executor, Allocator> {
    auto promise = std::make_shared<Promise<T, Executor, Allocator>>(state_->executor, state_->allocator);
    auto future  = promise->get_future();

    auto handle_result = [promise, cleanup = std::forward<F>(cleanup), state = state_]() mutable {
        try {
            // 无论成功失败都执行清理操作
            cleanup();

            // 然后传递原来的结果或异常
            forward_state_result<T>(promise, state);
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    };

    std::unique_lock<std::mutex> lock(state_->m_mutex);
    if (state_->ready) {
        state_->executor.post(handle_result, state_->allocator);
    } else {
        state_->m_continuations.push_back(handle_result);
    }

    return future;
}

template <typename T, typename Executor, typename Allocator>
template <typename F>
auto Future<T, Executor, Allocator>::tap(F&& inspector) -> Future<T, Executor, Allocator> {
    auto promise = std::make_shared<Promise<T, Executor, Allocator>>(state_->executor, state_->allocator);
    auto future  = promise->get_future();

    auto handle_result = [promise, inspector = std::forward<F>(inspector), state = state_]() mutable {
        try {
            inspect_and_forward_state_result<T>(promise, inspector, state);
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    };

    std::unique_lock<std::mutex> lock(state_->m_mutex);
    if (state_->ready) {
        state_->executor.post(handle_result, state_->allocator);
    } else {
        state_->m_continuations.push_back(handle_result);
    }

    return future;
}

namespace detail {

// 辅助结构体用于递归处理 tuple 中的 future
template <std::size_t I, std::size_t N>
struct when_all_helper {
    template <typename SharedState, typename Tuple>
    static void process(SharedState shared_state, Tuple& tuple_futures) {
        auto&                 fut           = std::get<I>(tuple_futures);
        constexpr std::size_t current_index = I;

        fut.then([shared_state](auto&& value) {
            std::lock_guard<std::mutex> lock(shared_state->mutex);
            std::get<current_index>(shared_state->results) = std::forward<decltype(value)>(value);
            shared_state->completed_count++;

            if (shared_state->completed_count == shared_state->total_count) {
                if (shared_state->first_exception) {
                    shared_state->promise->set_exception(shared_state->first_exception);
                } else {
                    shared_state->promise->set_value(std::move(shared_state->results));
                }
            }
        }).catch_error([shared_state](const mc::exception& e) {
            std::lock_guard<std::mutex> lock(shared_state->mutex);
            if (!shared_state->first_exception) {
                shared_state->first_exception = std::current_exception();

                // 如果是取消异常，立即取消其他子future
                if (e.code() == mc::canceled_exception_code) {
                    shared_state->execute_cancel_callbacks();
                }
            }
            shared_state->completed_count++;
            if (shared_state->completed_count == shared_state->total_count) {
                shared_state->promise->set_exception(shared_state->first_exception);
            }
        });

        // 递归处理下一个
        when_all_helper<I + 1, N>::process(shared_state, tuple_futures);
    }
};

// 递归终止条件
template <std::size_t N>
struct when_all_helper<N, N> {
    template <typename SharedState, typename Tuple>
    static void process(SharedState, Tuple&) {
        // 递归终止，什么都不做
    }
};

template <std::size_t I, std::size_t N>
struct when_any_helper {
    template <typename SharedState, typename Tuple, typename VariantType>
    static void process(SharedState shared_state, Tuple& tuple_futures) {
        auto&                 fut           = std::get<I>(tuple_futures);
        constexpr std::size_t current_index = I;

        fut.then([shared_state, current_index](auto&& value) {
            std::lock_guard<std::mutex> lock(shared_state->mutex);
            if (!shared_state->completed) {
                shared_state->completed   = true;
                VariantType variant_value = std::forward<decltype(value)>(value);
                shared_state->promise->set_value(std::make_pair(current_index, std::move(variant_value)));

                // 成功完成后，立即取消其他子future
                shared_state->execute_cancel_callbacks();
            }
        }).catch_error([shared_state](const mc::exception& e) {
            std::lock_guard<std::mutex> lock(shared_state->mutex);
            if (!shared_state->completed) {
                // 如果是取消异常，增加取消计数
                if (e.code() == mc::canceled_exception_code) {
                    shared_state->canceled_count++;

                    // 如果所有子future都被取消，传播取消异常
                    if (shared_state->canceled_count == shared_state->total_count) {
                        shared_state->completed = true;
                        shared_state->promise->set_exception(std::current_exception());
                    }
                } else {
                    // 非取消异常，立即传播
                    shared_state->completed = true;
                    shared_state->promise->set_exception(std::current_exception());
                }
            }
        });

        // 递归处理下一个
        when_any_helper<I + 1, N>::template process<SharedState, Tuple, VariantType>(shared_state, tuple_futures);
    }
};

// 递归终止条件
template <std::size_t N>
struct when_any_helper<N, N> {
    template <typename SharedState, typename Tuple, typename VariantType>
    static void process(SharedState, Tuple&) {
        // 递归终止，什么都不做
    }
};

} // namespace detail

// all 实现：等待所有 future 完成
template <typename... Futures>
auto all(Futures&&... futures)
    -> Future<std::tuple<typename std::decay_t<Futures>::value_type...>,
              typename std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>::executor_type,
              typename std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>::allocator_type> {
    static_assert(sizeof...(Futures) > 0, "all requires at least one future");

    using FirstFuture = std::tuple_element_t<0, std::tuple<Futures...>>;
    using Executor    = typename std::decay_t<FirstFuture>::executor_type;
    using Allocator   = typename std::decay_t<FirstFuture>::allocator_type;
    using ResultType  = std::tuple<typename std::decay_t<Futures>::value_type...>;

    auto  tuple_futures = std::forward_as_tuple(futures...);
    auto& first_future  = std::get<0>(tuple_futures);
    using SharedState   = detail::AllState<ResultType, Executor, Allocator>;
    auto shared_state   = std::make_shared<SharedState>(sizeof...(Futures), first_future);
    auto result_future  = shared_state->promise->get_future();

    shared_state->add_cancel_callback(tuple_futures);

    // 设置取消回调：当result_future被取消时，取消所有子future
    result_future.on_cancel([shared_state]() {
        shared_state->execute_cancel_callbacks();
    });

    // 使用递归模板处理所有 future
    detail::when_all_helper<0, sizeof...(Futures)>::process(shared_state, tuple_futures);

    return result_future;
}

// any 实现：等待任意一个 future 完成
template <typename... Futures>
auto any(Futures&&... futures)
    -> Future<std::pair<std::size_t, std::variant<typename std::decay_t<Futures>::value_type...>>,
              typename std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>::executor_type,
              typename std::decay_t<std::tuple_element_t<0, std::tuple<Futures...>>>::allocator_type> {
    static_assert(sizeof...(Futures) > 0, "any requires at least one future");

    using FirstFuture = std::tuple_element_t<0, std::tuple<Futures...>>;
    using Executor    = typename std::decay_t<FirstFuture>::executor_type;
    using Allocator   = typename std::decay_t<FirstFuture>::allocator_type;
    using VariantType = std::variant<typename std::decay_t<Futures>::value_type...>;
    using ResultType  = std::pair<std::size_t, VariantType>;
    using SharedState = detail::AnyState<ResultType, Executor, Allocator>;

    auto  tuple_futures = std::forward_as_tuple(futures...);
    auto& first_future  = std::get<0>(tuple_futures);
    auto  shared_state  = std::make_shared<SharedState>(sizeof...(Futures), first_future);
    auto  result_future = shared_state->promise->get_future();

    shared_state->add_cancel_callback(tuple_futures);

    // 设置取消回调：当result_future被取消时，取消所有子future
    result_future.on_cancel([shared_state]() {
        shared_state->execute_cancel_callbacks();
    });

    // 使用递归模板处理所有 future
    detail::when_any_helper<0, sizeof...(Futures)>::template process<decltype(shared_state), decltype(tuple_futures), VariantType>(shared_state, tuple_futures);

    return result_future;
}

template <typename T, typename Executor, typename Allocator>
void Future<T, Executor, Allocator>::cancel() {
    if (state_) {
        state_->cancel();
    }
}

template <typename T, typename Executor, typename Allocator>
bool Future<T, Executor, Allocator>::is_cancelled() const {
    return state_ && state_->cancelled.load();
}

template <typename T, typename Executor, typename Allocator>
template <typename F>
void Future<T, Executor, Allocator>::on_cancel(F&& callback) {
    if (state_) {
        state_->add_cancel_callback(std::forward<F>(callback));
    }
}

template <typename PromisePtr, typename State>
void set_promise_exception(PromisePtr& promise, State& state) {
    promise->set_exception(std::get<std::exception_ptr>(state->result));
}

} // namespace mc::futures

#endif // MC_FUTURES_DETAIL_FUTURE_IMPL_H