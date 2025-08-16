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
#include <mc/log/log_message.h>
#include <mc/log/logger.h>

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
inline log::logger get_logger(const char* name) {
    return log::logger::get(name);
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
// 日志宏定义
// ======================================================================

// 使用指定日志记录器的日志宏
#define mc_tlog(LOGGER, ...) MC_LOG_BASE(LOGGER, trace, __VA_ARGS__)
#define mc_dlog(LOGGER, ...) MC_LOG_BASE(LOGGER, debug, __VA_ARGS__)
#define mc_ilog(LOGGER, ...) MC_LOG_BASE(LOGGER, info, __VA_ARGS__)
#define mc_nlog(LOGGER, ...) MC_LOG_BASE(LOGGER, notice, __VA_ARGS__)
#define mc_wlog(LOGGER, ...) MC_LOG_BASE(LOGGER, warn, __VA_ARGS__)
#define mc_elog(LOGGER, ...) MC_LOG_BASE(LOGGER, error, __VA_ARGS__)
#define mc_flog(LOGGER, ...) MC_LOG_BASE(LOGGER, fatal, __VA_ARGS__)

// 使用默认日志记录器的全局日志宏
#define tlog(...) mc_tlog(mc::log::default_logger(), __VA_ARGS__)
#define dlog(...) mc_dlog(mc::log::default_logger(), __VA_ARGS__)
#define ilog(...) mc_ilog(mc::log::default_logger(), __VA_ARGS__)
#define nlog(...) mc_nlog(mc::log::default_logger(), __VA_ARGS__)
#define wlog(...) mc_wlog(mc::log::default_logger(), __VA_ARGS__)
#define elog(...) mc_elog(mc::log::default_logger(), __VA_ARGS__)
#define flog(...) mc_flog(mc::log::default_logger(), __VA_ARGS__)

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