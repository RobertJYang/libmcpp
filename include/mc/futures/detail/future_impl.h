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

// 调用 handler 并根据返回值类型设置 promise 结果
template <typename T, typename Promise, typename Handler, typename... Args>
void set_promise_value(Promise& promise, Handler&& handler, Args&&... args) {
    try {
        if constexpr (std::is_same_v<detail::result_t<T, Handler, Args...>, void>) {
            detail::invoke_func<T>(std::forward<Handler>(handler),
                                   std::forward<Args>(args)...);
            promise.set_value();
        } else if constexpr (detail::is_future_v<detail::result_t<T, Handler, Args...>>) {
            // 如果返回值是 Future 类型，我们需要等待它完成
            auto future = detail::invoke_func<T>(std::forward<Handler>(handler),
                                                 std::forward<Args>(args)...);
            future.then([promise](auto&& value) mutable {
                promise.template set_value<std::decay_t<decltype(value)>>(std::forward<decltype(value)>(value));
            }).catch_error([promise](const mc::exception& ec) mutable {
                promise.set_exception(std::current_exception());
            }).on_cancel(promise);
        } else {
            promise.set_value(detail::invoke_func<T>(std::forward<Handler>(handler),
                                                     std::forward<Args>(args)...));
        }
    } catch (...) {
        promise.set_exception(std::current_exception());
    }
}

template <bool IsVoid, typename T, typename Promise, typename F, typename State>
void handle_result_impl(Promise& promise, F&& func, State& state) {
    if constexpr (IsVoid) {
        if (std::holds_alternative<std::monostate>(state->result)) {
            set_promise_value<T>(promise, std::forward<F>(func));
        } else {
            set_promise_exception(promise, state);
        }
    } else {
        if (auto* value = std::get_if<T>(&state->result)) {
            set_promise_value<T>(promise, func, std::move(*value));
        } else {
            set_promise_exception(promise, state);
        }
    }
}

// 可以处理值传递或异常传递的情况
template <typename T, typename Promise, typename F, typename State>
void handle_state_result(Promise& promise, F&& func, State& state) {
    handle_result_impl<std::is_void_v<T>, T>(promise, std::forward<F>(func), state);
}

// 直接传递状态结果（不调用任何函数，仅传递值或异常）
template <typename T, typename Promise, typename State>
void forward_state_result(Promise& promise, State& state) {
    if constexpr (std::is_void_v<T>) {
        if (std::holds_alternative<std::monostate>(state->result)) {
            promise.set_value();
        } else {
            promise.set_exception(std::get<std::exception_ptr>(state->result));
        }
    } else {
        if (auto* value = std::get_if<T>(&state->result)) {
            promise.set_value(std::move(*value));
        } else {
            promise.set_exception(std::get<std::exception_ptr>(state->result));
        }
    }
}

// 在传递状态结果前先调用检查函数（用于 tap）
template <typename T, typename Promise, typename F, typename State>
void inspect_and_forward_state_result(Promise& promise, F&& inspector, State& state) {
    if constexpr (std::is_void_v<T>) {
        if (std::holds_alternative<std::monostate>(state->result)) {
            inspector(); // 查看成功状态
            promise.set_value();
        } else {
            // 对于异常情况，直接传递异常
            promise.set_exception(std::get<std::exception_ptr>(state->result));
        }
    } else {
        if (auto* value = std::get_if<T>(&state->result)) {
            inspector(*value);         // 查看值但不改变
            promise.set_value(*value); // 传递原值
        } else {
            promise.set_exception(std::get<std::exception_ptr>(state->result));
        }
    }
}

template <typename T, typename ResultType, typename Executor, typename Allocator, typename F>
auto make_continuation(Promise<ResultType, Executor, Allocator>&& promise,
                       F&& func, state_ptr<State<T, Executor, Allocator>> state) {
    return [promise = std::move(promise), func = std::forward<F>(func),
            state = std::move(state)]() mutable {
        try {
            handle_result_impl<std::is_void_v<T>, T>(promise, std::forward<F>(func), state);
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    };
}

template <typename T, typename Executor, typename Allocator>
template <typename U>
U Future<T, Executor, Allocator>::get() const {
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
template <typename Duration>
T Future<T, Executor, Allocator>::get_for(Duration duration) const {
    auto status = wait_for(duration);
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
bool Future<T, Executor, Allocator>::is_rejected() const {
    std::lock_guard<std::mutex> lock(state_->m_mutex);
    return state_->ready && std::holds_alternative<std::exception_ptr>(state_->result);
}

template <typename T, typename Executor, typename Allocator>
std::exception_ptr Future<T, Executor, Allocator>::get_exception() const {
    std::lock_guard<std::mutex> lock(state_->m_mutex);
    return std::get<std::exception_ptr>(state_->result);
}

template <typename T, typename Executor, typename Allocator>
template <typename ResultType>
ResultType* Future<T, Executor, Allocator>::get_if() const {
    std::lock_guard<std::mutex> lock(state_->m_mutex);
    return std::get_if<ResultType>(&state_->result);
}

template <typename T, typename Executor, typename Allocator>
void Future<T, Executor, Allocator>::wait() const {
    std::unique_lock<std::mutex> lock(state_->m_mutex);
    if (state_->deferred) {
        return;
    }
    state_->m_cv.wait(lock, [&] {
        return state_->ready.load(std::memory_order_acquire);
    });
}

template <typename T, typename Executor, typename Allocator>
template <typename Duration>
future_status Future<T, Executor, Allocator>::wait_for(Duration duration) const {
    std::unique_lock<std::mutex> lock(state_->m_mutex);
    if (state_->deferred) {
        return future_status::deferred;
    }

    using duration_type = std::chrono::steady_clock::duration;
    if (state_->m_cv.wait_for(lock, static_cast<duration_type>(duration), [&] {
        return state_->ready.load();
    })) {
        return future_status::ready;
    }
    return future_status::timeout;
}

template <typename T, typename Executor, typename Allocator>
template <typename TimePoint>
future_status Future<T, Executor, Allocator>::wait_until(TimePoint timeout_time) const {
    std::unique_lock<std::mutex> lock(state_->m_mutex);
    if (state_->deferred) {
        return future_status::deferred;
    }

    using time_point_type = std::chrono::steady_clock::time_point;
    if (state_->m_cv.wait_until(lock, time_point_type(timeout_time), [&] {
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
            boost::asio::dispatch(
                state_->executor,
                boost::asio::bind_allocator(state_->allocator, [state = state_, handler = std::move(handle_result)]() mutable {
                handler(state);
            }));
        } else {
            boost::asio::post(state_->executor, boost::asio::bind_allocator(state_->allocator, [state = state_, handler = std::move(handle_result)]() mutable {
                handler(state);
            }));
        }
    } else {
        state_->m_continuations.push_back(
            [e = state_->executor, a = state_->allocator, s = state_, h = std::move(handle_result)]() mutable {
            boost::asio::post(e, boost::asio::bind_allocator(a, [s, h = std::move(h)]() mutable {
                h(s);
            }));
        });
    }
}

template <typename T, typename Executor, typename Allocator>
template <typename F>
auto Future<T, Executor, Allocator>::then(F&& func, launch policy)
    -> std::enable_if_t<
        detail::is_future_v<detail::result_t<T, F>>,
        Future<typename detail::result_t<T, F>::value_type, Executor, Allocator>> {
    using ResultType = typename detail::result_t<T, F>::value_type;
    auto promise     = mc::make_promise<ResultType>(state_->executor, state_->allocator);
    auto future      = promise.get_future().on_cancel(*this);

    auto continuation = make_continuation<T, ResultType, Executor, Allocator, F>(
        std::move(promise), std::forward<F>(func), state_);

    std::lock_guard<std::mutex> lock(state_->m_mutex);
    if (state_->ready) {
        if (policy == launch::deferred) {
            future.state_->m_continuations.push_back(continuation);
            future.state_->deferred = true;
            future.state_->policy   = policy;
        } else if (policy == launch::dispatch) {
            boost::asio::dispatch(state_->executor, boost::asio::bind_allocator(state_->allocator, continuation));
        } else {
            boost::asio::post(state_->executor, boost::asio::bind_allocator(state_->allocator, continuation));
        }
    } else {
        state_->m_continuations.push_back([e = state_->executor, a = state_->allocator, c = std::move(continuation)]() mutable {
            boost::asio::post(e, boost::asio::bind_allocator(a, std::move(c)));
        });
    }

    return future;
}

template <typename T, typename Executor, typename Allocator>
template <typename F>
auto Future<T, Executor, Allocator>::then(F&& func, launch policy)
    -> std::enable_if_t<!detail::is_future_v<detail::result_t<T, F>>,
                        Future<detail::result_t<T, F>, Executor, Allocator>> {
    using ResultType = detail::result_t<T, F>;
    auto promise     = mc::make_promise<ResultType>(state_->executor, state_->allocator);
    auto future      = promise.get_future().on_cancel(*this);

    auto continuation = make_continuation<T, ResultType, Executor, Allocator, F>(
        std::move(promise), std::forward<F>(func), state_);

    std::unique_lock<std::mutex> lock(state_->m_mutex);
    if (state_->ready) {
        if (policy == launch::deferred) {
            future.state_->m_continuations.push_back(continuation);
            future.state_->deferred = true;
            future.state_->policy   = policy;
        } else if (policy == launch::dispatch) {
            boost::asio::dispatch(state_->executor, boost::asio::bind_allocator(state_->allocator, continuation));
        } else {
            boost::asio::post(state_->executor, boost::asio::bind_allocator(state_->allocator, continuation));
        }
    } else {
        state_->m_continuations.push_back([e = state_->executor, a = state_->allocator, c = std::move(continuation)]() mutable {
            boost::asio::post(e, boost::asio::bind_allocator(a, std::move(c)));
        });
    }

    return future;
}

template <typename T, typename Executor, typename Allocator>
template <typename OtherT>
auto Future<T, Executor, Allocator>::as_future() -> Future<OtherT, Executor, Allocator> {
    if constexpr (std::is_same_v<OtherT, T>) {
        return std::move(*this);
    } else if constexpr (std::is_void_v<T>) {
        return then([]() -> OtherT {
            if constexpr (std::is_same_v<OtherT, void>) {
                return;
            } else {
                return OtherT();
            }
        });
    } else {
        return then([](auto&& value) -> OtherT {
            if constexpr (std::is_same_v<OtherT, void>) {
                return;
            } else {
                return std::forward<decltype(value)>(value);
            }
        });
    }
}

template <typename T, typename Executor, typename Allocator>
template <typename OtherT, typename OtherExecutor>
auto Future<T, Executor, Allocator>::as_future(OtherExecutor&& executor) -> Future<OtherT, OtherExecutor, Allocator> {
    if constexpr (std::is_same_v<OtherExecutor, Executor>) {
        return this->as_future<OtherT>();
    } else if constexpr (std::is_void_v<T>) {
        auto promise = mc::make_promise<OtherT>(std::forward<OtherExecutor>(executor), state_->allocator);
        auto future  = promise.get_future().on_cancel(*this);
        this->then([promise]() mutable {
            if constexpr (std::is_same_v<OtherT, void>) {
                promise.set_value();
            } else {
                promise.set_value(OtherT());
            }
        }).catch_error([promise](const mc::exception&) mutable {
            promise.set_exception(std::current_exception());
        });
        return future;
    } else {
        auto promise = mc::make_promise<OtherT>(std::forward<OtherExecutor>(executor), state_->allocator);
        auto future  = promise.get_future().on_cancel(*this);
        this->then([promise](auto&& value) mutable {
            if constexpr (std::is_same_v<OtherT, void>) {
                promise.set_value();
            } else {
                promise.set_value(std::forward<decltype(value)>(value));
            }
        }).catch_error([promise](const mc::exception&) mutable {
            promise.set_exception(std::current_exception());
        });
        return future;
    }
}

template <typename T, typename Promise, typename Handler, typename State>
void handle_exception_case(Promise& promise, Handler&& handler, State& state) {
    try {
        std::exception_ptr eptr = std::get<std::exception_ptr>(state->result);
        std::rethrow_exception(eptr);
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

template <typename T, typename Promise, typename Handler, typename State>
void process_error_result(Promise& promise, Handler&& handler, State& state) {
    using HandlerResultType = std::invoke_result_t<Handler, const mc::exception&>;

    if constexpr (std::is_same_v<T, void>) {
        if (std::holds_alternative<std::monostate>(state->result)) {
            if constexpr (std::is_same_v<HandlerResultType, void>) {
                promise.set_value();
            } else {
                promise.set_exception(std::make_exception_ptr(std::runtime_error("Cannot change return type from void in catch_error")));
            }
        } else {
            handle_exception_case<T>(promise, std::forward<Handler>(handler), state);
        }
    } else {
        if (auto* value = std::get_if<T>(&state->result)) {
            if constexpr (std::is_same_v<HandlerResultType, T>) {
                promise.set_value(std::move(*value));
            } else {
                promise.set_exception(std::make_exception_ptr(std::runtime_error("catch_error handler return type must match previous chain type when no error occurs")));
            }
        } else {
            handle_exception_case<T>(promise, std::forward<Handler>(handler), state);
        }
    }
}

template <typename T, typename Executor, typename Allocator>
template <typename F>
auto Future<T, Executor, Allocator>::catch_error(F&& handler)
    -> Future<std::invoke_result_t<F, const mc::exception&>, Executor, Allocator> {
    using ResultType = std::invoke_result_t<F, const mc::exception&>;

    auto promise = mc::make_promise<ResultType>(state_->executor, state_->allocator);
    auto future  = promise.get_future().on_cancel(*this);

    auto handle_result = [promise, handler = std::forward<F>(handler), state = state_]() mutable {
        try {
            process_error_result<T>(promise, std::move(handler), state);
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    };

    std::unique_lock<std::mutex> lock(state_->m_mutex);
    if (state_->ready) {
        boost::asio::post(state_->executor, boost::asio::bind_allocator(state_->allocator, handle_result));
    } else {
        state_->m_continuations.push_back([e = state_->executor, a = state_->allocator, h = std::move(handle_result)]() mutable {
            boost::asio::post(e, boost::asio::bind_allocator(a, std::move(h)));
        });
    }

    return future;
}

template <typename T, typename Executor, typename Allocator>
template <typename F>
auto Future<T, Executor, Allocator>::finally(F&& cleanup) -> future_type {
    auto promise = mc::make_promise<T>(state_->executor, state_->allocator);
    auto future  = promise.get_future().on_cancel(*this);

    auto handle_result = [promise, cleanup = std::forward<F>(cleanup), state = state_]() mutable {
        try {
            // 无论成功失败都执行清理操作
            cleanup();

            // 然后传递原来的结果或异常
            forward_state_result<T>(promise, state);
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    };

    std::unique_lock<std::mutex> lock(state_->m_mutex);
    if (state_->ready) {
        boost::asio::post(state_->executor, boost::asio::bind_allocator(state_->allocator, handle_result));
    } else {
        state_->m_continuations.push_back([e = state_->executor, a = state_->allocator, h = std::move(handle_result)]() mutable {
            boost::asio::post(e, boost::asio::bind_allocator(a, std::move(h)));
        });
    }

    return future;
}

template <typename T, typename Executor, typename Allocator>
template <typename F>
auto Future<T, Executor, Allocator>::tap(F&& inspector) -> future_type {
    auto promise = mc::make_promise<T>(state_->executor, state_->allocator);
    auto future  = promise.get_future().on_cancel(*this);

    auto handle_result = [promise, inspector = std::forward<F>(inspector), state = state_]() mutable {
        try {
            inspect_and_forward_state_result<T>(promise, inspector, state);
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    };

    std::unique_lock<std::mutex> lock(state_->m_mutex);
    if (state_->ready) {
        boost::asio::post(state_->executor, boost::asio::bind_allocator(state_->allocator, handle_result));
    } else {
        state_->m_continuations.push_back([e = state_->executor, a = state_->allocator, h = std::move(handle_result)]() mutable {
            boost::asio::post(e, boost::asio::bind_allocator(a, std::move(h)));
        });
    }

    return future;
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
auto Future<T, Executor, Allocator>::on_cancel(F&& callback) & -> std::enable_if_t<
    detail::is_cancel_callback_v<F>, future_type&> {
    if (state_) {
        detail::on_cancel(state_, std::forward<F>(callback));
    }
    return *this;
}

template <typename T, typename Executor, typename Allocator>
template <typename F>
auto Future<T, Executor, Allocator>::on_cancel(F&& callback) && -> std::enable_if_t<
    detail::is_cancel_callback_v<F>, future_type> {
    if (state_) {
        detail::on_cancel(state_, std::forward<F>(callback));
    }
    return std::move(*this);
}

template <typename Promise, typename State>
void set_promise_exception(Promise& promise, State& state) {
    promise.set_exception(std::get<std::exception_ptr>(state->result));
}

} // namespace mc::futures

#endif // MC_FUTURES_DETAIL_FUTURE_IMPL_H
