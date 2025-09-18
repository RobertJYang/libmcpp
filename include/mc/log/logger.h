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

#include <mc/log/appender.h>
#include <mc/log/log_message.h>
#include <mc/reflect.h>

MC_REFLECTABLE("mc.log.level", mc::log::level)

namespace mc {
namespace log {

// 默认日志记录器名称
#ifndef DEFAULT_LOGGER
#define DEFAULT_LOGGER "default"
#endif

/**
 * @brief 日志记录器
 *
 * 日志记录器负责收集日志消息并将其分发到各个追加器
 */
class MC_API logger {
public:
    /**
     * @brief 获取指定名称的日志记录器
     *
     * @param name 日志记录器名称
     * @return logger 日志记录器
     */
    static logger get(const char* name = DEFAULT_LOGGER);

    logger();

    /**
     * @brief 构造函数
     *
     * @param name 日志记录器名称
     */
    logger(const std::string& name);

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
     * @brief 设置日志记录器名称
     *
     * @param name 日志记录器名称
     */
    void set_name(const std::string& name);

    /**
     * @brief 获取日志记录器名称
     *
     * @return const std::string& 日志记录器名称
     */
    const std::string& get_name() const;

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
    void add_appender(const appender_ptr& a);

    /**
     * @brief 删除指定名称的日志追加器
     *
     * @param name 日志追加器名称
     * @return bool 是否成功删除
     */
    bool remove_appender(const std::string& name);

    /**
     * @brief 查找指定名称的日志追加器
     *
     * @param name 日志追加器名称
     * @return appender_ptr 日志追加器指针，如果未找到则返回nullptr
     */
    appender_ptr find_appender(const std::string& name) const;

    /**
     * @brief 获取所有日志追加器
     *
     * @return std::vector<appender_ptr> 日志追加器列表
     */
    const std::vector<appender_ptr>& get_appenders() const;

    /**
     * @brief 清除所有日志追加器
     */
    void clear_appenders();

private:
    class impl;
    std::shared_ptr<impl> m_impl; // 实现细节
};

/**
 * @brief 记录日志
 */
#define MC_LOG_BASE(LOGGER, LEVEL, ...)                     \
    do {                                                    \
        if (LOGGER.is_enabled(mc::log::level::LEVEL)) {     \
            LOGGER.log(MC_LOG_MESSAGE(LEVEL, __VA_ARGS__)); \
        }                                                   \
    } while (0)

/**
 * @brief 不安全的记录日志（编译期不检查格式化字符串和格式化参数）
 */
#define MC_LOG_BASE_UNSAFE(LOGGER, LEVEL, ...)                     \
    do {                                                           \
        if (LOGGER.is_enabled(mc::log::level::LEVEL)) {            \
            LOGGER.log(MC_LOG_MESSAGE_UNSAFE(LEVEL, __VA_ARGS__)); \
        }                                                          \
    } while (0)

} // namespace log
} // namespace mc

#endif // MC_LOG_LOGGER_H