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

#include <mc/runtime/executor.h>

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

} // namespace mc::runtime