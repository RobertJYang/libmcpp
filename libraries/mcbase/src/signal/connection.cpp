/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <mc/signal/connection.h>

#include <mc/signal/detail/backend.h>

namespace mc {

connection::connection(std::shared_ptr<detail::connection_state> state) noexcept : m_state(std::move(state))
{}

void connection::disconnect() noexcept
{
    if (m_state) {
        m_state->disconnect();
    }
}

bool connection::connected() const noexcept
{
    return m_state && m_state->connected();
}

scoped_connection::scoped_connection(connection conn) noexcept : m_connection(std::move(conn))
{}

scoped_connection::~scoped_connection()
{
    disconnect();
}

scoped_connection::scoped_connection(scoped_connection&& other) noexcept = default;

scoped_connection& scoped_connection::operator=(scoped_connection&& other) noexcept = default;

connection scoped_connection::release() noexcept
{
    connection released = std::move(m_connection);
    m_connection        = connection();
    return released;
}

void scoped_connection::disconnect() noexcept
{
    m_connection.disconnect();
}

bool scoped_connection::connected() const noexcept
{
    return m_connection.connected();
}

connection_manager::~connection_manager()
{
    disconnect_all();
}

void connection_manager::add(connection conn)
{
    m_connections.push_back(std::move(conn));
}

void connection_manager::disconnect_all()
{
    for (auto& conn : m_connections) {
        conn.disconnect();
    }
    m_connections.clear();
}

bool connection_manager::empty() const
{
    return m_connections.empty();
}

std::size_t connection_manager::size() const
{
    return m_connections.size();
}

} // namespace mc
