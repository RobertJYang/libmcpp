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

#ifndef MC_LOG_LEVEL_H
#define MC_LOG_LEVEL_H

#include <string_view>

namespace mc {
namespace log {

/**
 * @brief 日志级别枚举
 */
enum class level {
    all,   // 所有日志
    trace, // 跟踪日志
    debug, // 调试日志
    info,  // 信息日志
    warn,  // 警告日志
    error, // 错误日志
    fatal, // 致命错误日志
    off    // 关闭日志
};

/**
 * @brief 获取日志级别的字符串表示
 *
 * @param lvl 日志级别
 * @return std::string 日志级别的字符串表示
 */
inline std::string_view to_string(level lvl) {
    switch (lvl) {
    case level::all:
        return "ALL";
    case level::trace:
        return "TRACE";
    case level::debug:
        return "DEBUG";
    case level::info:
        return "INFO";
    case level::warn:
        return "WARN";
    case level::error:
        return "ERROR";
    case level::fatal:
        return "FATAL";
    case level::off:
        return "OFF";
    default:
        return "UNKNOWN";
    }
}

} // namespace log
} // namespace mc

#endif // MC_LOG_LEVEL_H