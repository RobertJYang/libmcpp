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

#ifndef MC_FUTURES_STATE_H
#define MC_FUTURES_STATE_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <variant>

#include <mc/futures/callback_list.h>
#include <mc/futures/exceptions.h>
#include <mc/futures/status.h>

namespace mc::futures {

template <typename T>
using result_variant_t =
    typename std::conditional_t<std::is_same_v<T, void>,
                                std::variant<std::monostate, std::exception_ptr>,
                                std::variant<T, std::exception_ptr>>;

struct state_base {
    std::mutex              m_mutex;
    std::condition_variable m_cv;

    callback_list     m_continuations;
    std::atomic<bool> ready{false};
    std::atomic<bool> deferred{false};
    std::atomic<bool> cancelled{false};
    launch            policy{launch::async};
    callback_list     m_cancel_callbacks;

    void reset();
    void reuse();
};

template <typename T, typename Executor, typename Allocator>
struct state_value {
    using executor_type = Executor;

    state_value(Executor executor, Allocator allocator)
        : executor(std::move(executor)), allocator(std::move(allocator)) {
    }

    ~state_value() {
    }

    void reset() {
        this->~state_value<T, Executor, Allocator>();
    }

    void reuse(Executor executor, Allocator allocator) {
        new (this) state_value<T, Executor, Allocator>(std::move(executor), std::move(allocator));
        if constexpr (std::is_same_v<T, void>) {
            result = std::monostate{};
        } else {
            result = std::variant<T, std::exception_ptr>{};
        }
    }

    result_variant_t<T> result;
    executor_type       executor;
    Allocator           allocator;
};

template <typename T, typename Executor, typename Allocator>
struct State : public state_base, public state_value<T, Executor, Allocator> {
    using value_type = state_value<T, Executor, Allocator>;

    State(Executor executor, Allocator allocator)
        : state_base(), value_type(std::move(executor), std::move(allocator)) {
    }

    void mark_ready() {
        ready = true;
        m_cv.notify_all();

        // 移动到临时变量执行，避免回调过程中修改链表
        callback_list callbacks;
        m_continuations.swap(callbacks);
        callbacks.execute_and_clear();
    }

    // 取消操作
    void cancel() {
        // 先检查是否已经 ready，如果已经完成则忽略取消操作
        if (ready.load()) {
            return;
        }

        bool expected = false;
        if (!cancelled.compare_exchange_strong(expected, true)) {
            return;
        }

        // 移动到临时变量执行，避免回调过程中修改链表或死锁
        callback_list callbacks;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_cancel_callbacks.swap(callbacks);
        }
        callbacks.execute_and_clear();

        // 设置取消异常（再次检查 ready 状态以防竞态条件）
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!ready.load()) {
            this->result = std::make_exception_ptr(canceled_exception());
            this->mark_ready();
        }
    }

    // 添加取消回调
    template <typename F>
    void add_cancel_callback(F&& callback) {
        if (!cancelled.load()) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!cancelled.load()) {
                add_cancel_callback_inner(std::forward<F>(callback));
                return;
            }
        }

        // 已经撤销了立即执行回调，解锁后执行避免死锁
        safe_invoke(std::forward<F>(callback));
    }

    template <typename F>
    void add_cancel_callback_inner(F&& callback) {
        m_cancel_callbacks.push_back(std::forward<F>(callback));
    }

    void reset() {
        state_base::reset();
        value_type::reset();
    }

    void reuse(Executor executor, Allocator allocator) {
        state_base::reuse();
        value_type::reuse(std::move(executor), std::move(allocator));
    }
};

} // namespace mc::futures

#endif // MC_FUTURES_STATE_H