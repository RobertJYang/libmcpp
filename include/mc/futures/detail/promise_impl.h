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

namespace mc::futures {

template <typename T, typename Executor, typename Allocator>
Promise<T, Executor, Allocator>::Promise(Executor executor, const Allocator& alloc)
    : state_(std::make_shared<state_type>(std::move(executor), alloc)), allocator_(alloc) {
}

template <typename T, typename Executor, typename Allocator>
template <typename U, typename... Args>
void Promise<T, Executor, Allocator>::set_value(Args&&... args) {
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (state_->ready) {
        MC_THROW(promise_already_satisfied, "Promise 值已被设置");
    }

    if constexpr (std::is_same_v<U, void>) {
        state_->result = std::monostate{};
    } else {
        state_->result = std::get<0>(std::forward_as_tuple(std::forward<Args>(args)...));
    }
    state_->mark_ready();
}

template <typename T, typename Executor, typename Allocator>
void Promise<T, Executor, Allocator>::set_exception(std::exception_ptr e) {
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (state_->ready) {
        MC_THROW(promise_already_satisfied, "Promise 值已被设置");
    }
    state_->result = e;
    state_->mark_ready();
}

template <typename T, typename Executor, typename Allocator>
void Promise<T, Executor, Allocator>::cancel() {
    set_exception(std::make_exception_ptr(MC_MAKE_EXCEPTION(mc::canceled_exception, "Promise被取消")));
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