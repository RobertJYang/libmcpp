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
#include <mc/variant.h>
#include <mc/string.h>
#include <chrono>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <utility>

namespace mc {
namespace log {

/**
 * @brief 将字典转换为字符串
 * 
 * @param d 字典
 * @return std::string 字符串表示
 */
std::string dict_to_string(const dict& d);

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
private:
    level                                  m_level;     // 日志级别
    mutable std::string                    m_message;   // 消息内容
    context                                m_context;   // 上下文信息
    std::chrono::system_clock::time_point  m_timestamp; // 时间戳
    dict                                   m_args;      // 参数字典
    std::string                            m_format;    // 格式模板
    mutable bool                           m_formatted; // 是否已格式化
    
public:
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
          m_args(std::move(args)),
          m_formatted(true) {}
    
    /**
     * @brief 格式化构造函数
     * 
     * @param lvl 日志级别
     * @param ctx 上下文信息
     * @param fmt_template 格式模板
     * @param args 参数字典
     */
    message(level lvl, 
            context ctx,
            std::string fmt_template,
            dict args)
        : m_level(lvl), 
          m_message(""),        // 初始为空，将在需要时格式化
          m_context(std::move(ctx)),
          m_timestamp(std::chrono::system_clock::now()),
          m_args(std::move(args)),
          m_format(std::move(fmt_template)),
          m_formatted(false) {} // 标记为未格式化
    
    /**
     * @brief 获取日志级别
     * 
     * @return level 日志级别
     */
    level get_level() const {
        return m_level;
    }
    
    /**
     * @brief 获取上下文信息
     * 
     * @return const context& 上下文信息
     */
    const context& get_context() const {
        return m_context;
    }
    
    /**
     * @brief 获取时间戳
     * 
     * @return const std::chrono::system_clock::time_point& 时间戳
     */
    const std::chrono::system_clock::time_point& get_timestamp() const {
        return m_timestamp;
    }
    
    /**
     * @brief 获取参数字典
     * 
     * @return const dict& 参数字典
     */
    const dict& get_args() const {
        return m_args;
    }
    
    /**
     * @brief 获取格式模板
     * 
     * @return const std::string& 格式模板
     */
    const std::string& get_format_template() const {
        return m_format;
    }
    
    /**
     * @brief 获取消息内容
     * 
     * @return std::string 消息内容
     */
    std::string get_message() const {
        if (!m_formatted && !m_format.empty()) {
            // 如果未格式化且有格式模板，执行格式化
            m_message = mc::format(m_format, m_args);
            m_formatted = true;
        }
        return m_message;
    }
    
    /**
     * @brief 获取格式化后的消息（保持兼容性）
     * 
     * @return std::string 格式化后的消息
     */
    std::string get_formatted_message() const {
        return get_message();
    }
    
    /**
     * @brief 获取结构化数据
     * 
     * @return dict 结构化数据
     */
    dict to_structured_data() const {
        mc::mutable_dict result;
        
        // 基本元数据
        result["level"] = static_cast<int>(m_level);
        
        // 上下文信息
        mc::mutable_dict context_dict;
        context_dict["file"] = m_context.m_file;
        context_dict["function"] = m_context.m_function;
        context_dict["line"] = m_context.m_line;
        result["context"] = context_dict;
        
        // 消息内容
        if (!m_format.empty()) {
            result["message_template"] = m_format;
            result["args"] = m_args;
        }
        
        // 获取消息（如果需要会自动格式化）
        result["message"] = get_message();
        
        // 直接返回，会自动调用转换操作符
        return result;
    }
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
 * @brief 结构化日志消息格式化器
 */
class structured_message_formatter : public message_formatter {
public:
    /**
     * @brief 格式化日志消息为结构化格式
     * 
     * @param msg 日志消息
     * @return std::string 结构化格式的日志消息
     */
    std::string format(const message& msg) const override;
};

} // namespace log
} // namespace mc

#endif // MC_LOG_MESSAGE_H 