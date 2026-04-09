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
#include <mc/log/log.h>
#include <mc/log/log_level.h>
#include <mc/log/log_manager.h>
#include <mc/log/log_message.h>
#include <mc/log/logger.h>

namespace mc {

namespace log {

/**
 * @brief 获取 mdbctl 专用日志记录器（仅含 socket_appender，不含 console/file）
 *
 * @return logger mdbctl 日志记录器
 */
inline logger mdbctl_logger()
{
    return log_manager::instance().get_logger(MC_LOG_MDBCTL_LOGGER);
}

} // namespace log
} // namespace mc

// ======================================================================
// 日志宏定义
// ======================================================================

// 使用指定日志记录器的不限流（_easy）日志宏
#define mc_tlog_easy(LOGGER, ...) MC_LOG_BASE_EASY(LOGGER, trace, __VA_ARGS__)
#define mc_dlog_easy(LOGGER, ...) MC_LOG_BASE_EASY(LOGGER, debug, __VA_ARGS__)
#define mc_ilog_easy(LOGGER, ...) MC_LOG_BASE_EASY(LOGGER, info, __VA_ARGS__)
#define mc_nlog_easy(LOGGER, ...) MC_LOG_BASE_EASY(LOGGER, notice, __VA_ARGS__)
#define mc_wlog_easy(LOGGER, ...) MC_LOG_BASE_EASY(LOGGER, warn, __VA_ARGS__)
#define mc_elog_easy(LOGGER, ...) MC_LOG_BASE_EASY(LOGGER, error, __VA_ARGS__)
#define mc_flog_easy(LOGGER, ...) MC_LOG_BASE_EASY(LOGGER, fatal, __VA_ARGS__)

// 非调试日志分类宏
#define mc_operation_log(LOGGER, ...) MC_LOG_BASE_WITH_CATEGORY(LOGGER, mc::log::log_category::operation, __VA_ARGS__)

// mdbctl 终端日志宏（使用 mdbctl_logger，仅输出到 socket，不含 console/file）
#define mc_mdbctl_log(LOGGER, ...) MC_LOG_BASE_WITH_CATEGORY(LOGGER, mc::log::log_category::mdbctl, __VA_ARGS__)
#define mdbctl_log(...)            mc_mdbctl_log(mc::log::mdbctl_logger(), __VA_ARGS__)

// 运行日志宏（格式：时间、级别、日志内容，不含 trace、fatal）
#define mc_running_dlog(LOGGER, ...)                                                                                   \
    MC_LOG_BASE_WITH_CATEGORY_AND_LEVEL(LOGGER, mc::log::log_category::running, debug, __VA_ARGS__)
#define mc_running_ilog(LOGGER, ...)                                                                                   \
    MC_LOG_BASE_WITH_CATEGORY_AND_LEVEL(LOGGER, mc::log::log_category::running, info, __VA_ARGS__)
#define mc_running_nlog(LOGGER, ...)                                                                                   \
    MC_LOG_BASE_WITH_CATEGORY_AND_LEVEL(LOGGER, mc::log::log_category::running, notice, __VA_ARGS__)
#define mc_running_wlog(LOGGER, ...)                                                                                   \
    MC_LOG_BASE_WITH_CATEGORY_AND_LEVEL(LOGGER, mc::log::log_category::running, warn, __VA_ARGS__)
#define mc_running_elog(LOGGER, ...)                                                                                   \
    MC_LOG_BASE_WITH_CATEGORY_AND_LEVEL(LOGGER, mc::log::log_category::running, error, __VA_ARGS__)

// 维护日志宏（格式：LOGGER, error_code, message，错误码为空时仅输出时间、级别、内容，不含 trace、fatal）
#define mc_maintenance_dlog(LOGGER, error_code, ...)                                                                   \
    MC_LOG_BASE_WITH_CATEGORY_AND_LEVEL(LOGGER, mc::log::log_category::maintenance, debug, __VA_ARGS__,                \
                                        ("error_code", error_code))
#define mc_maintenance_ilog(LOGGER, error_code, ...)                                                                   \
    MC_LOG_BASE_WITH_CATEGORY_AND_LEVEL(LOGGER, mc::log::log_category::maintenance, info, __VA_ARGS__,                 \
                                        ("error_code", error_code))
#define mc_maintenance_nlog(LOGGER, error_code, ...)                                                                   \
    MC_LOG_BASE_WITH_CATEGORY_AND_LEVEL(LOGGER, mc::log::log_category::maintenance, notice, __VA_ARGS__,               \
                                        ("error_code", error_code))
#define mc_maintenance_wlog(LOGGER, error_code, ...)                                                                   \
    MC_LOG_BASE_WITH_CATEGORY_AND_LEVEL(LOGGER, mc::log::log_category::maintenance, warn, __VA_ARGS__,                 \
                                        ("error_code", error_code))
#define mc_maintenance_elog(LOGGER, error_code, ...)                                                                   \
    MC_LOG_BASE_WITH_CATEGORY_AND_LEVEL(LOGGER, mc::log::log_category::maintenance, error, __VA_ARGS__,                \
                                        ("error_code", error_code))

// 安全日志宏（格式：时间、级别、日志内容，仅 message 参数，使用 info 级别）
#define mc_security_log(LOGGER, ...)                                                                                   \
    MC_LOG_BASE_WITH_CATEGORY_AND_LEVEL(LOGGER, mc::log::log_category::security, info, __VA_ARGS__)
// 南向硬件流日志宏（输出到相关日志文件 LOG_LOCAL6，格式：时间 模块名 级别: 文件名(行数): 日志文本）
#define mc_hw_stream_info(LOGGER, ...)                                                                                 \
    MC_LOG_BASE_WITH_CATEGORY_AND_LEVEL(LOGGER, mc::log::log_category::hw_stream, info, __VA_ARGS__)
#define mc_hw_stream_warn(LOGGER, ...)                                                                                 \
    MC_LOG_BASE_WITH_CATEGORY_AND_LEVEL(LOGGER, mc::log::log_category::hw_stream, warn, __VA_ARGS__)
#define mc_hw_stream_notice(LOGGER, ...)                                                                               \
    MC_LOG_BASE_WITH_CATEGORY_AND_LEVEL(LOGGER, mc::log::log_category::hw_stream, notice, __VA_ARGS__)
#define mc_hw_stream_error(LOGGER, ...)                                                                                \
    MC_LOG_BASE_WITH_CATEGORY_AND_LEVEL(LOGGER, mc::log::log_category::hw_stream, error, __VA_ARGS__)

// mc 流日志宏（输出到相关日志文件 LOG_LOCAL5，格式与 hw_stream 相同）
#define mc_mc_stream_info(LOGGER, ...)                                                                                 \
    MC_LOG_BASE_WITH_CATEGORY_AND_LEVEL(LOGGER, mc::log::log_category::mc_stream, info, __VA_ARGS__)
#define mc_mc_stream_warn(LOGGER, ...)                                                                                 \
    MC_LOG_BASE_WITH_CATEGORY_AND_LEVEL(LOGGER, mc::log::log_category::mc_stream, warn, __VA_ARGS__)
#define mc_mc_stream_notice(LOGGER, ...)                                                                               \
    MC_LOG_BASE_WITH_CATEGORY_AND_LEVEL(LOGGER, mc::log::log_category::mc_stream, notice, __VA_ARGS__)
#define mc_mc_stream_error(LOGGER, ...)                                                                                \
    MC_LOG_BASE_WITH_CATEGORY_AND_LEVEL(LOGGER, mc::log::log_category::mc_stream, error, __VA_ARGS__)

// 使用默认日志记录器的全局不限流（_easy）日志宏
#define tlog_easy(...) mc_tlog_easy(mc::log::default_logger(), __VA_ARGS__)
#define dlog_easy(...) mc_dlog_easy(mc::log::default_logger(), __VA_ARGS__)
#define ilog_easy(...) mc_ilog_easy(mc::log::default_logger(), __VA_ARGS__)
#define nlog_easy(...) mc_nlog_easy(mc::log::default_logger(), __VA_ARGS__)
#define wlog_easy(...) mc_wlog_easy(mc::log::default_logger(), __VA_ARGS__)
#define elog_easy(...) mc_elog_easy(mc::log::default_logger(), __VA_ARGS__)
#define flog_easy(...) mc_flog_easy(mc::log::default_logger(), __VA_ARGS__)

// 全局非调试日志分类宏
#define operation_log(...) mc_operation_log(mc::log::default_logger(), __VA_ARGS__)

// 全局运行日志宏
#define running_dlog(...) mc_running_dlog(mc::log::default_logger(), __VA_ARGS__)
#define running_ilog(...) mc_running_ilog(mc::log::default_logger(), __VA_ARGS__)
#define running_nlog(...) mc_running_nlog(mc::log::default_logger(), __VA_ARGS__)
#define running_wlog(...) mc_running_wlog(mc::log::default_logger(), __VA_ARGS__)
#define running_elog(...) mc_running_elog(mc::log::default_logger(), __VA_ARGS__)

// 全局维护日志宏
#define maintenance_dlog(error_code, ...) mc_maintenance_dlog(mc::log::default_logger(), error_code, __VA_ARGS__)
#define maintenance_ilog(error_code, ...) mc_maintenance_ilog(mc::log::default_logger(), error_code, __VA_ARGS__)
#define maintenance_nlog(error_code, ...) mc_maintenance_nlog(mc::log::default_logger(), error_code, __VA_ARGS__)
#define maintenance_wlog(error_code, ...) mc_maintenance_wlog(mc::log::default_logger(), error_code, __VA_ARGS__)
#define maintenance_elog(error_code, ...) mc_maintenance_elog(mc::log::default_logger(), error_code, __VA_ARGS__)

// 全局安全日志宏
#define security_log(...) mc_security_log(mc::log::default_logger(), __VA_ARGS__)
// 全局南向硬件流日志宏
#define hw_stream_info(...)   mc_hw_stream_info(mc::log::default_logger(), __VA_ARGS__)
#define hw_stream_warn(...)   mc_hw_stream_warn(mc::log::default_logger(), __VA_ARGS__)
#define hw_stream_notice(...) mc_hw_stream_notice(mc::log::default_logger(), __VA_ARGS__)
#define hw_stream_error(...)  mc_hw_stream_error(mc::log::default_logger(), __VA_ARGS__)

// 全局 mc 流日志宏
#define mc_stream_info(...)   mc_mc_stream_info(mc::log::default_logger(), __VA_ARGS__)
#define mc_stream_warn(...)   mc_mc_stream_warn(mc::log::default_logger(), __VA_ARGS__)
#define mc_stream_notice(...) mc_mc_stream_notice(mc::log::default_logger(), __VA_ARGS__)
#define mc_stream_error(...)  mc_mc_stream_error(mc::log::default_logger(), __VA_ARGS__)

// ======================================================================
// 不安全的日志宏定义（编译期不检查格式化字符串和格式化参数）
// ======================================================================

// 使用指定日志记录器的日志宏
#define mc_tlog_unsafe(LOGGER, ...) MC_LOG_BASE_UNSAFE(LOGGER, trace, __VA_ARGS__)
#define mc_dlog_unsafe(LOGGER, ...) MC_LOG_BASE_UNSAFE(LOGGER, debug, __VA_ARGS__)
#define mc_ilog_unsafe(LOGGER, ...) MC_LOG_BASE_UNSAFE(LOGGER, info, __VA_ARGS__)
#define mc_nlog_unsafe(LOGGER, ...) MC_LOG_BASE_UNSAFE(LOGGER, info, __VA_ARGS__)
#define mc_wlog_unsafe(LOGGER, ...) MC_LOG_BASE_UNSAFE(LOGGER, warn, __VA_ARGS__)
#define mc_elog_unsafe(LOGGER, ...) MC_LOG_BASE_UNSAFE(LOGGER, error, __VA_ARGS__)
#define mc_flog_unsafe(LOGGER, ...) MC_LOG_BASE_UNSAFE(LOGGER, fatal, __VA_ARGS__)

// 使用默认日志记录器的全局日志宏
#define tlog_unsafe(...) mc_tlog_unsafe(mc::log::default_logger(), __VA_ARGS__)
#define dlog_unsafe(...) mc_dlog_unsafe(mc::log::default_logger(), __VA_ARGS__)
#define ilog_unsafe(...) mc_ilog_unsafe(mc::log::default_logger(), __VA_ARGS__)
#define nlog_unsafe(...) mc_nlog_unsafe(mc::log::default_logger(), __VA_ARGS__)
#define wlog_unsafe(...) mc_wlog_unsafe(mc::log::default_logger(), __VA_ARGS__)
#define elog_unsafe(...) mc_elog_unsafe(mc::log::default_logger(), __VA_ARGS__)
#define flog_unsafe(...) mc_flog_unsafe(mc::log::default_logger(), __VA_ARGS__)

#endif // MC_LOG_H
