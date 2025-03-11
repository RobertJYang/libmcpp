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

#ifndef MC_LOG_LOGGER_H
#define MC_LOG_LOGGER_H

#include <mc/log/log_message.h>
#include <mc/log/appender.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace mc {
namespace log {

// 默认日志记录器名称
#ifndef DEFAULT_LOGGER
#define DEFAULT_LOGGER "default"
#endif

/**
 * @brief 日志记录器配置
 */
struct logger_config {
    std::string m_name;                // 日志记录器名称
    level       m_level = level::info; // 日志级别
    
    logger_config() = default;
    
    logger_config(std::string name, level lvl = level::info)
        : m_name(std::move(name)), m_level(lvl) {}
};

/**
 * @brief 日志记录器
 * 
 * 日志记录器负责收集日志消息并将其分发到各个追加器
 */
class logger {
public:
    /**
     * @brief 获取指定名称的日志记录器
     * 
     * @param name 日志记录器名称
     * @return logger 日志记录器
     */
    static logger get(const std::string& name = DEFAULT_LOGGER);
    
    /**
     * @brief 更新指定名称的日志记录器
     * 
     * @param name 日志记录器名称
     * @param log 日志记录器
     */
    static void update(const std::string& name, logger& log);
    
    /**
     * @brief 默认构造函数
     */
    logger();
    
    /**
     * @brief 构造函数
     * 
     * @param name 日志记录器名称
     * @param parent 父日志记录器
     */
    logger(const std::string& name, const logger& parent = logger());
    
    /**
     * @brief 空指针构造函数
     */
    logger(std::nullptr_t);
    
    /**
     * @brief 复制构造函数
     * 
     * @param other 其他日志记录器
     */
    logger(const logger& other);
    
    /**
     * @brief 移动构造函数
     * 
     * @param other 其他日志记录器
     */
    logger(logger&& other) noexcept;
    
    /**
     * @brief 复制赋值运算符
     * 
     * @param other 其他日志记录器
     * @return logger& 日志记录器引用
     */
    logger& operator=(const logger& other);
    
    /**
     * @brief 移动赋值运算符
     * 
     * @param other 其他日志记录器
     * @return logger& 日志记录器引用
     */
    logger& operator=(logger&& other) noexcept;
    
    /**
     * @brief 设置日志级别
     * 
     * @param lvl 日志级别
     * @return logger& 日志记录器引用
     */
    logger& set_level(level lvl);
    
    /**
     * @brief 获取日志级别
     * 
     * @return level 日志级别
     */
    level get_level() const;
    
    /**
     * @brief 获取父日志记录器
     * 
     * @return logger 父日志记录器
     */
    logger get_parent() const;
    
    /**
     * @brief 设置日志记录器名称
     * 
     * @param name 日志记录器名称
     */
    void set_name(const std::string& name);
    
    /**
     * @brief 检查指定日志级别是否启用
     * 
     * @param lvl 日志级别
     * @return bool 是否启用
     */
    bool is_enabled(level lvl) const;
    
    /**
     * @brief 记录日志消息
     * 
     * @param msg 日志消息
     */
    void log(message msg);
    
    /**
     * @brief 添加日志追加器
     * 
     * @param a 日志追加器
     */
    void add_appender(const std::shared_ptr<appender>& a);
    
    /**
     * @brief 移除日志追加器
     * 
     * @param name 日志追加器名称
     */
    void remove_appender(const std::string& name);
    
    /**
     * @brief 获取日志追加器
     * 
     * @param name 日志追加器名称
     * @return std::shared_ptr<appender> 日志追加器
     */
    std::shared_ptr<appender> get_appender(const std::string& name) const;
    
    /**
     * @brief 获取所有日志追加器
     * 
     * @return std::vector<std::shared_ptr<appender>> 日志追加器列表
     */
    std::vector<std::shared_ptr<appender>> get_appenders() const;
    
private:
    class impl;
    std::shared_ptr<impl> m_impl; // 实现细节
};

/**
 * @brief 日志宏 - 跟踪级别
 */
#define MC_LOG_TRACE(LOGGER, ...) \
    do { \
        if (LOGGER.is_enabled(mc::log::level::trace)) { \
            mc::log::message_stream stream(mc::log::level::trace, \
                mc::log::context(__FILE__, __FUNCTION__, __LINE__)); \
            stream << __VA_ARGS__; \
            LOGGER.log(stream.get_message()); \
        } \
    } while (0)

/**
 * @brief 日志宏 - 调试级别
 */
#define MC_LOG_DEBUG(LOGGER, ...) \
    do { \
        if (LOGGER.is_enabled(mc::log::level::debug)) { \
            mc::log::message_stream stream(mc::log::level::debug, \
                mc::log::context(__FILE__, __FUNCTION__, __LINE__)); \
            stream << __VA_ARGS__; \
            LOGGER.log(stream.get_message()); \
        } \
    } while (0)

/**
 * @brief 日志宏 - 信息级别
 */
#define MC_LOG_INFO(LOGGER, ...) \
    do { \
        if (LOGGER.is_enabled(mc::log::level::info)) { \
            mc::log::message_stream stream(mc::log::level::info, \
                mc::log::context(__FILE__, __FUNCTION__, __LINE__)); \
            stream << __VA_ARGS__; \
            LOGGER.log(stream.get_message()); \
        } \
    } while (0)

/**
 * @brief 日志宏 - 警告级别
 */
#define MC_LOG_WARN(LOGGER, ...) \
    do { \
        if (LOGGER.is_enabled(mc::log::level::warn)) { \
            mc::log::message_stream stream(mc::log::level::warn, \
                mc::log::context(__FILE__, __FUNCTION__, __LINE__)); \
            stream << __VA_ARGS__; \
            LOGGER.log(stream.get_message()); \
        } \
    } while (0)

/**
 * @brief 日志宏 - 错误级别
 */
#define MC_LOG_ERROR(LOGGER, ...) \
    do { \
        if (LOGGER.is_enabled(mc::log::level::error)) { \
            mc::log::message_stream stream(mc::log::level::error, \
                mc::log::context(__FILE__, __FUNCTION__, __LINE__)); \
            stream << __VA_ARGS__; \
            LOGGER.log(stream.get_message()); \
        } \
    } while (0)

/**
 * @brief 日志宏 - 致命错误级别
 */
#define MC_LOG_FATAL(LOGGER, ...) \
    do { \
        if (LOGGER.is_enabled(mc::log::level::fatal)) { \
            mc::log::message_stream stream(mc::log::level::fatal, \
                mc::log::context(__FILE__, __FUNCTION__, __LINE__)); \
            stream << __VA_ARGS__; \
            LOGGER.log(stream.get_message()); \
        } \
    } while (0)

/**
 * @brief 结构化日志宏 - 跟踪级别
 */
#define MC_WLOG_TRACE(LOGGER, FORMAT, ...) \
    do { \
        if (LOGGER.is_enabled(mc::log::level::trace)) { \
            mc::log::message_stream stream(mc::log::level::trace, \
                mc::log::context(__FILE__, __FUNCTION__, __LINE__), FORMAT); \
            __VA_ARGS__; \
            LOGGER.log(stream.get_message()); \
        } \
    } while (0)

/**
 * @brief 结构化日志宏 - 调试级别
 */
#define MC_WLOG_DEBUG(LOGGER, FORMAT, ...) \
    do { \
        if (LOGGER.is_enabled(mc::log::level::debug)) { \
            mc::log::message_stream stream(mc::log::level::debug, \
                mc::log::context(__FILE__, __FUNCTION__, __LINE__), FORMAT); \
            __VA_ARGS__; \
            LOGGER.log(stream.get_message()); \
        } \
    } while (0)

/**
 * @brief 结构化日志宏 - 信息级别
 */
#define MC_WLOG_INFO(LOGGER, FORMAT, ...) \
    do { \
        if (LOGGER.is_enabled(mc::log::level::info)) { \
            mc::log::message_stream stream(mc::log::level::info, \
                mc::log::context(__FILE__, __FUNCTION__, __LINE__), FORMAT); \
            __VA_ARGS__; \
            LOGGER.log(stream.get_message()); \
        } \
    } while (0)

/**
 * @brief 结构化日志宏 - 警告级别
 */
#define MC_WLOG_WARN(LOGGER, FORMAT, ...) \
    do { \
        if (LOGGER.is_enabled(mc::log::level::warn)) { \
            mc::log::message_stream stream(mc::log::level::warn, \
                mc::log::context(__FILE__, __FUNCTION__, __LINE__), FORMAT); \
            __VA_ARGS__; \
            LOGGER.log(stream.get_message()); \
        } \
    } while (0)

/**
 * @brief 结构化日志宏 - 错误级别
 */
#define MC_WLOG_ERROR(LOGGER, FORMAT, ...) \
    do { \
        if (LOGGER.is_enabled(mc::log::level::error)) { \
            mc::log::message_stream stream(mc::log::level::error, \
                mc::log::context(__FILE__, __FUNCTION__, __LINE__), FORMAT); \
            __VA_ARGS__; \
            LOGGER.log(stream.get_message()); \
        } \
    } while (0)

/**
 * @brief 结构化日志宏 - 致命级别
 */
#define MC_WLOG_FATAL(LOGGER, FORMAT, ...) \
    do { \
        if (LOGGER.is_enabled(mc::log::level::fatal)) { \
            mc::log::message_stream stream(mc::log::level::fatal, \
                mc::log::context(__FILE__, __FUNCTION__, __LINE__), FORMAT); \
            __VA_ARGS__; \
            LOGGER.log(stream.get_message()); \
        } \
    } while (0)

} // namespace log
} // namespace mc

#endif // MC_LOG_LOGGER_H 