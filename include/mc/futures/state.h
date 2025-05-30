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
#include <list>
#include <mutex>
#include <variant>
#include <vector>

#include "exceptions.h"
#include "status.h"

namespace mc::futures {

template <typename T>
using result_variant_t =
    typename std::conditional_t<std::is_same_v<T, void>,
                                std::variant<std::monostate, std::exception_ptr>,
                                std::variant<T, std::exception_ptr>>;

template <typename F>
void safe_invoke(F&& callback) {
    try {
        callback();
    } catch (...) {
        // 忽略异常
    }
}

template <typename T, typename Executor, typename Allocator>
struct State {
    using executor_type = Executor;
    using callbacks     = std::vector<std::function<void()>>;

    std::mutex              mutex;
    std::condition_variable cv;
    std::atomic<bool>       ready{false};
    std::atomic<bool>       deferred{false};
    std::atomic<bool>       cancelled{false};
    launch                  policy{launch::async};

    result_variant_t<T> result;

    callbacks     continuations;
    executor_type executor;
    Allocator     allocator;

    // 取消回调列表
    callbacks  cancel_callbacks;
    std::mutex cancel_mutex;

    State(Executor executor, Allocator allocator)
        : executor(std::move(executor)), allocator(std::move(allocator)) {
    }

    void mark_ready() {
        ready = true;
        cv.notify_all();
        for (auto& cont : continuations) {
            executor.post(cont, allocator);
        }
        continuations.clear();
    }

    // 取消操作
    void cancel() {
        bool expected = false;
        if (!cancelled.compare_exchange_strong(expected, true)) {
            return;
        }

        callbacks temp_callbacks;
        {
            std::lock_guard<std::mutex> lock(cancel_mutex);
            temp_callbacks = std::move(cancel_callbacks);
            cancel_callbacks.clear();
        }

        for (auto& callback : temp_callbacks) {
            safe_invoke(std::forward<decltype(callback)>(callback));
        }

        // 设置取消异常
        std::lock_guard<std::mutex> lock(mutex);
        if (!ready) {
            result = std::make_exception_ptr(canceled_exception());
            ready  = true;
            cv.notify_all();
        }
    }

    // 添加取消回调
    template <typename F>
    void add_cancel_callback(F&& callback) {
        bool should_execute_immediately = false;
        {
            std::lock_guard<std::mutex> lock(cancel_mutex);
            if (cancelled.load()) {
                should_execute_immediately = true;
            } else {
                cancel_callbacks.emplace_back(std::forward<F>(callback));
            }
        }

        // 在锁外执行回调，避免死锁
        if (should_execute_immediately) {
            safe_invoke(std::forward<F>(callback));
        }
    }
};

} // namespace mc::futures

#endif // MC_FUTURES_STATE_H