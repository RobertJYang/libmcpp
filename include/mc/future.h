//-- Copyright (c) 2025 Huawei Technologies Co., Ltd.
//-- openUBMC is licensed under Mulan PSL v2.
//-- You can use this software according to the terms and conditions of the Mulan PSL v2.
//-- You may obtain a copy of Mulan PSL v2 at:
//--         http://license.coscl.org.cn/MulanPSL2
//-- THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
//-- EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
//-- MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
//-- See the Mulan PSL v2 for more details.

#ifndef MC_FUTURE_H
#define MC_FUTURE_H

#include <list>
#include <thread>
#include <variant>

#include "mc/futures/exceptions.h"
#include "mc/futures/state.h"
#include "mc/futures/status.h"

namespace mc {
namespace future {

template <typename T, typename Executor, typename Allocator = std::allocator<void>>
class Promise;

template <typename T, typename Executor, typename Allocator = std::allocator<void>>
class Future {
public:
    using value_type = T;
    using executor_type = typename Executor::executor_type;
    using allocator_type = Allocator;

    explicit Future(std::shared_ptr<future::State<T, Executor, Allocator>> state) : state_(state) {
    }
    ~Future() = default;

    // 禁止拷贝
    Future(const Future&) = delete;
    Future& operator=(const Future&) = delete;

    // 允许移动
    Future(Future&&) noexcept = default;
    Future& operator=(Future&&) noexcept = default;

    // 链式操作
    template <typename F>
    auto then(F&& func, launch policy = launch::async)
        -> Future<typename std::invoke_result_t<F, T>, Executor, Allocator>;

    // 错误处理
    template <typename F>
    auto catch_error(F&& handler)
        -> Future<typename std::invoke_result_t<F, std::exception&>, Executor, Allocator>;

    // 获取结果（异步等待）
    template <typename CompletionToken>
    void async_get(CompletionToken&& token, launch policy = launch::async);

    // 检查是否就绪
    bool is_ready() const;

    // 等待策略
    template <typename Rep, typename Period>
    future_status wait_for(const std::chrono::duration<Rep, Period>& timeout_duration) const;

    template <typename Clock, typename Duration>
    future_status wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time) const;

    void wait() const;

    // 同步获取结果
    template <typename U = T>
    U get();

    // 带超时的获取结果
    template <typename Rep, typename Period>
    T get_for(const std::chrono::duration<Rep, Period>& timeout_duration);

private:
    template <typename U, typename E, typename A>
    friend class Promise;

    template <typename U, typename E, typename A>
    friend class Future;
    using State = future::State<T, Executor, Allocator>;
    std::shared_ptr<State> state_;
};

template <typename T, typename Executor, typename Allocator>
class Promise {
public:
    using allocator_type = Allocator;

    explicit Promise(Executor& executor, const Allocator& alloc = Allocator());
    template <typename E,
              std::enable_if_t<std::is_same_v<typename Executor::executor_type, E>, int> = 0>
    explicit Promise(E& executor, const Allocator& alloc);

    ~Promise() = default;

    // 禁止拷贝
    Promise(const Promise&) = delete;
    Promise& operator=(const Promise&) = delete;

    // 允许移动
    Promise(Promise&&) noexcept = default;
    Promise& operator=(Promise&&) noexcept = default;

    // 设置值
    template <typename U = T, typename... Args>
    void set_value(Args&&... args);

    // 设置异常
    void set_exception(std::exception_ptr e);

    // 获取关联的Future
    Future<T, Executor, Allocator> get_future();

private:
    std::shared_ptr<typename Future<T, Executor, Allocator>::State> state_;
    bool future_retrieved_ = false;
    Allocator allocator_;
};

template <typename T, typename Executor, typename Allocator = std::allocator<void>>
auto make_promise(Executor& executor, Allocator alloc = Allocator()) {
    return Promise<T, Executor, Allocator>(executor, alloc);
}

} // namespace future
} // namespace mc

#include "mc/futures/detail/future_impl.h"
#include "mc/futures/detail/promise_impl.h"

#endif // MC_FUTURE_H
