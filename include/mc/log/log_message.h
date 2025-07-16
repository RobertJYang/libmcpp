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

#include <chrono>
#include <mc/dict.h>
#include <mc/log/log_level.h>
#include <mc/string.h>
#include <mc/variant.h>

#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace mc::log {

/**
 * @brief 日志上下文信息
 */
struct context {
    std::string m_file;     // 文件名
    std::string m_function; // 函数名
    uint32_t    m_line;     // 行号

    context(std::string file = "", std::string function = "", uint32_t line = 0)
        : m_file(std::move(file)), m_function(std::move(function)), m_line(line) {
    }
};

/**
 * @brief 日志消息结构
 */
class MC_API message {
public:
    /**
     * @brief 构造函数
     *
     * @param lvl 日志级别
     * @param msg 日志消息
     * @param ctx 上下文信息
     * @param args 参数字典
     */
    MC_API message(level lvl = level::info, std::string msg = "", context ctx = context(),
                   mc::mutable_dict args = mc::mutable_dict());

    /**
     * @brief 格式化构造函数
     *
     * @param lvl 日志级别
     * @param ctx 上下文信息
     * @param fmt_template 格式模板
     * @param args 参数字典
     */
    MC_API message(level lvl, context ctx, std::string fmt_template,
                   mc::mutable_dict args = mc::mutable_dict());

    message(const message& other)            = default;
    message& operator=(const message& other) = default;

    message(message&& other) noexcept            = default;
    message& operator=(message&& other) noexcept = default;

    /**
     * @brief 获取日志级别
     *
     * @return level 日志级别
     */
    MC_API level get_level() const;

    /**
     * @brief 获取上下文信息
     *
     * @return const context& 上下文信息
     */
    MC_API const context& get_context() const;

    /**
     * @brief 获取时间戳
     *
     * @return const std::chrono::system_clock::time_point& 时间戳
     */
    MC_API const std::chrono::system_clock::time_point& get_timestamp() const;

    /**
     * @brief 获取参数字典
     *
     * @return const dict& 参数字典
     */
    MC_API const dict& get_args() const;

    /**
     * @brief 获取格式模板
     *
     * @return const std::string& 格式模板
     */
    MC_API const std::string& get_format_template() const;

    /**
     * @brief 获取线程ID
     *
     * @return const std::thread::id& 线程ID
     */
    MC_API mc::thread_id get_thread_id() const;

    /**
     * @brief 获取消息内容
     *
     * @return std::string 消息内容
     */
    MC_API const std::string& get_message() const;

    /**
     * @brief 获取结构化数据
     *
     * @return dict 结构化数据
     */
    MC_API dict to_structured_data() const;

private:
    level                                 m_level;     // 日志级别
    mutable std::string                   m_message;   // 消息内容
    context                               m_context;   // 上下文信息
    std::chrono::system_clock::time_point m_timestamp; // 时间戳
    dict                                  m_args;      // 参数字典
    std::string                           m_format;    // 格式模板
    mc::thread_id                         m_thread_id; // 线程ID
    mutable bool                          m_formatted; // 是否已格式化
};

/**
 * @brief 日志消息列表
 */
using messages = std::vector<message>;

} // namespace mc::log

/**
 * @brief 基础日志宏 - 所有级别共用
 */
#define MC_LOG_MESSAGE(LEVEL, FORMAT, ...)                                                      \
    mc::log::message(mc::log::level::LEVEL, mc::log::context(__FILE__, __FUNCTION__, __LINE__), \
                     FORMAT, mc::mutable_dict() __VA_ARGS__)

#endif // MC_LOG_MESSAGE_H