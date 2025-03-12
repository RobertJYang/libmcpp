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

#ifndef MC_LOG_H
#define MC_LOG_H

/**
 * @file log.h
 * @brief 日志模块入口头文件
 * 
 * 包含整个日志系统的所有组件和宏定义，提供对外统一接口
 */

// 包含核心日志头文件（不包含宏定义，这些将在本文件中提供）
#include <mc/log/log_level.h>
#include <mc/log/log_manager.h>
#include <mc/log/logger.h>
#include <mc/log/log_message.h>
#include <mc/log/appenders/appender.h>
#include <mc/log/appenders/console_appender.h>
#include <mc/log/appenders/file_appender.h>
#include <mc/log/backends/log_backend.h>
#include <mc/log/backends/log_backend_adapter.h>
#include <mc/log/backends/log_backend_loader.h>
#include <mc/log/backends/file_log_backend.h>
#include <mc/log/backends/elk_log_backend.h>

namespace mc {

/**
 * @brief 获取默认日志记录器
 * 
 * @return log::logger 默认日志记录器
 */
inline log::logger get_default_logger() {
    return log::logger::get();
}

/**
 * @brief 获取指定名称的日志记录器
 * 
 * @param name 日志记录器名称
 * @return log::logger 日志记录器
 */
inline log::logger get_logger(const std::string& name) {
    return log::logger::get(name);
}

/**
 * @brief 创建控制台日志追加器
 * 
 * @param name 追加器名称
 * @param lvl 日志级别
 * @param use_color 是否使用颜色
 * @return std::shared_ptr<log::console_appender> 控制台日志追加器
 */
inline std::shared_ptr<log::console_appender> create_console_appender(
    const std::string& name = "console",
    log::level lvl = log::level::info,
    bool use_color = true) {
    return std::make_shared<log::console_appender>(
        log::console_appender_config(name, lvl, use_color));
}

/**
 * @brief 创建文件日志追加器
 * 
 * @param name 追加器名称
 * @param filename 日志文件名
 * @param lvl 日志级别
 * @param truncate 是否截断文件
 * @param flush_on_write 是否在每次写入后刷新
 * @return std::shared_ptr<log::file_appender> 文件日志追加器
 */
inline std::shared_ptr<log::file_appender> create_file_appender(
    const std::string& name,
    const std::string& filename,
    log::level lvl = log::level::info,
    bool truncate = false,
    bool flush_on_write = false) {
    return std::make_shared<log::file_appender>(
        log::file_appender_config(name, filename, lvl, truncate, flush_on_write));
}

namespace log {

/**
 * @brief 获取默认日志记录器
 * 
 * @return logger& 默认日志记录器引用
 */
inline logger& default_logger() {
    static logger default_log = log_manager::instance().get_logger();
    return default_log;
}

} // namespace log
} // namespace mc

// ======================================================================
// 日志宏定义 - 这些是推荐的日志接口
// ======================================================================

// 使用指定日志记录器的日志宏
#define mc_tlog(LOGGER, FORMAT, ...) MC_LOG_TRACE(LOGGER, FORMAT, __VA_ARGS__)
#define mc_dlog(LOGGER, FORMAT, ...) MC_LOG_DEBUG(LOGGER, FORMAT, __VA_ARGS__)
#define mc_ilog(LOGGER, FORMAT, ...) MC_LOG_INFO(LOGGER, FORMAT, __VA_ARGS__)
#define mc_wlog(LOGGER, FORMAT, ...) MC_LOG_WARN(LOGGER, FORMAT, __VA_ARGS__)
#define mc_elog(LOGGER, FORMAT, ...) MC_LOG_ERROR(LOGGER, FORMAT, __VA_ARGS__)
#define mc_flog(LOGGER, FORMAT, ...) MC_LOG_FATAL(LOGGER, FORMAT, __VA_ARGS__)

// 使用默认日志记录器的全局日志宏
#define tlog(FORMAT, ...) MC_LOG_TRACE(mc::log::default_logger(), FORMAT, __VA_ARGS__)
#define dlog(FORMAT, ...) MC_LOG_DEBUG(mc::log::default_logger(), FORMAT, __VA_ARGS__)
#define ilog(FORMAT, ...) MC_LOG_INFO(mc::log::default_logger(), FORMAT, __VA_ARGS__)
#define wlog(FORMAT, ...) MC_LOG_WARN(mc::log::default_logger(), FORMAT, __VA_ARGS__)
#define elog(FORMAT, ...) MC_LOG_ERROR(mc::log::default_logger(), FORMAT, __VA_ARGS__)
#define flog(FORMAT, ...) MC_LOG_FATAL(mc::log::default_logger(), FORMAT, __VA_ARGS__)

#endif // MC_LOG_H 