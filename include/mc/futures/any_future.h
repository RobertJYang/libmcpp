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

#ifndef MC_FUTURES_ANY_FUTURE_H
#define MC_FUTURES_ANY_FUTURE_H

#include <mc/exception.h>
#include <mc/futures/exceptions.h>
#include <mc/futures/state_pool.h>
#include <mc/futures/status.h>
#include <mc/runtime.h>
#include <mc/traits.h>

namespace mc::futures {

class any_promise;

namespace detail {
template <typename T, typename = void>
struct is_future : std::false_type {};

template <typename T>
struct is_future<T,
                 std::void_t<typename mc::traits::remove_cvref_t<T>::is_future>>
    : std::true_type {};

template <typename T>
constexpr bool is_future_v = is_future<std::remove_cv_t<T>>::value;

} // namespace detail

class MC_API any_future {
    friend class any_promise;

public:
    using is_future     = std::true_type;
    using duration_type = std::chrono::steady_clock::duration;
    using executor_type = mc::runtime::any_executor;

    any_future() = default;
    explicit any_future(state_base_ptr state);

    any_future(const any_future&)                = default;
    any_future& operator=(const any_future&)     = default;
    any_future(any_future&&) noexcept            = default;
    any_future& operator=(any_future&&) noexcept = default;

    bool valid() const noexcept;

    bool is_ready() const;
    bool is_cancelled() const;
    bool is_rejected() const;

    void wait() const;

    template <typename Duration>
    future_status wait_for(Duration duration) const
    {
        return wait_for_impl(duration);
    }

    template <typename TimePoint>
    future_status wait_until(TimePoint timeout_time) const
    {
        return wait_until_impl(timeout_time);
    }

    void cancel();

    const state_base_ptr& get_state() const noexcept;
    std::exception_ptr    get_exception() const noexcept;

    template <typename T>
    auto get() const
    {
        if (!m_state) {
            MC_THROW(invalid_future_exception, "Future 无效");
        }
        wait();
        return static_cast<const State<T>*>(m_state.get())->get_value();
    }

    any_executor get_executor() const
    {
        return m_state ? m_state->get_executor() : any_executor();
    }

    void timeout(any_future& src_future, duration_type duration, callback_type callback);

    static any_future delay(duration_type duration, executor_type executor);

    void add_continuation(callback_type continuation, launch policy = launch::async, std::optional<mc::any_executor> executor = std::nullopt);

protected:
    void on_cancel(callback_type callback);
    void on_cancel(any_promise& other_promise);
    void on_cancel(any_future& other_future);
    void finally(any_promise& promise, callback_type cleanup, launch policy = launch::async, std::optional<mc::any_executor> executor = std::nullopt);
    void tap_error(std::function<void(const mc::exception&)> inspector, launch policy);

    state_base_ptr m_state;

    future_status wait_for_impl(duration_type duration) const;
    future_status wait_until_impl(std::chrono::steady_clock::time_point timeout_time) const;
};

} // namespace mc::futures

#endif // MC_FUTURES_ANY_FUTURE_H
