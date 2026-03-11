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

#include <mc/futures/any_promise.h>

#include <mc/exception.h>
#include <mc/futures/exceptions.h>

#include <mutex>

namespace mc::futures {

any_promise::any_promise(state_base_ptr state)
    : m_state(std::move(state))
{
}

void any_promise::set_result_inner(bool strict_once)
{
    if (!m_state || m_state->is_cancelled()) {
        return;
    }

    if (strict_once) {
        MC_ASSERT_THROW(!m_state->is_ready() && !m_state->is_bound(), promise_already_satisfied, "Promise 值已被设置");
    } else if (m_state->is_ready()) {
        return;
    }

    m_state->m_ready.store(true, std::memory_order_release);
}

void any_promise::set_exception(std::exception_ptr e, bool strict_once)
{
    if (!m_state || m_state->is_cancelled()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_state->m_mutex);
        if (m_state->is_cancelled()) {
            return;
        }

        if (strict_once) {
            MC_ASSERT_THROW(!m_state->is_ready() && !m_state->is_bound(), promise_already_satisfied, "Promise 值已被设置");
        } else if (m_state->is_ready()) {
            return;
        }

        m_state->set_exception(std::move(e));
        m_state->m_ready.store(true, std::memory_order_release);
    }

    m_state->mark_ready();
}

void any_promise::set_exception(const mc::exception& e, bool strict_once)
{
    if (!m_state || m_state->is_cancelled()) {
        return;
    }

    try {
        e.dynamic_rethrow_exception();
    } catch (...) {
        set_exception(std::current_exception(), strict_once);
    }
}

void any_promise::cancel()
{
    if (m_state) {
        m_state->cancel();
    }
}

any_future any_promise::get_future()
{
    if (m_future_retrieved) {
        MC_THROW(future_already_retrieved, "Future 已被获取");
    }

    m_future_retrieved = true;
    return any_future(m_state);
}

bool any_promise::set_bound()
{
    if (!m_state || m_state->is_cancelled()) {
        return false;
    }

    MC_ASSERT_THROW(!m_state->is_ready() && !m_state->is_bound(), promise_already_satisfied, "Promise 已被绑定");
    m_state->m_bound.store(true, std::memory_order_release);
    return true;
}

const state_base_ptr& any_promise::get_state() const noexcept
{
    return m_state;
}

void any_promise::set_executor(const any_executor& executor)
{
    if (m_state) {
        m_state->m_executor = executor;
    }
}

void any_promise::reset()
{
    m_state.reset();
    m_future_retrieved = false;
}

} // namespace mc::futures
