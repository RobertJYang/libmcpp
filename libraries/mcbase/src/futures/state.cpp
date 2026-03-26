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
#include <mc/futures/any_promise.h>
#include <mc/futures/state.h>

namespace mc::futures {

namespace detail {

bool handle_result_state_common(any_promise& promise, state_base& state)
{
    if (state.is_cancelled()) {
        promise.cancel();
        return true;
    }

    if (state.is_rejected()) {
        state.copy_exception_to(promise);
        return true;
    }

    return false;
}

} // namespace detail

state_base::state_base(executor_type executor, destory_type destory, int value_size) noexcept
    : m_executor(std::move(executor)), m_destory(std::move(destory)), m_value_size(value_size)
{}

state_base::~state_base()
{}

void state_base::set_executor(executor_type executor) noexcept
{
    m_executor = std::move(executor);
}

std::shared_ptr<mc::exception> state_base::get_exception_object() const noexcept
{
    if (!m_exception) {
        return {};
    }

    std::shared_ptr<mc::exception> result;
    try {
        std::rethrow_exception(m_exception);
    } catch (const mc::exception& mc_ex) {
        result = mc_ex.dynamic_copy_exception();
    } catch (const std::exception& std_ex) {
        auto wrapped = mc::std_exception_wrapper::from_current_exception(std_ex);
        result       = wrapped.dynamic_copy_exception();
    } catch (...) {
        auto unknown = MC_MAKE_EXCEPTION(mc::exception, "Unknown Future Exception");
        result       = unknown.dynamic_copy_exception();
    }
    return result;
}

void state_base::rethrow_exception() const
{
    std::rethrow_exception(m_exception);
}

void state_base::copy_exception_to(any_promise& promise, bool strict_once) const
{
    if (!m_exception) {
        return;
    }

    try {
        std::rethrow_exception(m_exception);
    } catch (const mc::exception& mc_ex) {
        promise.set_exception(mc_ex, strict_once);
    } catch (...) {
        promise.set_current_exception(strict_once);
    }
}

void state_base::reset()
{
    m_ready.store(false);
    m_bound.store(false);
    m_cancelled.store(false);
    m_policy = launch::async;
    m_continuations.clear();
    m_cancel_callbacks.clear();
    m_executor  = executor_type{};
    m_exception = nullptr;
    m_destory   = nullptr;
}

void state_base::reuse_impl()
{}

void state_base::mark_ready()
{
    callback_list callbacks;
    callback_list cancel_callbacks;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_ready.store(true, std::memory_order_release);
        m_continuations.swap(callbacks);
        m_cancel_callbacks.swap(cancel_callbacks);
    }
    m_cv.notify_all();
    callbacks.execute_and_clear();

    // 直接清空 cancel_callbacks 因为不再需要执行了
    cancel_callbacks.clear();
}

void state_base::cancel()
{
    // 先检查是否已经 ready，如果已经完成则忽略取消操作
    if (m_ready.load()) {
        return;
    }

    bool expected = false;
    if (!m_cancelled.compare_exchange_strong(expected, true)) {
        return;
    }

    // 移动到临时变量执行，避免回调过程中修改链表或死锁
    callback_list callbacks;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cancel_callbacks.swap(callbacks);
    }
    callbacks.execute_and_clear();

    // 设置取消异常
    bool need_ready = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_ready.load()) {
            set_exception(std::make_exception_ptr(mc::canceled_exception()));
            need_ready = true;
            m_ready.store(true, std::memory_order_release);
        }
    }

    if (need_ready) {
        this->mark_ready();
    }
}

void state_base::add_cancel_callback(callback_type callback)
{
    if (!m_ready.load() && !m_cancelled.load()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_ready.load() && !m_cancelled.load()) {
            m_cancel_callbacks.push_back(std::move(callback));
            return;
        }
    }

    if (m_cancelled.load()) {
        safe_invoke(std::move(callback));
    }
}

void state_base::destory()
{
    if (m_destory) {
        m_destory(this);
    }
    reset();
}

} // namespace mc::futures
