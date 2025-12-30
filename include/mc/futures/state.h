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
#include <mc/memory.h>
#include <mc/runtime/condition_variable.h>

namespace mc::futures {

template <typename T>
using result_variant_t =
    typename std::conditional_t<std::is_same_v<T, void>,
                                std::variant<std::monostate, std::exception_ptr>,
                                std::variant<T, std::exception_ptr>>;

struct MC_API state_base {
    std::mutex                      m_mutex;
    mc::runtime::condition_variable m_cv;

    callback_list     m_continuations;
    std::atomic<bool> ready{false};
    std::atomic<bool> bound{false};
    std::atomic<bool> deferred{false};
    std::atomic<bool> cancelled{false};
    launch            policy{launch::async};
    callback_list     m_cancel_callbacks;

    MC_API void reset();
    MC_API void reuse();
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
struct State : public mc::enable_shared_from_this<State<T, Executor, Allocator>>,
               public state_base,
               public state_value<T, Executor, Allocator> {
    using value_type       = state_value<T, Executor, Allocator>;
    using shared_base_type = mc::enable_shared_from_this<State<T, Executor, Allocator>>;

    State(Executor executor, Allocator allocator)
        : shared_base_type(), state_base(),
          value_type(std::move(executor), std::move(allocator)) {
    }

    ~State() override {
    }

    void mark_ready() {
        callback_list callbacks;
        callback_list cancel_callbacks;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            ready.store(true, std::memory_order_release);
            m_continuations.swap(callbacks);
            m_cancel_callbacks.swap(cancel_callbacks);
        }
        m_cv.notify_all();
        callbacks.execute_and_clear();

        // 直接清空 cancel_callbacks 因为不再需要执行了
        cancel_callbacks.clear();
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
        bool need_ready = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!ready.load()) {
                this->result = std::make_exception_ptr(canceled_exception());
                need_ready   = true;
                ready.store(true, std::memory_order_release);
            }
        }
        if (need_ready) {
            this->mark_ready();
        }
    }

    // 添加取消回调
    template <typename F>
    void add_cancel_callback(F&& callback) {
        if (!ready.load() && !cancelled.load()) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!ready.load() && !cancelled.load()) {
                add_cancel_callback_inner(std::forward<F>(callback));
                return;
            }
        }

        if (cancelled.load()) {
            safe_invoke(std::forward<F>(callback));
        }
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
        new (static_cast<shared_base_type*>(this)) shared_base_type();
        state_base::reuse();
        value_type::reuse(std::move(executor), std::move(allocator));
    }
};

} // namespace mc::futures

#endif // MC_FUTURES_STATE_H
