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

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <gtest/gtest.h>
#include <mc/exception.h>
#include <mc/fmt/format.h>
#include <mc/fmt/formatter_chrono.h>
#include <mc/string_utils.h>
#include <mc/string_view.h>
#include <ratio>
#include <string>

using namespace mc::fmt;

namespace {

int parse_timezone_minutes(mc::string_view tz)
{
    const int hours   = mc::strings::to_number<int>(mc::string_view(tz.data() + 1, 2));
    const int minutes = mc::strings::to_number<int>(mc::string_view(tz.data() + 3, 2));
    return tz[0] == '+' ? (hours * 60 + minutes) : -(hours * 60 + minutes);
}

struct tz_scoped {
    mc::string m_saved;
    bool       m_had_tz = false;

    explicit tz_scoped(const char* tz)
    {
        if (const char* cur = getenv("TZ")) {
            m_saved  = cur;
            m_had_tz = true;
        }
        setenv("TZ", tz, 1);
        tzset();
    }

    ~tz_scoped()
    {
        if (m_had_tz) {
            setenv("TZ", m_saved.c_str(), 1);
        } else {
            unsetenv("TZ");
        }
        tzset();
    }
};

} // namespace

// Duration 基本格式化测试
TEST(chrono_format_test, duration_basic)
{
    using namespace std::chrono;

    EXPECT_EQ(sformat("{}", seconds(42)), "42s");
    EXPECT_EQ(sformat("{}", milliseconds(123)), "123ms");
    EXPECT_EQ(sformat("{}", microseconds(456)), "456μs");
    EXPECT_EQ(sformat("{}", nanoseconds(789)), "789ns");
    EXPECT_EQ(sformat("{}", minutes(3)), "3min");
    EXPECT_EQ(sformat("{}", hours(2)), "2h");
}

// Duration 精度测试
TEST(chrono_format_test, duration_precision)
{
    using namespace std::chrono;
    using double_seconds = duration<double>;

    auto duration = double_seconds(1.234567);

    // 测试精度控制
    EXPECT_EQ(sformat("{:.2}", duration), "1.23s");
    EXPECT_EQ(sformat("{:.4}", duration), "1.2346s");
    EXPECT_EQ(sformat("{:.0}", duration), "1s");
}

// Duration 负值测试
TEST(chrono_format_test, duration_negative)
{
    using namespace std::chrono;

    EXPECT_EQ(sformat("{}", -seconds(42)), "-42s");
    EXPECT_EQ(sformat("{:%H:%M:%S}", -seconds(3665)), "-01:01:05");
    EXPECT_EQ(sformat("{:%T}", -hours(2) - minutes(30)), "-02:30:00");
}

// Duration 时间格式测试
TEST(chrono_format_test, duration_time_format)
{
    using namespace std::chrono;

    auto duration = hours(2) + minutes(30) + seconds(45);

    // 24小时格式
    EXPECT_EQ(sformat("{:%H:%M:%S}", duration), "02:30:45");
    EXPECT_EQ(sformat("{:%T}", duration), "02:30:45");

    // 时分格式
    EXPECT_EQ(sformat("{:%H:%M}", duration), "02:30");
    EXPECT_EQ(sformat("{:%R}", duration), "02:30");

    // 12小时格式
    EXPECT_EQ(sformat("{:%I:%M:%S %p}", duration), "02:30:45 AM");
    EXPECT_EQ(sformat("{:%r}", duration), "02:30:45 AM");
}

// Duration 值和单位格式测试
TEST(chrono_format_test, duration_value_unit)
{
    using namespace std::chrono;

    EXPECT_EQ(sformat("{:%Q}", seconds(42)), "42");
    EXPECT_EQ(sformat("{:%q}", seconds(42)), "s");
    EXPECT_EQ(sformat("{:%Q%q}", seconds(42)), "42s");

    EXPECT_EQ(sformat("{:%Q}", milliseconds(123)), "123");
    EXPECT_EQ(sformat("{:%q}", milliseconds(123)), "ms");
    EXPECT_EQ(sformat("{:%Q%q}", milliseconds(123)), "123ms");
}

// Duration 子秒精度测试
TEST(chrono_format_test, duration_subsecond)
{
    using namespace std::chrono;

    // 毫秒精度
    auto ms_dur = seconds(1) + milliseconds(234);
    EXPECT_EQ(sformat("{:%S}", ms_dur), "01.234");

    // 微秒精度
    auto us_dur = seconds(1) + microseconds(234567);
    EXPECT_EQ(sformat("{:%S}", us_dur), "01.234567");

    // 纳秒精度
    auto ns_dur = seconds(1) + nanoseconds(234567890);
    EXPECT_EQ(sformat("{:%S}", ns_dur), "01.234567890");
}

// Time point 基本格式化测试
TEST(chrono_format_test, time_point_basic)
{
    using namespace std::chrono;

    // 使用固定时间点进行测试 (2024-07-02 11:40:45 UTC)
    system_clock::time_point tp = system_clock::from_time_t(1719920445);

    // 默认格式对齐 C++20 sys_time 的流输出（等价于 "%F %T"，UTC / gmtime）
    mc::string result = sformat("{}", tp);
    EXPECT_EQ(result, sformat("{:%F %T}", tp));
    EXPECT_EQ(result.size(), 19); // YYYY-MM-DD HH:MM:SS
    EXPECT_EQ(result[4], '-');
    EXPECT_EQ(result[7], '-');
    EXPECT_EQ(result[10], ' ');
    EXPECT_EQ(result[13], ':');
    EXPECT_EQ(result[16], ':');
}

// Time point 自定义格式测试
TEST(chrono_format_test, time_point_custom_format)
{
    using namespace std::chrono;

    // 使用固定时间点进行测试
    system_clock::time_point tp = system_clock::from_time_t(1719920445);

    // ISO 日期格式
    EXPECT_EQ(sformat("{:%F}", tp).substr(0, 10), "2024-07-02");

    // 年份格式
    EXPECT_EQ(sformat("{:%Y}", tp), "2024");
    EXPECT_EQ(sformat("{:%y}", tp), "24");

    // 月份格式
    EXPECT_EQ(sformat("{:%m}", tp), "07");

    // 日期格式
    EXPECT_EQ(sformat("{:%d}", tp), "02");
}

// Time point 时间格式测试
TEST(chrono_format_test, time_point_time_format)
{
    using namespace std::chrono;

    system_clock::time_point tp = system_clock::from_time_t(1719920445);

    // 基本时间格式（UTC / gmtime）
    mc::string iso_time = sformat("{:%T}", tp);
    EXPECT_EQ(iso_time.size(), 8); // HH:MM:SS
    EXPECT_EQ(iso_time[2], ':');
    EXPECT_EQ(iso_time[5], ':');

    mc::string hour_min = sformat("{:%R}", tp);
    EXPECT_EQ(hour_min.size(), 5); // HH:MM
    EXPECT_EQ(hour_min[2], ':');
}

// Time point 子秒精度测试 - 测试"转换为纳秒并格式化"功能
TEST(chrono_format_test, time_point_subsecond)
{
    using namespace std::chrono;

    // 创建一个包含子秒的时间点
    auto base_time      = system_clock::from_time_t(1719920445);
    auto tp_with_subsec = base_time + nanoseconds(123456789);

    // 测试秒格式化，应该包含子秒部分
    mc::string result = sformat("{:%S}", tp_with_subsec);

    // 验证结果包含子秒部分
    EXPECT_TRUE(result.find('.') != mc::string::npos);
    EXPECT_TRUE(result.find("45.123456789") != mc::string::npos || result.find("45.123456") != mc::string::npos);

    // 测试完整时间格式
    mc::string full_time = sformat("{:%T}", tp_with_subsec);
    EXPECT_TRUE(full_time.find('.') != mc::string::npos);

    EXPECT_EQ(sformat("{}", tp_with_subsec), sformat("{:%F %T}", tp_with_subsec));

    // 测试不同精度的子秒
    auto       tp_millisec = base_time + milliseconds(123);
    mc::string ms_result   = sformat("{:%S}", tp_millisec);
    EXPECT_TRUE(ms_result.find("45.123") != mc::string::npos);

    auto       tp_microsec = base_time + microseconds(123456);
    mc::string us_result   = sformat("{:%S}", tp_microsec);
    EXPECT_TRUE(us_result.find("45.123456") != mc::string::npos);

    // 测试没有子秒的情况
    mc::string no_subsec = sformat("{:%S}", base_time);
    EXPECT_EQ(no_subsec, "45");

    // 测试边界情况：1纳秒
    auto       tp_1ns    = base_time + nanoseconds(1);
    mc::string ns_result = sformat("{:%S}", tp_1ns);
    EXPECT_EQ(ns_result, "45.000000001");

    // 测试边界情况：999999999纳秒（接近1秒）
    auto       tp_999ns     = base_time + nanoseconds(999999999);
    mc::string ns999_result = sformat("{:%S}", tp_999ns);
    EXPECT_EQ(ns999_result, "45.999999999");
}

// 格式解析错误测试
TEST(chrono_format_test, format_parse_errors)
{
    using namespace std::chrono;

    formatter<seconds> dur_fmt;
    format_spec        spec;

    // 这应该会抛出异常，因为 duration 不支持年份格式
    EXPECT_THROW(dur_fmt.parse("%Y", spec), mc::format_error);

    formatter<system_clock::time_point> tp_fmt;

    // 这应该会抛出异常，因为 time_point 不支持 duration 值格式
    EXPECT_THROW(tp_fmt.parse("%Q", spec), mc::format_error);
}

// 填充和修饰符测试
TEST(chrono_format_test, padding_and_modifiers)
{
    using namespace std::chrono;

    auto duration = hours(5) + minutes(7) + seconds(9);

    // 默认零填充
    EXPECT_EQ(sformat("{:%H:%M:%S}", duration), "05:07:09");

    // 空格填充
    EXPECT_EQ(sformat("{:%_H:%_M:%_S}", duration), " 5: 7: 9");

    // 无填充
    EXPECT_EQ(sformat("{:%-H:%-M:%-S}", duration), "5:7:9");
}

// 边界情况测试
TEST(chrono_format_test, edge_cases)
{
    using namespace std::chrono;

    // 零值
    EXPECT_EQ(sformat("{}", seconds(0)), "0s");
    EXPECT_EQ(sformat("{:%H:%M:%S}", seconds(0)), "00:00:00");

    // 大值 - %H 输出模24的小时数
    auto       large_duration = hours(25) + minutes(70) + seconds(120);
    mc::string result         = sformat("{:%H:%M:%S}", large_duration);
    EXPECT_EQ(result, "02:12:00");

    mc::string result1 = sformat("{:%I:%M:%S %p}", large_duration);
    EXPECT_EQ(result1, "02:12:00 AM");

    // 浮点 duration
    using double_seconds = duration<double>;
    auto fp_duration     = double_seconds(1.5);
    EXPECT_EQ(sformat("{:.1}", fp_duration), "1.5s");
}

// 容器中的 chrono 类型测试
TEST(chrono_format_test, container_formatting)
{
    using namespace std::chrono;

    std::vector<seconds> durations{seconds(1), seconds(2), seconds(3)};
    EXPECT_EQ(sformat("{}", durations), "[1s,2s,3s]");

    std::vector<system_clock::time_point> time_points;
    time_points.push_back(system_clock::from_time_t(1719920445));
    time_points.push_back(system_clock::from_time_t(1719920446));
    EXPECT_EQ(sformat("{}", time_points), "[2024-07-02 11:40:45,2024-07-02 11:40:46]");
}

// 时区格式化测试（对齐 C++20 sys_time：%z 为相对 UTC 的 0min，即 +0000）
TEST(chrono_format_test, timezone_formatting)
{
    using namespace std::chrono;

    // 使用固定时间点进行测试 (2024-07-02 11:40:45 UTC，与 container_formatting 一致)
    constexpr std::time_t    utc_secs = 1719920445;
    system_clock::time_point tp       = system_clock::from_time_t(utc_secs);

    mc::string timezone_result = sformat("{:%z}", tp);
    EXPECT_EQ(timezone_result, "+0000");
    EXPECT_EQ(sformat("{:%Ez}", tp), "+00:00");
    EXPECT_EQ(sformat("{:%Oz}", tp), "+00:00");
    EXPECT_EQ(timezone_result.size(), 5);
    EXPECT_EQ(parse_timezone_minutes(timezone_result.view()), 0);

    mc::string full_result = sformat("{:%Y-%m-%d %H:%M:%S %z}", tp);
    EXPECT_EQ(full_result, "2024-07-02 11:40:45 +0000");
    EXPECT_EQ(full_result.substr(20, 5), timezone_result);

    {
        tz_scoped  shanghai("Asia/Shanghai");
        auto       now          = system_clock::now();
        mc::string now_timezone = sformat("{:%z}", now);
        EXPECT_EQ(now_timezone, "+0000");
        EXPECT_EQ(parse_timezone_minutes(now_timezone.view()), 0);
    }
}

// %Z 对齐 C++20 formatter<sys_time>：固定为 "UTC"
TEST(chrono_format_test, timezone_name_formatting)
{
    using namespace std::chrono;

    constexpr std::time_t    utc_secs = 1719920445;
    system_clock::time_point tp       = system_clock::from_time_t(utc_secs);

    mc::string timezone_name_result = sformat("{:%Z}", tp);

    EXPECT_EQ(timezone_name_result, "UTC");
    EXPECT_FALSE(timezone_name_result[0] == '+' || timezone_name_result[0] == '-');

    mc::string full_result_with_name = sformat("{:%Y-%m-%d %H:%M:%S %Z}", tp);
    EXPECT_EQ(full_result_with_name, "2024-07-02 11:40:45 UTC");

    mc::string offset_result = sformat("{:%z}", tp);
    mc::string name_result   = sformat("{:%Z}", tp);
    EXPECT_NE(offset_result, name_result) << "%z 和 %Z 应该输出不同的结果";
    EXPECT_EQ(offset_result, "+0000");
    EXPECT_EQ(name_result, "UTC");
}

// Duration 中使用年份/时区格式会被校验拒绝，应在运行期抛出异常
TEST(chrono_format_test, duration_uncommon_fields)
{
    using namespace std::chrono;

    auto duration = hours(49) + minutes(5);
    EXPECT_THROW(sformat_unsafe("{:%Y-%y-%m-%d}", duration), mc::format_error);
    EXPECT_THROW(sformat_unsafe("{:%z %Z}", duration), mc::format_error);
}

// 测试 12 小时制格式
TEST(chrono_format_test, Duration12HourFormat)
{
    using namespace std::chrono;

    // 构造包含小时/分钟/秒的 duration
    auto duration = hours(14) + minutes(30) + seconds(45);

    // 测试 12 小时制格式
    mc::string result = sformat("{:%I:%M:%S %p}", duration);
    // 14:30:45 应该转换为 02:30:45 PM
    EXPECT_EQ(result, "02:30:45 PM");

    // 测试上午时间
    auto       morning        = hours(9) + minutes(15) + seconds(30);
    mc::string morning_result = sformat("{:%I:%M:%S %p}", morning);
    EXPECT_EQ(morning_result, "09:15:30 AM");
}

// 测试 time_point 的时区 tokens（对齐 C++20 sys_time，与进程 TZ 无关）
TEST(chrono_format_test, TimePointTimezoneTokens)
{
    using namespace std::chrono;

    tz_scoped shanghai("Asia/Shanghai");

    auto       tp            = system_clock::time_point{} + seconds(0);
    mc::string offset_result = sformat("{:%z}", tp);
    mc::string name_result   = sformat("{:%Z}", tp);

    EXPECT_EQ(offset_result, "+0000");
    EXPECT_EQ(name_result, "UTC");

    mc::string combined = sformat("{:%z %Z}", tp);
    EXPECT_EQ(combined, "+0000 UTC");
}

// 测试 time_point 的 12 小时制格式（钟面为 UTC，与 TZ 无关）
TEST(chrono_format_test, TimePoint12HourFormat)
{
    using namespace std::chrono;

    tz_scoped ny("America/New_York");

    // 创建一个下午的 time_point（1970-01-01 14:30:45 UTC）
    auto tp = system_clock::time_point{} + hours(14) + minutes(30) + seconds(45);

    // 测试 12 小时制格式
    mc::string result = sformat("{:%I:%M:%S %p}", tp);
    // 14:30:45 应该转换为 02:30:45 PM
    EXPECT_EQ(result, "02:30:45 PM");

    // 测试午夜（hour == 0）
    auto       midnight        = system_clock::time_point{} + hours(0) + minutes(0) + seconds(0);
    mc::string midnight_result = sformat("{:%I:%M:%S %p}", midnight);
    EXPECT_EQ(midnight_result, "12:00:00 AM");
}
