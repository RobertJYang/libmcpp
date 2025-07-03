/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
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
#include <mc/dict.h>
#include <mc/string.h>

class test_format_with_spec : public ::testing::Test {};

// 测试基本的浮点数格式化
TEST_F(test_format_with_spec, test_float_precision) {
    mc::mutable_dict args;
    args["value"] = 3.14159;

    // 预期输出：2位小数精度
    std::string result   = mc::format_dict("数值: ${value:.2f}", args);
    std::string expected = "数值: 3.14";

    EXPECT_EQ(expected, result);
}

// 测试整数格式化 - 十六进制
TEST_F(test_format_with_spec, test_int_hex) {
    mc::mutable_dict args;
    args["value"] = 255;

    // 预期输出：十六进制格式
    std::string result   = mc::format_dict("十六进制: ${value:x}", args);
    std::string expected = "十六进制: ff";

    EXPECT_EQ(expected, result);
}

// 测试整数格式化 - 八进制
TEST_F(test_format_with_spec, test_int_oct) {
    mc::mutable_dict args;
    args["value"] = 64;

    // 预期输出：八进制格式
    std::string result   = mc::format_dict("八进制: ${value:o}", args);
    std::string expected = "八进制: 100";

    EXPECT_EQ(expected, result);
}

// 测试字符串宽度和对齐
TEST_F(test_format_with_spec, test_string_width) {
    mc::mutable_dict args;
    args["name"] = "Alice";

    // 预期输出：10个字符宽度，左对齐
    std::string result   = mc::format_dict("姓名: '${name:<10}'", args);
    std::string expected = "姓名: 'Alice     '";

    EXPECT_EQ(expected, result);
}

// 测试布尔值格式化
TEST_F(test_format_with_spec, test_bool_format) {
    mc::mutable_dict args;
    args["flag"] = true;

    // 预期输出：布尔值作为数字
    std::string result   = mc::format_dict("标志: ${flag:d}", args);
    std::string expected = "标志: 1";

    EXPECT_EQ(expected, result);
}

// 测试科学计数法
TEST_F(test_format_with_spec, test_scientific_notation) {
    mc::mutable_dict args;
    args["value"] = 1234.5;

    // 预期输出：科学计数法
    std::string result   = mc::format_dict("科学计数法: ${value:.2e}", args);
    std::string expected = "科学计数法: 1.23e+03";

    EXPECT_EQ(expected, result);
}

// 测试混合格式化
TEST_F(test_format_with_spec, test_mixed_format) {
    mc::mutable_dict args;
    args["name"]  = "产品";
    args["price"] = 99.99;
    args["count"] = 5;
    args["total"] = 499.95;

    std::string result = mc::format_dict(
        "${name}: 单价${price:.2f}元, 数量${count:d}, 总计${total:.2f}元",
        args);
    std::string expected = "产品: 单价99.99元, 数量5, 总计499.95元";

    EXPECT_EQ(expected, result);
}

// 测试无格式规范的情况（向后兼容）
TEST_F(test_format_with_spec, test_no_format_spec) {
    mc::mutable_dict args;
    args["value"] = 42;

    // 预期输出：无格式规范时使用默认格式
    std::string result   = mc::format_dict("值: ${value}", args);
    std::string expected = "值: 42";

    EXPECT_EQ(expected, result);
}

// 测试无效的格式规范（应该保留原始占位符）
TEST_F(test_format_with_spec, test_invalid_format_spec) {
    mc::mutable_dict args;
    args["value"] = 42;

    std::string result = mc::format_dict("值: ${value:invalid}", args);

    // 无效的格式规范，应该保留原样
    EXPECT_EQ("值: 42", result);
}

// TEST(FormatTest, PositionalArguments) {
//     // 基本位置参数测试
//     EXPECT_EQ(mc::format("{0}, {1}!", "Hello", "world"), "Hello, world!");

//     // 参数重用测试
//     EXPECT_EQ(mc::format("{0}, {0}!", "Hello"), "Hello, Hello!");

//     // 参数顺序调整测试
//     EXPECT_EQ(mc::format("{1}, {0}!", "world", "Hello"), "Hello, world!");

//     // 混合位置参数和格式说明符
//     EXPECT_EQ(mc::format("{0}, {1:}!", "Hello", "world"), "Hello, world!");

//     // 位置参数越界测试
//     EXPECT_THROW(mc::format("{2}", "Hello"), mc::format_error);

//     // 无效位置参数测试
//     EXPECT_THROW(mc::format("{-1}", "Hello"), mc::format_error);
// }

// // 测试位置参数与其他格式化特性的组合
// TEST(FormatTest, PositionalArgumentsWithFormatting) {
//     // 位置参数与数字格式化
//     EXPECT_EQ(mc::format("{0}, {1:d}!", "Test", 42), "Test, 42!");

//     // 位置参数与字符串格式化
//     EXPECT_EQ(mc::format("{1}, {0}!", "world", "Hello"), "Hello, world!");

//     // 位置参数与浮点数格式化
//     EXPECT_EQ(mc::format("{0}, {1:.2f}!", "Pi", 3.14159), "Pi, 3.14!");
// }

// TEST(FormatTest, FormatSpecifiers) {
//     // 基本格式化
//     EXPECT_EQ(mc::format("{0:d}", 42), "42");
//     EXPECT_EQ(mc::format("{0:x}", 255), "ff");
//     EXPECT_EQ(mc::format("{0:X}", 255), "FF");

//     // 对齐和填充
//     EXPECT_EQ(mc::format("{0:>5}", 42), "   42");
//     EXPECT_EQ(mc::format("{0:<5}", 42), "42   ");
//     EXPECT_EQ(mc::format("{0:^5}", 42), " 42  ");
//     EXPECT_EQ(mc::format("{0:0>5}", 42), "00042");

//     // 符号控制
//     EXPECT_EQ(mc::format("{0:+}", 42), "+42");
//     EXPECT_EQ(mc::format("{0:+}", -42), "-42");
//     EXPECT_EQ(mc::format("{0: }", 42), " 42");
//     EXPECT_EQ(mc::format("{0: }", -42), "-42");

//     // 替代形式（进制前缀）
//     EXPECT_EQ(mc::format("{0:#x}", 255), "0xff");
//     EXPECT_EQ(mc::format("{0:#X}", 255), "0xFF");

//     // 精度控制
//     EXPECT_EQ(mc::format("{0:.2f}", 3.14159), "3.14");
//     EXPECT_EQ(mc::format("{0:.0f}", 3.14159), "3");

//     // 组合使用
//     EXPECT_EQ(mc::format("{0:+#08X}", 255), "+0x000FF");
//     EXPECT_EQ(mc::format("{0:^10.2f}", 3.14159), "   3.14   ");
// }

// TEST(FormatTest, InvalidFormatSpec) {
//     // 无效的类型说明符
//     EXPECT_THROW(mc::format("{0:z}", 42), mc::format_error);

//     // 无效的对齐方式
//     EXPECT_THROW(mc::format("{0:@5}", 42), mc::format_error);

//     // 无效的符号说明符
//     EXPECT_THROW(mc::format("{0:@}", 42), mc::format_error);

//     // 精度格式错误
//     EXPECT_THROW(mc::format("{0:.}", 3.14), mc::format_error);
//     EXPECT_THROW(mc::format("{0:.a}", 3.14), mc::format_error);
// }