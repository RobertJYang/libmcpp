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

#include <mc/futures/any_future.h>
#include <mc/futures/any_promise.h>
#include <mc/futures/combinator.h>
#include <mutex>

namespace mc::futures {
namespace detail {

CombinatorState::CombinatorState(std::size_t total_count, any_promise promise)
    : m_total_count(total_count), m_promise(std::move(promise))
{}

void CombinatorState::add_cancel_callback(any_future& future)
{
    add_cancel_callback(future.get_state());
}

void CombinatorState::add_cancel_callback(const state_base_ptr& future_state)
{
    m_cancel_callbacks.push_back([wstate = mc::weak_ptr(future_state)]() {
        if (auto state = wstate.lock()) {
            state->cancel();
        }
    });
}

void CombinatorState::execute_cancel_callbacks()
{
    callback_list callbacks;
    {
        std::lock_guard lock(m_mutex);
        callbacks.swap(m_cancel_callbacks);
    }

    callbacks.execute_and_clear();
}

AllStateBase::AllStateBase(std::size_t total_count, any_promise promise)
    : CombinatorState(total_count, std::move(promise))
{}

void AllStateBase::set_exception(const mc::exception& e)
{
    {
        std::lock_guard lock(m_mutex);
        m_completed_count++;
        if (m_first_exception) {
            // 如果已经异常了，忽略其他异常
            return;
        }

        // all 操作传播第一个异常，因为这个异常发生后整体就失败了
        m_first_exception = true;
        m_promise.set_exception(e);
    }

    // 异常后取消所有子 future
    execute_cancel_callbacks();
}

void AllStateBase::cancel()
{
    m_promise.cancel();
}

bool AllStateBase::set_completed(std::size_t index)
{
    std::lock_guard lock(m_mutex);

    // 如果已经有异常了，忽略正常的结果
    if (m_first_exception) {
        return false;
    }

    m_completed_count++;
    if (m_completed_count == m_total_count) {
        return true;
    }

    return false;
}

AnyStateBase::AnyStateBase(std::size_t total_count, any_promise promise)
    : CombinatorState(total_count, std::move(promise))
{}

void AnyStateBase::set_exception(const mc::exception& e)
{
    {
        std::lock_guard lock(m_mutex);
        if (m_completed) {
            return;
        }

        // 记录异常
        auto ex = e.dynamic_copy_exception();
        if (m_last_exception) {
            auto old_messsgs = m_last_exception->take_messages();
            ex->append_log(std::move(old_messsgs));
        }
        m_last_exception = ex;

        // 增加失败计数
        m_failed_count++;
        if (m_failed_count < m_total_count) {
            return;
        }

        // 全部失败了
        m_completed = true;
        m_promise.set_exception(*m_last_exception);
    }

    execute_cancel_callbacks();
}

bool AnyStateBase::set_completed(std::size_t index)
{
    std::lock_guard lock(m_mutex);
    if (m_completed) {
        return false;
    }

    m_completed = true;
    return true;
}

void AnyStateBase::cancel()
{
    set_exception(mc::canceled_exception());
}

} // namespace detail
} // namespace mc::futures
