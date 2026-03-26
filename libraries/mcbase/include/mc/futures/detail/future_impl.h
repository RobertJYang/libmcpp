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

#include <cstddef>
#include <memory>
#include <new>

namespace mc::futures {

inline std::exception_ptr make_invalid_future_exception()
{
    return std::make_exception_ptr(MC_MAKE_EXCEPTION(invalid_future_exception, "Future 无效"));
}

template <typename T>
inline auto make_invalid_future(any_executor ex = any_executor())
{
    using value_type = std::conditional_t<std::is_same_v<T, std::monostate>, void, T>;
    return Future<value_type>(detail::make_rejected_state<value_type>(make_invalid_future_exception(), std::move(ex)));
}

// 调用 handler 并根据返回值类型设置 promise 结果
template <typename T, typename Promise, typename Handler, typename... Args>
MC_ALWAYS_INLINE void set_promise_value_unchecked(Promise& promise, Handler&& handler, Args&&... args)
{
    if constexpr (std::is_same_v<detail::result_t<T, Handler, Args...>, void> ||
                  std::is_void_v<typename Promise::value_type>) {
        detail::invoke_func<T>(std::forward<Handler>(handler), std::forward<Args>(args)...);
        promise.set_value();
    } else {
        promise.set_value(detail::invoke_func<T>(std::forward<Handler>(handler), std::forward<Args>(args)...));
    }
}

template <typename Promise, typename State, typename Invoke>
inline void invoke_with_exception_boundary(Promise& promise, State& state, void* body, Invoke invoke)
{
    try {
        invoke(body, promise, state);
    } catch (...) {
        promise.set_current_exception();
    }
}

template <typename T, typename Promise, typename State, typename Body>
inline void handle_error_state_common(Promise& promise, State& state, Body& body)
{
    if (state.is_cancelled()) {
        promise.cancel();
        return;
    }

    if (state.is_rejected()) {
        auto exception = state.get_exception_object();
        MC_ASSERT_THROW(exception != nullptr, mc::runtime_exception, "Future 异常状态缺少异常对象");
        const auto& mc_ex = *exception;
        if (mc_ex.code() == mc::canceled_exception_code) {
            promise.cancel();
        } else {
            try {
                body.apply_exception(promise, mc_ex);
            } catch (...) {
                promise.set_current_exception();
            }
        }
        return;
    }

    if constexpr (std::is_void_v<T>) {
        promise.set_value();
    } else {
        promise.set_value(state.get_value());
    }
}

// then/catch_error 的 continuation 上下文不再模板化 F：业务体单独放在擦除后的 body 中，由 holder 独占持有
namespace continuation_erasure {

inline void destroy_trivial_body(void* ptr) noexcept
{
    ::operator delete(ptr);
}

template <typename Body>
void destroy_body(void* ptr) noexcept
{
    auto* body = static_cast<Body*>(ptr);
    body->~Body();
    if constexpr (alignof(Body) > alignof(std::max_align_t)) {
        ::operator delete(ptr, std::align_val_t{alignof(Body)});
    } else {
        ::operator delete(ptr);
    }
}

template <typename Body, typename... Args>
Body* create_body(Args&&... args)
{
    void* storage = nullptr;
    if constexpr (alignof(Body) > alignof(std::max_align_t)) {
        storage = ::operator new(sizeof(Body), std::align_val_t{alignof(Body)});
    } else {
        storage = ::operator new(sizeof(Body));
    }

    try {
        return new (storage) Body(std::forward<Args>(args)...);
    } catch (...) {
        if constexpr (alignof(Body) > alignof(std::max_align_t)) {
            ::operator delete(storage, std::align_val_t{alignof(Body)});
        } else {
            ::operator delete(storage);
        }
        throw;
    }
}

template <typename Body>
constexpr auto select_body_destroy() noexcept
{
    if constexpr (std::is_trivially_destructible_v<Body> && alignof(Body) <= alignof(std::max_align_t)) {
        return &destroy_trivial_body;
    } else {
        return &destroy_body<Body>;
    }
}

template <typename Promise, typename StateType>
struct result_body_handle {
    result_body_handle() = default;

    result_body_handle(void* ptr, void (*invoke)(void*, Promise&, StateType&), void (*destroy)(void*) noexcept)
        : m_ptr(ptr), m_invoke(invoke), m_destroy(destroy)
    {}

    result_body_handle(const result_body_handle&)            = delete;
    result_body_handle& operator=(const result_body_handle&) = delete;

    result_body_handle(result_body_handle&& other) noexcept
    {
        move_from(std::move(other));
    }

    result_body_handle& operator=(result_body_handle&& other) noexcept
    {
        if (this != &other) {
            reset();
            move_from(std::move(other));
        }
        return *this;
    }

    ~result_body_handle()
    {
        reset();
    }

    void apply(Promise& promise, StateType& state)
    {
        m_invoke(m_ptr, promise, state);
    }

private:
    void reset() noexcept
    {
        if (m_destroy != nullptr) {
            m_destroy(m_ptr);
        }
        m_ptr     = nullptr;
        m_invoke  = nullptr;
        m_destroy = nullptr;
    }

    void move_from(result_body_handle&& other) noexcept
    {
        m_ptr           = other.m_ptr;
        m_invoke        = other.m_invoke;
        m_destroy       = other.m_destroy;
        other.m_ptr     = nullptr;
        other.m_invoke  = nullptr;
        other.m_destroy = nullptr;
    }

    void* m_ptr{nullptr};
    void (*m_invoke)(void*, Promise&, StateType&){nullptr};
    void (*m_destroy)(void*) noexcept {nullptr};
};

template <typename Promise>
struct error_body_handle {
    error_body_handle() = default;

    error_body_handle(void* ptr, void (*invoke)(void*, Promise&, const mc::exception&), void (*destroy)(void*) noexcept)
        : m_ptr(ptr), m_invoke(invoke), m_destroy(destroy)
    {}

    error_body_handle(const error_body_handle&)            = delete;
    error_body_handle& operator=(const error_body_handle&) = delete;

    error_body_handle(error_body_handle&& other) noexcept
    {
        move_from(std::move(other));
    }

    error_body_handle& operator=(error_body_handle&& other) noexcept
    {
        if (this != &other) {
            reset();
            move_from(std::move(other));
        }
        return *this;
    }

    ~error_body_handle()
    {
        reset();
    }

    void apply_exception(Promise& promise, const mc::exception& ex)
    {
        m_invoke(m_ptr, promise, ex);
    }

private:
    void reset() noexcept
    {
        if (m_destroy != nullptr) {
            m_destroy(m_ptr);
        }
        m_ptr     = nullptr;
        m_invoke  = nullptr;
        m_destroy = nullptr;
    }

    void move_from(error_body_handle&& other) noexcept
    {
        m_ptr           = other.m_ptr;
        m_invoke        = other.m_invoke;
        m_destroy       = other.m_destroy;
        other.m_ptr     = nullptr;
        other.m_invoke  = nullptr;
        other.m_destroy = nullptr;
    }

    void* m_ptr{nullptr};
    void (*m_invoke)(void*, Promise&, const mc::exception&){nullptr};
    void (*m_destroy)(void*) noexcept {nullptr};
};

template <typename T, typename Promise, typename F>
struct result_continuation_body {
    F func;

    template <typename Fin>
    explicit result_continuation_body(Fin&& f) : func(std::forward<Fin>(f))
    {}

    static void invoke(void* ptr, Promise& promise, State<T>& state)
    {
        auto* body = static_cast<result_continuation_body*>(ptr);
        if constexpr (std::is_void_v<T>) {
            set_promise_value_unchecked<T>(promise, body->func);
        } else {
            set_promise_value_unchecked<T>(promise, body->func, state.get_value());
        }
    }

    static void destroy(void* ptr) noexcept
    {
        delete static_cast<result_continuation_body*>(ptr);
    }
};

template <typename T, typename Promise, typename F>
struct error_continuation_body {
    F func;

    template <typename Fin>
    explicit error_continuation_body(Fin&& f) : func(std::forward<Fin>(f))
    {}

    static void invoke_exception(void* ptr, Promise& promise, const mc::exception& ex)
    {
        auto* body = static_cast<error_continuation_body*>(ptr);
        set_promise_value_unchecked<T>(promise, body->func, ex);
    }

    static void destroy(void* ptr) noexcept
    {
        delete static_cast<error_continuation_body*>(ptr);
    }
};

template <typename T, typename Promise, typename F>
auto make_result_body(F&& func)
{
    using body_type = result_continuation_body<T, Promise, std::decay_t<F>>;
    return result_body_handle<Promise, State<T>>(create_body<body_type>(std::forward<F>(func)), &body_type::invoke,
                                                 select_body_destroy<body_type>());
}

template <typename T, typename Promise, typename F>
auto make_error_body(F&& func)
{
    using body_type = error_continuation_body<T, Promise, std::decay_t<F>>;
    return error_body_handle<Promise>(create_body<body_type>(std::forward<F>(func)), &body_type::invoke_exception,
                                      select_body_destroy<body_type>());
}

} // namespace continuation_erasure

template <typename T, typename Promise>
struct continuation_callback_context {
    continuation_callback_context() = default;

    continuation_callback_context(Promise promise_in, state_base_ptr state_in,
                                  continuation_erasure::result_body_handle<Promise, State<T>> body_in)
        : promise(std::move(promise_in)), state(std::move(state_in)), body(std::move(body_in))
    {}

    continuation_callback_context(const continuation_callback_context&)                = delete;
    continuation_callback_context& operator=(const continuation_callback_context&)     = delete;
    continuation_callback_context(continuation_callback_context&&) noexcept            = default;
    continuation_callback_context& operator=(continuation_callback_context&&) noexcept = default;

    void apply()
    {
        body.apply(promise, *static_cast<State<T>*>(state.get()));
    }

    Promise                                                     promise;
    state_base_ptr                                              state;
    continuation_erasure::result_body_handle<Promise, State<T>> body;
};

template <typename T, typename Promise, typename F>
callback_type make_continuation(Promise promise, F&& func, state_base_ptr state)
{
    return callback_type::make_future_result_ready(continuation_callback_context<T, Promise>{
        std::move(promise), std::move(state),
        continuation_erasure::make_result_body<T, Promise>(std::forward<F>(func))});
}

template <typename T, typename Promise>
struct error_continuation_callback_context {
    error_continuation_callback_context() = default;

    error_continuation_callback_context(Promise promise_in, state_base_ptr state_in,
                                        continuation_erasure::error_body_handle<Promise> body_in)
        : promise(std::move(promise_in)), state(std::move(state_in)), body(std::move(body_in))
    {}

    error_continuation_callback_context(const error_continuation_callback_context&)                = delete;
    error_continuation_callback_context& operator=(const error_continuation_callback_context&)     = delete;
    error_continuation_callback_context(error_continuation_callback_context&&) noexcept            = default;
    error_continuation_callback_context& operator=(error_continuation_callback_context&&) noexcept = default;

    void apply()
    {
        handle_error_state_common<T>(promise, *static_cast<State<T>*>(state.get()), body);
    }

    Promise                                          promise;
    state_base_ptr                                   state;
    continuation_erasure::error_body_handle<Promise> body;
};

template <typename T, typename Promise, typename F>
callback_type make_error_continuation(Promise promise, F&& func, state_base_ptr state)
{
    return callback_type::make_future_error_ready(error_continuation_callback_context<T, Promise>{
        std::move(promise), std::move(state),
        continuation_erasure::make_error_body<T, Promise>(std::forward<F>(func))});
}

template <typename T, typename OtherT, typename Promise>
struct as_future_callback_context {
    using other_type = detail::state_tt<OtherT>;

    void apply()
    {
        auto* typed_state = static_cast<State<T>*>(state.get());
        if constexpr (std::is_same_v<other_type, void>) {
            promise.set_value();
        } else if constexpr (std::is_void_v<T>) {
            promise.set_value(other_type{});
        } else {
            promise.set_value(other_type(typed_state->get_value()));
        }
    }

    Promise        promise;
    state_base_ptr state;
};

template <typename T, typename OtherT, typename Promise>
callback_type make_as_future_continuation(Promise promise, state_base_ptr state)
{
    return callback_type::make_future_result_ready(
        as_future_callback_context<T, OtherT, Promise>{std::move(promise), std::move(state)});
}

template <typename T>
template <typename U>
U Future<T>::get() const
{
    return any_future::get<T>();
}

template <typename T>
template <typename Duration>
T Future<T>::get_for(Duration duration) const
{
    auto status = any_future::wait_for(duration);
    if (status == future_status::timeout) {
        MC_THROW(mc::timeout_exception, "Future 超时");
    }
    return get();
}

template <typename T>
template <typename CompletionToken>
void Future<T>::async_get(CompletionToken&& token, launch policy)
{
    async_get(std::forward<CompletionToken>(token), policy, any_future::get_executor());
}

template <typename T>
template <typename CompletionToken>
void Future<T>::async_get(CompletionToken&& token, launch policy, mc::any_executor executor)
{
    if (!m_state) {
        return;
    }

    auto continuation = [token = std::forward<CompletionToken>(token), state = get_state()]() mutable {
        if (state->is_ready()) {
            if constexpr (std::is_void_v<T>) {
                token();
            } else {
                token(static_cast<State<T>*>(state.get())->get_value());
            }
        }
    };
    add_continuation(std::move(continuation), policy, std::move(executor));
}

template <typename T>
template <typename F>
auto Future<T>::then(F&& func, launch policy) -> std::enable_if_t<detail::is_future_v<detail::result_t<T, F>>,
                                                                  Future<typename detail::result_t<T, F>::value_type>>
{
    return then(std::forward<F>(func), policy, any_future::get_executor());
}

template <typename T>
template <typename F>
auto Future<T>::then(F&& func, launch policy, mc::any_executor executor)
    -> std::enable_if_t<detail::is_future_v<detail::result_t<T, F>>,
                        Future<typename detail::result_t<T, F>::value_type>>
{
    using ResultType = typename detail::result_t<T, F>::value_type;

    any_executor ex(executor);
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
auto Future<T>::then(F&& func, launch policy)
    -> std::enable_if_t<!detail::is_future_v<detail::result_t<T, F>>, Future<detail::result_t<T, F>>>
{
    return then(std::forward<F>(func), policy, any_future::get_executor());
}

template <typename T>
template <typename F>
auto Future<T>::then(F&& func, launch policy, mc::any_executor executor)
    -> std::enable_if_t<!detail::is_future_v<detail::result_t<T, F>>, Future<detail::result_t<T, F>>>
{
    using ResultType = detail::result_t<T, F>;

    any_executor ex(executor);
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
auto Future<T>::as_future() -> Future<detail::state_tt<OtherT>>
{
    return as_future<OtherT>(any_future::get_executor());
}

template <typename T>
template <typename OtherT>
auto Future<T>::as_future(mc::any_executor executor) -> Future<detail::state_tt<OtherT>>
{
    using other_type = detail::state_tt<OtherT>;
    any_executor ex(executor);

    if constexpr (std::is_same_v<other_type, T>) {
        return std::move(*this);
    } else if constexpr (std::is_same_v<other_type, void>) {
        if (!m_state) {
            return Future<other_type>(make_invalid_future<other_type>(std::move(ex)));
        }
    } else if constexpr (std::is_same_v<T, void>) {
        if (!m_state) {
            return Future<other_type>(make_invalid_future<other_type>(std::move(ex)));
        }
    } else {
        if (!m_state) {
            return Future<other_type>(make_invalid_future<other_type>(std::move(ex)));
        }
    }

    auto promise = make_promise<other_type>(ex);
    auto future  = promise.get_future().on_cancel(*this);
    promise.set_policy(launch::async);
    add_continuation(make_as_future_continuation<T, OtherT>(std::move(promise), get_state()), launch::async, ex);
    return future;
}

template <typename T>
template <typename F>
auto Future<T>::catch_error(F&& func, launch policy)
    -> std::enable_if_t<detail::is_future_v<std::invoke_result_t<F, const mc::exception&>> &&
                            std::is_same_v<typename std::invoke_result_t<F, const mc::exception&>::value_type, T>,
                        Future<T>>
{
    return catch_error(std::forward<F>(func), policy, any_future::get_executor());
}

template <typename T>
template <typename F>
auto Future<T>::catch_error(F&& func, launch policy, mc::any_executor executor)
    -> std::enable_if_t<detail::is_future_v<std::invoke_result_t<F, const mc::exception&>> &&
                            std::is_same_v<typename std::invoke_result_t<F, const mc::exception&>::value_type, T>,
                        Future<T>>
{
    any_executor ex(executor);
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
auto Future<T>::catch_error(F&& func, launch policy)
    -> std::enable_if_t<!detail::is_future_v<std::invoke_result_t<F, const mc::exception&>> &&
                            std::is_same_v<std::invoke_result_t<F, const mc::exception&>, T>,
                        Future<T>>
{
    return catch_error(std::forward<F>(func), policy, any_future::get_executor());
}

template <typename T>
template <typename F>
auto Future<T>::catch_error(F&& func, launch policy, mc::any_executor executor)
    -> std::enable_if_t<!detail::is_future_v<std::invoke_result_t<F, const mc::exception&>> &&
                            std::is_same_v<std::invoke_result_t<F, const mc::exception&>, T>,
                        Future<T>>
{
    any_executor ex(executor);
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
auto Future<T>::finally(F&& cleanup, launch policy) -> future_type
{
    return finally(std::forward<F>(cleanup), policy, any_future::get_executor());
}

template <typename T>
template <typename F>
auto Future<T>::finally(F&& cleanup, launch policy, mc::any_executor executor) -> future_type
{
    any_executor ex(executor);
    if (!m_state) {
        auto future = Future<T>(make_invalid_future<T>(std::move(ex)));
        return future.finally(std::forward<F>(cleanup), policy, executor);
    }

    auto promise = make_promise<T>(ex);
    auto future  = promise.get_future().on_cancel(*this);
    any_future::finally(promise, [promise, cleanup = std::forward<F>(cleanup), state = get_state()]() mutable {
        cleanup();
        promise.set_state_value(*static_cast<const State<T>*>(state.get()));
    }, policy, ex);
    return future;
}

template <typename T>
template <typename F>
auto Future<T>::tap(F&& inspector, launch policy) -> future_type
{
    if (!m_state) {
        return Future<T>(detail::make_rejected_state<T>(make_invalid_future_exception(),
                                                        mc::any_executor(mc::runtime::immediate_executor())));
    }

    any_future::add_continuation([inspector = std::forward<F>(inspector), state = get_state()]() mutable {
        if (state->is_ready()) {
            if constexpr (std::is_void_v<T>) {
                inspector();
            } else {
                inspector(static_cast<State<T>*>(state.get())->get_value());
            }
        }
    }, policy, mc::any_executor(mc::runtime::immediate_executor())); // tap 回调立即执行
    return *this;
}

template <typename T>
template <typename F>
auto Future<T>::tap_error(F&& inspector, launch policy)
    -> std::enable_if_t<std::is_invocable_v<F, const mc::exception&>, future_type>
{
    if (!m_state) {
        auto future = Future<T>(detail::make_rejected_state<T>(make_invalid_future_exception(),
                                                               mc::any_executor(mc::runtime::immediate_executor())));
        return future.tap_error(std::forward<F>(inspector), policy);
    }

    any_future::tap_error(std::forward<F>(inspector), policy); // tap_error 回调立即执行
    return *this;
}

template <typename T>
template <typename F>
auto Future<T>::on_cancel(F&& callback) & -> std::enable_if_t<detail::is_cancel_callback_v<F>, future_type&>
{
    any_future::on_cancel(std::forward<F>(callback));
    return *this;
}

template <typename T>
template <typename F>
auto Future<T>::on_cancel(F&& callback) && -> std::enable_if_t<detail::is_cancel_callback_v<F>, future_type>
{
    any_future::on_cancel(std::forward<F>(callback));
    return std::move(*this);
}

} // namespace mc::futures

#endif // MC_FUTURES_DETAIL_FUTURE_IMPL_H
