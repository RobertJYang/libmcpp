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
#include <mc/runtime/executor.h>
#include <mc/runtime/runtime_context.h>

namespace mc::runtime {
executor::executor(const executor& other) noexcept : m_impl(other.m_impl)
{
    if (m_impl) {
        m_impl->add_ref();
    }
}

executor::executor(executor&& other) noexcept : m_impl(other.m_impl)
{
    other.m_impl = nullptr;
}

executor::~executor()
{
    if (m_impl && m_impl->release()) {
        delete m_impl;
    }
}

executor& executor::operator=(const executor& other) noexcept
{
    if (this != &other) {
        // 释放当前引用
        if (m_impl && m_impl->release()) {
            delete m_impl;
        }
        // 复制新引用
        m_impl = other.m_impl;
        if (m_impl) {
            m_impl->add_ref();
        }
    }
    return *this;
}

executor& executor::operator=(executor&& other) noexcept
{
    if (this != &other) {
        // 释放当前引用
        if (m_impl && m_impl->release()) {
            delete m_impl;
        }
        // 移动新引用
        m_impl       = other.m_impl;
        other.m_impl = nullptr;
    }
    return *this;
}

bool executor::valid() const noexcept
{
    return m_impl != nullptr;
}

bool executor::operator==(const executor& other) const noexcept
{
    if (m_impl == other.m_impl) {
        return true;
    }

    if (!m_impl || !other.m_impl) {
        return false;
    }

    return m_impl->equal(*other.m_impl);
}

bool executor::operator!=(const executor& other) const noexcept
{
    return !(*this == other);
}

void executor::on_work_started() const noexcept
{
    if (m_impl) {
        m_impl->on_work_started();
    }
}

void executor::on_work_finished() const noexcept
{
    if (m_impl) {
        m_impl->on_work_finished();
    }
}

execution_context& executor::context() const
{
    MC_ASSERT_THROW(m_impl, mc::invalid_op_exception, "Cannot get context from invalid executor");
    return m_impl->context();
}

bool executor::running_in_this_thread() const noexcept
{
    if (!m_impl) {
        return false;
    }
    return m_impl->running_in_this_thread();
}

executor& executor::bound_pool(thread_pool* pool) noexcept
{
    if (!m_impl) {
        return *this;
    }
    m_impl->bound_pool(pool);
    return *this;
}

thread_pool* executor::get_bound_pool() const noexcept
{
    if (!m_impl) {
        return nullptr;
    }
    return m_impl->get_bound_pool();
}

executor::operator boost::asio::any_io_executor() const
{
    if (m_impl) {
        if (auto ex = m_impl->to_any_io_executor()) {
            return *ex;
        }
    }

    return runtime::get_io_executor();
}

executor::operator boost::asio::io_context::executor_type() const
{
    if (m_impl) {
        if (auto ex = m_impl->to_io_executor()) {
            return *ex;
        }
    }

    return runtime::get_io_executor();
}

} // namespace mc::runtime