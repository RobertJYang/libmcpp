/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gtest/gtest.h>
#include <iostream>
#include <limits>
#include <string>

// 根据编译时宏选择使用哪个格式化库
#ifdef USE_MC_FORMAT
#include <mc/fmt/format.h>
using namespace mc::fmt;
#define compatibility_test mc_format_test
#define format             sformat
#else
#include <format>
#define compatibility_test std_format_test
#define format             std::format
#endif

using namespace testing;

// 基本浮点数格式化测试
TEST(compatibility_test, basic_formatting) {
    EXPECT_EQ(format("{}", 3.14159), "3.14159");
    EXPECT_EQ(format("{:.2f}", 3.14159), "3.14");
    EXPECT_EQ(format("{:.0f}", 3.14159), "3");
    EXPECT_EQ(format("{:#.0f}", 123.0), "123.");
    EXPECT_EQ(format("{:.02f}", 1.234), "1.23");
}

// 对齐和填充测试
TEST(compatibility_test, alignment) {
    EXPECT_EQ(format("{:10.2f}", 3.14), "      3.14");
    EXPECT_EQ(format("{:<10.2f}", 3.14), "3.14      ");
    EXPECT_EQ(format("{:^10.2f}", 3.14), "   3.14   ");
    EXPECT_EQ(format("{:*>10.2f}", 3.14), "******3.14");
}

// 符号处理测试
TEST(compatibility_test, sign_handling) {
    EXPECT_EQ(format("{:+f}; {:+f}", 3.14, -3.14), "+3.140000; -3.140000");
    EXPECT_EQ(format("{: f}; {: f}", 3.14, -3.14), " 3.140000; -3.140000");
    EXPECT_EQ(format("{:-f}; {:-f}", 3.14, -3.14), "3.140000; -3.140000");
}

// 特殊值测试
TEST(compatibility_test, special_values) {
    EXPECT_EQ(format("{}", std::numeric_limits<double>::infinity()), "inf");
    EXPECT_EQ(format("{}", -std::numeric_limits<double>::infinity()), "-inf");
    EXPECT_EQ(format("{}", std::numeric_limits<double>::quiet_NaN()), "nan");
}

// 零填充测试
TEST(compatibility_test, zero_padding) {
    EXPECT_EQ(format("{:03.2f}", -1.2), "-1.20");
    EXPECT_EQ(format("{:010.2f}", 3.14), "0000003.14");
    EXPECT_EQ(format("{:+010.2f}", 3.14), "+000003.14");
    EXPECT_EQ(format("{:-010.2f}", -3.14), "-000003.14");
}

// 零填充和对齐组合测试
TEST(compatibility_test, zero_padding_with_alignment) {
    EXPECT_EQ(format("{:<010.2f}", 3.14), "3.14      "); // 左对齐优先于零填充
    EXPECT_EQ(format("{:>010.2f}", 3.14), "      3.14"); // 右对齐时零填充生效
    EXPECT_EQ(format("{:^010.2f}", 3.14), "   3.14   "); // 居中对齐优先于零填充
}

// 精度测试
TEST(compatibility_test, precision) {
    EXPECT_EQ(format("{:.1f}", 0.000000001), "0.0");
    EXPECT_EQ(format("{:.2f}", 0.099), "0.10");
    EXPECT_EQ(format("{:.0e}", 9.5), "1e+01");
    EXPECT_EQ(format("{:.1e}", 1e-34), "1.0e-34");
}

// 舍入行为测试
TEST(compatibility_test, rounding_behavior) {
    EXPECT_EQ(format("{:.2f}", 3.145), "3.15"); // 四舍五入到3.15
    EXPECT_EQ(format("{:.2f}", 3.144), "3.14"); // 四舍五入到3.14
    EXPECT_EQ(format("{:.1f}", 3.15), "3.1");   // 四舍五入到3.1
}

// 大数测试
TEST(compatibility_test, large_numbers) {
    EXPECT_EQ(format("{:.2f}", 1234567.89), "1234567.89");
    EXPECT_EQ(format("{}", 123456789.0f), "123456792");
    EXPECT_EQ(format("{}", 1019666432.0f), "1019666432");
}

// 浮点数去除尾0和小数点的测试
TEST(compatibility_test, remove_trailing_zeros) {
    EXPECT_EQ(format("{:.0f}", 1.0), "1");
    EXPECT_EQ(format("{:.0e}", 1.0), "1e+00");
    EXPECT_EQ(format("{:.0E}", 1.0), "1E+00");
    EXPECT_EQ(format("{:.0g}", 1.0), "1");
    EXPECT_EQ(format("{:.0G}", 1.0), "1");
    EXPECT_EQ(format("{}", 1.0), "1");
    EXPECT_EQ(format("{:#.0f}", 1.0), "1.");
    EXPECT_EQ(format("{:#.0e}", 1.0), "1.e+00");
    EXPECT_EQ(format("{:#.0E}", 1.0), "1.E+00");
    EXPECT_EQ(format("{:#.0g}", 1.0), "1.");
    EXPECT_EQ(format("{:#.0G}", 1.0), "1.");
    EXPECT_EQ(format("{:.2f}", 1.0), "1.00");
    EXPECT_EQ(format("{:.2e}", 1.0), "1.00e+00");
    EXPECT_EQ(format("{:.2E}", 1.0), "1.00E+00");
    EXPECT_EQ(format("{:.2g}", 1.0), "1");
    EXPECT_EQ(format("{:.2G}", 1.0), "1");
}

// 综合格式化测试
TEST(compatibility_test, complex_formatting) {
    EXPECT_EQ(format("{:0.7f}:{:03}:{:+g}:{}:{}:{}",
                     1.234, 42, 3.13, "str", reinterpret_cast<void*>(1000), 'x'),
              "1.2340000:042:+3.13:str:0x3e8:x");
}

// 极值测试
TEST(compatibility_test, limits) {
    EXPECT_EQ(format("{:g}", std::numeric_limits<double>::max()).substr(0, 5), "1.797");
    EXPECT_EQ(format("{:g}", std::numeric_limits<double>::min()).substr(0, 5), "2.225");
}

// nan/inf 组合测试
TEST(compatibility_test, nan_inf_combinations) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    double inf = std::numeric_limits<double>::infinity();
    EXPECT_EQ(format("{:+10f}", nan), "      +nan");
    EXPECT_EQ(format("{:<10f}", inf), "inf       ");
    EXPECT_EQ(format("{:^10f}", -inf), "   -inf   ");
    EXPECT_EQ(format("{:.0f}", nan), "nan");
    EXPECT_EQ(format("{:.0f}", inf), "inf");
}

// 零值测试
TEST(compatibility_test, zero_values) {
    double pos_zero = 0.0;
    double neg_zero = -0.0;

    // 基本格式
    EXPECT_EQ(format("{}", pos_zero), "0");
    EXPECT_EQ(format("{}", neg_zero), "-0");

    // 固定点格式
    EXPECT_EQ(format("{:f}", pos_zero), "0.000000");
    EXPECT_EQ(format("{:f}", neg_zero), "-0.000000");
    EXPECT_EQ(format("{:.2f}", pos_zero), "0.00");
    EXPECT_EQ(format("{:.2f}", neg_zero), "-0.00");
    EXPECT_EQ(format("{:.0f}", pos_zero), "0");
    EXPECT_EQ(format("{:.0f}", neg_zero), "-0");

    // 科学计数法
    EXPECT_EQ(format("{:e}", pos_zero), "0.000000e+00");
    EXPECT_EQ(format("{:e}", neg_zero), "-0.000000e+00");
    EXPECT_EQ(format("{:.2e}", pos_zero), "0.00e+00");
    EXPECT_EQ(format("{:.2e}", neg_zero), "-0.00e+00");
    EXPECT_EQ(format("{:E}", pos_zero), "0.000000E+00");
    EXPECT_EQ(format("{:E}", neg_zero), "-0.000000E+00");

    // 通用格式
    EXPECT_EQ(format("{:g}", pos_zero), "0");
    EXPECT_EQ(format("{:g}", neg_zero), "-0");
    EXPECT_EQ(format("{:.2g}", pos_zero), "0");
    EXPECT_EQ(format("{:.2g}", neg_zero), "-0");
    EXPECT_EQ(format("{:.0g}", pos_zero), "0");
    EXPECT_EQ(format("{:.0g}", neg_zero), "-0");
    EXPECT_EQ(format("{:G}", pos_zero), "0");
    EXPECT_EQ(format("{:G}", neg_zero), "-0");

    // 十六进制浮点
    EXPECT_EQ(format("{:a}", pos_zero), "0p+0");
    EXPECT_EQ(format("{:a}", neg_zero), "-0p+0");
    EXPECT_EQ(format("{:A}", pos_zero), "0P+0");
    EXPECT_EQ(format("{:A}", neg_zero), "-0P+0");

    // 符号处理
    EXPECT_EQ(format("{:+}", pos_zero), "+0");
    EXPECT_EQ(format("{:+}", neg_zero), "-0");
    EXPECT_EQ(format("{: }", pos_zero), " 0");
    EXPECT_EQ(format("{: }", neg_zero), "-0");
    EXPECT_EQ(format("{:+.2f}", pos_zero), "+0.00");
    EXPECT_EQ(format("{:+.2f}", neg_zero), "-0.00");

    // 替代格式
    EXPECT_EQ(format("{:#.2f}", pos_zero), "0.00");
    EXPECT_EQ(format("{:#.2f}", neg_zero), "-0.00");
    EXPECT_EQ(format("{:#.0f}", pos_zero), "0.");
    EXPECT_EQ(format("{:#.0f}", neg_zero), "-0.");
    EXPECT_EQ(format("{:#.2g}", pos_zero), "0.0");
    EXPECT_EQ(format("{:#.2g}", neg_zero), "-0.0");
    EXPECT_EQ(format("{:#.0g}", pos_zero), "0.");
    EXPECT_EQ(format("{:#.0g}", neg_zero), "-0.");

    // 对齐和填充
    EXPECT_EQ(format("{:>8.2f}", pos_zero), "    0.00");
    EXPECT_EQ(format("{:>8.2f}", neg_zero), "   -0.00");
    EXPECT_EQ(format("{:08.2f}", pos_zero), "00000.00");
    EXPECT_EQ(format("{:08.2f}", neg_zero), "-0000.00");
}

// 科学计数法
TEST(compatibility_test, scientific) {
    EXPECT_EQ(format("{:e}", 12345.0), "1.234500e+04");
    EXPECT_EQ(format("{:.2e}", 0.00123), "1.23e-03");
    EXPECT_EQ(format("{:E}", 0.00123), "1.230000E-03");
}

// 十六进制浮点
TEST(compatibility_test, hex_float) {
    EXPECT_EQ(format("{:a}", 16.5), "1.08p+4");
    EXPECT_EQ(format("{:A}", -16.5), "-1.08P+4");
}

// 动态宽度与精度
TEST(compatibility_test, dynamic_width_precision) {
    EXPECT_EQ(format("{0:{1}.{2}f}", 1.23456, 8, 3), "   1.235");
    EXPECT_EQ(format("{0:.{1}f}", 1.23456, 2), "1.23");
    EXPECT_EQ(format("{0:{1}f}", 1.2, 6), "1.200000");
}

// 对齐与填充
TEST(compatibility_test, align_fill) {
    EXPECT_EQ(format("{:>8.2f}", 1.2), "    1.20");
    EXPECT_EQ(format("{:*<8.1f}", 1.2), "1.2*****");
    EXPECT_EQ(format("{:^10.3f}", 1.234), "  1.234   ");
}

// 整数格式化测试
TEST(compatibility_test, integer_format) {
    EXPECT_EQ(format("{}", 42), "42");
    EXPECT_EQ(format("{:d}", -42), "-42");
    EXPECT_EQ(format("{:b}", 10), "1010");
    EXPECT_EQ(format("{:o}", 10), "12");
    EXPECT_EQ(format("{:x}", 255), "ff");
    EXPECT_EQ(format("{:X}", 255), "FF");
    EXPECT_EQ(format("{:#x}", 255), "0xff");
    EXPECT_EQ(format("{:#o}", 10), "012");
    EXPECT_EQ(format("{:+d}", 42), "+42");
    EXPECT_EQ(format("{:05d}", 42), "00042");
    EXPECT_EQ(format("{:>6d}", 42), "    42");
    EXPECT_EQ(format("{:<6d}", 42), "42    ");
    EXPECT_EQ(format("{:^6d}", 42), "  42  ");
    EXPECT_EQ(format("{}", std::numeric_limits<int>::max()), std::to_string(std::numeric_limits<int>::max()));
    EXPECT_EQ(format("{}", std::numeric_limits<int>::min()), std::to_string(std::numeric_limits<int>::min()));
}

// 不同整数类型的格式化测试
TEST(compatibility_test, integer_format_types) {
    using std::numeric_limits;

    // short
    short          s  = -1234;
    unsigned short us = 4321;
    EXPECT_EQ(format("{}", static_cast<int>(s)), "-1234");
    EXPECT_EQ(format("{}", static_cast<unsigned int>(us)), "4321");
    EXPECT_EQ(format("{:x}", static_cast<unsigned int>(us)), "10e1");

    // int/unsigned int
    int          i  = -56789;
    unsigned int ui = 56789;
    EXPECT_EQ(format("{}", i), "-56789");
    EXPECT_EQ(format("{}", ui), "56789");
    EXPECT_EQ(format("{:b}", ui), "1101110111010101");

    // long/unsigned long
    long          l  = -1234567;
    unsigned long ul = 1234567;
    EXPECT_EQ(format("{}", l), "-1234567");
    EXPECT_EQ(format("{}", ul), "1234567");
    EXPECT_EQ(format("{:o}", ul), "4553207");

    // long long/unsigned long long
    long long          ll  = -9876543210LL;
    unsigned long long ull = 9876543210ULL;
    EXPECT_EQ(format("{}", ll), "-9876543210");
    EXPECT_EQ(format("{}", ull), "9876543210");
    EXPECT_EQ(format("{:X}", ull), "24CB016EA");

    // 0
    EXPECT_EQ(format("{}", 0), "0");
    EXPECT_EQ(format("{}", 0u), "0");
    EXPECT_EQ(format("{}", 0ll), "0");

    // 极值
    EXPECT_EQ(format("{}", static_cast<int>(numeric_limits<short>::max())), std::to_string(numeric_limits<short>::max()));
    EXPECT_EQ(format("{}", static_cast<int>(numeric_limits<short>::min())), std::to_string(numeric_limits<short>::min()));
    EXPECT_EQ(format("{}", static_cast<unsigned int>(numeric_limits<unsigned short>::max())), std::to_string(numeric_limits<unsigned short>::max()));
    EXPECT_EQ(format("{}", numeric_limits<int>::max()), std::to_string(numeric_limits<int>::max()));
    EXPECT_EQ(format("{}", numeric_limits<int>::min()), std::to_string(numeric_limits<int>::min()));
    EXPECT_EQ(format("{}", numeric_limits<unsigned int>::max()), std::to_string(numeric_limits<unsigned int>::max()));
    EXPECT_EQ(format("{}", numeric_limits<long>::max()), std::to_string(numeric_limits<long>::max()));
    EXPECT_EQ(format("{}", numeric_limits<long>::min()), std::to_string(numeric_limits<long>::min()));
    EXPECT_EQ(format("{}", numeric_limits<unsigned long>::max()), std::to_string(numeric_limits<unsigned long>::max()));
    EXPECT_EQ(format("{}", numeric_limits<long long>::max()), std::to_string(numeric_limits<long long>::max()));
    EXPECT_EQ(format("{}", numeric_limits<long long>::min()), std::to_string(numeric_limits<long long>::min()));
    EXPECT_EQ(format("{}", numeric_limits<unsigned long long>::max()), std::to_string(numeric_limits<unsigned long long>::max()));

    // 填充、对齐、符号
    EXPECT_EQ(format("{:06d}", static_cast<int>(12)), "000012");
    EXPECT_EQ(format("{:+d}", static_cast<long long>(-99)), "-99");
    EXPECT_EQ(format("{:+d}", static_cast<long long>(99)), "+99");
    EXPECT_EQ(format("{:>8d}", static_cast<unsigned int>(77)), "      77");
    EXPECT_EQ(format("{:<8d}", static_cast<unsigned int>(77)), "77      ");
    EXPECT_EQ(format("{:^8d}", static_cast<unsigned int>(77)), "   77   ");

    // 进制
    EXPECT_EQ(format("{:#x}", static_cast<unsigned long long>(255)), "0xff");
    EXPECT_EQ(format("{:#o}", static_cast<unsigned long long>(10)), "012");
    EXPECT_EQ(format("{:#b}", static_cast<unsigned int>(42)), "0b101010");
}

// 字符串格式化测试
TEST(compatibility_test, string_format) {
    EXPECT_EQ(format("{}", "hello"), "hello");
    EXPECT_EQ(format("{:10}", "hello"), "hello     ");
    EXPECT_EQ(format("{:^10}", "hello"), "  hello   ");
    EXPECT_EQ(format("{:<10}", "hello"), "hello     ");
    EXPECT_EQ(format("{:>10}", "hello"), "     hello");
    EXPECT_EQ(format("{:.2}", "hello"), "he");
}

// 指针格式化测试
TEST(compatibility_test, pointer_format) {
    int         x      = 42;
    void*       ptr    = &x;
    std::string result = format("{}", ptr);
    EXPECT_TRUE(result.find("0x") == 0 || result.find("0X") == 0);
}

// 字符格式化测试
TEST(compatibility_test, char_format) {
    EXPECT_EQ(format("{}", 'A'), "A");
    EXPECT_EQ(format("{:c}", 'A'), "A");
    EXPECT_EQ(format("{:d}", 'A'), "65");
}

// 符号处理测试
TEST(compatibility_test, sign_handling_integers) {
    EXPECT_EQ(format("{:+}", 42), "+42");
    EXPECT_EQ(format("{:+}", -42), "-42");
    EXPECT_EQ(format("{: }", 42), " 42");
    EXPECT_EQ(format("{: }", -42), "-42");
    EXPECT_EQ(format("{:-}", 42), "42");
    EXPECT_EQ(format("{:-}", -42), "-42");
}

// 零填充测试
TEST(compatibility_test, zero_padding_integers) {
    EXPECT_EQ(format("{:05}", 42), "00042");
    EXPECT_EQ(format("{:05d}", 42), "00042");
    EXPECT_EQ(format("{:05x}", 42), "0002a");
}

// 替代格式测试
TEST(compatibility_test, alternate_form) {
    EXPECT_EQ(format("{:#x}", 42), "0x2a");
    EXPECT_EQ(format("{:#X}", 42), "0X2A");
    EXPECT_EQ(format("{:#o}", 42), "052");
    EXPECT_EQ(format("{:#b}", 42), "0b101010");
    EXPECT_EQ(format("{:#.2f}", 3.14159), "3.14");
    EXPECT_EQ(format("{:#.2g}", 3.14159), "3.1");
}

// bool 类型格式化测试
TEST(compatibility_test, bool_format) {
    EXPECT_EQ(format("{}", true), "true");
    EXPECT_EQ(format("{}", false), "false");
    EXPECT_EQ(format("{:d}", true), "1");
    EXPECT_EQ(format("{:d}", false), "0");
    EXPECT_EQ(format("{:b}", true), "1");
    EXPECT_EQ(format("{:b}", false), "0");
    EXPECT_EQ(format("{:x}", true), "1");
    EXPECT_EQ(format("{:x}", false), "0");
    EXPECT_EQ(format("{:>6}", true), "  true");
    EXPECT_EQ(format("{:<6}", false), "false ");
    EXPECT_EQ(format("{:^7}", true), " true  ");
    // 混合类型
    EXPECT_EQ(format("{} {}", true, false), "true false");
    EXPECT_EQ(format("{:d} {:d}", true, false), "1 0");
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}