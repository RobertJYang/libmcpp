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

namespace detail {

template <typename StatePtr, typename Result>
bool try_set_result(const StatePtr& state, Result&& result, bool strict_once = true) {
    {
        std::lock_guard<std::mutex> lock(state->m_mutex);
        if (state->cancelled.load()) {
            return false;
        }

        if (strict_once) {
            MC_ASSERT_THROW(!state->ready && !state->bound, promise_already_satisfied, "Promise 值已被设置");
        } else if (state->ready) {
            return false;
        }

        state->result = std::forward<Result>(result);
        state->ready.store(true, std::memory_order_release);
    }

    state->mark_ready();
    return true;
}

} // namespace detail

template <typename T, typename Executor, typename Allocator>
Promise<T, Executor, Allocator>::Promise(Executor executor, const Allocator& alloc)
    : state_(make_pooled_state<T>(std::move(executor), alloc)), allocator_(alloc) {
}

template <typename T, typename Executor, typename Allocator>
template <typename U, typename... Args>
void Promise<T, Executor, Allocator>::set_value(Args&&... args) {
    if (state_->cancelled.load()) {
        return;
    }

    if constexpr (detail::is_future_v<std::decay_t<U>>) {
        {
            std::lock_guard<std::mutex> lock(state_->m_mutex);
            if (state_->cancelled.load()) {
                return;
            }

            if (state_->ready || state_->bound) {
                MC_THROW(promise_already_satisfied, "Promise 值已被设置");
            }

            state_->bound.store(true, std::memory_order_release);
        }

        auto inner_future = std::get<0>(std::forward_as_tuple(std::forward<Args>(args)...));
        std::move(inner_future).then([state = state_](auto&& value) mutable {
            detail::try_set_result(state, std::forward<decltype(value)>(value), false);
        }).catch_error([state = state_](const mc::exception&) mutable {
            detail::try_set_result(state, std::current_exception(), false);
        }).on_cancel(*this);
        return;
    } else if constexpr (std::is_same_v<U, void>) {
        detail::try_set_result(state_, std::monostate{});
    } else if constexpr (sizeof...(Args) == 0) {
        detail::try_set_result(state_, U{});
    } else {
        detail::try_set_result(state_, std::get<0>(std::forward_as_tuple(std::forward<Args>(args)...)));
    }
}

template <typename T, typename Executor, typename Allocator>
void Promise<T, Executor, Allocator>::set_exception(std::exception_ptr e) {
    if (state_->cancelled.load()) {
        return;
    }
    detail::try_set_result(state_, std::move(e));
}

template <typename T, typename Executor, typename Allocator>
void Promise<T, Executor, Allocator>::set_exception(const mc::exception& e) {
    try {
        e.dynamic_rethrow_exception();
    } catch (...) {
        set_exception(std::current_exception());
    }
}

template <typename T, typename Executor, typename Allocator>
void Promise<T, Executor, Allocator>::cancel() {
    if (state_) {
        state_->cancel();
    }
}

template <typename T, typename Executor, typename Allocator>
template <typename F>
auto Promise<T, Executor, Allocator>::on_cancel(F&& callback)
    -> std::enable_if_t<detail::is_cancel_callback_v<F>, void> {
    if (state_) {
        detail::on_cancel(state_, std::forward<F>(callback));
    }
}

template <typename T, typename Executor, typename Allocator>
Future<T, Executor, Allocator> Promise<T, Executor, Allocator>::get_future() {
    if (future_retrieved_) {
        MC_THROW(future_already_retrieved, "Future 已被获取");
    }
    future_retrieved_ = true;
    return Future<T, Executor, Allocator>(state_);
}

template <typename T, typename Executor, typename Allocator>
Promise<T, Executor, Allocator>::~Promise() {
    state_.reset();
}

} // namespace mc::futures

#endif // MC_FUTURES_DETAIL_PROMISE_IMPL_H
