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
#ifndef MC_FORMATTER_CHRONO_H
#define MC_FORMATTER_CHRONO_H

#include <array>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

#include <mc/fmt/format_parser.h>
#include <mc/fmt/format_spec.h>
#include <mc/fmt/formatter.h>

namespace mc::fmt {

namespace detail {

// 数字系统枚举
enum class numeric_system {
    standard,   // 标准数字系统
    alternative // 替代数字系统（例如中文数字）
};

// 填充类型枚举
enum class pad_type {
    none, // 不填充
    zero, // 用0填充
    space // 用空格填充
};

// chrono 格式说明符类型
enum class chrono_spec_type {
    // 时间字段
    year,    // %Y, %y - 年份
    month,   // %m, %B, %b - 月份
    day,     // %d, %e - 日期
    weekday, // %A, %a, %w, %u - 星期
    hour_24, // %H - 24小时制小时
    hour_12, // %I - 12小时制小时
    minute,  // %M - 分钟
    second,  // %S - 秒

    // 组合格式
    datetime, // %c - 日期时间
    date,     // %x, %F - 日期
    time,     // %X, %T, %R, %r - 时间

    // duration 专用
    duration_value, // %Q - duration 值
    duration_unit,  // %q - duration 单位

    // 其他
    am_pm,           // %p - AM/PM
    timezone_offset, // %z - 时区偏移 (+0800)
    timezone_name,   // %Z - 时区名称 (CST)
    literal,         // 文字字符
    unknown          // 未知格式
};

// chrono 格式规范
struct chrono_format_spec {
    chrono_spec_type type          = chrono_spec_type::unknown;
    char             original_char = 0;                        // 原始格式字符
    char             modifier      = 0;                        // 修饰符 (E, O)
    pad_type         pad           = pad_type::zero;           // 填充类型
    int              precision     = -1;                       // 精度
    bool             has_precision = false;                    // 是否有精度设置
    numeric_system   num_sys       = numeric_system::standard; // 数字系统

    constexpr chrono_format_spec() = default;
    constexpr chrono_format_spec(chrono_spec_type t, char c)
        : type(t), original_char(c)
    {
    }
};

// 获取时间单位字符串
template <typename Period>
constexpr const char* get_unit_str()
{
    if constexpr (std::is_same_v<Period, std::atto>) {
        return "as";
    } else if constexpr (std::is_same_v<Period, std::femto>) {
        return "fs";
    } else if constexpr (std::is_same_v<Period, std::pico>) {
        return "ps";
    } else if constexpr (std::is_same_v<Period, std::nano>) {
        return "ns";
    } else if constexpr (std::is_same_v<Period, std::micro>) {
        return "μs";
    } else if constexpr (std::is_same_v<Period, std::milli>) {
        return "ms";
    } else if constexpr (std::is_same_v<Period, std::centi>) {
        return "cs";
    } else if constexpr (std::is_same_v<Period, std::deci>) {
        return "ds";
    } else if constexpr (std::is_same_v<Period, std::ratio<1>>) {
        return "s";
    } else if constexpr (std::is_same_v<Period, std::deca>) {
        return "das";
    } else if constexpr (std::is_same_v<Period, std::hecto>) {
        return "hs";
    } else if constexpr (std::is_same_v<Period, std::kilo>) {
        return "ks";
    } else if constexpr (std::is_same_v<Period, std::mega>) {
        return "Ms";
    } else if constexpr (std::is_same_v<Period, std::giga>) {
        return "Gs";
    } else if constexpr (std::is_same_v<Period, std::tera>) {
        return "Ts";
    } else if constexpr (std::is_same_v<Period, std::peta>) {
        return "Ps";
    } else if constexpr (std::is_same_v<Period, std::exa>) {
        return "Es";
    } else if constexpr (std::is_same_v<Period, std::ratio<60>>) {
        return "min";
    } else if constexpr (std::is_same_v<Period, std::ratio<3600>>) {
        return "h";
    } else if constexpr (std::is_same_v<Period, std::ratio<86400>>) {
        return "d";
    } else {
        return "s"; // 默认单位
    }
}

// chrono 格式解析器基类
template <typename Derived>
struct chrono_format_handler {
    // 文本处理
    constexpr void on_text(std::string_view text)
    {
        static_cast<Derived*>(this)->on_text(text);
    }

    // 时间字段处理
    constexpr void on_year(numeric_system ns, pad_type pad)
    {
        static_cast<Derived*>(this)->on_year(ns, pad);
    }
    constexpr void on_short_year(numeric_system ns)
    {
        static_cast<Derived*>(this)->on_short_year(ns);
    }
    constexpr void on_month(numeric_system ns, pad_type pad)
    {
        static_cast<Derived*>(this)->on_month(ns, pad);
    }
    constexpr void on_day(numeric_system ns, pad_type pad)
    {
        static_cast<Derived*>(this)->on_day(ns, pad);
    }
    constexpr void on_hour_24(numeric_system ns, pad_type pad)
    {
        static_cast<Derived*>(this)->on_hour_24(ns, pad);
    }
    constexpr void on_hour_12(numeric_system ns, pad_type pad)
    {
        static_cast<Derived*>(this)->on_hour_12(ns, pad);
    }
    constexpr void on_minute(numeric_system ns, pad_type pad)
    {
        static_cast<Derived*>(this)->on_minute(ns, pad);
    }
    constexpr void on_second(numeric_system ns, pad_type pad)
    {
        static_cast<Derived*>(this)->on_second(ns, pad);
    }

    // 组合格式处理
    constexpr void on_datetime(numeric_system ns)
    {
        static_cast<Derived*>(this)->on_datetime(ns);
    }
    constexpr void on_iso_date()
    {
        static_cast<Derived*>(this)->on_iso_date();
    }
    constexpr void on_iso_time()
    {
        static_cast<Derived*>(this)->on_iso_time();
    }
    constexpr void on_12_hour_time()
    {
        static_cast<Derived*>(this)->on_12_hour_time();
    }
    constexpr void on_24_hour_time()
    {
        static_cast<Derived*>(this)->on_24_hour_time();
    }

    // duration 专用
    constexpr void on_duration_value()
    {
        static_cast<Derived*>(this)->on_duration_value();
    }
    constexpr void on_duration_unit()
    {
        static_cast<Derived*>(this)->on_duration_unit();
    }

    // 其他
    constexpr void on_am_pm()
    {
        static_cast<Derived*>(this)->on_am_pm();
    }

    constexpr void on_timezone_offset(numeric_system ns)
    {
        static_cast<Derived*>(this)->on_timezone_offset(ns);
    }

    constexpr void on_timezone_name(numeric_system ns)
    {
        static_cast<Derived*>(this)->on_timezone_name(ns);
    }

    // 错误处理
    constexpr void on_invalid_spec()
    {
        static_cast<Derived*>(this)->on_invalid_spec();
    }

    constexpr void on_incomplete_spec()
    {
        static_cast<Derived*>(this)->on_incomplete_spec();
    }
};

// chrono 格式验证器
template <bool IsCompileTime = false>
struct chrono_format_checker : chrono_format_handler<chrono_format_checker<IsCompileTime>> {
    bool has_precision_integral = false;
    bool is_duration_formatter  = false;
    bool has_invalid_spec       = false;

    constexpr chrono_format_checker(bool is_duration = false, bool has_precision = false)
        : has_precision_integral(has_precision), is_duration_formatter(is_duration)
    {
    }

    constexpr void on_text(std::string_view)
    {
    }

    constexpr void on_year(numeric_system, pad_type)
    {
        if (is_duration_formatter) {
            MC_COMPILE_TIME_ERROR("Duration 不支持年份格式说明符 (%Y, %y)");
            has_invalid_spec = true;
        }
    }
    constexpr void on_short_year(numeric_system)
    {
        if (is_duration_formatter) {
            MC_COMPILE_TIME_ERROR("Duration 不支持年份格式说明符 (%y)");
            has_invalid_spec = true;
        }
    }
    constexpr void on_month(numeric_system, pad_type)
    {
        if (is_duration_formatter) {
            MC_COMPILE_TIME_ERROR("Duration 不支持月份格式说明符 (%m, %B, %b)");
            has_invalid_spec = true;
        }
    }
    constexpr void on_day(numeric_system, pad_type)
    {
        if (is_duration_formatter) {
            MC_COMPILE_TIME_ERROR("Duration 不支持日期格式说明符 (%d, %e)");
            has_invalid_spec = true;
        }
    }
    constexpr void on_hour_24(numeric_system, pad_type)
    {
    }
    constexpr void on_hour_12(numeric_system, pad_type)
    {
    }
    constexpr void on_minute(numeric_system, pad_type)
    {
    }
    constexpr void on_second(numeric_system, pad_type)
    {
    }

    constexpr void on_datetime(numeric_system)
    {
        if (is_duration_formatter) {
            MC_COMPILE_TIME_ERROR("Duration 不支持日期时间格式说明符 (%c)");
            has_invalid_spec = true;
        }
    }
    constexpr void on_iso_date()
    {
        if (is_duration_formatter) {
            MC_COMPILE_TIME_ERROR("Duration 不支持ISO日期格式说明符 (%F)");
            has_invalid_spec = true;
        }
    }
    constexpr void on_iso_time()
    {
    }
    constexpr void on_12_hour_time()
    {
    }
    constexpr void on_24_hour_time()
    {
    }

    constexpr void on_duration_value()
    {
        if (!is_duration_formatter) {
            // time_point 不支持 duration 值格式
            MC_COMPILE_TIME_ERROR("Time_point 不支持 Duration 值格式说明符 (%Q)");
            has_invalid_spec = true;
        } else if (has_precision_integral) {
            MC_COMPILE_TIME_ERROR("Duration 值格式说明符 (%Q) 不能与整数精度一起使用");
            has_invalid_spec = true;
        }
    }
    constexpr void on_duration_unit()
    {
        if (!is_duration_formatter) {
            // time_point 不支持 duration 单位格式
            MC_COMPILE_TIME_ERROR("Time_point 不支持 Duration 单位格式说明符 (%q)");
            has_invalid_spec = true;
        }
    }

    constexpr void on_am_pm()
    {
    }
    constexpr void on_timezone_offset(numeric_system)
    {
        if (is_duration_formatter) {
            MC_COMPILE_TIME_ERROR("Duration 不支持时区偏移格式说明符 (%z)");
            has_invalid_spec = true;
        }
    }

    constexpr void on_timezone_name(numeric_system)
    {
        if (is_duration_formatter) {
            MC_COMPILE_TIME_ERROR("Duration 不支持时区名称格式说明符 (%Z)");
            has_invalid_spec = true;
        }
    }

    // 处理无效的格式说明符
    constexpr void on_invalid_spec()
    {
        MC_COMPILE_TIME_ERROR("无效的 Chrono 格式说明符");
        has_invalid_spec = true;
    }

    // 处理不完整的格式说明符（% 后面没有字符）
    constexpr void on_incomplete_spec()
    {
        MC_COMPILE_TIME_ERROR("不完整的 Chrono 格式说明符：% 后面没有字符");
        has_invalid_spec = true;
    }

    constexpr bool is_valid() const
    {
        return !has_invalid_spec;
    }
};

// 解析 chrono 格式字符串
template <typename Handler>
constexpr std::string_view parse_chrono_format(std::string_view fmt, Handler&& handler)
{
    auto it         = fmt.begin();
    auto end        = fmt.end();
    auto text_begin = it;

    while (it != end) {
        if (*it != '%') {
            ++it;
            continue;
        }

        // 输出前面的文本
        if (it != text_begin) {
            handler.on_text(std::string_view(text_begin, it - text_begin));
        }

        ++it; // 跳过 '%'
        if (it == end) {
            // 格式错误：% 后面没有字符
            handler.on_incomplete_spec();
            break;
        }

        pad_type       pad      = pad_type::zero;
        numeric_system ns       = numeric_system::standard;
        char           modifier = 0;
        (void)modifier; // 防止未使用警告

        // 处理填充修饰符
        if (*it == '-') {
            pad = pad_type::none;
            ++it;
        } else if (*it == '_') {
            pad = pad_type::space;
            ++it;
        }

        if (it == end) {
            handler.on_incomplete_spec();
            break;
        }

        // 处理数字系统修饰符
        if (*it == 'E' || *it == 'O') {
            modifier = *it;
            if (*it == 'O') {
                ns = numeric_system::alternative;
            }
            ++it;
        }

        if (it == end) {
            handler.on_incomplete_spec();
            break;
        }

        // 处理格式说明符
        char spec = *it++;
        switch (spec) {
        case '%':
            handler.on_text("%");
            break;
        case 'n':
            handler.on_text("\n");
            break;
        case 't':
            handler.on_text("\t");
            break;

        // 年份
        case 'Y':
            handler.on_year(ns, pad);
            break;
        case 'y':
            handler.on_short_year(ns);
            break;

        // 月份
        case 'm':
            handler.on_month(ns, pad);
            break;
        case 'B':
        case 'b':
        case 'h':
            handler.on_month(ns, pad);
            break;

        // 日期
        case 'd':
            handler.on_day(ns, pad);
            break;
        case 'e':
            handler.on_day(ns, pad_type::space);
            break;

        // 小时
        case 'H':
            handler.on_hour_24(ns, pad);
            break;
        case 'I':
            handler.on_hour_12(ns, pad);
            break;

        // 分钟和秒
        case 'M':
            handler.on_minute(ns, pad);
            break;
        case 'S':
            handler.on_second(ns, pad);
            break;

        // 组合格式
        case 'c':
            handler.on_datetime(ns);
            break;
        case 'F':
            handler.on_iso_date();
            break;
        case 'T':
            handler.on_iso_time();
            break;
        case 'R':
            handler.on_24_hour_time();
            break;
        case 'r':
            handler.on_12_hour_time();
            break;

        // duration 专用
        case 'Q':
            handler.on_duration_value();
            break;
        case 'q':
            handler.on_duration_unit();
            break;

        // 其他
        case 'p':
            handler.on_am_pm();
            break;
        case 'z':
            handler.on_timezone_offset(ns);
            break;
        case 'Z':
            handler.on_timezone_name(ns);
            break;

        default:
            // 未知格式说明符
            handler.on_invalid_spec();
            break;
        }

        text_begin = it;
    }

    // 处理剩余文本
    if (text_begin != it) {
        handler.on_text(std::string_view(text_begin, it - text_begin));
    }

    return std::string_view(it, end - it);
}

} // namespace detail

// duration 的格式化器特化
template <typename Rep, typename Period>
struct formatter<std::chrono::duration<Rep, Period>> {
private:
    struct custom_spec {
        constexpr custom_spec()
        {
        }

        std::string_view format_str;
        int              precision     = -1;
        bool             has_precision = false;
    };

public:
    template <bool IsCompileTime = false>
    constexpr bool parse(std::string_view fmt_str, format_spec& spec)
    {
        custom_spec custom;
        if (fmt_str.empty()) {
            return true;
        }

        auto it  = fmt_str.begin();
        auto end = fmt_str.end();

        // 解析精度
        if (it != end && *it == '.') {
            ++it;
            if (it != end && mc::fmt::detail::isdigit(*it)) {
                custom.has_precision = true;
                custom.precision     = 0;
                while (it != end && mc::fmt::detail::isdigit(*it)) {
                    custom.precision = custom.precision * 10 + (*it - '0');
                    ++it;
                }
            }
        }

        // 剩余部分为格式字符串
        if (it != end) {
            custom.format_str = std::string_view(it, end - it);
        }

        // 验证格式字符串
        if (!custom.format_str.empty()) {
            detail::chrono_format_checker<IsCompileTime> checker(
                true,
                custom.has_precision && !std::is_floating_point_v<Rep>);
            detail::parse_chrono_format(custom.format_str, checker);
            if (!checker.is_valid()) {
                if constexpr (!IsCompileTime) {
                    detail::throw_format_error("duration格式说明符无效: ${format}",
                                               mc::dict{{"format", custom.format_str}});
                } else {
                    return false;
                }
            }
        }

        if constexpr (!IsCompileTime) {
            spec.set_custom_spec<custom_spec>(custom);
        }

        return true;
    }

    // format 方法，执行格式化
    template <typename Context>
    void format(const std::chrono::duration<Rep, Period>& d,
                Context& ctx, const format_spec& spec) const;
};

// time_point 的格式化器特化
template <typename Clock, typename Duration>
struct formatter<std::chrono::time_point<Clock, Duration>> {
private:
    struct custom_spec {
        constexpr custom_spec()
        {
        }

        std::string_view format_str;
    };

public:
    template <bool IsCompileTime = false>
    constexpr bool parse(std::string_view fmt_str, format_spec& spec)
    {
        custom_spec custom;
        if (fmt_str.empty()) {
            return true;
        }

        custom.format_str = fmt_str;

        detail::chrono_format_checker<IsCompileTime> checker(false);
        detail::parse_chrono_format(custom.format_str, checker);
        if (!checker.is_valid()) {
            if constexpr (!IsCompileTime) {
                detail::throw_format_error("time_point格式说明符无效: ${format}",
                                           mc::dict{{"format", custom.format_str}});
            } else {
                return false;
            }
        }

        if constexpr (!IsCompileTime) {
            spec.set_custom_spec<custom_spec>(custom);
        }

        return true;
    }

    // format 方法，执行格式化
    template <typename Context>
    void format(const std::chrono::time_point<Clock, Duration>& tp,
                Context& ctx, const format_spec& spec) const;
};

} // namespace mc::fmt

#endif // MC_FORMATTER_CHRONO_H