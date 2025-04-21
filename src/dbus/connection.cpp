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

#include <mc/dbus/connection.h>
#include <mc/dbus/transport/transport_factory.h>
#include <mc/log.h>

namespace mc::dbus {

constexpr std::size_t max_write_length = 1024;

std::shared_ptr<connection> connection::create(io_context_type& io_context) {
    return std::make_shared<connection>(io_context);
}

connection::connection(io_context_type& io_context)
    : m_io_context(io_context), m_state(connection_state::disconnected), m_next_serial(1) {
}

connection::~connection() {
    disconnect();
}

void connection::connect(const std::string& address, connect_handler handler) {
    if (m_state.load() != connection_state::disconnected) {
        if (handler) {
            boost::system::error_code ec(boost::asio::error::already_connected);
            handler(ec);
        }
        return;
    }

    m_state.store(connection_state::connecting);

    try {
        m_transport = transport_factory::get_instance().create(m_io_context, address);
        if (!m_transport) {
            elog("创建传输层失败: ${error}", ("error", "没有可用的传输方式"));
            return;
        }

        m_transport->connect([this, handler](const boost::system::error_code& ec) {
            this->handle_connect(ec, handler);
        });
    } catch (const mc::exception& e) {
        m_state.store(connection_state::disconnected);
        if (handler) {
            boost::system::error_code ec(boost::asio::error::invalid_argument);
            handler(ec);
        }
        elog("创建传输层失败: ${error}", ("error", e.what()));
    }
}

void connection::disconnect() {
    connection_state expected = connection_state::connected;
    if (m_state.compare_exchange_strong(expected, connection_state::disconnecting)) {
        if (m_transport) {
            m_transport->close();
        }
        m_state.store(connection_state::disconnected);
    }
}

connection_state connection::get_state() const {
    return m_state.load();
}

void connection::set_transport(transport_ptr transport) {
    if (m_state.load() != connection_state::disconnected) {
        return;
    }

    m_transport = std::move(transport);
}

void connection::handle_connect(const boost::system::error_code& ec, connect_handler handler) {
    if (ec) {
        m_state.store(connection_state::disconnected);
        if (handler) {
            handler(ec);
        }
        return;
    }

    // 连接成功，开始认证过程
    m_state.store(connection_state::authenticating);

    // TODO: 实现DBus认证过程

    // 暂时跳过认证，直接设置为已连接状态
    boost::system::error_code auth_ec;
    handle_authenticate(auth_ec);

    if (handler) {
        handler(auth_ec);
    }
}

void connection::handle_authenticate(const boost::system::error_code& ec) {
    if (ec) {
        m_state.store(connection_state::disconnected);
        if (m_transport) {
            m_transport->close();
        }
        return;
    }

    m_state.store(connection_state::connected);

    std::lock_guard<std::mutex> lock(m_mutex);
    start_read();
}

void connection::start_read() {
    if (m_state.load() != connection_state::connected || !m_transport) {
        return;
    }

    auto                      data = m_read_stream.get_writeable_data();
    std::weak_ptr<connection> conn = shared_from_this();
    m_transport->read(data, [conn](const auto& ec, std::size_t bytes_transferred) {
        if (auto self = conn.lock()) {
            self->handle_read(ec, bytes_transferred);
        }
    });
}

void connection::handle_read(const boost::system::error_code& ec, std::size_t bytes_transferred) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (ec) {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }

        ilog("DBus连接读取错误: ${error}", ("error", ec.message()));
        disconnect();
        return;
    }

    if (m_read_stream.readable_bytes() < DBUS_MESSAGE_MIN_SIZE) {
        return;
    }

    start_read();
}

void connection::process_message() {
    if (!read_message()) {
        return;
    }

    if (m_current_message->type == message_type::method_call) {
        on_message(std::move(m_current_message));
    } else {
        // TODO:: 暂时清空消息等待读取下一个消息
        m_current_message.reset();
    }
}

bool connection::read_message() {
    try {
        if (m_read_stream.readable_bytes() < DBUS_MESSAGE_MIN_SIZE) {
            return false;
        }

        if (!m_current_message) {
            m_current_message = std::make_shared<variants_message>();
            m_read_stream >> static_cast<message_header&>(*m_current_message);
        }

        if (m_current_message->need_bytes() > m_read_stream.readable_bytes()) {
            return false;
        }

        m_read_stream >> m_current_message->body;
        return true;
    } catch (const mc::exception& e) {
        elog("读取消息失败: ${error}", ("error", e.what()));
        disconnect();
        return false;
    }
}

void connection::start_write() {
    if (m_is_writing) {
        return;
    }

    auto data = m_write_stream.get_data(max_write_length);
    if (data.empty()) {
        return;
    }

    m_is_writing = true;

    std::weak_ptr<connection> conn = shared_from_this();
    m_transport->write(data, [conn](const auto& ec, std::size_t bytes_transferred) {
        if (auto self = conn.lock()) {
            self->handle_write(ec, bytes_transferred);
        }
    });
}

void connection::handle_write(const boost::system::error_code& ec, std::size_t bytes_transferred) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_is_writing = false;

    if (ec) {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }

        elog("DBus消息发送错误: ${error}，关闭连接", ("error", ec.message()));
        disconnect();
        return;
    }

    dlog("DBus消息发送成功，字节数: ${bytes}", ("bytes", bytes_transferred));
    m_write_stream.skip(bytes_transferred);
    start_write();
}

} // namespace mc::dbus
