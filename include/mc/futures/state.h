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

#include "status.h"

namespace mc {
namespace future {

template <typename T>
using result_variant_t =
    typename std::conditional_t<std::is_same_v<T, void>,
                                std::variant<std::monostate, std::exception_ptr>,
                                std::variant<T, std::exception_ptr>>;

template <typename T, typename Executor, typename Allocator>
struct State {
    std::mutex              mutex;
    std::condition_variable cv;
    bool                    ready = false;
    result_variant_t<T>     result;

    std::list<std::function<void()>> continuations;
    typename Executor::executor_type executor;
    std::atomic<bool>                deferred{false};
    launch                           policy = launch::async;
    Allocator                        allocator;

    explicit State(typename Executor::executor_type e, const Allocator& alloc)
        : executor(e), allocator(alloc) {
    }

    void mark_ready() {
        ready = true;
        cv.notify_all();
        for (auto& cont : continuations) {
            executor.post(cont, allocator);
        }
        continuations.clear();
    }
};

} // namespace future
} // namespace mc

#endif // MC_FUTURES_STATE_H