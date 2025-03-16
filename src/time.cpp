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
#include "mc/variant.h"
#include "mc/exception.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <regex>

namespace mc {

// 定义跨平台timegm替代函数
// timegm是一个GNU扩展，非标准C/C++函数
static time_t portable_timegm(struct tm *tm) {
    // 保存当前时区
    char *tz = getenv("TZ");
    
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
                              std::tm& tm_result, 
                              int64_t& milliseconds) {
    // 正则表达式匹配ISO 8601格式
    std::regex iso_regex(R"((\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(?:\.(\d{1,3}))?Z?)");
    std::smatch matches;
    
    if (!std::regex_match(iso_str, matches, iso_regex)) {
        return false;
    }
    
    // 解析年月日时分秒
    tm_result.tm_year = std::stoi(matches[1].str()) - 1900; // 年份从1900年开始
    tm_result.tm_mon = std::stoi(matches[2].str()) - 1;     // 月份从0开始
    tm_result.tm_mday = std::stoi(matches[3].str());
    tm_result.tm_hour = std::stoi(matches[4].str());
    tm_result.tm_min = std::stoi(matches[5].str());
    tm_result.tm_sec = std::stoi(matches[6].str());
    
    // 解析毫秒部分（如果存在）
    milliseconds = 0;
    if (matches[7].matched) {
        std::string ms_str = matches[7].str();
        // 补齐到三位
        if (ms_str.length() == 1) ms_str += "00";
        else if (ms_str.length() == 2) ms_str += "0";
        milliseconds = std::stoi(ms_str);
    }
    
    return true;
}

// 工具函数：将tm结构和毫秒转换为ISO 8601字符串
static std::string tm_to_iso_string(const std::tm& tm, int64_t milliseconds = 0, bool include_ms = false) {
    std::ostringstream oss;
    
    // 格式化年月日时分秒
    oss << std::setfill('0')
        << std::setw(4) << (tm.tm_year + 1900) << '-'
        << std::setw(2) << (tm.tm_mon + 1) << '-'
        << std::setw(2) << tm.tm_mday << 'T'
        << std::setw(2) << tm.tm_hour << ':'
        << std::setw(2) << tm.tm_min << ':'
        << std::setw(2) << tm.tm_sec;
    
    // 如果需要，添加毫秒部分
    if (include_ms && milliseconds > 0) {
        oss << '.' << std::setw(3) << milliseconds;
    }
    
    return oss.str();
}

time_point time_point::now() {
    auto now_tp = std::chrono::system_clock::now();
    return time_point(now_tp);
}

time_point time_point::from_iso_string(const std::string& s) {
    try {
        // 检查字符串是否为空
        if (s.empty()) {
            MC_THROW(mc::parse_error_exception, "无法将空字符串转换为时间点");
        }
        
        std::tm tm_result = {};
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

time_point::operator std::string() const {
    try {
        // 获取时间点的毫秒数
        int64_t ms_since_epoch = time_since_epoch().count();
        
        if (ms_since_epoch < 0) {
            MC_THROW(mc::parse_error_exception, "不支持负时间点转换为ISO字符串");
        }
        
        // 分离秒和毫秒部分
        int64_t seconds = ms_since_epoch / 1000;
        int64_t millis = ms_since_epoch % 1000;
        
        // 转换为时间结构
        std::time_t time_secs = static_cast<std::time_t>(seconds);
        std::tm* tm_info = std::gmtime(&time_secs);
        
        if (!tm_info) {
            MC_THROW(mc::parse_error_exception, "无法转换时间戳: ${ts}", ("ts", seconds));
        }
        
        // 格式化为ISO字符串
        bool include_ms = (millis > 0);
        return tm_to_iso_string(*tm_info, millis, include_ms);
    } catch (const mc::exception& e) {
        std::string msg = "转换时间点为字符串失败: ";
        msg += e.what();
        throw mc::exception(e.code(), msg);
    } catch (const std::exception& e) {
        MC_THROW(mc::parse_error_exception, "转换时间点为字符串失败: ${error}", ("error", e.what()));
    }
}

std::string time_point_sec::to_non_delimited_iso_string() const {
    try {
        // 转换为时间结构
        std::time_t time_secs = static_cast<std::time_t>(m_utc_seconds);
        std::tm* tm_info = std::gmtime(&time_secs);
        
        if (!tm_info) {
            MC_THROW(mc::parse_error_exception, "无法转换时间戳: ${ts}", ("ts", m_utc_seconds));
        }
        
        // 格式化为非分隔ISO字符串
        std::ostringstream oss;
        oss << std::setfill('0')
            << std::setw(4) << (tm_info->tm_year + 1900)
            << std::setw(2) << (tm_info->tm_mon + 1)
            << std::setw(2) << tm_info->tm_mday
            << 'T'
            << std::setw(2) << tm_info->tm_hour
            << std::setw(2) << tm_info->tm_min
            << std::setw(2) << tm_info->tm_sec;
        
        return oss.str();
    } catch (const std::exception& e) {
        MC_THROW(mc::parse_error_exception, "转换时间点为非分隔ISO字符串失败: ${error}", ("error", e.what()));
    }
}

std::string time_point_sec::to_iso_string() const {
    try {
        // 转换为时间结构
        std::time_t time_secs = static_cast<std::time_t>(m_utc_seconds);
        std::tm* tm_info = std::gmtime(&time_secs);
        
        if (!tm_info) {
            MC_THROW(mc::parse_error_exception, "无法转换时间戳: ${ts}", ("ts", m_utc_seconds));
        }
        
        // 格式化为ISO字符串
        return tm_to_iso_string(*tm_info);
    } catch (const std::exception& e) {
        MC_THROW(mc::parse_error_exception, "转换时间点为ISO字符串失败: ${error}", ("error", e.what()));
    }
}

time_point_sec::operator std::string() const {
    return this->to_iso_string();
}

time_point_sec time_point_sec::from_iso_string(const std::string& s) {
    try {
        std::tm tm_result = {};
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

// Variant转换实现
void to_variant(const milliseconds& ms, variant& v) {
    v = ms.count();
}

void from_variant(const variant& v, milliseconds& ms) {
    ms = milliseconds(v.as<int64_t>());
}

void to_variant(const time_point& tp, variant& v) {
    v = std::string(tp);
}

void from_variant(const variant& v, time_point& tp) {
    tp = time_point::from_iso_string(v.as<std::string>());
}

void to_variant(const time_point_sec& tps, variant& v) {
    v = std::string(tps);
}

void from_variant(const variant& v, time_point_sec& tps) {
    tps = time_point_sec::from_iso_string(v.as<std::string>());
}

} // namespace mc 