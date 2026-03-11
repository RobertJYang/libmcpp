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

#ifndef MC_PROTOCOL_HANDLER_H
#define MC_PROTOCOL_HANDLER_H

#include "base.h"
#include <string>
#include <vector>

namespace mc::protocol {

/**
 * @brief 协议处理器类
 */
class protocol_handler {
public:
    MC_REFLECTABLE("ProtocolHandler")

    protocol_handler()  = default;
    ~protocol_handler() = default;

    /**
     * @brief 设置协议版本
     */
    void set_version(protocol_version version)
    {
        m_version = version;
    }

    /**
     * @brief 获取协议版本
     */
    protocol_version get_version() const
    {
        return m_version;
    }

    /**
     * @brief 处理请求消息
     */
    std::string handle_request(const std::string& request)
    {
        m_last_request = request;
        return "Processed request: " + request;
    }

    /**
     * @brief 处理响应消息
     */
    bool handle_response(const std::string& response)
    {
        m_last_response = response;
        return !response.empty();
    }

    /**
     * @brief 发送通知消息
     */
    void send_notification(const std::string& message)
    {
        m_notifications.push_back(message);
    }

    /**
     * @brief 获取通知数量
     */
    size_t get_notification_count() const
    {
        return m_notifications.size();
    }

    /**
     * @brief 获取最后的请求
     */
    const std::string& get_last_request() const
    {
        return m_last_request;
    }

    /**
     * @brief 获取最后的响应
     */
    const std::string& get_last_response() const
    {
        return m_last_response;
    }

    /**
     * @brief 清空所有消息
     */
    void clear_messages()
    {
        m_last_request.clear();
        m_last_response.clear();
        m_notifications.clear();
    }

    /**
     * @brief 获取协议版本字符串
     */
    std::string get_version_string() const
    {
        switch (m_version) {
        case protocol_version::V1_0:
            return "1.0";
        case protocol_version::V1_1:
            return "1.1";
        case protocol_version::V2_0:
            return "2.0";
        default:
            return "unknown";
        }
    }

private:
    protocol_version         m_version{protocol_version::V1_0};
    std::string              m_last_request;
    std::string              m_last_response;
    std::vector<std::string> m_notifications;
};

} // namespace mc::protocol

#endif // MC_PROTOCOL_HANDLER_H