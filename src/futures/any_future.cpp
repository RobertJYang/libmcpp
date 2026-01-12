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

#include <mc/exception.h>
#include <mc/future.h>
#include <mc/futures/any_future.h>
#include <mc/futures/any_promise.h>
#include <mc/futures/exceptions.h>

#include <mutex>

namespace mc::futures {

any_future::any_future(state_base_ptr state)
    : m_state(std::move(state)) {
}

bool any_future::valid() const noexcept {
    return m_state != nullptr;
}

bool any_future::is_ready() const {
    if (!m_state) {
        return false;
    }

    return m_state->is_ready();
}

bool any_future::is_cancelled() const {
    if (!m_state) {
        return false;
    }

    return m_state->is_cancelled();
}

void any_future::wait() const {
    if (!m_state) {
        return;
    }

    std::unique_lock<std::mutex> lock(m_state->m_mutex);
    m_state->m_cv.wait(lock, [&] {
        return m_state->is_ready();
    });
}

void any_future::cancel() {
    if (!m_state) {
        return;
    }

    m_state->cancel();
}

void any_future::on_cancel(callback_type callback) {
    if (m_state) {
        m_state->add_cancel_callback(std::move(callback));
    }
}

static void on_cancel_impl(state_base_ptr& state, state_base_ptr other_state) {
    if (!state || !other_state) {
        return;
    }

    state->add_cancel_callback([wstate = mc::weak_ptr(other_state)]() {
        if (auto state = wstate.lock()) {
            state->cancel();
        }
    });

    other_state->add_cancel_callback([wstate = mc::weak_ptr(state)]() {
        if (auto state = wstate.lock()) {
            state->cancel();
        }
    });
}

void any_future::on_cancel(any_promise& other_promise) {
    on_cancel_impl(m_state, other_promise.get_state());
}

void any_future::on_cancel(any_future& other_future) {
    on_cancel_impl(m_state, other_future.get_state());
}

const state_base_ptr& any_future::get_state() const noexcept {
    return m_state;
}

bool any_future::is_rejected() const {
    if (!m_state) {
        return false;
    }

    return m_state->is_rejected();
}

std::exception_ptr any_future::get_exception() const noexcept {
    if (!m_state) {
        return nullptr;
    }

    return m_state->get_exception();
}

void any_future::add_continuation(callback_type continuation, launch policy, std::optional<mc::any_executor> executor) {
    if (!m_state) {
        return;
    }

    std::unique_lock lock(m_state->m_mutex);

    auto ex = executor ? *executor : m_state->m_executor;
    if (m_state->is_ready()) {
        lock.unlock(); // 解锁，防止 dispatch 或 immediate_executor 立即执行回调造成死锁

        if (policy == launch::dispatch) {
            boost::asio::dispatch(ex, std::move(continuation));
        } else {
            boost::asio::post(ex, std::move(continuation));
        }
    } else {
        m_state->m_continuations.push_back([e = std::move(ex), policy, c = std::move(continuation)]() {
            if (policy == launch::dispatch) {
                boost::asio::dispatch(e, std::move(c));
            } else {
                boost::asio::post(e, std::move(c));
            }
        });
    }
}

void any_future::finally(any_promise& promise, callback_type cleanup, launch policy, std::optional<mc::any_executor> executor) {
    if (!m_state) {
        promise.set_exception(make_invalid_future_exception());
        return;
    }

    add_continuation([promise, cleanup = std::move(cleanup)]() mutable {
        try {
            cleanup();
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }, policy, executor);
}

void any_future::tap_error(std::function<void(const mc::exception&)> inspector, launch policy) {
    if (!m_state) {
        return;
    }

    any_future::add_continuation([handler = std::move(inspector), state = get_state()]() mutable {
        if (state->is_rejected()) {
            try {
                std::rethrow_exception(state->get_exception());
            } catch (const mc::exception& mc_ex) {
                handler(mc_ex);
            } catch (const std::exception& std_ex) {
                auto wrapped_ex = mc::std_exception_wrapper::from_current_exception(std_ex);
                handler(wrapped_ex);
            } catch (...) {
                auto unknown_ex = MC_MAKE_EXCEPTION(mc::exception, "Unknown Future Exception");
                handler(unknown_ex);
            }
        }
    }, policy, mc::any_executor(mc::runtime::immediate_executor()));
}

future_status any_future::wait_for_impl(std::chrono::steady_clock::duration duration) const {
    if (!m_state) {
        return future_status::invalid;
    }

    std::unique_lock<std::mutex> lock(m_state->m_mutex);
    if (m_state->m_policy == launch::deferred) {
        return future_status::deferred;
    }

    using duration_type = std::chrono::steady_clock::duration;
    if (m_state->m_cv.wait_for(lock, static_cast<duration_type>(duration), [&] {
        return m_state->is_ready();
    })) {
        return future_status::ready;
    }
    return future_status::timeout;
}

future_status any_future::wait_until_impl(std::chrono::steady_clock::time_point timeout_time) const {
    if (!m_state) {
        return future_status::invalid;
    }

    std::unique_lock<std::mutex> lock(m_state->m_mutex);
    if (m_state->m_policy == launch::deferred) {
        return future_status::deferred;
    }

    using time_point_type = std::chrono::steady_clock::time_point;
    if (m_state->m_cv.wait_until(lock, time_point_type(timeout_time), [&] {
        return m_state->is_ready();
    })) {
        return future_status::ready;
    }
    return future_status::timeout;
}

void any_future::timeout(any_future& src_future, duration_type duration, callback_type callback) {
    if (!m_state) {
        return;
    }

    struct timer_data {
        using timer_type = mc::runtime::basic_timer<executor_type>;

        timer_data(any_executor executor, duration_type duration)
            : timer(executor, duration),
              completed(false) {
        }

        timer_type        timer;
        std::atomic<bool> completed;
        state_base_ptr    src_state;
        state_base_ptr    dst_state;
    };
    auto data       = std::make_shared<timer_data>(m_state->get_executor(), duration);
    data->src_state = src_future.get_state();
    data->dst_state = m_state;

    src_future.add_continuation([data, callback = std::move(callback)]() mutable {
        if (!data->completed.exchange(true)) {
            data->timer.cancel();
            if (data->src_state->is_rejected()) {
                any_promise promise(std::move(data->dst_state));
                promise.set_exception(data->src_state->get_exception());
            } else if (data->src_state->is_ready()) {
                safe_invoke(std::move(callback));
            }
        }
    });

    data->timer.async_wait([data](const auto& ec) mutable {
        if (!ec && !data->completed.exchange(true)) {
            any_promise promise(std::move(data->dst_state));
            promise.set_exception(std::make_exception_ptr(MC_MAKE_EXCEPTION(mc::timeout_exception, "Future 操作超时")));
        }
    });

    this->on_cancel([data]() {
        if (!data->completed.exchange(true)) {
            data->timer.cancel();
        }
    });
}

any_future any_future::delay(duration_type duration, executor_type executor) {
    any_promise promise = mc::make_promise<void>(executor);
    auto        future  = promise.get_future();

    if (duration == duration_type::zero()) {
        promise.set_value();
        return future;
    }

    using timer_type = mc::runtime::basic_timer<executor_type>;
    auto timer       = std::make_shared<timer_type>(executor, static_cast<duration_type>(duration));

    future.on_cancel([timer]() {
        timer->cancel();
    });

    timer->async_wait([promise = std::move(promise), timer](const auto& ec) mutable {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }

        if (!ec) {
            promise.set_value();
        } else {
            promise.set_exception(std::make_exception_ptr(
                MC_MAKE_EXCEPTION(mc::timeout_exception, "定时器错误: ${error}", ("error", ec.message()))));
        }
    });

    return future;
}

} // namespace mc::futures
