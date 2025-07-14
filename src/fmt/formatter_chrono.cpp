#include <mc/fmt/format.h>
#include <mc/fmt/formatter_chrono.h>

namespace mc::fmt::detail {

static void pad_number(std::string& out, int value, int width, pad_type pad) {
    char buffer[32];
    int  len = std::snprintf(buffer, sizeof(buffer), "%d", value);

    if (width <= 0 || len >= width || pad == pad_type::none) {
        out.append(buffer, len);
    } else {
        char fill_char  = (pad == pad_type::space) ? ' ' : '0';
        int  fill_count = width - len;
        out.append(fill_count, fill_char);
        out.append(buffer, len);
    }
}

void format_float(std::string& out, double value, int precision, bool has_precision) {
    detail::format_spec spec;
    format_context      ctx(out);
    if (has_precision) {
        spec.precision = precision;
    }

    detail::format_to(ctx, spec, value);
}

duration_format_handler_base::duration_format_handler_base(std::string& output, int precision, bool has_precision)
    : m_output(output), m_precision(precision), m_has_precision(has_precision) {
}

void duration_format_handler_base::on_text(std::string_view text) {
    m_output.append(text);
}

void duration_format_handler_base::on_year(numeric_system, pad_type) {
    m_output += "0";
}

void duration_format_handler_base::on_short_year(numeric_system) {
    m_output += "00";
}

void duration_format_handler_base::on_month(numeric_system, pad_type) {
    m_output += "0";
}

void duration_format_handler_base::on_day(numeric_system, pad_type pad) {
    int days = get_days();
    pad_number(m_output, days, 2, pad);
}

void duration_format_handler_base::on_hour_24(numeric_system, pad_type pad) {
    int hours = get_hours() % 24;
    pad_number(m_output, hours, 2, pad);
}

void duration_format_handler_base::on_hour_12(numeric_system, pad_type pad) {
    int hours = get_hours() % 12;
    if (hours == 0) {
        hours = 12;
    }
    pad_number(m_output, hours, 2, pad);
}

void duration_format_handler_base::on_minute(numeric_system, pad_type pad) {
    int minutes = get_minutes() % 60;
    pad_number(m_output, minutes, 2, pad);
}

void duration_format_handler_base::on_second(numeric_system, pad_type pad) {
    int seconds = get_seconds() % 60;
    pad_number(m_output, seconds, 2, pad);

    int subseconds = get_subseconds();
    if (subseconds != 0) {
        m_output += '.';
        int digits = get_subseconds_digits();
        pad_number(m_output, subseconds, digits, pad_type::zero);
    }
}

void duration_format_handler_base::on_datetime(numeric_system) {
    on_iso_time();
}

void duration_format_handler_base::on_iso_date() {
    m_output += "0000-00-00";
}

void duration_format_handler_base::on_iso_time() {
    on_hour_24(numeric_system::standard, pad_type::zero);
    m_output += ':';
    on_minute(numeric_system::standard, pad_type::zero);
    m_output += ':';
    on_second(numeric_system::standard, pad_type::zero);
}

void duration_format_handler_base::on_12_hour_time() {
    on_hour_12(numeric_system::standard, pad_type::zero);
    m_output += ':';
    on_minute(numeric_system::standard, pad_type::zero);
    m_output += ':';
    on_second(numeric_system::standard, pad_type::zero);
    m_output += ' ';
    on_am_pm();
}

void duration_format_handler_base::on_24_hour_time() {
    on_hour_24(numeric_system::standard, pad_type::zero);
    m_output += ':';
    on_minute(numeric_system::standard, pad_type::zero);
}

void duration_format_handler_base::on_duration_value() {
    output_duration_value();
}

void duration_format_handler_base::on_duration_unit() {
    m_output += get_unit_str();
}

void duration_format_handler_base::on_am_pm() {
    int hours = get_hours() % 24;
    m_output += (hours < 12) ? "AM" : "PM";
}

void duration_format_handler_base::on_timezone_offset(numeric_system) {
    m_output += "+0000";
}

void duration_format_handler_base::on_timezone_name(numeric_system) {
    m_output += "UTC";
}

void duration_format_handler_base::on_invalid_spec() {
    // 在实际格式化时不需要做任何操作，错误检查在编译期完成
}

void duration_format_handler_base::on_incomplete_spec() {
    // 在实际格式化时不需要做任何操作，错误检查在编译期完成
}

time_point_format_handler_base::time_point_format_handler_base(std::string& output)
    : m_output(output) {
}

void time_point_format_handler_base::on_text(std::string_view text) {
    m_output.append(text);
}

void time_point_format_handler_base::on_year(numeric_system, pad_type pad) {
    ensure_tm();
    pad_number(m_output, 1900 + m_tm.tm_year, 4, pad);
}

void time_point_format_handler_base::on_short_year(numeric_system) {
    ensure_tm();
    pad_number(m_output, (1900 + m_tm.tm_year) % 100, 2, pad_type::zero);
}

void time_point_format_handler_base::on_month(numeric_system, pad_type pad) {
    ensure_tm();
    pad_number(m_output, m_tm.tm_mon + 1, 2, pad);
}

void time_point_format_handler_base::on_day(numeric_system, pad_type pad) {
    ensure_tm();
    pad_number(m_output, m_tm.tm_mday, 2, pad);
}

void time_point_format_handler_base::on_hour_24(numeric_system, pad_type pad) {
    ensure_tm();
    pad_number(m_output, m_tm.tm_hour, 2, pad);
}

void time_point_format_handler_base::on_hour_12(numeric_system, pad_type pad) {
    ensure_tm();
    int hour = m_tm.tm_hour % 12;
    if (hour == 0) {
        hour = 12;
    }
    pad_number(m_output, hour, 2, pad);
}

void time_point_format_handler_base::on_minute(numeric_system, pad_type pad) {
    ensure_tm();
    pad_number(m_output, m_tm.tm_min, 2, pad);
}

void time_point_format_handler_base::on_second(numeric_system, pad_type pad) {
    ensure_tm();
    pad_number(m_output, m_tm.tm_sec, 2, pad);

    uint32_t subsec_ns = get_subseconds_ns();
    if (subsec_ns > 0) {
        m_output += '.';
        std::string subsec_str = std::to_string(subsec_ns);
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

void time_point_format_handler_base::on_datetime(numeric_system) {
    on_iso_time();
}

void time_point_format_handler_base::on_iso_date() {
    on_year(numeric_system::standard, pad_type::zero);
    m_output += '-';
    on_month(numeric_system::standard, pad_type::zero);
    m_output += '-';
    on_day(numeric_system::standard, pad_type::zero);
}

void time_point_format_handler_base::on_iso_time() {
    on_hour_24(numeric_system::standard, pad_type::zero);
    m_output += ':';
    on_minute(numeric_system::standard, pad_type::zero);
    m_output += ':';
    on_second(numeric_system::standard, pad_type::zero);
}

void time_point_format_handler_base::on_12_hour_time() {
    on_hour_12(numeric_system::standard, pad_type::zero);
    m_output += ':';
    on_minute(numeric_system::standard, pad_type::zero);
    m_output += ':';
    on_second(numeric_system::standard, pad_type::zero);
    m_output += ' ';
    on_am_pm();
}

void time_point_format_handler_base::on_24_hour_time() {
    on_hour_24(numeric_system::standard, pad_type::zero);
    m_output += ':';
    on_minute(numeric_system::standard, pad_type::zero);
}

void time_point_format_handler_base::on_duration_value() {
    // time_point 不支持 duration 值
}

void time_point_format_handler_base::on_duration_unit() {
    // time_point 不支持 duration 单位
}
// 平台判断宏
#if defined(__APPLE__) || defined(__FreeBSD__) ||  \
    defined(__NetBSD__) || defined(__OpenBSD__) || \
    (defined(__linux__) && defined(__GLIBC__))
#define MC_HAS_TM_GMTOFF 1
#else
#define MC_HAS_TM_GMTOFF 0
#endif

void time_point_format_handler_base::on_am_pm() {
    ensure_tm();
    m_output += (m_tm.tm_hour < 12) ? "AM" : "PM";
}

void time_point_format_handler_base::on_timezone_offset(numeric_system) {
    ensure_tm();

    // 获取本地时间结构
    std::tm local_tm = *std::localtime(&m_time_t);

    int offset_minutes = 0;
#if MC_HAS_TM_GMTOFF
    // 支持 tm_gmtoff 的平台
    offset_minutes = local_tm.tm_gmtoff / 60;
#else
    // 通过比较 UTC 和本地时间来计算偏移量
    std::tm utc_tm    = *std::gmtime(&m_time_t);
    int     hour_diff = local_tm.tm_hour - utc_tm.tm_hour;
    int     min_diff  = local_tm.tm_min - utc_tm.tm_min;
    if (hour_diff > 12) {
        hour_diff -= 24;
    } else if (hour_diff < -12) {
        hour_diff += 24;
    }
    offset_minutes = hour_diff * 60 + min_diff;
#endif
    int hours   = std::abs(offset_minutes) / 60;
    int minutes = std::abs(offset_minutes) % 60;
    m_output += (offset_minutes >= 0 ? "+" : "-");
    m_output += (hours < 10 ? "0" : "") + std::to_string(hours);
    m_output += (minutes < 10 ? "0" : "") + std::to_string(minutes);
}

void time_point_format_handler_base::on_timezone_name(numeric_system) {
    ensure_tm();

    std::tm     local_tm = *std::localtime(&m_time_t);
    const char* tz_name  = nullptr;
#if MC_HAS_TM_GMTOFF
    tz_name = local_tm.tm_zone;
#else
    tz_name = getenv("TZ");
    if (!tz_name) {
        std::tm utc_tm    = *std::gmtime(&m_time_t);
        int     hour_diff = local_tm.tm_hour - utc_tm.tm_hour;
        int     min_diff  = local_tm.tm_min - utc_tm.tm_min;
        if (hour_diff > 12) {
            hour_diff -= 24;
        } else if (hour_diff < -12) {
            hour_diff += 24;
        }
        int offset_minutes = hour_diff * 60 + min_diff;
        if (offset_minutes == 0) {
            tz_name = "UTC";
        } else if (offset_minutes == 480) {
            tz_name = "CST";
        } else if (offset_minutes == -300) {
            tz_name = "EST";
        } else if (offset_minutes == -360) {
            tz_name = "CST";
        } else if (offset_minutes == -420) {
            tz_name = "MST";
        } else if (offset_minutes == -480) {
            tz_name = "PST";
        } else {
            int hours   = std::abs(offset_minutes) / 60;
            int minutes = std::abs(offset_minutes) % 60;
            m_output += (offset_minutes >= 0 ? "+" : "-");
            m_output += (hours < 10 ? "0" : "") + std::to_string(hours);
            m_output += (minutes < 10 ? "0" : "") + std::to_string(minutes);
            return;
        }
    }
#endif
    if (tz_name) {
        m_output += tz_name;
    } else {
        on_timezone_offset(numeric_system::standard);
    }
}

void time_point_format_handler_base::on_invalid_spec() {
    // 在实际格式化时不需要做任何操作，错误检查在编译期完成
}

void time_point_format_handler_base::on_incomplete_spec() {
    // 在实际格式化时不需要做任何操作，错误检查在编译期完成
}

} // namespace mc::fmt::detail