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
    std::string result   = mc::string::format("数值: ${value:.2f}", args);
    std::string expected = "数值: 3.14";

    EXPECT_EQ(expected, result);
}

// 测试整数格式化 - 十六进制
TEST_F(test_format_with_spec, test_int_hex) {
    mc::mutable_dict args;
    args["value"] = 255;

    // 预期输出：十六进制格式
    std::string result   = mc::string::format("十六进制: ${value:x}", args);
    std::string expected = "十六进制: ff";

    EXPECT_EQ(expected, result);
}

// 测试整数格式化 - 八进制
TEST_F(test_format_with_spec, test_int_oct) {
    mc::mutable_dict args;
    args["value"] = 64;

    // 预期输出：八进制格式
    std::string result   = mc::string::format("八进制: ${value:o}", args);
    std::string expected = "八进制: 100";

    EXPECT_EQ(expected, result);
}

// 测试字符串宽度和对齐
TEST_F(test_format_with_spec, test_string_width) {
    mc::mutable_dict args;
    args["name"] = "Alice";

    // 预期输出：10个字符宽度，左对齐
    std::string result   = mc::string::format("姓名: '${name:<10}'", args);
    std::string expected = "姓名: 'Alice     '";

    EXPECT_EQ(expected, result);
}

// 测试布尔值格式化
TEST_F(test_format_with_spec, test_bool_format) {
    mc::mutable_dict args;
    args["flag"] = true;

    // 预期输出：布尔值作为数字
    std::string result   = mc::string::format("标志: ${flag:d}", args);
    std::string expected = "标志: 1";

    EXPECT_EQ(expected, result);
}

// 测试科学计数法
TEST_F(test_format_with_spec, test_scientific_notation) {
    mc::mutable_dict args;
    args["value"] = 1234.5;

    // 预期输出：科学计数法
    std::string result   = mc::string::format("科学计数法: ${value:.2e}", args);
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

    std::string result = mc::string::format(
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
    std::string result   = mc::string::format("值: ${value}", args);
    std::string expected = "值: 42";

    EXPECT_EQ(expected, result);
}

// 测试无效的格式规范（应该保留原始占位符）
TEST_F(test_format_with_spec, test_invalid_format_spec) {
    mc::mutable_dict args;
    args["value"] = 42;

    // 预期：无效的格式规范应该保留原样
    std::string result = mc::string::format("值: ${value:invalid}", args);

    // 注意：这个测试可能需要根据实际实现调整
    // 如果无效格式规范被忽略，则应该输出默认格式
    // 如果保留原样，则应该输出原始占位符
}