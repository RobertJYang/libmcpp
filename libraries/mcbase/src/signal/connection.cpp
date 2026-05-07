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

#include <algorithm>
#include <mutex>

namespace mc {

connection::connection(std::shared_ptr<detail::connection_state> state) noexcept : m_state(std::move(state))
{}

std::uint64_t connection::add_disconnect_callback(std::function<void()> callback) const
{
    if (!m_state) {
        return 0;
    }
    return m_state->add_disconnect_callback(std::move(callback));
}

void connection::remove_disconnect_callback(std::uint64_t callback_id) const noexcept
{
    if (!m_state || callback_id == 0) {
        return;
    }
    m_state->remove_disconnect_callback(callback_id);
}

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

scoped_connection::scoped_connection(scoped_connection&& other) noexcept
    : m_connection(other.release())
{}

scoped_connection& scoped_connection::operator=(scoped_connection&& other) noexcept
{
    if (this != &other) {
        disconnect();
        m_connection = other.release();
    }
    return *this;
}

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

struct connection_manager::state {
    struct tracked_connection {
        std::uint64_t entry_id{0};
        connection    conn;
        std::uint64_t callback_id{0};
    };

    mutable std::mutex              mutex;
    std::vector<tracked_connection> connections;
    std::uint64_t                   next_entry_id{1};

    void compact_locked()
    {
        connections.erase(std::remove_if(connections.begin(), connections.end(),
                                         [](const tracked_connection& entry) {
            return !entry.conn.connected();
        }),
                          connections.end());
    }

    void remove_entry(std::uint64_t entry_id)
    {
        std::lock_guard<std::mutex> lock(mutex);
        connections.erase(std::remove_if(connections.begin(), connections.end(),
                                         [entry_id](const tracked_connection& entry) {
            return entry.entry_id == entry_id;
        }),
                          connections.end());
    }

    void update_callback_id(std::uint64_t entry_id, std::uint64_t callback_id)
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto& entry : connections) {
            if (entry.entry_id == entry_id) {
                entry.callback_id = callback_id;
                return;
            }
        }
    }
};

connection_manager::connection_manager() : m_state(std::make_shared<state>())
{}

connection_manager::connection_manager(connection_manager&& other) noexcept : m_state(std::move(other.m_state))
{
    other.m_state = std::make_shared<state>();
}

connection_manager& connection_manager::operator=(connection_manager&& other) noexcept
{
    if (this != &other) {
        disconnect_all();
        m_state       = std::move(other.m_state);
        other.m_state = std::make_shared<state>();
    }
    return *this;
}

connection_manager::~connection_manager()
{
    disconnect_all();
}

void connection_manager::add(connection conn)
{
    if (!conn.connected()) {
        return;
    }

    const auto    state    = m_state;
    std::uint64_t entry_id = 0;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->compact_locked();
        entry_id = state->next_entry_id++;
        state->connections.push_back({entry_id, conn, 0});
    }

    const auto callback_id = conn.add_disconnect_callback(
        [weak_state = std::weak_ptr<connection_manager::state>(state), entry_id]() noexcept {
        if (const auto locked_state = weak_state.lock()) {
            locked_state->remove_entry(entry_id);
        }
    });

    if (callback_id == 0 || !conn.connected()) {
        state->remove_entry(entry_id);
    } else {
        state->update_callback_id(entry_id, callback_id);
    }
}

void connection_manager::disconnect_all()
{
    std::vector<connection> connections;
    {
        std::lock_guard<std::mutex> lock(m_state->mutex);
        m_state->compact_locked();
        connections.reserve(m_state->connections.size());
        for (const auto& entry : m_state->connections) {
            entry.conn.remove_disconnect_callback(entry.callback_id);
            connections.push_back(entry.conn);
        }
        m_state->connections.clear();
    }

    for (auto& conn : connections) {
        conn.disconnect();
    }
}

bool connection_manager::empty() const
{
    std::lock_guard<std::mutex> lock(m_state->mutex);
    m_state->compact_locked();
    return m_state->connections.empty();
}

std::size_t connection_manager::size() const
{
    std::lock_guard<std::mutex> lock(m_state->mutex);
    m_state->compact_locked();
    return m_state->connections.size();
}

} // namespace mc
