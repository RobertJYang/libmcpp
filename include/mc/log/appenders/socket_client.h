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

#ifndef MC_LOG_APPENDERS_SOCKET_CLIENT_H
#define MC_LOG_APPENDERS_SOCKET_CLIENT_H

#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>

namespace mc {
namespace log {

/**
 * @brief unix域socket封装，负责底层连接与发送
 */
class unix_socket {
public:
    unix_socket();
    ~unix_socket();

    unix_socket(const unix_socket&)            = delete;
    unix_socket& operator=(const unix_socket&) = delete;
    unix_socket(unix_socket&& other) noexcept;
    unix_socket& operator=(unix_socket&& other) noexcept;

    bool connect(std::string_view path);
    bool send(std::string_view data);
    bool send(const void* data, size_t length);
    bool recv(std::string& out, size_t max_length);
    bool recv_all(std::string& out, size_t length);
    void close();
    bool is_valid() const;

private:
    int m_fd{-1};
};

/**
 * @brief 面向上层的socket客户端，提供线程安全的发送能力
 */
class socket_client {
public:
    socket_client();
    ~socket_client();

    socket_client(const socket_client&)            = delete;
    socket_client& operator=(const socket_client&) = delete;
    socket_client(socket_client&&)                 = delete;
    socket_client& operator=(socket_client&&)      = delete;

    void set_path(std::string_view path);
    void set_hb_path(std::string_view hb_path);
    bool connect();
    bool send(std::string_view data);
    bool recv(std::string& out, size_t max_length);
    bool recv_all(std::string& out, size_t length);
    bool heartbeat();
    void disconnect();
    bool is_connected() const;

private:
    bool ensure_connected_locked();
    bool ensure_hb_connected_locked();

    std::string        m_path;
    std::string        m_hb_path;
    unix_socket        m_socket;
    unix_socket        m_hb_socket; // 专门用于心跳的 socket
    mutable std::mutex m_mutex;
};

} // namespace log
} // namespace mc

#endif // MC_LOG_APPENDERS_SOCKET_CLIENT_H