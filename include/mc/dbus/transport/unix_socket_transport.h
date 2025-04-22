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

#ifndef MC_DBUS_UNIX_SOCKET_TRANSPORT_H
#define MC_DBUS_UNIX_SOCKET_TRANSPORT_H

#include <mc/dbus/transport/transport.h>

namespace mc::dbus {

/**
 * @brief Unix套接字传输层实现
 *
 * 通过Unix域套接字实现DBus通信。
 */
class unix_socket_transport : public transport {
public:
    /**
     * @brief 构造函数
     * @param io_context IO上下文
     * @param entry 地址条目
     */
    explicit unix_socket_transport(io_context_type& io_context, address_entry_ptr entry);

    /**
     * @brief 析构函数
     */
    ~unix_socket_transport() override;

    /**
     * @brief 连接到DBus服务器
     * @param address Unix套接字路径
     */
    mc::future<error_code> connect() override;

    /**
     * @brief 关闭连接
     */
    void close() override;

    std::size_t write_some(buffer buf, error_code& ec) override;
    std::size_t read_some(buffer buf, error_code& ec) override;

    /**
     * @brief 发送消息
     * @param buf 要发送的消息
     */
    mc::future<std::pair<error_code, std::size_t>> async_write(buffer buf) override;

    /**
     * @brief 读取消息
     * @param buf 读取缓冲区
     */
    mc::future<std::pair<error_code, std::size_t>> async_read(buffer buf) override;

private:
    using socket_type = boost::asio::local::stream_protocol::socket;
    bool verify_connection();

    socket_type m_socket; ///< Unix域套接字
};

} // namespace mc::dbus

#endif // MC_DBUS_UNIX_SOCKET_TRANSPORT_H