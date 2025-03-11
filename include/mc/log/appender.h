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

#ifndef MC_LOG_APPENDER_H
#define MC_LOG_APPENDER_H

#include <mc/log/log_message.h>
#include <memory>
#include <string>

namespace mc {
namespace log {

/**
 * @brief 日志追加器配置
 */
struct appender_config {
    std::string m_name;                                // 追加器名称
    level       m_level = level::info;                // 日志级别
    std::shared_ptr<message_formatter> m_formatter;   // 消息格式化器
    
    appender_config() : m_formatter(std::make_shared<default_message_formatter>()) {}
    
    appender_config(std::string name, level lvl = level::info)
        : m_name(std::move(name)), 
          m_level(lvl),
          m_formatter(std::make_shared<default_message_formatter>()) {}
};

/**
 * @brief 日志追加器接口
 * 
 * 日志追加器负责将日志消息输出到特定目标（控制台、文件等）
 */
class appender {
public:
    /**
     * @brief 构造函数
     * 
     * @param config 追加器配置
     */
    explicit appender(appender_config config = appender_config())
        : m_config(std::move(config)) {}
    
    /**
     * @brief 虚析构函数
     */
    virtual ~appender() = default;
    
    /**
     * @brief 追加日志消息
     * 
     * @param msg 日志消息
     */
    virtual void append(const message& msg) = 0;
    
    /**
     * @brief 获取追加器名称
     * 
     * @return const std::string& 追加器名称
     */
    const std::string& name() const { return m_config.m_name; }
    
    /**
     * @brief 获取日志级别
     * 
     * @return level 日志级别
     */
    level get_level() const { return m_config.m_level; }
    
    /**
     * @brief 设置日志级别
     * 
     * @param lvl 日志级别
     */
    void set_level(level lvl) { m_config.m_level = lvl; }
    
    /**
     * @brief 获取消息格式化器
     * 
     * @return std::shared_ptr<message_formatter> 消息格式化器
     */
    std::shared_ptr<message_formatter> get_formatter() const { return m_config.m_formatter; }
    
    /**
     * @brief 设置消息格式化器
     * 
     * @param formatter 消息格式化器
     */
    void set_formatter(std::shared_ptr<message_formatter> formatter) { 
        m_config.m_formatter = std::move(formatter); 
    }
    
protected:
    appender_config m_config; // 追加器配置
};

} // namespace log
} // namespace mc

#endif // MC_LOG_APPENDER_H 