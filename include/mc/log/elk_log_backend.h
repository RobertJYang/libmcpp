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

#ifndef MC_LOG_ELK_LOG_BACKEND_H
#define MC_LOG_ELK_LOG_BACKEND_H

#include <mc/log/log_backend.h>
#include <mc/log/log_message.h>
#include <string>
#include <memory>

namespace mc {
namespace log {

/**
 * @brief ELK 日志后端配置
 */
struct elk_backend_config {
    std::string m_host;    // ELK 主机地址
    uint16_t    m_port;    // ELK 端口
    std::string m_index;   // ELK 索引名称
    
    elk_backend_config(std::string host = "localhost", uint16_t port = 9200, std::string index = "logs")
        : m_host(std::move(host)), m_port(port), m_index(std::move(index)) {}
};

/**
 * @brief ELK 日志后端
 * 
 * 将日志以结构化格式发送到 ELK 平台
 */
class elk_log_backend : public log_backend {
public:
    /**
     * @brief 构造函数
     * 
     * @param config ELK 配置
     */
    explicit elk_log_backend(elk_backend_config config = elk_backend_config())
        : m_config(std::move(config)) {}
    
    /**
     * @brief 处理日志消息
     * 
     * @param msg 日志消息
     */
    void process(const message& msg) override {
        // 如果消息级别低于配置的级别，则跳过
        if (msg.get_level() < m_level) {
            return;
        }
        
        // 创建结构化数据
        mc::mutable_dict elk_data;
        
        // 基本元数据
        auto time_t = std::chrono::system_clock::to_time_t(msg.get_timestamp());
        std::ostringstream time_ss;
        time_ss << std::put_time(std::localtime(&time_t), "%Y-%m-%dT%H:%M:%S.000Z");
        elk_data["@timestamp"] = time_ss.str();
        elk_data["level"] = to_string(msg.get_level());
        elk_data["logger"] = m_name;
        
        // 上下文信息
        mc::mutable_dict context_dict;
        context_dict["file"] = msg.get_context().m_file;
        context_dict["function"] = msg.get_context().m_function;
        context_dict["line"] = msg.get_context().m_line;
        elk_data["context"] = context_dict;
        
        // 消息内容
        if (!msg.get_format_template().empty()) {
            elk_data["message_template"] = msg.get_format_template();
            elk_data["message"] = msg.get_formatted_message();
            
            // 关键：保留原始参数
            mc::mutable_dict params;
            for (const auto& pair : msg.get_args()) {
                params[pair.first] = pair.second;
            }
            elk_data["params"] = params;
        } else {
            elk_data["message"] = msg.get_message();
        }
        
        // 发送到ELK
        send_to_elk(elk_data);
    }
    
    /**
     * @brief 设置 ELK 配置
     * 
     * @param config ELK 配置
     */
    void set_config(elk_backend_config config) {
        m_config = std::move(config);
    }
    
private:
    elk_backend_config m_config; // ELK 配置
    
    /**
     * @brief 发送数据到 ELK
     * 
     * @param data 结构化数据
     */
    void send_to_elk(const dict& data) {
        // 实际实现会连接到 ELK 并发送数据
        // 这里是一个简单的实现，实际应用中需要使用 HTTP 客户端
        
        // 将数据转换为 JSON 格式
        std::string json = dict_to_string(data);
        
        // TODO: 实现实际的 ELK 连接和数据发送
        // 例如使用 HTTP POST 请求发送到 Elasticsearch
        // 或者使用 filebeat/logstash 接口
    }
};

} // namespace log
} // namespace mc

#endif // MC_LOG_ELK_LOG_BACKEND_H 