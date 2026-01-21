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
#ifndef MC_LOG_DEFAULT_LOGGER
#define MC_LOG_DEFAULT_LOGGER "default"
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
    static logger get(const char* name = MC_LOG_DEFAULT_LOGGER);

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
     * @brief 检查指定日志类别是否为调试日志
     *
     * @param category 日志类别
     * @return bool 是否为调试日志
     */
    bool is_debug_log(log_category category) const;

    /**
     * @brief 设置系统ID
     *
     * @param system_id 系统ID，将在输出的日志中打印
     * @return logger& 日志记录器引用
     */
    logger& system(int system_id);

    /**
     * @brief 设置日志打印间隔
     *
     * @param period_s 日志打印间隔（秒），0表示不限制
     * @return logger& 日志记录器引用
     */
    logger& period(int period_s);

    /**
     * @brief 克隆一个独立的 logger 实例（深拷贝 impl），用于临时配置不影响原对象
     *
     * @return logger 独立的 logger 副本
     */
    logger clone() const;

    /**
     * @brief 按日志格式格式化消息后抛出异常
     *
     * @param fmt_template 格式模板字符串，支持 ${key} 占位符
     * @param args 参数字典，用于替换格式模板中的占位符
     * @throw runtime_exception 总是抛出运行时异常
     */
    void raise(const std::string& fmt_template, const mc::dict& args = mc::dict());

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
    /**
     * @brief 应用 system_id 到日志消息
     *
     * @param msg 原始日志消息
     * @return message 包含 system_id 的日志消息（如果设置了 system_id）
     */
    message apply_system_id(const message& msg) const;

    /**
     * @brief 应用 period 到日志消息（在日志末尾添加 [period: period_s(s)]）
     *
     * @param msg 原始日志消息
     * @return message 包含 period 信息的日志消息（如果设置了 period_s）
     */
    message apply_period(const message& msg) const;

    /**
     * @brief 检查是否应该打印日志（基于 period 时间间隔）
     *
     * @param msg 原始日志消息，用于区分不同日志的周期控制
     * @return bool 如果应该打印返回 true，否则返回 false
     */
    bool should_log_period(const message& msg);

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
 * @brief 记录日志
 */
#define MC_LOG_BASE_WITH_CATEGORY(LOGGER, CATEGORY, ...)                     \
    do {                                                                     \
        if (!LOGGER.is_debug_log(CATEGORY)) {                                \
            LOGGER.log(MC_LOG_MESSAGE_WITH_CATEGORY(CATEGORY, __VA_ARGS__)); \
        }                                                                    \
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