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
 * @file time.h
 * @brief 基于毫秒精度的时间模块，直接封装std::chrono
 */
#ifndef MC_TIME_H
#define MC_TIME_H

#include <mc/string.h>
#include <mc/variant.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace mc {

/**
 * @brief 毫秒精度的时间间隔类
 */
class milliseconds {
public:
    /**
     * @brief 构造函数
     * @param count 毫秒数
     */
    constexpr explicit milliseconds(int64_t count = 0) : m_count(count)
    {}

    /**
     * @brief 从 std::chrono::duration 构造
     */
    template <typename Rep, typename Period>
    constexpr milliseconds(const std::chrono::duration<Rep, Period>& duration)
        : m_count(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count())
    {}

    /**
     * @brief 获取最大毫秒值
     * @return milliseconds 最大毫秒值
     */
    static constexpr milliseconds maximum()
    {
        return milliseconds(0x7fffffffffffffffll);
    }

    /**
     * @brief 获取零毫秒值
     * @return milliseconds 零毫秒值
     */
    static constexpr milliseconds zero()
    {
        return milliseconds(0);
    }

    /**
     * @brief 加法运算符
     */
    friend constexpr milliseconds operator+(const milliseconds& l, const milliseconds& r)
    {
        return milliseconds(l.m_count + r.m_count);
    }

    /**
     * @brief 减法运算符
     */
    friend constexpr milliseconds operator-(const milliseconds& l, const milliseconds& r)
    {
        return milliseconds(l.m_count - r.m_count);
    }

    /**
     * @brief 相等比较运算符
     */
    constexpr bool operator==(const milliseconds& c) const
    {
        return m_count == c.m_count;
    }

    /**
     * @brief 不等比较运算符
     */
    constexpr bool operator!=(const milliseconds& c) const
    {
        return m_count != c.m_count;
    }

    /**
     * @brief 大于比较运算符
     */
    friend constexpr bool operator>(const milliseconds& a, const milliseconds& b)
    {
        return a.m_count > b.m_count;
    }

    /**
     * @brief 大于等于比较运算符
     */
    friend constexpr bool operator>=(const milliseconds& a, const milliseconds& b)
    {
        return a.m_count >= b.m_count;
    }

    /**
     * @brief 小于比较运算符
     */
    constexpr friend bool operator<(const milliseconds& a, const milliseconds& b)
    {
        return a.m_count < b.m_count;
    }

    /**
     * @brief 小于等于比较运算符
     */
    constexpr friend bool operator<=(const milliseconds& a, const milliseconds& b)
    {
        return a.m_count <= b.m_count;
    }

    /**
     * @brief 加法赋值运算符
     */
    constexpr milliseconds& operator+=(const milliseconds& c)
    {
        m_count += c.m_count;
        return *this;
    }

    /**
     * @brief 减法赋值运算符
     */
    constexpr milliseconds& operator-=(const milliseconds& c)
    {
        m_count -= c.m_count;
        return *this;
    }

    /**
     * @brief 获取毫秒数
     * @return int64_t 毫秒数
     */
    constexpr int64_t count() const
    {
        return m_count;
    }

    /**
     * @brief 转换为秒数
     * @return int64_t 秒数
     */
    constexpr int64_t to_seconds() const
    {
        return m_count / 1000;
    }

    /**
     * @brief 转换为std::chrono::duration
     */
    template <typename Rep, typename Period>
    constexpr operator std::chrono::duration<Rep, Period>() const
    {
        return std::chrono::duration_cast<std::chrono::duration<Rep, Period>>(std::chrono::milliseconds(m_count));
    }

private:
    friend class time_point;
    int64_t m_count;
};

/**
 * @brief 创建秒时间间隔
 * @param s 秒数
 * @return milliseconds 毫秒时间间隔
 */
inline constexpr milliseconds seconds(int64_t s)
{
    return milliseconds(s * 1000);
}

/**
 * @brief 创建分钟时间间隔
 * @param m 分钟数
 * @return milliseconds 毫秒时间间隔
 */
inline constexpr milliseconds minutes(int64_t m)
{
    return seconds(60 * m);
}

/**
 * @brief 创建小时时间间隔
 * @param h 小时数
 * @return milliseconds 毫秒时间间隔
 */
inline constexpr milliseconds hours(int64_t h)
{
    return minutes(60 * h);
}

/**
 * @brief 创建天时间间隔
 * @param d 天数
 * @return milliseconds 毫秒时间间隔
 */
inline constexpr milliseconds days(int64_t d)
{
    return hours(24 * d);
}

/**
 * @brief 时间点类
 */
class MC_API time_point {
public:
    /**
     * @brief 构造函数
     * @param elapsed 时间间隔
     */
    constexpr explicit time_point(milliseconds elapsed = milliseconds()) : m_elapsed(elapsed)
    {}

    /**
     * @brief 从std::chrono::time_point构造
     */
    template <typename Clock, typename Duration>
    time_point(const std::chrono::time_point<Clock, Duration>& tp)
    {
        auto ms_tp = std::chrono::time_point_cast<std::chrono::milliseconds>(tp);
        m_elapsed  = milliseconds(ms_tp.time_since_epoch().count());
    }

    /**
     * @brief 获取当前时间点
     * @return time_point 当前时间点
     */
    static time_point now();

    /**
     * @brief 获取最大时间点
     * @return time_point 最大时间点
     */
    static constexpr time_point maximum()
    {
        return time_point(milliseconds::maximum());
    }

    /**
     * @brief 获取最小时间点
     * @return time_point 最小时间点
     */
    static constexpr time_point min()
    {
        return time_point();
    }

    /**
     * @brief 从ISO字符串转换为时间点
     * @param s ISO字符串
     * @return time_point 时间点
     */
    static time_point from_iso_string(const std::string& s);

    /**
     * @brief 转换为字符串
     * @return std::string_view 字符串视图
     * @note 返回的字符串视图仅在当前调用堆栈内有效，由thread_local存储管理
     */
    operator std::string_view() const;

    /**
     * @brief 转换为字符串表示
     * @return std::string_view 字符串视图
     * @note 返回的字符串视图仅在当前调用堆栈内有效，由thread_local存储管理
     */
    std::string_view to_string() const;

    /**
     * @brief 获取从纪元以来的时间间隔
     * @return const milliseconds& 时间间隔
     */
    constexpr const milliseconds& time_since_epoch() const
    {
        return m_elapsed;
    }

    /**
     * @brief 获取从纪元以来的秒数
     * @return uint32_t 秒数
     */
    constexpr uint32_t sec_since_epoch() const
    {
        return m_elapsed.count() / 1000;
    }

    /**
     * @brief 转换为std::chrono::time_point
     */
    template <typename Clock, typename Duration>
    operator std::chrono::time_point<Clock, Duration>() const
    {
        return std::chrono::time_point<Clock, Duration>(std::chrono::milliseconds(m_elapsed.count()));
    }

    /**
     * @brief 大于比较运算符
     */
    constexpr bool operator>(const time_point& t) const
    {
        return m_elapsed.m_count > t.m_elapsed.m_count;
    }

    /**
     * @brief 大于等于比较运算符
     */
    constexpr bool operator>=(const time_point& t) const
    {
        return m_elapsed.m_count >= t.m_elapsed.m_count;
    }

    /**
     * @brief 小于比较运算符
     */
    constexpr bool operator<(const time_point& t) const
    {
        return m_elapsed.m_count < t.m_elapsed.m_count;
    }

    /**
     * @brief 小于等于比较运算符
     */
    constexpr bool operator<=(const time_point& t) const
    {
        return m_elapsed.m_count <= t.m_elapsed.m_count;
    }

    /**
     * @brief 相等比较运算符
     */
    constexpr bool operator==(const time_point& t) const
    {
        return m_elapsed.m_count == t.m_elapsed.m_count;
    }

    /**
     * @brief 不等比较运算符
     */
    constexpr bool operator!=(const time_point& t) const
    {
        return m_elapsed.m_count != t.m_elapsed.m_count;
    }

    /**
     * @brief 加法赋值运算符
     */
    constexpr time_point& operator+=(const milliseconds& m)
    {
        m_elapsed += m;
        return *this;
    }

    /**
     * @brief 减法赋值运算符
     */
    constexpr time_point& operator-=(const milliseconds& m)
    {
        m_elapsed -= m;
        return *this;
    }

    /**
     * @brief 加法运算符
     */
    constexpr time_point operator+(const milliseconds& m) const
    {
        return time_point(m_elapsed + m);
    }

    /**
     * @brief 减法运算符
     */
    constexpr time_point operator-(const milliseconds& m) const
    {
        return time_point(m_elapsed - m);
    }

    /**
     * @brief 减法运算符
     */
    constexpr milliseconds operator-(const time_point& m) const
    {
        return milliseconds(m_elapsed.count() - m.m_elapsed.count());
    }

    /**
     * @brief 转换为ISO字符串
     * @return std::string_view ISO字符串
     */
    std::string_view to_iso_string() const;

private:
    milliseconds m_elapsed;
};

/**
 * @brief 精确到秒的时间点类
 */
class MC_API time_point_sec {
public:
    /**
     * @brief 默认构造函数
     */
    constexpr time_point_sec() : m_utc_seconds(0)
    {}

    /**
     * @brief 构造函数
     * @param seconds 秒数
     */
    constexpr explicit time_point_sec(uint32_t seconds) : m_utc_seconds(seconds)
    {}

    /**
     * @brief 从时间点构造
     * @param t 时间点
     */
    constexpr time_point_sec(const time_point& t) : m_utc_seconds(t.time_since_epoch().count() / 1000ll)
    {}

    /**
     * @brief 从std::chrono::time_point构造
     */
    template <typename Clock, typename Duration>
    time_point_sec(const std::chrono::time_point<Clock, Duration>& tp)
    {
        auto s_tp     = std::chrono::time_point_cast<std::chrono::seconds>(tp);
        m_utc_seconds = static_cast<uint32_t>(s_tp.time_since_epoch().count());
    }

    /**
     * @brief 获取当前时间点
     * @return time_point 当前时间点
     */
    static time_point_sec now();

    /**
     * @brief 获取最大时间点
     * @return time_point_sec 最大时间点
     */
    static constexpr time_point_sec maximum()
    {
        return time_point_sec(0xffffffff);
    }

    /**
     * @brief 获取最小时间点
     * @return time_point_sec 最小时间点
     */
    static constexpr time_point_sec min()
    {
        return time_point_sec(0);
    }

    /**
     * @brief 转换为时间点
     */
    constexpr operator time_point() const
    {
        return time_point(seconds(m_utc_seconds));
    }

    /**
     * @brief 转换为std::chrono::time_point
     */
    template <typename Clock, typename Duration>
    operator std::chrono::time_point<Clock, Duration>() const
    {
        return std::chrono::time_point<Clock, Duration>(std::chrono::seconds(m_utc_seconds));
    }

    /**
     * @brief 获取从纪元以来的秒数
     * @return uint32_t 秒数
     */
    constexpr uint32_t sec_since_epoch() const
    {
        return m_utc_seconds;
    }

    /**
     * @brief 赋值运算符
     */
    constexpr time_point_sec operator=(const time_point& t)
    {
        m_utc_seconds = static_cast<uint32_t>(t.time_since_epoch().count() / 1000ll);
        return *this;
    }

    /**
     * @brief 小于比较运算符
     */
    constexpr friend bool operator<(const time_point_sec& a, const time_point_sec& b)
    {
        return a.m_utc_seconds < b.m_utc_seconds;
    }

    /**
     * @brief 大于比较运算符
     */
    constexpr friend bool operator>(const time_point_sec& a, const time_point_sec& b)
    {
        return a.m_utc_seconds > b.m_utc_seconds;
    }

    /**
     * @brief 小于等于比较运算符
     */
    constexpr friend bool operator<=(const time_point_sec& a, const time_point_sec& b)
    {
        return a.m_utc_seconds <= b.m_utc_seconds;
    }

    /**
     * @brief 大于等于比较运算符
     */
    constexpr friend bool operator>=(const time_point_sec& a, const time_point_sec& b)
    {
        return a.m_utc_seconds >= b.m_utc_seconds;
    }

    /**
     * @brief 相等比较运算符
     */
    constexpr friend bool operator==(const time_point_sec& a, const time_point_sec& b)
    {
        return a.m_utc_seconds == b.m_utc_seconds;
    }

    /**
     * @brief 不等比较运算符
     */
    constexpr friend bool operator!=(const time_point_sec& a, const time_point_sec& b)
    {
        return a.m_utc_seconds != b.m_utc_seconds;
    }

    /**
     * @brief 加法赋值运算符
     */
    constexpr time_point_sec& operator+=(uint32_t m)
    {
        m_utc_seconds += m;
        return *this;
    }

    /**
     * @brief 加法赋值运算符
     */
    constexpr time_point_sec& operator+=(milliseconds m)
    {
        m_utc_seconds = static_cast<uint32_t>(static_cast<int64_t>(m_utc_seconds) + m.to_seconds());
        return *this;
    }

    /**
     * @brief 减法赋值运算符
     */
    constexpr time_point_sec& operator-=(uint32_t m)
    {
        m_utc_seconds -= m;
        return *this;
    }

    /**
     * @brief 减法赋值运算符
     */
    constexpr time_point_sec& operator-=(milliseconds m)
    {
        m_utc_seconds = static_cast<uint32_t>(static_cast<int64_t>(m_utc_seconds) - m.to_seconds());
        return *this;
    }

    /**
     * @brief 加法运算符
     */
    constexpr time_point_sec operator+(uint32_t offset) const
    {
        return time_point_sec(m_utc_seconds + offset);
    }

    /**
     * @brief 减法运算符
     */
    constexpr time_point_sec operator-(uint32_t offset) const
    {
        return time_point_sec(m_utc_seconds - offset);
    }

    /**
     * @brief 加法运算符
     */
    friend constexpr time_point operator+(const time_point_sec& t, const milliseconds& m)
    {
        return time_point(t) + m;
    }

    /**
     * @brief 减法运算符
     */
    friend constexpr time_point operator-(const time_point_sec& t, const milliseconds& m)
    {
        return time_point(t) - m;
    }

    /**
     * @brief 减法运算符
     */
    friend constexpr milliseconds operator-(const time_point_sec& t, const time_point_sec& m)
    {
        return time_point(t) - time_point(m);
    }

    /**
     * @brief 减法运算符
     */
    friend constexpr milliseconds operator-(const time_point& t, const time_point_sec& m)
    {
        return time_point(t) - time_point(m);
    }

    /**
     * @brief 转换为ISO字符串
     * @return std::string_view ISO字符串
     */
    std::string_view to_iso_string() const;

    /**
     * @brief 转换为字符串
     * @return std::string_view 字符串视图
     * @note 返回的字符串视图仅在当前调用堆栈内有效，由thread_local存储管理
     */
    operator std::string_view() const;

    /**
     * @brief 转换为字符串表示
     * @return std::string_view 字符串视图
     * @note 返回的字符串视图仅在当前调用堆栈内有效，由thread_local存储管理
     */
    std::string_view to_string() const;

    /**
     * @brief 从ISO字符串转换为时间点
     * @param s ISO字符串
     * @return time_point_sec 时间点
     */
    static time_point_sec from_iso_string(const std::string& s);

private:
    uint32_t m_utc_seconds;
};

/**
 * @brief 可选时间点类型
 */
typedef std::optional<time_point> opt_time_point;

// variant转换函数声明
MC_API void to_variant(const milliseconds& ms, variant& v);
MC_API void from_variant(const variant& v, milliseconds& ms);
MC_API void to_variant(const time_point& tp, variant& v);
MC_API void from_variant(const variant& v, time_point& tp);
MC_API void to_variant(const time_point_sec& tps, variant& v);
MC_API void from_variant(const variant& v, time_point_sec& tps);

} // namespace mc

#endif // MC_TIME_H