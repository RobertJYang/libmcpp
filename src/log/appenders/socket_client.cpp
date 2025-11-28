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

#include <mc/log/appenders/socket_client.h>

#include <cstdint>
#include <cerrno>
#include <cstring>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace mc {
namespace log {

namespace {
constexpr size_t           MAX_UNIX_PATH_LENGTH = sizeof(sockaddr_un::sun_path) - 1;
constexpr std::string_view HEARTBEAT_MESSAGE    = "ok";
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#ifndef MSG_WAITALL
#define MSG_WAITALL 0
#endif
} // namespace

unix_socket::unix_socket() = default;

unix_socket::~unix_socket() {
    close();
}

unix_socket::unix_socket(unix_socket&& other) noexcept : m_fd(other.m_fd) {
    other.m_fd = -1;
}

unix_socket& unix_socket::operator=(unix_socket&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    close();
    m_fd       = other.m_fd;
    other.m_fd = -1;
    return *this;
}

bool unix_socket::connect(std::string_view path) {
    if (path.empty() || path.size() > MAX_UNIX_PATH_LENGTH) {
        return false;
    }

    close();

    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return false;
    }

    sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, path.data(), path.size());
    addr.sun_path[path.size()] = '\0';

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_un)) != 0) {
        ::close(fd);
        return false;
    }

    m_fd = fd;
    return true;
}

bool unix_socket::send(std::string_view data) {
    return send(data.data(), data.size());
}

bool unix_socket::send(const void* data, size_t length) {
    if (!is_valid() || data == nullptr || length == 0) {
        return false;
    }

    const auto* buffer = static_cast<const uint8_t*>(data);
    size_t      sent   = 0;
    while (sent < length) {
        ssize_t ret = ::send(m_fd, buffer + sent, length - sent, MSG_NOSIGNAL);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        sent += static_cast<size_t>(ret);
    }
    return true;
}

bool unix_socket::recv(std::string& out, size_t max_length) {
    if (!is_valid() || max_length == 0) {
        return false;
    }

    out.resize(max_length);
    ssize_t ret = ::recv(m_fd, out.data(), max_length, 0);
    if (ret <= 0) {
        out.clear();
        return false;
    }

    out.resize(static_cast<size_t>(ret));
    return true;
}

bool unix_socket::recv_all(std::string& out, size_t length) {
    if (!is_valid() || length == 0) {
        return false;
    }

    out.resize(length);
    ssize_t ret = ::recv(m_fd, out.data(), length, MSG_WAITALL);
    if (ret != static_cast<ssize_t>(length)) {
        out.clear();
        return false;
    }

    return true;
}

void unix_socket::close() {
    if (m_fd < 0) {
        return;
    }

    ::close(m_fd);
    m_fd = -1;
}

bool unix_socket::is_valid() const {
    return m_fd >= 0;
}

socket_client::socket_client() = default;

socket_client::~socket_client() {
    disconnect();
}

void socket_client::set_path(std::string_view path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_path.assign(path.begin(), path.end());
}

void socket_client::set_hb_path(std::string_view hb_path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hb_path.assign(hb_path.begin(), hb_path.end());
}

bool socket_client::connect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!ensure_connected_locked()) {
        return false;
    }

    if (!ensure_hb_connected_locked()) {
        m_socket.close();
        return false;
    }
    return true;
}

bool socket_client::send(std::string_view data) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!ensure_connected_locked()) {
        return false;
    }

    if (m_socket.send(data)) {
        return true;
    }

    m_socket.close();
    if (!ensure_connected_locked()) {
        return false;
    }
    return m_socket.send(data);
}

void socket_client::disconnect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_socket.close();
    m_hb_socket.close();
}

bool socket_client::recv(std::string& out, size_t max_length) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!ensure_connected_locked()) {
        return false;
    }

    if (m_socket.recv(out, max_length)) {
        return true;
    }

    m_socket.close();
    if (!ensure_connected_locked()) {
        out.clear();
        return false;
    }
    return m_socket.recv(out, max_length);
}

bool socket_client::recv_all(std::string& out, size_t length) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!ensure_connected_locked()) {
        return false;
    }

    if (m_socket.recv_all(out, length)) {
        return true;
    }

    m_socket.close();
    if (!ensure_connected_locked()) {
        out.clear();
        return false;
    }

    return m_socket.recv_all(out, length);
}

bool socket_client::heartbeat() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (HEARTBEAT_MESSAGE.empty()) {
        return false;
    }

    if (!m_hb_path.empty()) {
        if (!ensure_hb_connected_locked()) {
            return false;
        }

        if (!m_hb_socket.send(HEARTBEAT_MESSAGE)) {
            m_hb_socket.close();
            if (!ensure_hb_connected_locked()) {
                return false;
            }
            if (!m_hb_socket.send(HEARTBEAT_MESSAGE)) {
                return false;
            }
        }

        std::string response;
        if (m_hb_socket.recv_all(response, HEARTBEAT_MESSAGE.size())) {
            return true;
        }

        m_hb_socket.close();
        if (!ensure_hb_connected_locked()) {
            return false;
        }

        if (!m_hb_socket.send(HEARTBEAT_MESSAGE)) {
            return false;
        }

        return m_hb_socket.recv_all(response, HEARTBEAT_MESSAGE.size());
    }
}

bool socket_client::is_connected() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_socket.is_valid();
}

bool socket_client::ensure_connected_locked() {
    if (m_socket.is_valid()) {
        return true;
    }

    if (m_path.empty()) {
        return false;
    }

    return m_socket.connect(m_path);
}

bool socket_client::ensure_hb_connected_locked() {
    if (m_hb_socket.is_valid()) {
        return true;
    }

    if (m_hb_path.empty()) {
        return false;
    }

    return m_hb_socket.connect(m_hb_path);
}

} // namespace log
} // namespace mc

