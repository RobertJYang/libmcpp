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

#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace mc {
namespace log {

/**
 * @brief 日志类别枚举
 */
enum class log_category : uint8_t {
    debug,        // 调试类别
    mdbctl,       // mdbctl 终端输出类别，输出到 mdbctl 终端
    operation,    // 操作类别
    running,      // 运行日志类别，格式：时间、级别、日志内容
    maintenance,  // 维护日志类别，格式：时间、级别、错误码、日志内容
    security,     // 安全日志类别，格式：时间、级别、日志内容
    hw_stream,    // 南向硬件流日志类别（syslog LOG_LOCAL6）
    mc_stream,    // mc 流日志类别（syslog LOG_LOCAL5）
    serial_printf // 串口 printf 方式输出类别，由 file_appender 承载
};

/**
 * @brief 日志级别枚举
 */
enum class level {
    all,    // 所有日志
    trace,  // 跟踪日志
    debug,  // 调试日志
    info,   // 信息日志
    notice, // 注意日志
    warn,   // 警告日志
    error,  // 错误日志
    fatal,  // 致命错误日志
    off     // 关闭日志
};

/**
 * @brief 获取日志级别的字符串表示
 *
 * @param lvl 日志级别
 * @return std::string 日志级别的字符串表示
 */
inline std::string_view to_string(level lvl)
{
    switch (lvl) {
        case level::all:
            return "ALL";
        case level::trace:
            return "TRACE";
        case level::debug:
            return "DEBUG";
        case level::info:
            return "INFO";
        case level::notice:
            return "NOTICE";
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

/**
 * @brief 将字符串转换为日志级别（大小写不敏感）
 *
 * @param name 日志级别名称
 * @return std::optional<level> 转换成功返回对应级别，失败返回std::nullopt
 */
inline std::optional<level> to_level(std::string_view name)
{
    if (name.empty()) {
        return std::nullopt;
    }

    std::string normalized;
    normalized.reserve(name.size());
    for (char ch : name) {
        normalized.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }

    if (normalized == "ALL") {
        return level::all;
    }
    if (normalized == "TRACE") {
        return level::trace;
    }
    if (normalized == "DEBUG") {
        return level::debug;
    }
    if (normalized == "INFO") {
        return level::info;
    }
    if (normalized == "NOTICE") {
        return level::notice;
    }
    if (normalized == "WARN" || normalized == "WARNING") {
        return level::warn;
    }
    if (normalized == "ERROR") {
        return level::error;
    }
    if (normalized == "FATAL") {
        return level::fatal;
    }
    if (normalized == "OFF") {
        return level::off;
    }

    return std::nullopt;
}

} // namespace log
} // namespace mc

#endif // MC_LOG_LEVEL_H