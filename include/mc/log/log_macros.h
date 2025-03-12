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

#ifndef MC_LOG_MACROS_H
#define MC_LOG_MACROS_H

#include <mc/log/logger.h>
#include <mc/log/log_manager.h>

namespace mc {
namespace log {

/**
 * @brief 获取默认日志记录器
 * 
 * @return logger 默认日志记录器
 */
inline logger& default_logger() {
    static logger default_log = log_manager::instance().get_logger();
    return default_log;
}

} // namespace log
} // namespace mc

// 日志宏 - 使用默认日志记录器
#define tlog(FORMAT, ...) MC_LOG_TRACE(mc::log::default_logger(), FORMAT, __VA_ARGS__)
#define dlog(FORMAT, ...) MC_LOG_DEBUG(mc::log::default_logger(), FORMAT, __VA_ARGS__)
#define ilog(FORMAT, ...) MC_LOG_INFO(mc::log::default_logger(), FORMAT, __VA_ARGS__)
#define wlog(FORMAT, ...) MC_LOG_WARN(mc::log::default_logger(), FORMAT, __VA_ARGS__)
#define elog(FORMAT, ...) MC_LOG_ERROR(mc::log::default_logger(), FORMAT, __VA_ARGS__)
#define flog(FORMAT, ...) MC_LOG_FATAL(mc::log::default_logger(), FORMAT, __VA_ARGS__)

// 指定日志记录器的日志宏
#define mc_tlog(LOGGER, FORMAT, ...) MC_LOG_TRACE(LOGGER, FORMAT, __VA_ARGS__)
#define mc_dlog(LOGGER, FORMAT, ...) MC_LOG_DEBUG(LOGGER, FORMAT, __VA_ARGS__)
#define mc_ilog(LOGGER, FORMAT, ...) MC_LOG_INFO(LOGGER, FORMAT, __VA_ARGS__)
#define mc_wlog(LOGGER, FORMAT, ...) MC_LOG_WARN(LOGGER, FORMAT, __VA_ARGS__)
#define mc_elog(LOGGER, FORMAT, ...) MC_LOG_ERROR(LOGGER, FORMAT, __VA_ARGS__)
#define mc_flog(LOGGER, FORMAT, ...) MC_LOG_FATAL(LOGGER, FORMAT, __VA_ARGS__)

#endif // MC_LOG_MACROS_H 