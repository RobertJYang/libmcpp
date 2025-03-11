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
 * @brief MC 日志系统
 * 
 * 提供日志记录功能，支持不同级别的日志、格式化日志消息、多种日志输出目标等。
 */

#include <mc/log/log_level.h>
#include <mc/log/log_message.h>
#include <mc/log/appender.h>
#include <mc/log/console_appender.h>
#include <mc/log/file_appender.h>
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

} // namespace mc

// 全局日志宏
#define MC_TRACE(...) MC_LOG_TRACE(mc::get_default_logger(), __VA_ARGS__)
#define MC_DEBUG(...) MC_LOG_DEBUG(mc::get_default_logger(), __VA_ARGS__)
#define MC_INFO(...)  MC_LOG_INFO(mc::get_default_logger(), __VA_ARGS__)
#define MC_WARN(...)  MC_LOG_WARN(mc::get_default_logger(), __VA_ARGS__)
#define MC_ERROR(...) MC_LOG_ERROR(mc::get_default_logger(), __VA_ARGS__)
#define MC_FATAL(...) MC_LOG_FATAL(mc::get_default_logger(), __VA_ARGS__)

#endif // MC_LOG_H 