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

#ifndef MC_NETWORK_CLIENT_H
#define MC_NETWORK_CLIENT_H

#include "base.h"
#include <string>

namespace mc::network {

/**
 * @brief 网络客户端类
 */
class network_client {
public:
    MC_REFLECTABLE("NetworkClient")

    network_client()  = default;
    ~network_client() = default;

    /**
     * @brief 连接到服务器
     */
    bool connect(const std::string& host, int port)
    {
        m_host   = host;
        m_port   = port;
        m_status = connection_status::CONNECTING;

        // 模拟连接过程
        if (port > 0 && port < 65536) {
            m_status = connection_status::CONNECTED;
            return true;
        }

        m_status = connection_status::ERROR;
        return false;
    }

    /**
     * @brief 断开连接
     */
    void disconnect()
    {
        m_status = connection_status::DISCONNECTED;
        m_host.clear();
        m_port = 0;
    }

    /**
     * @brief 发送数据
     */
    bool send_data(const std::string& data)
    {
        if (m_status != connection_status::CONNECTED) {
            return false;
        }
        m_last_sent = data;
        return true;
    }

    /**
     * @brief 接收数据
     */
    std::string receive_data()
    {
        if (m_status != connection_status::CONNECTED) {
            return "";
        }
        // 模拟回显
        return "Echo: " + m_last_sent;
    }

    /**
     * @brief 获取连接状态
     */
    connection_status get_status() const
    {
        return m_status;
    }

    /**
     * @brief 获取主机地址
     */
    const std::string& get_host() const
    {
        return m_host;
    }

    /**
     * @brief 获取端口
     */
    int get_port() const
    {
        return m_port;
    }

    /**
     * @brief 设置协议类型
     */
    void set_protocol(protocol_type type)
    {
        m_protocol = type;
    }

    /**
     * @brief 获取协议类型
     */
    protocol_type get_protocol() const
    {
        return m_protocol;
    }

private:
    std::string       m_host;
    int               m_port{0};
    connection_status m_status{connection_status::DISCONNECTED};
    protocol_type     m_protocol{protocol_type::TCP};
    std::string       m_last_sent;
};

} // namespace mc::network

#endif // MC_NETWORK_CLIENT_H