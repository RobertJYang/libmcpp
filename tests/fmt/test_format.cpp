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

#include <gtest/gtest.h>
#include <iostream>
#include <limits>
#include <mc/fmt/format.h>

using namespace mc::fmt;

// 基本浮点数格式化测试
TEST(format_test, basic_formatting) {
    EXPECT_EQ(sformat("{}", 3.14159), "3.14159");
    EXPECT_EQ(sformat("{:.2f}", 3.14159), "3.14");
    EXPECT_EQ(sformat("{:.0f}", 3.14159), "3");
    EXPECT_EQ(sformat("{:#.0f}", 123.0), "123.");
    EXPECT_EQ(sformat("{:.02f}", 1.234), "1.23");
}

// 对齐和填充测试
TEST(format_test, alignment) {
    EXPECT_EQ(sformat("{:10.2f}", 3.14), "      3.14");
    EXPECT_EQ(sformat("{:<10.2f}", 3.14), "3.14      ");
    EXPECT_EQ(sformat("{:^10.2f}", 3.14), "   3.14   ");
    EXPECT_EQ(sformat("{:*>10.2f}", 3.14), "******3.14");
}

// 符号处理测试
TEST(format_test, sign_handling) {
    EXPECT_EQ(sformat("{:+f}; {:+f}", 3.14, -3.14), "+3.140000; -3.140000");
    EXPECT_EQ(sformat("{: f}; {: f}", 3.14, -3.14), " 3.140000; -3.140000");
    EXPECT_EQ(sformat("{:-f}; {:-f}", 3.14, -3.14), "3.140000; -3.140000");
}

// 特殊值测试
TEST(format_test, special_values) {
    EXPECT_EQ(sformat("{}", std::numeric_limits<double>::infinity()), "inf");
    EXPECT_EQ(sformat("{}", -std::numeric_limits<double>::infinity()), "-inf");
    EXPECT_EQ(sformat("{}", std::numeric_limits<double>::quiet_NaN()), "nan");
}

// 零填充测试
TEST(format_test, zero_padding) {
    EXPECT_EQ(sformat("{:03.2f}", -1.2), "-1.20");
    EXPECT_EQ(sformat("{:010.2f}", 3.14), "0000003.14");
    EXPECT_EQ(sformat("{:+010.2f}", 3.14), "+000003.14");
    EXPECT_EQ(sformat("{:-010.2f}", -3.14), "-000003.14");
}

// 零填充和对齐组合测试
TEST(format_test, zero_padding_with_alignment) {
    EXPECT_EQ(sformat("{:<010.2f}", 3.14), "3.14      "); // 左对齐优先于零填充
    EXPECT_EQ(sformat("{:>010.2f}", 3.14), "      3.14"); // 右对齐时零填充不生效（与 std::sformat 一致）
    EXPECT_EQ(sformat("{:^010.2f}", 3.14), "   3.14   "); // 居中对齐优先于零填充
}

// 精度测试
TEST(format_test, precision) {
    EXPECT_EQ(sformat("{:.1f}", 0.000000001), "0.0");
    EXPECT_EQ(sformat("{:.2f}", 0.099), "0.10");
    EXPECT_EQ(sformat("{:.0e}", 9.5), "1e+01");
    EXPECT_EQ(sformat("{:.1e}", 1e-34), "1.0e-34");
}

// 舍入行为测试
TEST(format_test, rounding_behavior) {
    EXPECT_EQ(sformat("{:.2f}", 3.145), "3.15"); // 四舍五入到3.15
    EXPECT_EQ(sformat("{:.2f}", 3.144), "3.14"); // 四舍五入到3.14
    EXPECT_EQ(sformat("{:.1f}", 3.15), "3.1");   // 四舍五入到3.1
}

// 大数测试
TEST(format_test, large_numbers) {
    EXPECT_EQ(sformat("{:.2f}", 1234567.89), "1234567.89");
    EXPECT_EQ(sformat("{}", 123456789.0f), "123456792");   // float 精度限制
    EXPECT_EQ(sformat("{}", 1019666432.0f), "1019666432"); // float 精度限制
}

// 浮点数去除尾0和小数点的测试
TEST(format_test, remove_trailing_zeros) {
    EXPECT_EQ(sformat("{:.0f}", 1.0), "1");
    EXPECT_EQ(sformat("{:.0e}", 1.0), "1e+00");
    EXPECT_EQ(sformat("{:.0E}", 1.0), "1E+00");
    EXPECT_EQ(sformat("{:.0g}", 1.0), "1");
    EXPECT_EQ(sformat("{:.0G}", 1.0), "1");
    EXPECT_EQ(sformat("{}", 1.0), "1");
    EXPECT_EQ(sformat("{:#.0f}", 1.0), "1.");
    EXPECT_EQ(sformat("{:#.0e}", 1.0), "1.e+00");
    EXPECT_EQ(sformat("{:#.0E}", 1.0), "1.E+00");
    EXPECT_EQ(sformat("{:#.0g}", 1.0), "1.");
    EXPECT_EQ(sformat("{:#.0G}", 1.0), "1.");
    EXPECT_EQ(sformat("{:.2f}", 1.0), "1.00");
    EXPECT_EQ(sformat("{:.2e}", 1.0), "1.00e+00");
    EXPECT_EQ(sformat("{:.2E}", 1.0), "1.00E+00");
    EXPECT_EQ(sformat("{:.2g}", 1.0), "1");
    EXPECT_EQ(sformat("{:.2G}", 1.0), "1");
}

// 综合格式化测试
TEST(format_test, complex_formatting) {
    EXPECT_EQ(sformat("{:0.7f}:{:03}:{:+g}:{}:{}:{}",
                      1.234, 42, 3.13, "str", reinterpret_cast<void*>(1000), 'x'),
              "1.2340000:042:+3.13:str:0x3e8:x");
}

// 极值测试
TEST(format_test, limits) {
    EXPECT_EQ(sformat("{:g}", std::numeric_limits<double>::max()).substr(0, 5), "1.797");
    EXPECT_EQ(sformat("{:g}", std::numeric_limits<double>::min()).substr(0, 5), "2.225");
    EXPECT_EQ(sformat("{:g}", std::numeric_limits<double>::denorm_min()).substr(0, 5), "4.940");
}

// nan/inf 组合测试
TEST(format_test, nan_inf_combinations) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    double inf = std::numeric_limits<double>::infinity();
    EXPECT_EQ(sformat("{:+10f}", nan), "      +nan");
    EXPECT_EQ(sformat("{:<10f}", inf), "inf       ");
    EXPECT_EQ(sformat("{:^10f}", -inf), "   -inf   ");
    EXPECT_EQ(sformat("{:.0f}", nan), "nan");
    EXPECT_EQ(sformat("{:.0f}", inf), "inf");
}

// 负零测试
TEST(format_test, negative_zero) {
    double neg_zero = -0.0;
    EXPECT_EQ(sformat("{}", neg_zero), "-0");
    EXPECT_EQ(sformat("{:+}", neg_zero), "-0");
    EXPECT_EQ(sformat("{: f}", neg_zero), "-0.000000"); // 空格格式需要完整精度
}

// 科学计数法
TEST(format_test, scientific) {
    EXPECT_EQ(sformat("{:e}", 12345.0), "1.234500e+04");
    EXPECT_EQ(sformat("{:.2e}", 0.00123), "1.23e-03");
    EXPECT_EQ(sformat("{:E}", 0.00123), "1.230000E-03");
}

// 十六进制浮点
TEST(format_test, hex_float) {
    EXPECT_EQ(sformat("{:a}", 16.5), "1.08p+4");   // 不包含 0x 前缀
    EXPECT_EQ(sformat("{:A}", -16.5), "-1.08P+4"); // 不包含 0X 前缀
}

// 动态宽度与精度
TEST(format_test, dynamic_width_precision) {
    EXPECT_EQ(sformat("{0:{1}.{2}f}", 1.23456, 8, 3), "   1.235");
    EXPECT_EQ(sformat("{0:.{1}f}", 1.23456, 2), "1.23");
    EXPECT_EQ(sformat("{0:{1}f}", 1.2, 6), "1.200000");
}

// 对齐与填充
TEST(format_test, align_fill) {
    EXPECT_EQ(sformat("{:>8.2f}", 1.2), "    1.20");
    EXPECT_EQ(sformat("{:*<8.1f}", 1.2), "1.2*****");
    EXPECT_EQ(sformat("{:^10.3f}", 1.234), "  1.234   "); // 修正居中对齐
}

// 非法格式符
TEST(format_test, invalid_spec) {
    EXPECT_THROW(sformat("{:.2d}", 1.23), mc::format_error);
    EXPECT_THROW(sformat("{:z}", 1.23), mc::format_error);
    EXPECT_THROW(sformat("{:q}", 1.23), mc::format_error);
}

// 组合场景
TEST(format_test, multi_args) {
    EXPECT_EQ(sformat("{0:.2f}:{1:>6.1e}:{2}", 1.234, 1234.5, "ok"), "1.23:1.2e+03:ok");
}

// 没有参数
TEST(format_test, no_args) {
    EXPECT_TRUE(mc::fmt::detail::compile_check("abc")); // OK
    EXPECT_EQ(sformat("abc"), "abc");

    EXPECT_FALSE(mc::fmt::detail::compile_check("abc{}")); // 需要参数但为提供
}

// 整数格式化测试
TEST(format_test, integer_format) {
    EXPECT_EQ(sformat("{}", 42), "42");
    EXPECT_EQ(sformat("{:d}", -42), "-42");
    EXPECT_EQ(sformat("{:b}", 10), "1010");
    EXPECT_EQ(sformat("{:o}", 10), "12");
    EXPECT_EQ(sformat("{:x}", 255), "ff");
    EXPECT_EQ(sformat("{:X}", 255), "FF");
    EXPECT_EQ(sformat("{:#x}", 255), "0xff");
    EXPECT_EQ(sformat("{:#o}", 10), "012");
    EXPECT_EQ(sformat("{:+d}", 42), "+42");
    EXPECT_EQ(sformat("{:05d}", 42), "00042");
    EXPECT_EQ(sformat("{:>6d}", 42), "    42");
    EXPECT_EQ(sformat("{:<6d}", 42), "42    ");
    EXPECT_EQ(sformat("{:^6d}", 42), "  42  ");
    EXPECT_EQ(sformat("{}", std::numeric_limits<int>::max()), std::to_string(std::numeric_limits<int>::max()));
    EXPECT_EQ(sformat("{}", std::numeric_limits<int>::min()), std::to_string(std::numeric_limits<int>::min()));
    EXPECT_THROW(sformat("{:f}", 42), mc::format_error); // 错误格式符
}

// 不同整数类型的格式化测试
TEST(format_test, integer_format_types) {
    using std::numeric_limits;

    // short
    short          s  = -1234;
    unsigned short us = 4321;
    EXPECT_EQ(sformat("{}", static_cast<int>(s)), "-1234");
    EXPECT_EQ(sformat("{}", static_cast<unsigned int>(us)), "4321");
    EXPECT_EQ(sformat("{:x}", static_cast<unsigned int>(us)), "10e1");

    // int/unsigned int
    int          i  = -56789;
    unsigned int ui = 56789;
    EXPECT_EQ(sformat("{}", i), "-56789");
    EXPECT_EQ(sformat("{}", ui), "56789");
    EXPECT_EQ(sformat("{:b}", ui), "1101110111010101");

    // long/unsigned long
    long          l  = -1234567;
    unsigned long ul = 1234567;
    EXPECT_EQ(sformat("{}", l), "-1234567");
    EXPECT_EQ(sformat("{}", ul), "1234567");
    EXPECT_EQ(sformat("{:o}", ul), "4553207");

    // long long/unsigned long long
    long long          ll  = -9876543210LL;
    unsigned long long ull = 9876543210ULL;
    EXPECT_EQ(sformat("{}", ll), "-9876543210");
    EXPECT_EQ(sformat("{}", ull), "9876543210");
    EXPECT_EQ(sformat("{:X}", ull), "24CB016EA");

    // 0
    EXPECT_EQ(sformat("{}", 0), "0");
    EXPECT_EQ(sformat("{}", 0u), "0");
    EXPECT_EQ(sformat("{}", 0ll), "0");

    // 极值
    EXPECT_EQ(sformat("{}", static_cast<int>(numeric_limits<short>::max())), std::to_string(numeric_limits<short>::max()));
    EXPECT_EQ(sformat("{}", static_cast<int>(numeric_limits<short>::min())), std::to_string(numeric_limits<short>::min()));
    EXPECT_EQ(sformat("{}", static_cast<unsigned int>(numeric_limits<unsigned short>::max())), std::to_string(numeric_limits<unsigned short>::max()));
    EXPECT_EQ(sformat("{}", numeric_limits<int>::max()), std::to_string(numeric_limits<int>::max()));
    EXPECT_EQ(sformat("{}", numeric_limits<int>::min()), std::to_string(numeric_limits<int>::min()));
    EXPECT_EQ(sformat("{}", numeric_limits<unsigned int>::max()), std::to_string(numeric_limits<unsigned int>::max()));
    EXPECT_EQ(sformat("{}", numeric_limits<long>::max()), std::to_string(numeric_limits<long>::max()));
    EXPECT_EQ(sformat("{}", numeric_limits<long>::min()), std::to_string(numeric_limits<long>::min()));
    EXPECT_EQ(sformat("{}", numeric_limits<unsigned long>::max()), std::to_string(numeric_limits<unsigned long>::max()));
    EXPECT_EQ(sformat("{}", numeric_limits<long long>::max()), std::to_string(numeric_limits<long long>::max()));
    EXPECT_EQ(sformat("{}", numeric_limits<long long>::min()), std::to_string(numeric_limits<long long>::min()));
    EXPECT_EQ(sformat("{}", numeric_limits<unsigned long long>::max()), std::to_string(numeric_limits<unsigned long long>::max()));

    // 填充、对齐、符号
    EXPECT_EQ(sformat("{:06d}", static_cast<int>(12)), "000012");
    EXPECT_EQ(sformat("{:+d}", static_cast<long long>(-99)), "-99");
    EXPECT_EQ(sformat("{:+d}", static_cast<long long>(99)), "+99");
    EXPECT_EQ(sformat("{:>8d}", static_cast<unsigned int>(77)), "      77");
    EXPECT_EQ(sformat("{:<8d}", static_cast<unsigned int>(77)), "77      ");
    EXPECT_EQ(sformat("{:^8d}", static_cast<unsigned int>(77)), "   77   ");

    // 进制
    EXPECT_EQ(sformat("{:#x}", static_cast<unsigned long long>(255)), "0xff");
    EXPECT_EQ(sformat("{:#o}", static_cast<unsigned long long>(10)), "012");
    EXPECT_EQ(sformat("{:#b}", static_cast<unsigned int>(42)), "0b101010");
}

// 字符串格式化测试
TEST(format_test, string_format) {
    EXPECT_EQ(sformat("{}", "hello"), "hello");
    EXPECT_EQ(sformat("{:>8}", "hi"), "      hi");
    EXPECT_EQ(sformat("{:<8}", "hi"), "hi      ");
    EXPECT_EQ(sformat("{:^8}", "hi"), "   hi   ");
    EXPECT_EQ(sformat("{:.2}", "hello"), "he");
    EXPECT_EQ(sformat("{:*>6.3}", "abcdef"), "***abc");
    EXPECT_EQ(sformat("{}", ""), "");
    std::string long_str(100, 'a');
    EXPECT_EQ(sformat("{}", long_str), long_str);
    EXPECT_THROW(sformat("{:d}", "str"), mc::format_error); // 错误格式符
}

// 指针格式化测试
TEST(format_test, pointer_format) {
    int   a = 123;
    void* p = &a;
    EXPECT_TRUE(sformat("{}", p).find("0x") == 0); // 以0x开头
    EXPECT_EQ(sformat("{}", static_cast<void*>(nullptr)), "0x0");
    EXPECT_EQ(sformat("{:>10}", static_cast<void*>(nullptr)), "       0x0");
    EXPECT_THROW(sformat("{:d}", p), mc::format_error); // 错误格式符
}

// 字符格式化测试
TEST(format_test, char_format) {
    EXPECT_EQ(sformat("{}", 'A'), "A");
    EXPECT_EQ(sformat("{:c}", 'B'), "B");
    EXPECT_EQ(sformat("{:>4c}", 'C'), "   C");
    EXPECT_EQ(sformat("{:<4c}", 'D'), "D   ");
    EXPECT_EQ(sformat("{:^5c}", 'E'), "  E  ");
    EXPECT_EQ(sformat("{}", '\n'), "\n");
    EXPECT_EQ(sformat("{:d}", 'A'), "65"); // 字符的 ASCII 值
}

// bool 类型格式化测试
TEST(format_test, bool_format) {
    EXPECT_EQ(sformat("{}", true), "true");
    EXPECT_EQ(sformat("{}", false), "false");
    EXPECT_EQ(sformat("{:d}", true), "1");
    EXPECT_EQ(sformat("{:d}", false), "0");
    EXPECT_EQ(sformat("{:b}", true), "1");
    EXPECT_EQ(sformat("{:b}", false), "0");
    EXPECT_EQ(sformat("{:x}", true), "1");
    EXPECT_EQ(sformat("{:x}", false), "0");
    EXPECT_EQ(sformat("{:>6}", true), "  true");
    EXPECT_EQ(sformat("{:<6}", false), "false ");
    EXPECT_EQ(sformat("{:^7}", true), " true  ");
    // 混合类型
    EXPECT_EQ(sformat("{} {}", true, false), "true false");
    EXPECT_EQ(sformat("{:d} {:d}", true, false), "1 0");
}

TEST(format_test, index_args) {
    EXPECT_EQ(sformat("{0} {}", "first", "second"), "first second");
    EXPECT_EQ(sformat("{} {1}", "first", "second"), "first second");

    // 隐式索引总是从前一个显示索引开始自增
    EXPECT_EQ(sformat("{0} {1} {0} {} {}", "first", "second", "third"),
              "first second first second third");

    // 隐式索引后最好使用显式索引避免歧义
    EXPECT_EQ(sformat("{0} {1} {} {2}", "first",
                      "second", "third"),
              "first second third third");
}

// 编译期检查测试
TEST(format_test, compile_check) {
    // 测试基本的编译期检查
    bool valid_format = MC_FORMAT_COMPILE_CHECK(
        "{} {} {}",
        42, 3.14, "hello");
    EXPECT_TRUE(valid_format);

    // 测试字符串字面量处理（这个之前会导致编译错误）
    bool string_literal = MC_FORMAT_COMPILE_CHECK(
        "{}",
        "test");
    EXPECT_TRUE(string_literal);

    // 测试 chrono 错误检测
    using namespace std::chrono;
    auto duration = seconds(42);

    // 测试有效的 chrono 格式
    bool valid_chrono = MC_FORMAT_COMPILE_CHECK(
        "{:%H:%M:%S}",
        duration);
    EXPECT_TRUE(valid_chrono);

    // 测试无效的 chrono 格式（无效的格式说明符）
    bool invalid_chrono = MC_FORMAT_COMPILE_CHECK(
        "{:%H:%M:%}",
        duration);
    EXPECT_FALSE(invalid_chrono);

    // 测试 duration 不支持的格式说明符
    bool invalid_duration = MC_FORMAT_COMPILE_CHECK(
        "{:%Y}",
        duration);
    EXPECT_FALSE(invalid_duration);

    // 测试不完整的格式说明符
    bool incomplete_format = MC_FORMAT_COMPILE_CHECK(
        "{:%H:%M%}",
        duration);
    EXPECT_FALSE(incomplete_format);
}