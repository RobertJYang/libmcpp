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

#include <mc/runtime/runtime_context.h>
#include <mc/runtime/runtime_executor.h>
#include <mc/singleton.h>

#include "runtime/include/thread_pool_impl.h"

namespace mc::runtime {

runtime_executor::runtime_executor() : m_context(&mc::singleton<runtime_context>::instance())
{}

runtime_executor::runtime_executor(runtime_context& ctx) : m_context(&ctx)
{}

bool runtime_executor::operator==(const runtime_executor& other) const noexcept
{
    if (m_context != other.m_context || m_bound_pool != other.m_bound_pool) {
        return false;
    }

    return true;
}

bool runtime_executor::operator!=(const runtime_executor& other) const noexcept
{
    return !(*this == other);
}

void runtime_executor::on_work_started() const noexcept
{
    detail::thread_pool_impl::on_work_started(select_pool());
}

void runtime_executor::on_work_finished() const noexcept
{
    detail::thread_pool_impl::on_work_finished(select_pool());
}

runtime_context& runtime_executor::get_runtime_context() const noexcept
{
    return *m_context;
}

bool runtime_executor::running_in_this_thread() const noexcept
{
    if (auto* current_pool = detail::thread_pool_impl::current_pool()) {
        // 指定了绑定 pool 则必须精确匹配
        if (m_bound_pool != nullptr) {
            return m_bound_pool == current_pool;
        }

        // 否则任何一个 pool 都行
        return current_pool == &m_context->io() || current_pool == &m_context->work();
    }
    return false;
}

thread_pool& runtime_executor::select_pool() const
{
    // 优先级1：使用绑定的 pool
    if (m_bound_pool != nullptr) {
        return *m_bound_pool;
    }

    // 优先级2：使用当前线程所在的 pool
    if (auto* current_pool = detail::thread_pool_impl::current_pool()) {
        return *current_pool;
    }

    // 优先级3：默认使用 io pool
    return m_context->io();
}

} // namespace mc::runtime
