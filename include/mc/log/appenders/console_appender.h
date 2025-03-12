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

#ifndef MC_LOG_CONSOLE_APPENDER_H
#define MC_LOG_CONSOLE_APPENDER_H

#include <mc/log/appenders/appender.h>
#include <iostream>
#include <mutex>

namespace mc {
namespace log {

/**
 * @brief 控制台日志追加器配置
 */
struct console_appender_config : public appender_config {
    bool m_use_color = true; // 是否使用颜色
    
    console_appender_config() : appender_config("console") {}
    
    console_appender_config(std::string name, level lvl = level::info, bool use_color = true)
        : appender_config(std::move(name), lvl), m_use_color(use_color) {}
};

/**
 * @brief 控制台日志追加器
 * 
 * 将日志消息输出到控制台
 */
class console_appender : public appender {
public:
    /**
     * @brief 构造函数
     * 
     * @param config 控制台追加器配置
     */
    explicit console_appender(console_appender_config config = console_appender_config())
        : appender(config), m_console_config(std::move(config)) {}
    
    /**
     * @brief 追加日志消息
     * 
     * @param msg 日志消息
     */
    void append(const message& msg) override {
        if (msg.get_level() < m_config.m_level) {
            return;
        }
        
        // 格式化消息
        auto formatted_msg = m_config.m_formatter->format(msg);
        if (formatted_msg.empty()) {
            formatted_msg = msg.get_formatted_message();
        }
        
        if (m_console_config.m_use_color) {
            formatted_msg = colorize(msg.get_level(), formatted_msg);
        }
        
        std::lock_guard<std::mutex> lock(m_mutex);
        std::cout << formatted_msg << std::endl;
    }
    
    /**
     * @brief 设置是否使用颜色
     * 
     * @param use_color 是否使用颜色
     */
    void set_use_color(bool use_color) {
        m_console_config.m_use_color = use_color;
    }
    
    /**
     * @brief 获取是否使用颜色
     * 
     * @return bool 是否使用颜色
     */
    bool get_use_color() const {
        return m_console_config.m_use_color;
    }
    
private:
    /**
     * @brief 为日志消息添加颜色
     * 
     * @param lvl 日志级别
     * @param msg 日志消息
     * @return std::string 添加颜色后的日志消息
     */
    std::string colorize(level lvl, const std::string& msg) const {
        // ANSI 颜色代码
        const char* reset = "\033[0m";
        const char* color = nullptr;
        
        switch (lvl) {
            case level::trace: color = "\033[90m"; break; // 灰色
            case level::debug: color = "\033[36m"; break; // 青色
            case level::info:  color = "\033[32m"; break; // 绿色
            case level::warn:  color = "\033[33m"; break; // 黄色
            case level::error: color = "\033[31m"; break; // 红色
            case level::fatal: color = "\033[35m"; break; // 紫色
            default:           color = "\033[0m";  break; // 默认
        }
        
        return std::string(color) + msg + std::string(reset);
    }
    
    console_appender_config m_console_config; // 控制台追加器配置
    std::mutex m_mutex; // 互斥锁，保证线程安全
};

} // namespace log
} // namespace mc

#endif // MC_LOG_CONSOLE_APPENDER_H 