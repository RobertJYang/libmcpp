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

#ifndef MC_LOG_MANAGER_H
#define MC_LOG_MANAGER_H

#include <mc/log/appender.h>
#include <mc/log/appender_factory.h>
#include <mc/log/logger.h>
#include <mc/reflect.h>
#include <mc/variant.h>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace mc {
namespace log {

/**
 * @brief appender配置结构
 */
struct appender_config {
    std::string name;       // appender名称
    std::string type;       // appender类型
    mc::dict    properties; // appender属性配置
    std::string lib_path;   // 外部appender动态库路径，为空表示内部appender
};

/**
 * @brief logger配置结构
 */
struct logger_config {
    std::string              name;      // logger名称
    log::level               level;     // 日志级别
    std::vector<std::string> appenders; // 关联的appender名称列表
    bool                     condition; // 是否输出日志，false 时不打印

    logger_config(const std::string& name = MC_LOG_DEFAULT_LOGGER) : name(name), condition(true)
    {}
};

/**
 * @brief 日志系统配置结构
 */
struct logging_config {
    std::vector<appender_config> appenders; // appender配置列表
    std::vector<logger_config>   loggers;   // logger配置列表
};

/**
 * @brief 日志管理器
 *
 * 负责管理日志记录器和追加器
 */
class MC_API log_manager {
public:
    /**
     * @brief 获取单例实例
     *
     * @return log_manager& 单例实例引用
     */
    static log_manager& instance();

    /**
     * @brief 获取日志记录器
     *
     * @param name 日志记录器名称
     * @return logger 日志记录器
     */
    logger get_logger(const char* name = MC_LOG_DEFAULT_LOGGER);

    /**
     * @brief 加载追加器
     *
     * @param lib_path 动态库路径
     * @param appender_name 追加器名称
     * @return bool 是否成功加载
     */
    bool load_appender(const std::string& lib_path, const std::string& appender_name);

    /**
     * @brief 从目录加载所有追加器
     *
     * @param dir_path 目录路径
     */
    void load_appenders(const std::string& dir_path);

    /**
     * @brief 创建追加器
     *
     * @param type 追加器类型
     * @return appender_ptr 追加器指针
     */
    template <typename T>
    std::shared_ptr<T> create_appender(const std::string& type)
    {
        return appender_factory::instance().create_by_type<T>(type);
    }

    /**
     * @brief 应用日志系统配置
     *
     * @param config 日志系统配置
     * @return bool 是否成功应用配置
     */
    bool apply_config(const logging_config& config);

    void set_dlog_level(level level);

private:
    log_manager();
    log_manager(const log_manager&)            = delete;
    log_manager& operator=(const log_manager&) = delete;

    logger create_new_logger(const logger_config& log_config);

    bool load_appenders_from_config(const std::vector<appender_config>& appender_configs);
    bool load_single_appender(const appender_config& app_config);
    void update_existing_logger(logger& log, const logger_config& log_config);

    std::unordered_map<std::string, logger> m_loggers; // 日志记录器映射
    std::mutex                              m_mutex;   // 互斥锁
};

} // namespace log
} // namespace mc

MC_REFLECTABLE("mc.log.appender_config", mc::log::appender_config)
MC_REFLECTABLE("mc.log.logger_config", mc::log::logger_config)
MC_REFLECTABLE("mc.log.logging_config", mc::log::logging_config)

#endif // MC_LOG_MANAGER_H
