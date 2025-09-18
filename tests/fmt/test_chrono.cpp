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
#include <gtest/gtest.h>
#include <mc/exception.h>
#include <mc/fmt/format.h>
#include <mc/fmt/formatter_chrono.h>
#include <string>

using namespace mc::fmt;

// Duration 基本格式化测试
TEST(chrono_format_test, duration_basic) {
    using namespace std::chrono;

    EXPECT_EQ(sformat("{}", seconds(42)), "42s");
    EXPECT_EQ(sformat("{}", milliseconds(123)), "123ms");
    EXPECT_EQ(sformat("{}", microseconds(456)), "456μs");
    EXPECT_EQ(sformat("{}", nanoseconds(789)), "789ns");
    EXPECT_EQ(sformat("{}", minutes(3)), "3min");
    EXPECT_EQ(sformat("{}", hours(2)), "2h");
}

// Duration 精度测试
TEST(chrono_format_test, duration_precision) {
    using namespace std::chrono;
    using double_seconds = duration<double>;

    auto duration = double_seconds(1.234567);

    // 测试精度控制
    EXPECT_EQ(sformat("{:.2}", duration), "1.23s");
    EXPECT_EQ(sformat("{:.4}", duration), "1.2346s");
    EXPECT_EQ(sformat("{:.0}", duration), "1s");
}

// Duration 负值测试
TEST(chrono_format_test, duration_negative) {
    using namespace std::chrono;

    EXPECT_EQ(sformat("{}", -seconds(42)), "-42s");
    EXPECT_EQ(sformat("{:%H:%M:%S}", -seconds(3665)), "-01:01:05");
    EXPECT_EQ(sformat("{:%T}", -hours(2) - minutes(30)), "-02:30:00");
}

// Duration 时间格式测试
TEST(chrono_format_test, duration_time_format) {
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
TEST(chrono_format_test, duration_value_unit) {
    using namespace std::chrono;

    EXPECT_EQ(sformat("{:%Q}", seconds(42)), "42");
    EXPECT_EQ(sformat("{:%q}", seconds(42)), "s");
    EXPECT_EQ(sformat("{:%Q%q}", seconds(42)), "42s");

    EXPECT_EQ(sformat("{:%Q}", milliseconds(123)), "123");
    EXPECT_EQ(sformat("{:%q}", milliseconds(123)), "ms");
    EXPECT_EQ(sformat("{:%Q%q}", milliseconds(123)), "123ms");
}

// Duration 子秒精度测试
TEST(chrono_format_test, duration_subsecond) {
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
TEST(chrono_format_test, time_point_basic) {
    using namespace std::chrono;

    // 使用固定时间点进行测试 (2024-07-02 12:30:45 UTC)
    system_clock::time_point tp = system_clock::from_time_t(1719920445);

    // 默认格式（注意：这依赖于系统的本地时区）
    std::string result = sformat("{}", tp);
    EXPECT_EQ(result.size(), 19); // YYYY-MM-DD HH:MM:SS 格式
    EXPECT_EQ(result[4], '-');
    EXPECT_EQ(result[7], '-');
    EXPECT_EQ(result[10], ' ');
    EXPECT_EQ(result[13], ':');
    EXPECT_EQ(result[16], ':');
}

// Time point 自定义格式测试
TEST(chrono_format_test, time_point_custom_format) {
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
TEST(chrono_format_test, time_point_time_format) {
    using namespace std::chrono;

    system_clock::time_point tp = system_clock::from_time_t(1719920445);

    // 基本时间格式（结果依赖于系统时区，这里只检查格式）
    std::string iso_time = sformat("{:%T}", tp);
    EXPECT_EQ(iso_time.size(), 8); // HH:MM:SS
    EXPECT_EQ(iso_time[2], ':');
    EXPECT_EQ(iso_time[5], ':');

    std::string hour_min = sformat("{:%R}", tp);
    EXPECT_EQ(hour_min.size(), 5); // HH:MM
    EXPECT_EQ(hour_min[2], ':');
}

// Time point 子秒精度测试 - 测试"转换为纳秒并格式化"功能
TEST(chrono_format_test, time_point_subsecond) {
    using namespace std::chrono;

    // 创建一个包含子秒的时间点
    auto base_time      = system_clock::from_time_t(1719920445);
    auto tp_with_subsec = base_time + nanoseconds(123456789);

    // 测试秒格式化，应该包含子秒部分
    std::string result = sformat("{:%S}", tp_with_subsec);

    // 验证结果包含子秒部分
    EXPECT_TRUE(result.find('.') != std::string::npos);
    EXPECT_TRUE(result.find("45.123456789") != std::string::npos ||
                result.find("45.123456") != std::string::npos);

    // 测试完整时间格式
    std::string full_time = sformat("{:%T}", tp_with_subsec);
    EXPECT_TRUE(full_time.find('.') != std::string::npos);

    // 测试不同精度的子秒
    auto        tp_millisec = base_time + milliseconds(123);
    std::string ms_result   = sformat("{:%S}", tp_millisec);
    EXPECT_TRUE(ms_result.find("45.123") != std::string::npos);

    auto        tp_microsec = base_time + microseconds(123456);
    std::string us_result   = sformat("{:%S}", tp_microsec);
    EXPECT_TRUE(us_result.find("45.123456") != std::string::npos);

    // 测试没有子秒的情况
    std::string no_subsec = sformat("{:%S}", base_time);
    EXPECT_EQ(no_subsec, "45");

    // 测试边界情况：1纳秒
    auto        tp_1ns    = base_time + nanoseconds(1);
    std::string ns_result = sformat("{:%S}", tp_1ns);
    EXPECT_EQ(ns_result, "45.000000001");

    // 测试边界情况：999999999纳秒（接近1秒）
    auto        tp_999ns     = base_time + nanoseconds(999999999);
    std::string ns999_result = sformat("{:%S}", tp_999ns);
    EXPECT_EQ(ns999_result, "45.999999999");
}

// 格式解析错误测试
TEST(chrono_format_test, format_parse_errors) {
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
TEST(chrono_format_test, padding_and_modifiers) {
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
TEST(chrono_format_test, edge_cases) {
    using namespace std::chrono;

    // 零值
    EXPECT_EQ(sformat("{}", seconds(0)), "0s");
    EXPECT_EQ(sformat("{:%H:%M:%S}", seconds(0)), "00:00:00");

    // 大值 - %H 输出模24的小时数
    auto        large_duration = hours(25) + minutes(70) + seconds(120);
    std::string result         = sformat("{:%H:%M:%S}", large_duration);
    EXPECT_EQ(result, "02:12:00");

    std::string result1 = sformat("{:%I:%M:%S %p}", large_duration);
    EXPECT_EQ(result1, "02:12:00 AM");

    // 浮点 duration
    using double_seconds = duration<double>;
    auto fp_duration     = double_seconds(1.5);
    EXPECT_EQ(sformat("{:.1}", fp_duration), "1.5s");
}

// 容器中的 chrono 类型测试
TEST(chrono_format_test, container_formatting) {
    using namespace std::chrono;

    std::vector<seconds> durations{seconds(1), seconds(2), seconds(3)};
    EXPECT_EQ(sformat("{}", durations), "[1s,2s,3s]");

    std::vector<system_clock::time_point> time_points;
    time_points.push_back(system_clock::from_time_t(1719920445));
    time_points.push_back(system_clock::from_time_t(1719920446));
    EXPECT_EQ(sformat("{}", time_points), "[2024-07-02 11:40:45,2024-07-02 11:40:46]");
}

// 时区格式化测试
TEST(chrono_format_test, timezone_formatting) {
    using namespace std::chrono;

    // 使用固定时间点进行测试 (2024-07-02 12:30:45 UTC)
    system_clock::time_point tp = system_clock::from_time_t(1719920445);

    // 测试时区偏移格式
    std::string timezone_result = sformat("{:%z}", tp);

    // 验证时区格式：应该是 +/-HHMM 格式
    EXPECT_EQ(timezone_result.size(), 5); // 例如 "+0800" 或 "-0500"
    EXPECT_TRUE(timezone_result[0] == '+' || timezone_result[0] == '-');
    EXPECT_TRUE(std::isdigit(timezone_result[1]));
    EXPECT_TRUE(std::isdigit(timezone_result[2]));
    EXPECT_TRUE(std::isdigit(timezone_result[3]));
    EXPECT_TRUE(std::isdigit(timezone_result[4]));

    // 精确验证时区偏移值
    // 计算期望的时区偏移
    std::time_t utc_time = 1719920445;
    std::tm     local_tm = *std::localtime(&utc_time);
    std::tm     utc_tm   = *std::gmtime(&utc_time);

    // 使用与格式化函数相同的逻辑计算期望偏移量
    int expected_offset_minutes = 0;

    // 通过比较 UTC 和本地时间来计算偏移量
    int hour_diff = local_tm.tm_hour - utc_tm.tm_hour;
    int min_diff  = local_tm.tm_min - utc_tm.tm_min;

    // 处理跨日的情况
    if (hour_diff > 12) {
        hour_diff -= 24;
    } else if (hour_diff < -12) {
        hour_diff += 24;
    }

    expected_offset_minutes = hour_diff * 60 + min_diff;

    // 解析格式化结果中的时区偏移
    int actual_offset_minutes = 0;
    if (timezone_result[0] == '+') {
        actual_offset_minutes = std::stoi(timezone_result.substr(1, 2)) * 60 +
                                std::stoi(timezone_result.substr(3, 2));
    } else {
        actual_offset_minutes = -(std::stoi(timezone_result.substr(1, 2)) * 60 +
                                  std::stoi(timezone_result.substr(3, 2)));
    }

    // 验证时区偏移值是否正确
    EXPECT_EQ(actual_offset_minutes, expected_offset_minutes)
        << "时区偏移不匹配: 期望 " << expected_offset_minutes
        << " 分钟, 实际 " << actual_offset_minutes
        << " 分钟 (格式化结果: " << timezone_result << ")";

    // 测试完整的时间格式化包含时区
    std::string full_result = sformat("{:%Y-%m-%d %H:%M:%S %z}", tp);

    // 验证格式：YYYY-MM-DD HH:MM:SS +/-HHMM
    EXPECT_EQ(full_result.size(), 25);                             // 19 + 1 + 5
    EXPECT_EQ(full_result[19], ' ');                               // 时间和时区之间的空格
    EXPECT_TRUE(full_result[20] == '+' || full_result[20] == '-'); // 时区符号

    // 验证完整结果中的时区部分与单独时区结果一致
    std::string full_timezone = full_result.substr(20, 5);
    EXPECT_EQ(full_timezone, timezone_result);

    // 测试当前时间的时区格式化
    auto        now          = system_clock::now();
    std::string now_timezone = sformat("{:%z}", now);
    EXPECT_EQ(now_timezone.size(), 5);
    EXPECT_TRUE(now_timezone[0] == '+' || now_timezone[0] == '-');

    // 验证当前时间的时区偏移也是正确的
    std::time_t now_utc      = std::chrono::system_clock::to_time_t(now);
    std::tm     now_local_tm = *std::localtime(&now_utc);
    std::tm     now_utc_tm   = *std::gmtime(&now_utc);

    // 使用与格式化函数相同的逻辑计算期望偏移量
    int now_expected_offset_minutes = 0;

    // 通过比较 UTC 和本地时间来计算偏移量
    int now_hour_diff = now_local_tm.tm_hour - now_utc_tm.tm_hour;
    int now_min_diff  = now_local_tm.tm_min - now_utc_tm.tm_min;

    // 处理跨日的情况
    if (now_hour_diff > 12) {
        now_hour_diff -= 24;
    } else if (now_hour_diff < -12) {
        now_hour_diff += 24;
    }

    now_expected_offset_minutes = now_hour_diff * 60 + now_min_diff;

    int now_actual_offset_minutes = 0;
    if (now_timezone[0] == '+') {
        now_actual_offset_minutes = std::stoi(now_timezone.substr(1, 2)) * 60 +
                                    std::stoi(now_timezone.substr(3, 2));
    } else {
        now_actual_offset_minutes = -(std::stoi(now_timezone.substr(1, 2)) * 60 +
                                      std::stoi(now_timezone.substr(3, 2)));
    }

    EXPECT_EQ(now_actual_offset_minutes, now_expected_offset_minutes)
        << "当前时间时区偏移不匹配: 期望 " << now_expected_offset_minutes
        << " 分钟, 实际 " << now_actual_offset_minutes
        << " 分钟 (格式化结果: " << now_timezone << ")";
}

// 测试时区名称格式化
TEST(chrono_format_test, timezone_name_formatting) {
    using namespace std::chrono;

    // 使用固定时间点进行测试 (2024-07-02 12:30:45 UTC)
    system_clock::time_point tp = system_clock::from_time_t(1719920445);

    // 测试时区名称格式
    std::string timezone_name_result = sformat("{:%Z}", tp);

    // 验证时区名称格式：应该是时区缩写（如 CST、PST 等）
    EXPECT_FALSE(timezone_name_result.empty());
    EXPECT_GT(timezone_name_result.size(), 0);
    EXPECT_LT(timezone_name_result.size(), 10); // 时区名称通常很短

    // 验证时区名称不是偏移量格式（不应该包含 + 或 - 开头）
    EXPECT_FALSE(timezone_name_result[0] == '+' || timezone_name_result[0] == '-');

    // 测试完整的时间格式化包含时区名称
    std::string full_result_with_name = sformat("{:%Y-%m-%d %H:%M:%S %Z}", tp);

    // 验证格式：YYYY-MM-DD HH:MM:SS TZNAME
    EXPECT_GT(full_result_with_name.size(), 20); // 至少包含日期时间和时区名称
    EXPECT_EQ(full_result_with_name[19], ' ');   // 时间和时区之间的空格

    // 验证完整结果中的时区名称部分与单独时区名称结果一致
    std::string full_timezone_name = full_result_with_name.substr(20);
    EXPECT_EQ(full_timezone_name, timezone_name_result);

    // 测试 %z 和 %Z 的不同行为
    std::string offset_result = sformat("{:%z}", tp);
    std::string name_result   = sformat("{:%Z}", tp);

    // 验证它们确实不同
    EXPECT_NE(offset_result, name_result)
        << "%z 和 %Z 应该输出不同的结果";

    // 验证 %z 是偏移量格式（+HHMM 或 -HHMM）
    EXPECT_EQ(offset_result.size(), 5);
    EXPECT_TRUE(offset_result[0] == '+' || offset_result[0] == '-');
    EXPECT_TRUE(std::isdigit(offset_result[1]));
    EXPECT_TRUE(std::isdigit(offset_result[2]));
    EXPECT_TRUE(std::isdigit(offset_result[3]));
    EXPECT_TRUE(std::isdigit(offset_result[4]));

    // 验证 %Z 是时区名称格式（字母缩写）
    EXPECT_FALSE(name_result[0] == '+' || name_result[0] == '-');
    for (char c : name_result) {
        EXPECT_TRUE(std::isalpha(c) || std::isspace(c))
            << "时区名称应该只包含字母和空格，但包含: " << c;
    }
}
