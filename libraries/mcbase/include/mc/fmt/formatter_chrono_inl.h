#ifndef MC_FMT_FORMATTER_CHRONO_INL_H
#define MC_FMT_FORMATTER_CHRONO_INL_H
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

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

namespace mc::fmt {
namespace detail {
MC_API void format_float(mc::string& out, double value, int precision, bool has_precision);

class MC_API duration_format_handler_base {
public:
    duration_format_handler_base(mc::string& output, int precision, bool has_precision);
    virtual ~duration_format_handler_base() = default;

    void on_text(mc::string_view text);
    void on_year(numeric_system, pad_type);
    void on_short_year(numeric_system);
    void on_month(numeric_system, pad_type);
    void on_day(numeric_system, pad_type);
    void on_hour_24(numeric_system, pad_type);
    void on_hour_12(numeric_system, pad_type);
    void on_minute(numeric_system, pad_type);
    void on_second(numeric_system, pad_type);
    void on_datetime(numeric_system);
    void on_iso_date();
    void on_iso_time();
    void on_12_hour_time();
    void on_24_hour_time();
    void on_duration_value();
    void on_duration_unit();
    void on_am_pm();
    void on_timezone_offset(numeric_system, bool iso8601_colon_offset);
    void on_timezone_name(numeric_system);

    // 错误处理方法
    void on_invalid_spec();
    void on_incomplete_spec();

protected:
    mc::string& m_output;
    int         m_precision;
    bool        m_has_precision;

    virtual int         get_days()              = 0;
    virtual int         get_hours()             = 0;
    virtual int         get_minutes()           = 0;
    virtual int         get_seconds()           = 0;
    virtual int         get_subseconds()        = 0;
    virtual int         get_subseconds_digits() = 0;
    virtual const char* get_unit_str() const    = 0;
    virtual void        output_duration_value() = 0;
};

class MC_API time_point_format_handler_base {
public:
    time_point_format_handler_base(mc::string& output);
    ~time_point_format_handler_base() = default;

    void on_text(mc::string_view text);
    void on_year(numeric_system, pad_type);
    void on_short_year(numeric_system);
    void on_month(numeric_system, pad_type);
    void on_day(numeric_system, pad_type);
    void on_hour_24(numeric_system, pad_type);
    void on_hour_12(numeric_system, pad_type);
    void on_minute(numeric_system, pad_type);
    void on_second(numeric_system, pad_type);
    void on_datetime(numeric_system);
    void on_iso_date();
    void on_iso_time();
    void on_12_hour_time();
    void on_24_hour_time();
    void on_duration_value();
    void on_duration_unit();
    void on_am_pm();
    void on_timezone_offset(numeric_system, bool iso8601_colon_offset);
    void on_timezone_name(numeric_system);

    // 错误处理方法
    void on_invalid_spec();
    void on_incomplete_spec();

protected:
    mc::string& m_output;
    std::tm     m_tm{}; // 显式零初始化
    bool        m_tm_valid = false;
    std::time_t m_time_t   = 0;

    virtual void     ensure_tm()               = 0;
    virtual uint32_t get_subseconds_ns() const = 0;
};

template <typename Rep, typename Period>
class duration_format_handler : public duration_format_handler_base {
private:
    const std::chrono::duration<Rep, Period>& m_duration;

public:
    duration_format_handler(const std::chrono::duration<Rep, Period>& d, mc::string& output, int precision,
                            bool has_precision)
        : duration_format_handler_base(output, precision, has_precision), m_duration(d)
    {}

protected:
    int get_days() override
    {
        using namespace std::chrono;
        auto total_days = duration_cast<duration<long long, std::ratio<86400>>>(m_duration).count();
        return static_cast<int>(total_days);
    }

    int get_hours() override
    {
        using namespace std::chrono;
        auto total_hours = duration_cast<hours>(m_duration).count();
        return static_cast<int>(total_hours);
    }

    int get_minutes() override
    {
        using namespace std::chrono;
        auto total_minutes = duration_cast<minutes>(m_duration).count();
        return static_cast<int>(total_minutes);
    }

    int get_seconds() override
    {
        using namespace std::chrono;
        auto total_seconds = duration_cast<seconds>(m_duration).count();
        return static_cast<int>(total_seconds);
    }

    int get_subseconds() override
    {
        using namespace std::chrono;
        auto total_seconds = duration_cast<seconds>(m_duration);
        auto subsec        = m_duration - duration_cast<std::chrono::duration<Rep, Period>>(total_seconds);
        return static_cast<int>(subsec.count());
    }

    int get_subseconds_digits() override
    {
        if constexpr (std::is_same_v<Period, std::milli>) {
            return 3;
        } else if constexpr (std::is_same_v<Period, std::micro>) {
            return 6;
        } else if constexpr (std::is_same_v<Period, std::nano>) {
            return 9;
        } else {
            long long factor = Period::den / Period::num;
            int       digits = 0;
            while (factor > 1) {
                factor /= 10;
                digits++;
            }
            return digits;
        }
    }

    const char* get_unit_str() const override
    {
        return detail::get_unit_str<Period>();
    }

    void output_duration_value() override
    {
        if constexpr (std::is_floating_point_v<Rep>) {
            detail::format_float(m_output, static_cast<double>(m_duration.count()), m_precision, m_has_precision);
        } else {
            std::ostringstream os;
            os << m_duration.count();
            m_output.append(os.str());
        }
    }
};

template <typename Clock, typename Duration>
class time_point_format_handler : public time_point_format_handler_base {
private:
    const std::chrono::time_point<Clock, Duration>& m_time_point;

public:
    time_point_format_handler(const std::chrono::time_point<Clock, Duration>& tp, mc::string& output)
        : time_point_format_handler_base(output), m_time_point(tp)
    {}

protected:
    void ensure_tm() override
    {
        if (!this->m_tm_valid) {
            auto sys_tp      = std::chrono::time_point_cast<std::chrono::system_clock::duration>(m_time_point);
            this->m_time_t   = std::chrono::system_clock::to_time_t(sys_tp); // 赋值
            this->m_tm       = *std::gmtime(&this->m_time_t);
            this->m_tm_valid = true;
        }
    }
    uint32_t get_subseconds_ns() const override
    {
        auto time_since_epoch    = m_time_point.time_since_epoch();
        auto seconds_since_epoch = std::chrono::duration_cast<std::chrono::seconds>(time_since_epoch);
        auto subseconds          = time_since_epoch - seconds_since_epoch;
        if (subseconds.count() > 0) {
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(subseconds);
            return static_cast<uint32_t>(ns.count());
        }
        return 0;
    }
};

} // namespace detail

template <typename Rep, typename Period>
template <typename Context>
void formatter<std::chrono::duration<Rep, Period>>::format(const std::chrono::duration<Rep, Period>& d, Context& ctx,
                                                           const format_spec& spec) const
{
    auto& out   = ctx.out();
    auto  count = d.count();

    // 处理负数
    bool is_negative = count < 0;
    if (is_negative) {
        out.push_back('-');
    }

    auto abs_duration = is_negative ? -d : d;

    auto& custom = spec.get_custom_spec<custom_spec>();
    if (custom.format_str.empty()) {
        // 默认格式：值 + 单位
        if (std::is_floating_point_v<Rep>) {
            detail::format_float(out, static_cast<double>(std::abs(count)), custom.precision, custom.has_precision);
        } else {
            std::ostringstream os;
            os << std::abs(count);
            out.append(os.str());
        }
        out.append(detail::get_unit_str<Period>());
    } else {
        detail::duration_format_handler handler(abs_duration, out, custom.precision, custom.has_precision);
        detail::parse_chrono_format(custom.format_str, handler);
    }
}

template <typename Clock, typename Duration>
template <typename Context>
void formatter<std::chrono::time_point<Clock, Duration>>::format(const std::chrono::time_point<Clock, Duration>& tp,
                                                                 Context& ctx, const format_spec& spec) const
{
    auto& out = ctx.out();

    auto& custom = spec.get_custom_spec<custom_spec>();
    if (custom.format_str.empty()) {
        // 对齐 C++20 sys_time 的 operator<<：等价于 format("%F %T")（UTC / gmtime，含 %T 子秒规则）
        detail::time_point_format_handler handler(tp, out);
        detail::parse_chrono_format(mc::string_view("%F %T"), handler);
    } else {
        detail::time_point_format_handler handler(tp, out);
        detail::parse_chrono_format(custom.format_str, handler);
    }
}

} // namespace mc::fmt

#endif // MC_FMT_FORMATTER_CHRONO_INL_H