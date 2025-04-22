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

constexpr std::size_t max_write_length    = 1024;
constexpr std::size_t read_buffer_length  = 4096;
constexpr std::size_t write_buffer_length = 4096;

connection_ptr connection::create(io_context_type& io_context) {
    return std::make_shared<connection>(io_context);
}

connection::connection(io_context_type& io_context)
    : m_io_context(io_context), m_state(connection_state::disconnected),
      m_read_stream(stream::buffer::create(read_buffer_length)),
      m_write_stream(stream::buffer::create(write_buffer_length)) {
}

connection::~connection() {
    disconnect();
}

mc::future<error_code> connection::connect(const std::string& address) {
    auto promise = mc::make_promise<error_code>(m_io_context);

    if (m_state.load() != connection_state::disconnected) {
        promise.set_value(boost::asio::error::already_connected);
        return promise.get_future();
    }
    m_state.store(connection_state::connecting);

    try {
        m_transport = transport_factory::get_instance().create(m_io_context, address);
        if (!m_transport) {
            MC_THROW(mc::exception, "没有可用的传输方式");
        }
    } catch (const mc::exception& e) {
        elog("创建传输层失败: ${error}", ("error", e.what()));
        m_state.store(connection_state::disconnected);
        promise.set_exception(std::make_exception_ptr(e));
    }

    std::weak_ptr<connection> conn = shared_from_this();
    m_transport->connect()
        .then([conn, promise](const auto& ec) mutable {
            auto self = conn.lock();
            MC_ASSERT(self, "连接已销毁");
            self->handle_connect(ec, promise);
        })
        .catch_error([promise](const std::exception& ec) mutable {
            promise.set_exception(std::make_exception_ptr(ec));
            return 0;
        });

    return promise.get_future();
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

void connection::handle_connect(const error_code& ec, promise<error_code>& promise) {
    if (ec) {
        m_state.store(connection_state::disconnected);
        if (m_transport) {
            m_transport->close();
        }
        promise.set_value(ec);
        return;
    }
    m_state.store(connection_state::authenticating);
    m_transport->set_connected(true);

    handle_authenticate(promise);
}

std::string encode_as_hex(int num) {
    std::ostringstream out;
    std::ostringstream numString;
    std::string        finalNumString;

    numString << num;

    finalNumString = numString.str();
    out << std::hex;

    for (const char& s : finalNumString) {
        out << std::hex << (int)s;
    }

    return out.str();
}

void connection::handle_authenticate(promise<error_code>& promise) {
    m_state.store(connection_state::connected);

    error_code ec;

    // // 1. 首先发送空字节
    // char null_byte = '\0';
    // m_transport->write_all(transport::buffer(&null_byte, 1), ec);
    // if (ec) {
    //     std::cout << "发送初始字节失败: " << ec.message() << std::endl;
    //     promise.set_value(ec);
    //     return;
    // }

    // // 2. 读取服务器响应 (应该是 "DATA" 或类似消息)
    // m_transport->read_line(m_read_stream.get_writeable_data(), ec);
    // if (ec) {
    //     std::cout << "读取初始响应失败: " << ec.message() << std::endl;
    //     promise.set_value(ec);
    //     return;
    // }
    // std::cout << "服务器初始响应: " << m_read_stream.get_data() << std::endl;
    // m_read_stream.skip(m_read_stream.get_data().length());

    // 3. 发送AUTH EXTERNAL 带用户ID的十六进制表示
    uid_t             uid = getuid();
    std::stringstream ss;
    ss << std::hex;
    std::string uid_str = std::to_string(uid);
    for (char c : uid_str) {
        ss << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
    std::string hex_uid = ss.str();

    std::string auth_command = "AUTH ANONYMOUS";
    m_transport->write_line(auth_command, ec);
    if (ec) {
        elog("发送AUTH EXTERNAL命令失败: ${error}", ("error", ec.message()));
        promise.set_value(ec);
        return;
    }

    // 4. 读取认证响应
    m_transport->read_line(m_read_stream.get_writeable_data(), ec);
    if (ec) {
        elog("读取认证响应失败: ${error}", ("error", ec.message()));
        promise.set_value(ec);
        return;
    }

    std::string_view response = m_read_stream.get_data();
    elog("认证响应: ${data}", ("data", response));
    m_read_stream.skip(response.length());

    // 5. 如果成功，发送BEGIN
    if (response.find("OK") == 0) {
        m_transport->write_line("BEGIN", ec);
        if (ec) {
            elog("发送BEGIN命令失败: ${error}", ("error", ec.message()));
            promise.set_value(ec);
            return;
        }
        elog("认证成功完成");
        return;
    } else {
        elog("认证失败，服务器响应: ${response}", ("response", response));
        ec = make_error_code(boost::system::errc::permission_denied);
        promise.set_value(ec);
        return;
    }

    // m_daemon_proxy = std::make_shared<daemon_proxy>(shared_from_this());
    // m_daemon_proxy->hello();

    // std::lock_guard<std::mutex> lock(m_mutex);
    // start_read();
}

void connection::start_read() {
    if (m_state.load() != connection_state::connected || !m_transport) {
        return;
    }

    auto                      data = m_read_stream.get_writeable_data();
    std::weak_ptr<connection> conn = shared_from_this();
    elog("开始读取数据: ${data}", ("data", data.length()));
    // m_transport->read(data, [conn](const auto& ec, std::size_t bytes_transferred) {
    //     if (auto self = conn.lock()) {
    //         elog("读取到数据: bytes_transferred=${bytes}, ec=${ec}",
    //              ("bytes", bytes_transferred)("ec", ec.message()));
    //         self->handle_read(ec, bytes_transferred);
    //     }
    // });
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

    elog("DBus连接读取成功，字节数: ${bytes}", ("bytes", bytes_transferred));
    while (m_read_stream.readable_bytes() > DBUS_MESSAGE_MIN_SIZE) {
        process_message();
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

    auto data = m_write_stream.peek(max_write_length);
    if (data.empty()) {
        return;
    }

    m_is_writing = true;

    std::weak_ptr<connection> conn = shared_from_this();
    // m_transport->write(data, [conn](const auto& ec, std::size_t bytes_transferred) {
    //     if (auto self = conn.lock()) {
    //         self->handle_write(ec, bytes_transferred);
    //     }
    // });
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
