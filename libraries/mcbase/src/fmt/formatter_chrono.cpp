/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "securec.h"
#include <mc/fmt/format.h>
#include <mc/fmt/formatter_chrono.h>

namespace mc::fmt::detail {

static void pad_number(mc::string& out, int value, int width, pad_type pad)
{
    char buffer[32];
    int  len = snprintf_s(buffer, sizeof(buffer), sizeof(buffer), "%d", value);

    if (width <= 0 || len >= width || pad == pad_type::none) {
        out.append(buffer, len);
    } else {
        char fill_char  = (pad == pad_type::space) ? ' ' : '0';
        int  fill_count = width - len;
        out.append(fill_count, fill_char);
        out.append(buffer, len);
    }
}

void format_float(mc::string& out, double value, int precision, bool has_precision)
{
    detail::format_spec spec;
    format_context      ctx(out);
    if (has_precision) {
        spec.precision = precision;
    }

    detail::format_to(ctx, spec, value);
}

duration_format_handler_base::duration_format_handler_base(mc::string& output, int precision, bool has_precision)
    : m_output(output), m_precision(precision), m_has_precision(has_precision)
{}

void duration_format_handler_base::on_text(mc::string_view text)
{
    m_output.append(text);
}

void duration_format_handler_base::on_year(numeric_system, pad_type)
{
    m_output += "0";
}

void duration_format_handler_base::on_short_year(numeric_system)
{
    m_output += "00";
}

void duration_format_handler_base::on_month(numeric_system, pad_type)
{
    m_output += "0";
}

void duration_format_handler_base::on_day(numeric_system, pad_type pad)
{
    int days = get_days();
    pad_number(m_output, days, 2, pad);
}

void duration_format_handler_base::on_hour_24(numeric_system, pad_type pad)
{
    int hours = get_hours() % 24;
    pad_number(m_output, hours, 2, pad);
}

void duration_format_handler_base::on_hour_12(numeric_system, pad_type pad)
{
    int hours = get_hours() % 12;
    if (hours == 0) {
        hours = 12;
    }
    pad_number(m_output, hours, 2, pad);
}

void duration_format_handler_base::on_minute(numeric_system, pad_type pad)
{
    int minutes = get_minutes() % 60;
    pad_number(m_output, minutes, 2, pad);
}

void duration_format_handler_base::on_second(numeric_system, pad_type pad)
{
    int seconds = get_seconds() % 60;
    pad_number(m_output, seconds, 2, pad);

    int subseconds = get_subseconds();
    if (subseconds != 0) {
        m_output += '.';
        int digits = get_subseconds_digits();
        pad_number(m_output, subseconds, digits, pad_type::zero);
    }
}

void duration_format_handler_base::on_datetime(numeric_system)
{
    on_iso_time();
}

void duration_format_handler_base::on_iso_date()
{
    m_output += "0000-00-00";
}

void duration_format_handler_base::on_iso_time()
{
    on_hour_24(numeric_system::standard, pad_type::zero);
    m_output += ':';
    on_minute(numeric_system::standard, pad_type::zero);
    m_output += ':';
    on_second(numeric_system::standard, pad_type::zero);
}

void duration_format_handler_base::on_12_hour_time()
{
    on_hour_12(numeric_system::standard, pad_type::zero);
    m_output += ':';
    on_minute(numeric_system::standard, pad_type::zero);
    m_output += ':';
    on_second(numeric_system::standard, pad_type::zero);
    m_output += ' ';
    on_am_pm();
}

void duration_format_handler_base::on_24_hour_time()
{
    on_hour_24(numeric_system::standard, pad_type::zero);
    m_output += ':';
    on_minute(numeric_system::standard, pad_type::zero);
}

void duration_format_handler_base::on_duration_value()
{
    output_duration_value();
}

void duration_format_handler_base::on_duration_unit()
{
    m_output += get_unit_str();
}

void duration_format_handler_base::on_am_pm()
{
    int hours = get_hours() % 24;
    m_output += (hours < 12) ? "AM" : "PM";
}

void duration_format_handler_base::on_timezone_offset(numeric_system, bool iso8601_colon_offset)
{
    if (iso8601_colon_offset) {
        m_output += "+00:00";
    } else {
        m_output += "+0000";
    }
}

void duration_format_handler_base::on_timezone_name(numeric_system)
{
    m_output += "UTC";
}

void duration_format_handler_base::on_invalid_spec()
{
    // 在实际格式化时不需要做任何操作，错误检查在编译期完成
}

void duration_format_handler_base::on_incomplete_spec()
{
    // 在实际格式化时不需要做任何操作，错误检查在编译期完成
}

time_point_format_handler_base::time_point_format_handler_base(mc::string& output) : m_output(output)
{}

void time_point_format_handler_base::on_text(mc::string_view text)
{
    m_output.append(text);
}

void time_point_format_handler_base::on_year(numeric_system, pad_type pad)
{
    ensure_tm();
    pad_number(m_output, 1900 + m_tm.tm_year, 4, pad);
}

void time_point_format_handler_base::on_short_year(numeric_system)
{
    ensure_tm();
    pad_number(m_output, (1900 + m_tm.tm_year) % 100, 2, pad_type::zero);
}

void time_point_format_handler_base::on_month(numeric_system, pad_type pad)
{
    ensure_tm();
    pad_number(m_output, m_tm.tm_mon + 1, 2, pad);
}

void time_point_format_handler_base::on_day(numeric_system, pad_type pad)
{
    ensure_tm();
    pad_number(m_output, m_tm.tm_mday, 2, pad);
}

void time_point_format_handler_base::on_hour_24(numeric_system, pad_type pad)
{
    ensure_tm();
    pad_number(m_output, m_tm.tm_hour, 2, pad);
}

void time_point_format_handler_base::on_hour_12(numeric_system, pad_type pad)
{
    ensure_tm();
    int hour = m_tm.tm_hour % 12;
    if (hour == 0) {
        hour = 12;
    }
    pad_number(m_output, hour, 2, pad);
}

void time_point_format_handler_base::on_minute(numeric_system, pad_type pad)
{
    ensure_tm();
    pad_number(m_output, m_tm.tm_min, 2, pad);
}

void time_point_format_handler_base::on_second(numeric_system, pad_type pad)
{
    ensure_tm();
    pad_number(m_output, m_tm.tm_sec, 2, pad);

    uint32_t subsec_ns = get_subseconds_ns();
    if (subsec_ns > 0) {
        m_output += '.';
        mc::string subsec_str = mc::to_string(subsec_ns);
        // 补齐前导零
        while (subsec_str.length() < 9) {
            subsec_str = "0" + subsec_str;
        }
        // 移除尾随零，但至少保留1位
        while (subsec_str.length() > 1 && subsec_str.back() == '0') {
            subsec_str.pop_back();
        }
        m_output += subsec_str;
    }
}

void time_point_format_handler_base::on_datetime(numeric_system)
{
    on_iso_time();
}

void time_point_format_handler_base::on_iso_date()
{
    on_year(numeric_system::standard, pad_type::zero);
    m_output += '-';
    on_month(numeric_system::standard, pad_type::zero);
    m_output += '-';
    on_day(numeric_system::standard, pad_type::zero);
}

void time_point_format_handler_base::on_iso_time()
{
    on_hour_24(numeric_system::standard, pad_type::zero);
    m_output += ':';
    on_minute(numeric_system::standard, pad_type::zero);
    m_output += ':';
    on_second(numeric_system::standard, pad_type::zero);
}

void time_point_format_handler_base::on_12_hour_time()
{
    on_hour_12(numeric_system::standard, pad_type::zero);
    m_output += ':';
    on_minute(numeric_system::standard, pad_type::zero);
    m_output += ':';
    on_second(numeric_system::standard, pad_type::zero);
    m_output += ' ';
    on_am_pm();
}

void time_point_format_handler_base::on_24_hour_time()
{
    on_hour_24(numeric_system::standard, pad_type::zero);
    m_output += ':';
    on_minute(numeric_system::standard, pad_type::zero);
}

void time_point_format_handler_base::on_duration_value()
{
    // time_point 不支持 duration 值
}

void time_point_format_handler_base::on_duration_unit()
{
    // time_point 不支持 duration 单位
}

void time_point_format_handler_base::on_am_pm()
{
    ensure_tm();
    m_output += (m_tm.tm_hour < 12) ? "AM" : "PM";
}

void time_point_format_handler_base::on_timezone_offset(numeric_system, bool iso8601_colon_offset)
{
    ensure_tm();
    // C++20 formatter<sys_time>: %z 为 0min；%Ez/%Oz 在时与分之间插入 ':'（零偏移为 +00:00）
    if (iso8601_colon_offset) {
        m_output += "+00:00";
    } else {
        m_output += "+0000";
    }
}

void time_point_format_handler_base::on_timezone_name(numeric_system)
{
    // C++20 std::formatter<std::chrono::sys_time>: %Z 替换为 "UTC"（CharT 拓宽至 char 即本字面值）
    m_output += "UTC";
}

void time_point_format_handler_base::on_invalid_spec()
{
    // 在实际格式化时不需要做任何操作，错误检查在编译期完成
}

void time_point_format_handler_base::on_incomplete_spec()
{
    // 在实际格式化时不需要做任何操作，错误检查在编译期完成
}

} // namespace mc::fmt::detail