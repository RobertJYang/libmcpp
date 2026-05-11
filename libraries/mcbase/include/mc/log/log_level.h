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

#include <cstdint>
#include <mc/string_utils.h>
#include <optional>
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
 * @return mc::string_view 日志级别的字符串表示
 */
inline mc::string_view to_string(level lvl)
{
    switch (lvl) {
        case level::all:
            return mc::string_view("ALL");
        case level::trace:
            return mc::string_view("TRACE");
        case level::debug:
            return mc::string_view("DEBUG");
        case level::info:
            return mc::string_view("INFO");
        case level::notice:
            return mc::string_view("NOTICE");
        case level::warn:
            return mc::string_view("WARN");
        case level::error:
            return mc::string_view("ERROR");
        case level::fatal:
            return mc::string_view("FATAL");
        case level::off:
            return mc::string_view("OFF");
        default:
            return mc::string_view("UNKNOWN");
    }
}

/**
 * @brief 将字符串转换为日志级别（大小写不敏感）
 *
 * @param name 日志级别名称
 * @param result 转换成功时写入对应级别
 * @return 转换成功返回 true，否则返回 false
 */
inline bool try_to_level(mc::string_view name, level& result)
{
    if (name.empty()) {
        return false;
    }

    if (mc::strings::iequals(mc::string_view(name), "ALL")) {
        result = level::all;
        return true;
    }
    if (mc::strings::iequals(mc::string_view(name), "TRACE")) {
        result = level::trace;
        return true;
    }
    if (mc::strings::iequals(mc::string_view(name), "DEBUG")) {
        result = level::debug;
        return true;
    }
    if (mc::strings::iequals(mc::string_view(name), "INFO")) {
        result = level::info;
        return true;
    }
    if (mc::strings::iequals(mc::string_view(name), "NOTICE")) {
        result = level::notice;
        return true;
    }
    if (mc::strings::iequals(mc::string_view(name), "WARN") || mc::strings::iequals(mc::string_view(name), "WARNING")) {
        result = level::warn;
        return true;
    }
    if (mc::strings::iequals(mc::string_view(name), "ERROR")) {
        result = level::error;
        return true;
    }
    if (mc::strings::iequals(mc::string_view(name), "FATAL")) {
        result = level::fatal;
        return true;
    }
    if (mc::strings::iequals(mc::string_view(name), "OFF")) {
        result = level::off;
        return true;
    }

    return false;
}

/**
 * @brief 兼容旧接口，将字符串转换为日志级别（大小写不敏感）
 *
 * @param name 日志级别名称
 * @return std::optional<level> 转换成功返回对应级别，失败返回 std::nullopt
 */
inline std::optional<level> to_level(mc::string_view name)
{
    level result = level::all;
    if (try_to_level(mc::string_view(name), result)) {
        return result;
    }

    return std::nullopt;
}

} // namespace log
} // namespace mc

#endif // MC_LOG_LEVEL_H