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
    : transport(io_context, std::move(entry)), m_socket(io_context), m_is_connected(false) {
}

unix_socket_transport::~unix_socket_transport() {
    close();
}

void unix_socket_transport::connect(connect_handler handler) {
    if (m_is_connected) {
        if (handler) {
            boost::system::error_code ec(boost::asio::error::already_connected);
            handler(ec);
        }
        return;
    }

    try {
        std::string_view socket_path = m_entry->get_value("path");
        if (socket_path.empty()) {
            socket_path = m_entry->get_value("abstract");
        }

        boost::asio::local::stream_protocol::endpoint endpoint(socket_path);
        m_socket.async_connect(endpoint, [this, handler](const boost::system::error_code& ec) {
            if (!ec) {
                m_is_connected = true;
            }

            if (handler) {
                handler(ec);
            }
        });
    } catch (const std::exception& e) {
        elog("Unix套接字连接异常: ${error}", ("error", e.what()));
        if (handler) {
            boost::system::error_code ec(boost::asio::error::invalid_argument);
            handler(ec);
        }
    }
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

void unix_socket_transport::write(buffer buf, write_handler handler) {
    if (!m_is_connected) {
        if (handler) {
            boost::system::error_code ec(boost::asio::error::not_connected);
            handler(ec, 0);
        }
        return;
    }

    try {
        boost::asio::async_write(
            m_socket, boost::asio::buffer(buf),
            [handler](const boost::system::error_code& ec, std::size_t bytes_transferred) {
                if (handler) {
                    handler(ec, bytes_transferred);
                }
            });
    } catch (const std::exception& e) {
        elog("Unix套接字发送异常: ${error}", ("error", e.what()));
        if (handler) {
            boost::system::error_code ec(boost::asio::error::invalid_argument);
            handler(ec, 0);
        }
    }
}

void unix_socket_transport::read(writable_buffer buf, read_handler handler) {
    if (!m_is_connected) {
        if (handler) {
            boost::system::error_code ec(boost::asio::error::not_connected);
            handler(ec, 0);
        }
        return;
    }

    m_socket.async_read_some(
        boost::asio::buffer(buf.data, buf.length),
        [handler](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            if (ec) {
                if (handler) {
                    handler(ec, 0);
                }
                return;
            }

            if (handler) {
                handler(ec, bytes_transferred);
            }
        });
}

// 注册Unix套接字传输层创建器
static auto _ = mc::dbus::transport_factory::get_instance().register_creator(
    "unix", [](auto& io_context, auto entry) {
        return std::make_unique<unix_socket_transport>(io_context, std::move(entry));
    });

} // namespace mc::dbus