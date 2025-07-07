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
// 日志宏定义 - 这些是推荐的日志接口
// ======================================================================

// 使用指定日志记录器的日志宏
#define mc_tlog(LOGGER, FORMAT, ...) MC_LOG_TRACE(LOGGER, FORMAT, __VA_ARGS__)
#define mc_dlog(LOGGER, FORMAT, ...) MC_LOG_DEBUG(LOGGER, FORMAT, __VA_ARGS__)
#define mc_ilog(LOGGER, FORMAT, ...) MC_LOG_INFO(LOGGER, FORMAT, __VA_ARGS__)
#define mc_nlog(LOGGER, FORMAT, ...) MC_LOG_NOTICE(LOGGER, FORMAT, __VA_ARGS__)
#define mc_wlog(LOGGER, FORMAT, ...) MC_LOG_WARN(LOGGER, FORMAT, __VA_ARGS__)
#define mc_elog(LOGGER, FORMAT, ...) MC_LOG_ERROR(LOGGER, FORMAT, __VA_ARGS__)
#define mc_flog(LOGGER, FORMAT, ...) MC_LOG_FATAL(LOGGER, FORMAT, __VA_ARGS__)

// 使用默认日志记录器的全局日志宏
#define tlog(FORMAT, ...) MC_LOG_TRACE(mc::log::default_logger(), FORMAT, __VA_ARGS__)
#define dlog(FORMAT, ...) MC_LOG_DEBUG(mc::log::default_logger(), FORMAT, __VA_ARGS__)
#define ilog(FORMAT, ...) MC_LOG_INFO(mc::log::default_logger(), FORMAT, __VA_ARGS__)
#define nlog(FORMAT, ...) MC_LOG_NOTICE(mc::log::default_logger(), FORMAT, __VA_ARGS__)
#define wlog(FORMAT, ...) MC_LOG_WARN(mc::log::default_logger(), FORMAT, __VA_ARGS__)
#define elog(FORMAT, ...) MC_LOG_ERROR(mc::log::default_logger(), FORMAT, __VA_ARGS__)
#define flog(FORMAT, ...) MC_LOG_FATAL(mc::log::default_logger(), FORMAT, __VA_ARGS__)

#endif // MC_LOG_H