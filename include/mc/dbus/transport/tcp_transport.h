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

#ifndef MC_DBUS_TCP_TRANSPORT_H
#define MC_DBUS_TCP_TRANSPORT_H

#include <boost/asio.hpp>
#include <memory>
#include <string>

#include <mc/dbus/transport.h>

namespace mc {
namespace dbus {

/**
 * @brief TCP传输层实现
 *
 * 通过TCP连接实现DBus通信。
 */
class tcp_transport : public transport {
public:
    /**
     * @brief 构造函数
     * @param io_context IO上下文
     */
    explicit tcp_transport(io_context_type& io_context);

    /**
     * @brief 析构函数
     */
    ~tcp_transport() override;

    /**
     * @brief 连接到DBus服务器
     * @param address TCP地址 (格式: "host:port")
     * @param handler 连接完成回调
     */
    void connect(const address_entry& entry, connect_handler handler) override;

    /**
     * @brief 关闭连接
     */
    void close() override;

    /**
     * @brief 发送消息
     * @param message 要发送的消息
     * @param handler 发送完成回调
     */
    void write(std::string_view message, write_handler handler) override;

    /**
     * @brief 读取消息
     * @param handler 读取完成回调
     */
    void read(read_handler handler) override;

private:
    /**
     * @brief 解析TCP地址
     * @param address 地址字符串 (格式: "host:port")
     * @return 主机名和端口号的对
     */
    std::pair<std::string, uint16_t> parse_address(const std::string& address);

    /**
     * @brief 解析主机名
     * @param host 主机名或IP地址
     * @param port 端口号
     * @param handler 解析完成回调
     */
    void resolve_host(const std::string& host, uint16_t port, connect_handler handler);

    boost::asio::ip::tcp::socket   m_socket;       ///< TCP套接字
    boost::asio::ip::tcp::resolver m_resolver;     ///< 主机名解析器
    std::vector<uint8_t>           m_read_buffer;  ///< 读取缓冲区
    bool                           m_is_connected; ///< 连接状态标志
};

} // namespace dbus
} // namespace mc

#endif // MC_DBUS_TCP_TRANSPORT_H