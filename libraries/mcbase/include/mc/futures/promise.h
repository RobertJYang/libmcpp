//-- Copyright (c) 2025 Huawei Technologies Co., Ltd.
//-- openUBMC is licensed under Mulan PSL v2.
//-- You can use this software according to the terms and conditions of the Mulan PSL v2.
//-- You may obtain a copy of Mulan PSL v2 at:
//--         http://license.coscl.org.cn/MulanPSL2
//-- THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
//-- EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
//-- MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
//-- See the Mulan PSL v2 for more details.

#ifndef MC_FUTURES_PROMISE_H
#define MC_FUTURES_PROMISE_H

#include <mc/futures/any_future.h>
#include <mc/futures/any_promise.h>

namespace mc ::futures {

template <typename T>
class Future;

namespace detail {
template <typename T>
struct is_executor_impl
    : std::bool_constant<std::is_constructible_v<mc::runtime::any_executor, mc::traits::remove_cvref_t<T>>> {};

template <typename T>
constexpr bool is_executor_v = is_executor_impl<T>::value;

template <typename T, typename = void>
struct is_execution_context : std::false_type {};

template <typename T>
struct is_execution_context<T, std::void_t<typename T::executor_type, decltype(std::declval<T&>().get_executor())>>
    : std::bool_constant<!std::is_same_v<mc::traits::remove_cvref_t<T>, typename T::executor_type> &&
                         is_executor_v<decltype(std::declval<T&>().get_executor())>> {};

template <typename T>
constexpr bool is_execution_context_v = is_execution_context<T>::value;
} // namespace detail

template <typename T>
class Promise : public any_promise {
public:
    static_assert(!std::is_same_v<T, std::monostate>, "T must not be std::monostate");

    using future_type = Future<T>;
    using state_type  = typename future_type::state_type;
    using is_promise  = std::true_type;
    using value_type  = T;

    Promise() = default;
    template <typename Executor, std::enable_if_t<detail::is_executor_v<Executor>, int> = 0>
    explicit Promise(Executor&& executor);
    explicit Promise(any_promise&& future) : any_promise(std::forward<any_promise>(future))
    {}

    Promise(const Promise&)                = default;
    Promise& operator=(const Promise&)     = default;
    Promise(Promise&&) noexcept            = default;
    Promise& operator=(Promise&&) noexcept = default;

    // 设置值（非 void 类型：要求 1 个参数且类型可转换）
    template <typename... Args, std::enable_if_t<!std::is_void_v<T> && sizeof...(Args) == 1, int> = 0>
    void set_value(Args&&... args);

    // 设置值（void 类型：要求 0 个参数）
    template <typename... Args, std::enable_if_t<std::is_void_v<T> && sizeof...(Args) == 0, int> = 0>
    void set_value(Args&&...)
    {
        any_promise::set_value();
    }

    bool set_state_value(const state_base& state)
    {
        if (!state.is_ready()) {
            return false;
        }

        if (state.is_cancelled()) {
            cancel();
            return false;
        }

        if (state.is_rejected()) {
            state.copy_exception_to(*this);
            return false;
        }

        if constexpr (std::is_void_v<T>) {
            set_value();
        } else {
            set_value(mc::futures::State<T>::get_value(state));
        }
        return true;
    }

    template <typename Future, std::enable_if_t<detail::is_future_v<Future>, int> = 0>
    void set_future_value(Future&& future);

    // 获取关联的Future
    future_type get_future();
};

template <typename T, typename Executor>
auto make_promise(Executor executor)
{
    return Promise<detail::state_tt<T>>(std::move(executor));
}

namespace detail {

// make_deferred_future 的内部 holder
template <typename T, typename Task>
struct deferred_future_holder {
    Promise<T> promise;
    Task       task;

    template <typename U>
    deferred_future_holder(Promise<T> p, U&& t) : promise(std::move(p)), task(std::forward<U>(t))
    {}
};

template <typename T, typename Task>
void deferred_future_task_fn(void* ctx) noexcept
{
    auto* holder = static_cast<deferred_future_holder<T, Task>*>(ctx);
    try {
        if constexpr (std::is_void_v<T>) {
            holder->task();
            holder->promise.set_value();
        } else {
            holder->promise.set_value(holder->task());
        }
    } catch (...) {
        holder->promise.set_current_exception();
    }
}

template <typename T, typename Task>
void deferred_future_task_dtor(void* ctx) noexcept
{
    delete static_cast<deferred_future_holder<T, Task>*>(ctx);
}

} // namespace detail

template <typename T, typename Executor, typename Task>
auto make_deferred_future(Executor executor, Task&& task)
{
    using value_type = detail::state_tt<T>;
    using task_type  = std::decay_t<Task>;

    auto promise = make_promise<T>(std::move(executor));
    promise.set_policy(launch::deferred);

    auto future = promise.get_future();

    using holder_type = detail::deferred_future_holder<value_type, task_type>;
    auto* holder      = new holder_type(std::move(promise), std::forward<Task>(task));

    future.get_state()->install_deferred_task(&detail::deferred_future_task_fn<value_type, task_type>,
                                              static_cast<void*>(holder),
                                              &detail::deferred_future_task_dtor<value_type, task_type>);

    return future;
}

} // namespace mc::futures

#include <mc/futures/detail/promise_impl.h>

#endif // MC_FUTURES_PROMISE_H
