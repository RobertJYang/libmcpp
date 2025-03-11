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

#ifndef MC_LOG_MESSAGE_H
#define MC_LOG_MESSAGE_H

#include <mc/log/log_level.h>
#include <mc/dict.h>
#include <chrono>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <utility>

namespace mc {
namespace log {

/**
 * @brief 日志上下文信息
 */
struct context {
    std::string m_file;       // 文件名
    std::string m_function;   // 函数名
    uint32_t    m_line;       // 行号
    
    context(std::string file = "", std::string function = "", uint32_t line = 0)
        : m_file(std::move(file)), m_function(std::move(function)), m_line(line) {}
};

/**
 * @brief 日志消息结构
 */
class message {
public:
    level                                  m_level;     // 日志级别
    std::string                            m_message;   // 日志消息
    context                                m_context;   // 上下文信息
    std::chrono::system_clock::time_point  m_timestamp; // 时间戳
    dict                                   m_args;      // 参数字典
    
    /**
     * @brief 构造函数
     * 
     * @param lvl 日志级别
     * @param msg 日志消息
     * @param ctx 上下文信息
     * @param args 参数字典
     */
    message(level lvl = level::info, 
            std::string msg = "", 
            context ctx = context(),
            dict args = dict())
        : m_level(lvl), 
          m_message(std::move(msg)), 
          m_context(std::move(ctx)),
          m_timestamp(std::chrono::system_clock::now()),
          m_args(std::move(args)) {}
    
    /**
     * @brief 获取格式化的日志消息
     * 
     * @return std::string 格式化后的日志消息
     */
    std::string formatted_message() const;
};

/**
 * @brief 日志消息列表
 */
using messages = std::vector<message>;

/**
 * @brief 日志消息格式化器
 */
class message_formatter {
public:
    /**
     * @brief 格式化日志消息
     * 
     * @param msg 日志消息
     * @return std::string 格式化后的日志消息
     */
    virtual std::string format(const message& msg) const = 0;
    
    /**
     * @brief 虚析构函数
     */
    virtual ~message_formatter() = default;
};

/**
 * @brief 默认日志消息格式化器
 */
class default_message_formatter : public message_formatter {
public:
    /**
     * @brief 格式化日志消息
     * 
     * @param msg 日志消息
     * @return std::string 格式化后的日志消息
     */
    std::string format(const message& msg) const override;
};

/**
 * @brief 日志消息流
 * 
 * 用于构建结构化日志消息
 */
class message_stream {
public:
    /**
     * @brief 构造函数
     * 
     * @param lvl 日志级别
     * @param ctx 上下文信息
     * @param format 格式化字符串
     */
    message_stream(level lvl, context ctx = context(), std::string format = "")
        : m_level(lvl), m_context(std::move(ctx)), m_format(std::move(format)) {}
    
    /**
     * @brief 获取日志消息
     * 
     * @return message 日志消息
     */
    message get_message() {
        // 替换格式字符串中的占位符
        std::string result = m_format;
        for (const auto& pair : m_args) {
            std::string placeholder = "${" + pair.first + "}";
            size_t pos = result.find(placeholder);
            if (pos != std::string::npos) {
                result.replace(pos, placeholder.length(), pair.second.as_string());
            }
        }
        
        return message(m_level, result, m_context, m_args);
    }
    
    /**
     * @brief 添加参数
     * 
     * @param key 参数名
     * @param value 参数值
     * @return message_stream& 日志消息流
     */
    template<typename T>
    message_stream& operator()(const char* key, T&& value) {
        // 创建一个可变的dict
        mc::mutable_dict mutable_args(m_args);
        // 添加或更新键值对
        mutable_args(key, std::forward<T>(value));
        // 将可变dict转换回不可变dict
        m_args = mutable_args;
        return *this;
    }
    
private:
    level       m_level;    // 日志级别
    context     m_context;  // 上下文信息
    std::string m_format;   // 格式化字符串
    dict        m_args;     // 参数字典
};

} // namespace log
} // namespace mc

#endif // MC_LOG_MESSAGE_H 