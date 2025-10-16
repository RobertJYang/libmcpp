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
 * @file time_test.cpp
 * @brief 时间模块的单元测试
 */
#include "mc/time.h"
#include "mc/variant.h"
#include <chrono>
#include <cmath>
#include <gtest/gtest.h>
#include <thread>

using namespace mc;

// 测试毫秒类
TEST(TimeTest, MillisecondsTest) {
    // 基本构造和值测试
    milliseconds ms(1000);
    EXPECT_EQ(ms.count(), 1000);
    EXPECT_EQ(ms.to_seconds(), 1);

    // 辅助函数测试
    auto sec = seconds(1);
    EXPECT_EQ(sec.count(), 1000);

    auto min = minutes(1);
    EXPECT_EQ(min.count(), 60 * 1000);

    auto hour = hours(1);
    EXPECT_EQ(hour.count(), 60 * 60 * 1000);

    auto day = days(1);
    EXPECT_EQ(day.count(), 24 * 60 * 60 * 1000);

    // 运算符测试
    milliseconds ms1(1000);
    milliseconds ms2(500);

    // 加法测试
    auto ms3 = ms1 + ms2;
    EXPECT_EQ(ms3.count(), 1500);

    // 减法测试
    auto ms4 = ms1 - ms2;
    EXPECT_EQ(ms4.count(), 500);

    // 比较运算符测试
    EXPECT_GT(ms1, ms2);
    EXPECT_GE(ms1, ms2);
    EXPECT_LT(ms2, ms1);
    EXPECT_LE(ms2, ms1);
    EXPECT_NE(ms1, ms2);
    EXPECT_EQ(ms1, milliseconds(1000));

    // 赋值运算符测试
    milliseconds ms5(100);
    ms5 += ms2;
    EXPECT_EQ(ms5.count(), 600);

    milliseconds ms6(1000);
    ms6 -= ms2;
    EXPECT_EQ(ms6.count(), 500);
}

// 测试时间点类
TEST(TimeTest, TimePointTest) {
    // 构造函数测试
    time_point tp1(milliseconds(1000));
    EXPECT_EQ(tp1.time_since_epoch().count(), 1000);
    EXPECT_EQ(tp1.sec_since_epoch(), 1);

    // 获取当前时间
    time_point now = time_point::now();
    EXPECT_GT(now.time_since_epoch().count(), 0);

    // 运算符测试
    time_point tp2(milliseconds(2000));

    // 比较运算符
    EXPECT_GT(tp2, tp1);
    EXPECT_GE(tp2, tp1);
    EXPECT_LT(tp1, tp2);
    EXPECT_LE(tp1, tp2);
    EXPECT_NE(tp1, tp2);
    EXPECT_EQ(tp1, time_point(milliseconds(1000)));

    // 加减运算符
    auto tp3 = tp1 + milliseconds(1000);
    EXPECT_EQ(tp3.time_since_epoch().count(), 2000);

    auto tp4 = tp2 - milliseconds(1000);
    EXPECT_EQ(tp4.time_since_epoch().count(), 1000);

    auto diff = tp2 - tp1;
    EXPECT_EQ(diff.count(), 1000);

    // 赋值运算符
    time_point tp5(milliseconds(1000));
    tp5 += milliseconds(500);
    EXPECT_EQ(tp5.time_since_epoch().count(), 1500);

    time_point tp6(milliseconds(2000));
    tp6 -= milliseconds(500);
    EXPECT_EQ(tp6.time_since_epoch().count(), 1500);
}

// 测试精确到秒的时间点类
TEST(TimeTest, TimePointSecTest) {
    // 构造函数测试
    time_point_sec tps1(1000);
    EXPECT_EQ(tps1.sec_since_epoch(), 1000);

    // 从time_point构造
    time_point     tp(milliseconds(2000));
    time_point_sec tps2(tp);
    EXPECT_EQ(tps2.sec_since_epoch(), 2);

    // 比较运算符
    time_point_sec tps3(2000);
    EXPECT_GT(tps3, tps1);
    EXPECT_GE(tps3, tps1);
    EXPECT_LT(tps1, tps3);
    EXPECT_LE(tps1, tps3);
    EXPECT_NE(tps1, tps3);
    EXPECT_EQ(tps1, time_point_sec(1000));

    // 转换为time_point
    time_point tp2 = static_cast<time_point>(tps1);
    EXPECT_EQ(tp2.time_since_epoch().count(), 1000 * 1000);

    // 加减运算符
    auto tps4 = tps1 + 1000;
    EXPECT_EQ(tps4.sec_since_epoch(), 2000);

    auto tps5 = tps3 - 1000;
    EXPECT_EQ(tps5.sec_since_epoch(), 1000);

    // 时间点与秒级时间点互操作
    auto diff1 = tp2 - tps1;
    EXPECT_EQ(diff1.count(), 0); // 应该是0，因为tp2是从tps1转换而来

    auto diff2 = tps3 - tps1;
    EXPECT_EQ(diff2.count(), 1000 * 1000);

    // 赋值运算符
    time_point_sec tps6(1000);
    tps6 += 500;
    EXPECT_EQ(tps6.sec_since_epoch(), 1500);

    time_point_sec tps7(2000);
    tps7 -= 500;
    EXPECT_EQ(tps7.sec_since_epoch(), 1500);

    time_point_sec tps8;
    tps8 = time_point(milliseconds(3000));
    EXPECT_EQ(tps8.sec_since_epoch(), 3);
}

// 测试ISO字符串转换
TEST(TimeTest, IsoStringTest) {
    // 时间点与字符串转换 (注意：这个值是1970年开始往后的特定时间戳)
    time_point  tp1(milliseconds(1577836800000)); // 2020-01-01T00:00:00
    std::string iso_str = std::string(tp1);
    EXPECT_EQ(iso_str, "2020-01-01T00:00:00.000");

    // 字符串解析测试
    time_point tp2 = time_point::from_iso_string("2020-01-01T00:00:00");
    EXPECT_EQ(tp2.time_since_epoch().count(), 1577836800000);

    // 带毫秒的转换测试
    time_point  tp3(milliseconds(1577836800123)); // 2020-01-01T00:00:00.123
    std::string iso_str_ms = std::string(tp3);
    EXPECT_EQ(iso_str_ms, "2020-01-01T00:00:00.123");

    time_point tp4 = time_point::from_iso_string("2020-01-01T00:00:00.123");
    EXPECT_EQ(tp4.time_since_epoch().count(), 1577836800123);

    // 秒级时间点的ISO字符串转换
    time_point_sec tps1(1577836800); // 2020-01-01T00:00:00
    std::string    tps_iso(tps1.to_iso_string());
    EXPECT_EQ(tps_iso, "2020-01-01T00:00:00");

    time_point_sec tps2 = time_point_sec::from_iso_string("2020-01-01T00:00:00");
    EXPECT_EQ(tps2.sec_since_epoch(), 1577836800);
}

// 测试variant转换
TEST(TimeTest, VariantConversionTest) {
    // 毫秒转variant
    milliseconds ms(1000);
    variant      v1;
    to_variant(ms, v1);
    EXPECT_EQ(v1.as<int64_t>(), 1000);

    // variant转毫秒
    milliseconds ms2;
    from_variant(v1, ms2);
    EXPECT_EQ(ms2.count(), 1000);

    // 时间点转variant
    time_point tp(milliseconds(1577836800000)); // 2020-01-01T00:00:00
    variant    v2;
    to_variant(tp, v2);
    EXPECT_EQ(v2.as<std::string>(), "2020-01-01T00:00:00.000");

    // variant转时间点
    time_point tp2;
    from_variant(v2, tp2);
    EXPECT_EQ(tp2.time_since_epoch().count(), 1577836800000);

    // 秒级时间点转variant
    time_point_sec tps(1577836800); // 2020-01-01T00:00:00
    variant        v3;
    to_variant(tps, v3);
    EXPECT_EQ(v3.as<std::string>(), "2020-01-01T00:00:00");

    // variant转秒级时间点
    time_point_sec tps2;
    from_variant(v3, tps2);
    EXPECT_EQ(tps2.sec_since_epoch(), 1577836800);
}

// 测试实际系统时间操作
TEST(TimeTest, SystemTimeTest) {
    // 获取当前时间
    time_point start = time_point::now();

    // 休眠一段时间
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 再次获取时间
    time_point end = time_point::now();

    // 确认时间差
    auto diff = end - start;
    EXPECT_GE(diff.count(), 100); // 至少过了100毫秒

    // 测试ISO字符串解析和格式化的一致性
    time_point  now     = time_point::from_iso_string("2020-01-01T12:00:00");
    std::string iso_str = std::string(now);
    time_point  parsed  = time_point::from_iso_string(iso_str);

    // 检查两个时间点是否相同
    auto time_diff = now - parsed;
    EXPECT_LT(std::abs(time_diff.count()), 2); // 允许最多1毫秒的误差
}