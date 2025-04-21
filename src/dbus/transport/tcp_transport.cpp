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

#include <mc/dbus/message.h>
#include <mc/dbus/tcp_transport.h>
#include <mc/log.h>
#include <regex>

namespace mc {
namespace dbus {

// 构造函数
tcp_transport::tcp_transport(io_context_type& io_context)
    : transport(io_context), m_socket(io_context), m_resolver(io_context), m_is_connected(false) {
    m_read_buffer.resize(4096); // 初始化读取缓冲区大小
}

// 析构函数
tcp_transport::~tcp_transport() {
    close();
}

// 连接到DBus服务器
void tcp_transport::connect(const std::string& address, connect_handler handler) {
    if (m_is_connected) {
        if (handler) {
            boost::system::error_code ec(boost::asio::error::already_connected);
            handler(ec);
        }
        return;
    }

    try {
        // 解析地址字符串
        auto [host, port] = parse_address(address);
        if (host.empty() || port == 0) {
            if (handler) {
                boost::system::error_code ec(boost::asio::error::invalid_argument);
                handler(ec);
            }
            return;
        }

        // 解析主机名并连接
        resolve_host(host, port, handler);
    } catch (const std::exception& e) {
        elog("TCP连接异常: ${error}", ("error", e.what()));
        if (handler) {
            boost::system::error_code ec(boost::asio::error::invalid_argument);
            handler(ec);
        }
    }
}

// 关闭连接
void tcp_transport::close() {
    if (!m_is_connected) {
        return;
    }

    boost::system::error_code ec;
    m_socket.close(ec);
    m_is_connected = false;

    if (ec) {
        elog("TCP套接字关闭错误: ${error}", ("error", ec.message()));
    }
}

// 发送消息
void tcp_transport::write(const std::shared_ptr<message>& message, write_handler handler) {
    if (!m_is_connected) {
        if (handler) {
            boost::system::error_code ec(boost::asio::error::not_connected);
            handler(ec, 0);
        }
        return;
    }

    try {
        // 序列化消息
        auto data = message->serialize();
        if (data.empty()) {
            if (handler) {
                boost::system::error_code ec(boost::asio::error::invalid_argument);
                handler(ec, 0);
            }
            return;
        }

        // 发送消息数据
        boost::asio::async_write(
            m_socket, boost::asio::buffer(data),
            [handler](const boost::system::error_code& ec, std::size_t bytes_transferred) {
                if (handler) {
                    handler(ec, bytes_transferred);
                }
            });
    } catch (const std::exception& e) {
        elog("TCP发送异常: ${error}", ("error", e.what()));
        if (handler) {
            boost::system::error_code ec(boost::asio::error::invalid_argument);
            handler(ec, 0);
        }
    }
}

// 读取消息
void tcp_transport::read(read_handler handler) {
    if (!m_is_connected) {
        if (handler) {
            boost::system::error_code ec(boost::asio::error::not_connected);
            handler(ec, std::vector<uint8_t>());
        }
        return;
    }

    // 异步读取数据
    m_socket.async_read_some(
        boost::asio::buffer(m_read_buffer),
        [this, handler](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            if (ec) {
                if (handler) {
                    handler(ec, std::vector<uint8_t>());
                }
                return;
            }

            // 返回读取到的数据
            std::vector<uint8_t> data(m_read_buffer.begin(),
                                      m_read_buffer.begin() + bytes_transferred);
            if (handler) {
                handler(ec, std::move(data));
            }
        });
}

// 解析TCP地址
std::pair<std::string, uint16_t> tcp_transport::parse_address(const std::string& address) {
    // 格式: tcp:host=hostname,port=1234
    if (address.find("tcp:") != 0) {
        return {"", 0};
    }

    std::string host;
    uint16_t    port = 0;

    // 查找主机名
    size_t host_pos = address.find("host=");
    if (host_pos != std::string::npos) {
        size_t host_end = address.find(",", host_pos);
        if (host_end == std::string::npos) {
            host_end = address.length();
        }
        host = address.substr(host_pos + 5, host_end - host_pos - 5);
    }

    // 查找端口号
    size_t port_pos = address.find("port=");
    if (port_pos != std::string::npos) {
        size_t port_end = address.find(",", port_pos);
        if (port_end == std::string::npos) {
            port_end = address.length();
        }
        std::string port_str = address.substr(port_pos + 5, port_end - port_pos - 5);
        try {
            port = static_cast<uint16_t>(std::stoi(port_str));
        } catch (...) {
            port = 0;
        }
    }

    return {host, port};
}

// 解析主机名
void tcp_transport::resolve_host(const std::string& host, uint16_t port, connect_handler handler) {
    // 将主机名解析为IP地址
    m_resolver.async_resolve(host, std::to_string(port),
                             [this, handler](const boost::system::error_code&             ec,
                                             boost::asio::ip::tcp::resolver::results_type results) {
                                 if (ec) {
                                     if (handler) {
                                         handler(ec);
                                     }
                                     return;
                                 }

                                 // 连接到解析出的第一个地址
                                 boost::asio::async_connect(
                                     m_socket, results,
                                     [this, handler](const boost::system::error_code& ec,
                                                     const boost::asio::ip::tcp::endpoint&) {
                                         if (!ec) {
                                             m_is_connected = true;
                                         }

                                         if (handler) {
                                             handler(ec);
                                         }
                                     });
                             });
}

} // namespace dbus
} // namespace mc