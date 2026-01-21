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

/**
 * @file time.cpp
 * @brief 基于标准库chrono直接封装的时间模块
 */
#include "mc/time.h"
#include "mc/exception.h"
#include "mc/variant.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <regex>
#include <sstream>

namespace mc {

// 定义跨平台timegm替代函数
// timegm是一个GNU扩展，非标准C/C++函数
static time_t portable_timegm(struct tm* tm) {
    // 保存当前时区
    char* tz = getenv("TZ");

    // 设置为UTC时区
    setenv("TZ", "", 1);
    tzset();

    // 调用mktime，它会根据当前时区计算time_t
    time_t ret = mktime(tm);

    // 恢复之前的时区
    if (tz) {
        setenv("TZ", tz, 1);
    } else {
        unsetenv("TZ");
    }
    tzset();

    return ret;
}

// 工具函数：解析ISO 8601日期时间字符串
// 支持格式："2020-01-01T12:30:45" 或 "2020-01-01T12:30:45.123"
static bool parse_iso_datetime(const std::string& iso_str,
                               std::tm&           tm_result,
                               int64_t&           milliseconds) {
    // 正则表达式匹配ISO 8601格式
    std::regex  iso_regex(R"((\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(?:\.(\d{1,3}))?Z?)");
    std::smatch matches;

    if (!std::regex_match(iso_str, matches, iso_regex)) {
        return false;
    }

    // 解析年月日时分秒
    tm_result.tm_year = std::stoi(matches[1].str()) - 1900; // 年份从1900年开始
    tm_result.tm_mon  = std::stoi(matches[2].str()) - 1;    // 月份从0开始
    tm_result.tm_mday = std::stoi(matches[3].str());
    tm_result.tm_hour = std::stoi(matches[4].str());
    tm_result.tm_min  = std::stoi(matches[5].str());
    tm_result.tm_sec  = std::stoi(matches[6].str());

    // 解析毫秒部分（如果存在）
    milliseconds = 0;
    if (matches[7].matched) {
        std::string ms_str = matches[7].str();
        // 补齐到三位
        if (ms_str.length() == 1) {
            ms_str += "00";
        } else if (ms_str.length() == 2) {
            ms_str += "0";
        }
        milliseconds = std::stoi(ms_str);
    }

    return true;
}

time_point time_point::now() {
    return time_point(std::chrono::steady_clock::now());
}

time_point time_point::from_iso_string(const std::string& s) {
    try {
        // 检查字符串是否为空
        if (s.empty()) {
            MC_THROW(mc::parse_error_exception, "无法将空字符串转换为时间点");
        }

        std::tm tm_result         = {};
        int64_t milliseconds_part = 0;

        if (!parse_iso_datetime(s, tm_result, milliseconds_part)) {
            MC_THROW(mc::parse_error_exception, "无效的ISO日期时间格式: ${iso}", ("iso", s));
        }

        // 转换为time_t
        std::time_t time_secs = portable_timegm(&tm_result); // 使用自定义函数，假定输入是UTC时间

        if (time_secs == -1) {
            MC_THROW(mc::parse_error_exception, "无法转换时间: ${iso}", ("iso", s));
        }

        // 计算毫秒级时间
        int64_t ms_since_epoch = time_secs * 1000 + milliseconds_part;

        return time_point(milliseconds(ms_since_epoch));
    } catch (const mc::exception& e) {
        std::string msg = "无法将ISO格式字符串转换为时间点: ";
        msg += e.what();
        throw mc::exception(e.code(), msg);
    } catch (const std::exception& e) {
        MC_THROW(mc::parse_error_exception, "无法将ISO格式字符串转换为时间点: ${error}", ("error", e.what()));
    }
}

// 更新日期/时间的特定部分
template <int offset>
void update_time_part(std::string& str, int value, int& cache_value) {
    if (value != cache_value) {
        str[offset]     = '0' + (value / 10);
        str[offset + 1] = '0' + (value % 10);
        cache_value     = value;
    }
};

/**
 * @brief 时间缓存结构，用于存储时间格式化状态
 * @tparam WithMilliseconds 是否包含毫秒部分
 */
template <bool WithMilliseconds>
struct time_cache {
    // 根据是否包含毫秒预设字符串初始值和长度
    std::string str      = WithMilliseconds ? "0000-00-00T00:00:00.000" : "0000-00-00T00:00:00";
    int64_t     ms_epoch = -1;
    int64_t     seconds  = -1;
    int         minute   = -1;
    int         hour     = -1;
    int         day      = -1;
    int         month    = -1;
    int         year     = -1;
};

/**
 * @brief 格式化时间为ISO 8601字符串，使用缓存提高效率（仅处理到秒级别）
 *
 * @tparam WithMilliseconds 是否包含毫秒部分（决定缓存结构的字符串长度）
 * @param seconds 从纪元开始的秒数
 * @param cache 时间缓存结构
 * @return std::string_view 指向缓存字符串的视图
 */
template <bool WithMilliseconds>
static std::string_view format_cached_iso_datetime(int64_t seconds, time_cache<WithMilliseconds>& cache) {
    if (seconds < 0) {
        MC_THROW(mc::parse_error_exception, "不支持负时间点转换为ISO字符串");
    }

    bool seconds_changed = (seconds != cache.seconds);

    if (seconds_changed) {
        // 只有在秒数变化时才需要获取和处理时间结构
        std::time_t time_secs = static_cast<std::time_t>(seconds);
        std::tm*    tm_info   = std::gmtime(&time_secs);

        if (!tm_info) {
            MC_THROW(mc::parse_error_exception, "无法转换时间戳: ${ts}", ("ts", seconds));
        }

        // 更新年份（特殊处理，因为是4位数）
        int curr_year = tm_info->tm_year + 1900;
        if (curr_year != cache.year) {
            cache.str[0] = '0' + (curr_year / 1000);
            cache.str[1] = '0' + (curr_year / 100 % 10);
            cache.str[2] = '0' + (curr_year / 10 % 10);
            cache.str[3] = '0' + (curr_year % 10);
            cache.year   = curr_year;
        }

        // 使用模板函数更新其他时间部分
        update_time_part<5>(cache.str, tm_info->tm_mon + 1, cache.month);
        update_time_part<8>(cache.str, tm_info->tm_mday, cache.day);
        update_time_part<11>(cache.str, tm_info->tm_hour, cache.hour);
        update_time_part<14>(cache.str, tm_info->tm_min, cache.minute);

        // 秒总是更新(秒数变化是我们进入这个分支的条件)
        cache.str[17] = '0' + (tm_info->tm_sec / 10);
        cache.str[18] = '0' + (tm_info->tm_sec % 10);

        // 更新秒数缓存
        cache.seconds = seconds;
    }

    // 返回缓存字符串，不处理毫秒
    return std::string_view(cache.str);
}

std::string_view time_point::to_string() const {
    try {
        // 使用带毫秒的缓存
        thread_local time_cache<true> cache;

        // 获取时间点的毫秒数
        int64_t ms_since_epoch = time_since_epoch().count();

        if (ms_since_epoch < 0) {
            MC_THROW(mc::bad_cast_exception, "不支持负时间点转换为ISO字符串");
        }

        // 分离秒和毫秒部分
        int64_t seconds = ms_since_epoch / 1000;
        int64_t millis  = ms_since_epoch % 1000;

        // 格式化日期时间部分（到秒级别）
        format_cached_iso_datetime<true>(seconds, cache);

        // 在这里处理毫秒部分
        int64_t cached_ms_epoch = cache.ms_epoch;
        if (cached_ms_epoch != ms_since_epoch) {
            cache.str[20]  = '0' + (millis / 100);
            cache.str[21]  = '0' + (millis / 10 % 10);
            cache.str[22]  = '0' + (millis % 10);
            cache.ms_epoch = ms_since_epoch;
        }

        return std::string_view(cache.str);
    } catch (const std::exception& e) {
        MC_THROW(mc::bad_cast_exception, "转换时间点为ISO字符串失败: ${error}", ("error", e.what()));
    }
}

time_point::operator std::string_view() const {
    return to_string();
}

std::string_view time_point::to_iso_string() const {
    return to_string();
}

time_point_sec time_point_sec::from_iso_string(const std::string& s) {
    try {
        std::tm tm_result    = {};
        int64_t milliseconds = 0; // 秒级精度忽略毫秒部分

        if (!parse_iso_datetime(s, tm_result, milliseconds)) {
            MC_THROW(mc::parse_error_exception, "无效的ISO日期时间格式: ${iso}", ("iso", s));
        }

        // 转换为time_t
        std::time_t time_secs = portable_timegm(&tm_result); // 使用自定义函数，假定输入是UTC时间

        if (time_secs == -1) {
            MC_THROW(mc::parse_error_exception, "无法转换时间: ${iso}", ("iso", s));
        }

        return time_point_sec(static_cast<uint32_t>(time_secs));
    } catch (const std::exception& e) {
        MC_THROW(mc::parse_error_exception, "从ISO字符串转换为时间点失败: ${error}", ("error", e.what()));
    }
}

std::string_view time_point_sec::to_string() const {
    try {
        // 使用不带毫秒的缓存
        thread_local time_cache<false> cache;

        // 秒级精度
        int64_t seconds = static_cast<int64_t>(m_utc_seconds);

        if (seconds < 0) {
            MC_THROW(mc::parse_error_exception, "不支持负时间点转换为ISO字符串");
        }

        // 格式化日期时间部分（不包含毫秒）
        return format_cached_iso_datetime<false>(seconds, cache);
    } catch (const std::exception& e) {
        MC_THROW(mc::parse_error_exception, "无法将ISO格式字符串转换为时间点: ${error}", ("error", e.what()));
    }
}

time_point_sec time_point_sec::now() {
    auto now_tp = std::chrono::system_clock::now();
    return time_point_sec(now_tp);
}

time_point_sec::operator std::string_view() const {
    return to_string();
}

std::string_view time_point_sec::to_iso_string() const {
    return to_string();
}

// Variant转换实现
void to_variant(const milliseconds& ms, variant& v) {
    v = ms.count();
}

void from_variant(const variant& v, milliseconds& ms) {
    ms = milliseconds(v.as<int64_t>());
}

void to_variant(const time_point& tp, variant& v) {
    v = std::string(tp.to_string());
}

void from_variant(const variant& v, time_point& tp) {
    tp = time_point::from_iso_string(v.as<std::string>());
}

void to_variant(const time_point_sec& tps, variant& v) {
    v = std::string(tps.to_string());
}

void from_variant(const variant& v, time_point_sec& tps) {
    tps = time_point_sec::from_iso_string(v.as<std::string>());
}

} // namespace mc