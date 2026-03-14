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

#ifndef MC_FUTURES_DETAIL_PROMISE_IMPL_H
#define MC_FUTURES_DETAIL_PROMISE_IMPL_H

#include <mc/futures/state_pool.h>

namespace mc::futures {

template <typename T>
template <typename Executor, std::enable_if_t<detail::is_executor_v<Executor>, int>>
Promise<T>::Promise(Executor&& executor) : any_promise(make_pooled_state<T>(std::forward<Executor>(executor)))
{}

template <typename T>
template <typename Future, std::enable_if_t<detail::is_future_v<Future>, int>>
void Promise<T>::set_future_value(Future&& future)
{
    auto promise = *this;
    future
        .then([promise](auto&& value) mutable {
        using value_type = typename Promise::value_type;
        if constexpr (std::is_same_v<value_type, void>) {
            promise.any_promise::set_value(false);
        } else {
            promise.any_promise::template set_value<value_type>(std::forward<decltype(value)>(value), false);
        }
    })
        .catch_error([promise](const mc::exception& ec) mutable {
        promise.any_promise::set_exception(std::current_exception(), false);
    }).on_cancel(promise);
}

template <typename T>
template <typename... Args, std::enable_if_t<!std::is_void_v<T> && sizeof...(Args) == 1, int>>
void Promise<T>::set_value(Args&&... args)
{
    using value_type = std::tuple_element_t<0, std::tuple<Args...>>;
    if constexpr (detail::is_future_v<value_type>) {
        if (any_promise::set_bound()) {
            set_future_value(std::forward<Args>(args)...);
        }
    } else {
        any_promise::set_value<T, Args...>(std::forward<Args>(args)...);
    }
}

template <typename T>
Future<T> Promise<T>::get_future()
{
    return Future<T>(any_promise::get_future());
}

} // namespace mc::futures

#endif // MC_FUTURES_DETAIL_PROMISE_IMPL_H
