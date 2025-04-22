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

#include <mc/dbus/transport/transport_factory.h>
#include <mc/dbus/transport/unix_socket_transport.h>
#include <mc/log.h>

namespace mc::dbus {

unix_socket_transport::unix_socket_transport(io_context_type& io_context, address_entry_ptr entry)
    : transport(io_context, std::move(entry)), m_socket(io_context) {
}

unix_socket_transport::~unix_socket_transport() {
    close();
}

bool unix_socket_transport::verify_connection() {
    // 检查socket是否真的连接到了正确的地址
    struct sockaddr_un peer_addr;
    socklen_t          peer_addr_size = sizeof(peer_addr);
    int ret = getpeername(m_socket.native_handle(), (struct sockaddr*)&peer_addr, &peer_addr_size);
    if (ret == 0) {
        std::cout << "连接到: " << peer_addr.sun_path << std::endl;
        return true;
    } else {
        std::cout << "无法获取对端地址: " << strerror(errno) << std::endl;
        return false;
    }
}

mc::future<transport::error_code> unix_socket_transport::connect() {
    auto promise = mc::make_promise<error_code>(m_io_context);

    if (m_is_connected) {
        promise.set_value(boost::asio::error::already_connected);
        return promise.get_future();
    }

    try {
        std::string_view socket_path = m_entry->get_value("path");
        if (socket_path.empty()) {
            socket_path = m_entry->get_value("abstract");
        }

        boost::asio::local::stream_protocol::endpoint endpoint(socket_path);
        m_socket.async_connect(endpoint, [this, promise](const auto& ec) mutable {
            promise.set_value(ec);
            if (ec) {
                return;
            }

            verify_connection();
        });

        m_socket.non_blocking(true);
    } catch (const std::exception& e) {
        elog("Unix套接字连接异常: ${error}", ("error", e.what()));
        promise.set_value(boost::asio::error::invalid_argument);
    }

    return promise.get_future();
}

void unix_socket_transport::close() {
    if (!m_is_connected) {
        return;
    }

    boost::system::error_code ec;
    m_socket.close(ec);
    m_is_connected = false;

    if (ec) {
        elog("Unix套接字关闭错误: ${error}", ("error", ec.message()));
    }
}

mc::future<std::pair<transport::error_code, std::size_t>>
unix_socket_transport::async_write(buffer buf) {
    auto promise = mc::make_promise<std::pair<error_code, std::size_t>>(m_io_context);
    auto future  = promise.get_future();

    if (!m_is_connected) {
        promise.set_value(std::make_pair(boost::asio::error::not_connected, 0));
        return promise.get_future();
    }

    auto buffer = boost::asio::buffer(buf);
    boost::asio::async_write(m_socket, buffer,
                             [promise](const auto& ec, std::size_t bytes) mutable {
                                 promise.set_value(std::make_pair(ec, bytes));
                             });

    return future;
}

std::size_t unix_socket_transport::write_some(buffer buf, error_code& ec) {
    if (!m_is_connected) {
        ec = boost::asio::error::not_connected;
        return 0;
    }

    return m_socket.write_some(boost::asio::buffer(buf.data(), buf.length()), ec);
}

std::size_t unix_socket_transport::read_some(buffer buf, error_code& ec) {
    if (!m_is_connected) {
        ec = boost::asio::error::not_connected;
        return 0;
    }

    auto buffer = boost::asio::buffer(const_cast<char*>(buf.data()), buf.length());
    return m_socket.read_some(buffer, ec);
}

mc::future<std::pair<transport::error_code, std::size_t>>
unix_socket_transport::async_read(buffer buf) {
    auto promise = mc::make_promise<std::pair<error_code, std::size_t>>(m_io_context);
    auto future  = promise.get_future();

    if (!m_is_connected) {
        promise.set_value(std::make_pair(boost::asio::error::not_connected, 0));
        return promise.get_future();
    }

    auto buffer = boost::asio::buffer(const_cast<char*>(buf.data()), buf.length());
    m_socket.async_read_some(buffer, [promise](const auto& ec, std::size_t bytes) mutable {
        promise.set_value(std::make_pair(ec, bytes));
    });

    return future;
}

// 注册Unix套接字传输层创建器
static auto _ = mc::dbus::transport_factory::get_instance().register_creator(
    "unix", [](auto& io_context, auto entry) {
        return std::make_unique<unix_socket_transport>(io_context, std::move(entry));
    });

} // namespace mc::dbus