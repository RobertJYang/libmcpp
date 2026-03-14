/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "include/connection_manager.h"

#include "mc/exception.h"

#include <algorithm>

namespace mc::core {

connection_id_type connection_manager::add_connection(signal_type sig, mc::connection_type conn, connection_id_type id)
{
    if (id == INVALID_CONNECTION_ID) {
        id = new_id();
    } else if (m_connections.find(id) != m_connections.end()) {
        MC_THROW(mc::invalid_arg_exception, "connection id=${id} already exists", ("id", id));
    }

    m_connections.emplace(std::piecewise_construct, std::forward_as_tuple(id),
                          std::forward_as_tuple(sig, std::move(conn)));
    m_signal_connections[sig].push_back(id);
    return id;
}

void connection_manager::remove_connection(connection_id_type id)
{
    auto it = m_connections.find(id);
    if (it == m_connections.end()) {
        return;
    }

    // 显式断开连接，确保信号不再触发
    it->second.conn.disconnect();

    auto& sig    = it->second.sig;
    auto  it_sig = m_signal_connections.find(sig);
    if (it_sig != m_signal_connections.end()) {
        auto& sig_conns = it_sig->second;
        sig_conns.erase(std::remove(sig_conns.begin(), sig_conns.end(), id), sig_conns.end());

        if (sig_conns.empty()) {
            m_signal_connections.erase(sig);
        }
    }

    m_connections.erase(it);
}

size_t connection_manager::remove_connections(signal_type sig)
{
    auto it = m_signal_connections.find(sig);
    if (it == m_signal_connections.end()) {
        return 0;
    }

    size_t count = it->second.size();
    // 显式断开所有连接，确保信号不再触发
    for (auto id : it->second) {
        auto conn_it = m_connections.find(id);
        if (conn_it != m_connections.end()) {
            conn_it->second.conn.disconnect();
            m_connections.erase(conn_it);
        }
    }
    m_signal_connections.erase(it);
    return count;
}

void connection_manager::clear()
{
    // 显式断开所有连接，确保信号不再触发
    for (auto& [id, info] : m_connections) {
        info.conn.disconnect();
    }
    m_connections.clear();
    m_signal_connections.clear();
    m_next_id = 1;
}

constexpr int      MAX_ID_TRY = 100000;
connection_id_type connection_manager::new_id()
{
    for (int i = 0; i < MAX_ID_TRY; ++i) {
        connection_id_type id = m_next_id++;
        if (m_connections.find(id) == m_connections.end()) {
            return id;
        }
    }

    MC_THROW(mc::invalid_arg_exception, "failed to generate new connection id");
}

} // namespace mc::core