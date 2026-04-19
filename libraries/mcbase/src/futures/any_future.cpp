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
#include <mc/runtime/steady_timer.h>

#include <mutex>

namespace mc::futures {
namespace {

struct wait_hook_guard {
    explicit wait_hook_guard(wait_hook_t enter_hook, wait_hook_t leave_hook) : m_leave_hook(leave_hook)
    {
        if (enter_hook) {
            enter_hook();
        }
    }

    ~wait_hook_guard()
    {
        if (m_leave_hook) {
            m_leave_hook();
        }
    }

private:
    wait_hook_t m_leave_hook;
};

inline wait_hook_guard make_wait_hook_guard()
{
    return wait_hook_guard(blocking_wait_enter_hook(), blocking_wait_leave_hook());
}

} // namespace

any_future::any_future(state_base_ptr state) : m_state(std::move(state))
{}

bool any_future::valid() const noexcept
{
    return m_state != nullptr;
}

bool any_future::is_ready() const
{
    if (!m_state) {
        return false;
    }

    return m_state->is_ready();
}

bool any_future::is_cancelled() const
{
    if (!m_state) {
        return false;
    }

    return m_state->is_cancelled();
}

void any_future::wait() const
{
    if (!m_state) {
        return;
    }

    m_state->try_start_deferred_task();

    std::unique_lock lock(m_state->m_mutex);
    while (!m_state->is_ready()) {
        auto guard = make_wait_hook_guard();
        m_state->m_cv.wait(lock);
    }
}

void any_future::cancel()
{
    if (!m_state) {
        return;
    }

    m_state->cancel();
}

void any_future::on_cancel_impl(callback_type callback)
{
    if (m_state) {
        m_state->add_cancel_callback(std::move(callback));
    }
}

void any_future::tap_error_impl(std::function<void(const mc::exception&)> inspector, launch policy)
{
    if (!m_state) {
        return;
    }

    any_future::add_continuation([handler = std::move(inspector), state = get_state()]() mutable {
        if (!state->is_rejected()) {
            return;
        }
        if (auto exception = state->get_exception_object()) {
            try {
                handler(*exception);
            } catch (...) {
            }
        }
    }, policy, mc::any_executor(mc::runtime::immediate_executor()));
}

static void link_cancel_states(state_base_ptr& state, state_base_ptr other_state)
{
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

void any_future::on_cancel(any_promise& other_promise)
{
    link_cancel_states(m_state, other_promise.get_state());
}

void any_future::on_cancel(any_future& other_future)
{
    link_cancel_states(m_state, other_future.get_state());
}

const state_base_ptr& any_future::get_state() const noexcept
{
    return m_state;
}

bool any_future::is_rejected() const
{
    if (!m_state) {
        return false;
    }

    return m_state->is_rejected();
}

std::shared_ptr<mc::exception> any_future::get_exception() const noexcept
{
    if (!m_state) {
        return {};
    }

    return m_state->get_exception_object();
}

void any_future::add_continuation_impl(callback_type continuation, launch policy)
{
    if (!m_state) {
        return;
    }

    add_continuation_impl(std::move(continuation), policy, m_state->m_executor);
}

void any_future::add_continuation_impl(callback_type continuation, launch policy, executor_type executor)
{
    if (!m_state) {
        return;
    }

    // 注册 continuation 也是 deferred 任务的拉取触发点。
    m_state->try_start_deferred_task();

    std::unique_lock lock(m_state->m_mutex);

    if (m_state->is_ready()) {
        lock.unlock(); // 解锁，防止 dispatch 或 immediate_executor 立即执行回调造成死锁

        if (policy == launch::dispatch) {
            executor.dispatch(std::move(continuation));
        } else {
            executor.post(std::move(continuation));
        }
    } else {
        m_state->m_continuations.push_back([e = std::move(executor), policy, c = std::move(continuation)]() {
            if (policy == launch::dispatch) {
                e.dispatch(std::move(c));
            } else {
                e.post(std::move(c));
            }
        });
    }
}

void any_future::finally_impl(any_promise& promise, callback_type cleanup, launch policy)
{
    if (!m_state) {
        promise.set_exception(MC_MAKE_EXCEPTION(invalid_future_exception, "Future 无效"));
        return;
    }

    finally_impl(promise, std::move(cleanup), policy, m_state->m_executor);
}

void any_future::finally_impl(any_promise& promise, callback_type cleanup, launch policy, executor_type executor)
{
    if (!m_state) {
        promise.set_exception(MC_MAKE_EXCEPTION(invalid_future_exception, "Future 无效"));
        return;
    }

    add_continuation([promise, cleanup = std::move(cleanup)]() mutable {
        try {
            cleanup();
        } catch (...) {
            promise.set_current_exception();
        }
    }, policy, std::move(executor));
}

future_status any_future::wait_for_impl(std::chrono::steady_clock::duration duration) const
{
    if (!m_state) {
        return future_status::invalid;
    }

    // 与 std::future 一致：deferred 任务未启动时返回 deferred 状态而不主动触发；
    // 任务已启动后退化为正常超时等待（worker 线程上 m_cv.wait_until 会嵌套驱动 task 队列）。
    if (m_state->get_policy() == launch::deferred && !m_state->is_ready()
        && !m_state->is_deferred_task_started()) {
        return future_status::deferred;
    }

    std::unique_lock lock(m_state->m_mutex);

    using duration_type = std::chrono::steady_clock::duration;
    auto timeout_at     = std::chrono::steady_clock::now() + static_cast<duration_type>(duration);
    while (!m_state->is_ready()) {
        auto guard = make_wait_hook_guard();
        if (m_state->m_cv.wait_until(lock, timeout_at) == std::cv_status::timeout) {
            return m_state->is_ready() ? future_status::ready : future_status::timeout;
        }
    }
    return future_status::ready;
}

future_status any_future::wait_until_impl(std::chrono::steady_clock::time_point timeout_time) const
{
    if (!m_state) {
        return future_status::invalid;
    }

    // 与 std::future 一致：deferred 任务未启动时返回 deferred 状态而不主动触发。
    if (m_state->get_policy() == launch::deferred && !m_state->is_ready()
        && !m_state->is_deferred_task_started()) {
        return future_status::deferred;
    }

    std::unique_lock lock(m_state->m_mutex);

    using time_point_type = std::chrono::steady_clock::time_point;
    auto deadline         = time_point_type(timeout_time);
    while (!m_state->is_ready()) {
        auto guard = make_wait_hook_guard();
        if (m_state->m_cv.wait_until(lock, deadline) == std::cv_status::timeout) {
            return m_state->is_ready() ? future_status::ready : future_status::timeout;
        }
    }
    return future_status::ready;
}

void any_future::timeout_impl(any_future& src_future, duration_type duration, callback_type callback)
{
    if (!m_state) {
        return;
    }

    struct timer_data {
        timer_data(any_executor executor, duration_type duration) : timer(std::move(executor)), completed(false)
        {
            timer.expires_after(duration);
        }

        mc::runtime::steady_timer timer;
        std::atomic<bool>         completed;
        state_base_ptr            src_state;
        state_base_ptr            dst_state;
    };
    auto data       = std::make_shared<timer_data>(m_state->get_executor(), duration);
    data->src_state = src_future.get_state();
    data->dst_state = m_state;

    src_future.add_continuation([data, callback = std::move(callback)]() mutable {
        if (!data->completed.exchange(true)) {
            data->timer.cancel();
            if (data->src_state->is_rejected()) {
                any_promise promise(std::move(data->dst_state));
                data->src_state->copy_exception_to(promise);
            } else if (data->src_state->is_ready()) {
                safe_invoke(std::move(callback));
            }
        }
    });

    data->timer.async_wait([data](const auto& ec) mutable {
        if (!ec && !data->completed.exchange(true)) {
            any_promise promise(std::move(data->dst_state));
            promise.set_exception(MC_MAKE_EXCEPTION(mc::timeout_exception, "Future 操作超时"));
        }
    });

    this->on_cancel([data]() {
        if (!data->completed.exchange(true)) {
            data->timer.cancel();
        }
    });
}

any_future any_future::delay(duration_type duration, executor_type executor)
{
    any_promise promise = mc::make_promise<void>(executor);
    auto        future  = promise.get_future();

    if (duration == duration_type::zero()) {
        promise.set_value();
        return future;
    }

    auto timer = std::make_shared<mc::runtime::steady_timer>(std::move(executor));
    timer->expires_after(static_cast<duration_type>(duration));

    future.on_cancel([timer]() {
        timer->cancel();
    });

    timer->async_wait([promise = std::move(promise), timer](const auto& ec) mutable {
        if (ec == std::make_error_code(std::errc::operation_canceled)) {
            return;
        }

        if (!ec) {
            promise.set_value();
        } else {
            promise.set_exception(
                MC_MAKE_EXCEPTION(mc::timeout_exception, "定时器错误: ${error}", ("error", ec.message())));
        }
    });

    return future;
}

} // namespace mc::futures
