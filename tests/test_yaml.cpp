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
 * @file test_yaml.cpp
 * @brief YAML编解码功能的单元测试
 */
#include <gtest/gtest.h>
#include <mc/yaml.h>
#include <mc/dict.h>
#include <mc/variant.h>
#include <mc/exception.h>
#include <limits>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>

using namespace mc;
using namespace mc::yaml;

namespace mc {
namespace yaml {
namespace test {

// 基本类型编码测试
TEST(YamlEncodeTest, BasicTypes) {
    // null值测试
    EXPECT_EQ(yaml_encode(variant()), "null");
    EXPECT_EQ(yaml_encode(variant(nullptr)), "null");

    // 布尔值测试
    EXPECT_EQ(yaml_encode(variant(true)), "true");
    EXPECT_EQ(yaml_encode(variant(false)), "false");

    // 字符串测试
    EXPECT_EQ(yaml_encode(variant(std::string("hello"))), "hello");
    EXPECT_EQ(yaml_encode(variant(std::string(""))), "\"\"");
    EXPECT_EQ(yaml_encode(variant(std::string("hello\nworld"))), "\"hello\\nworld\"");
    EXPECT_EQ(yaml_encode(variant(std::string("hello\"world"))), "\"hello\\\"world\"");
    
    // 需要引号的字符串测试
    EXPECT_EQ(yaml_encode(variant(std::string("true"))), "\"true\""); // 保留字需要引号
    EXPECT_EQ(yaml_encode(variant(std::string("123"))), "\"123\""); // 数字开头需要引号
    EXPECT_EQ(yaml_encode(variant(std::string("hello:world"))), "\"hello:world\""); // 包含特殊字符需要引号
}

// 数字类型编码测试
TEST(YamlEncodeTest, NumberTypes) {
    // int8_t测试
    EXPECT_EQ(yaml_encode(variant(int8_t{0})), "0");
    EXPECT_EQ(yaml_encode(variant(int8_t{127})), "127");
    EXPECT_EQ(yaml_encode(variant(int8_t{-128})), "-128");

    // uint8_t测试
    EXPECT_EQ(yaml_encode(variant(uint8_t{0})), "0");
    EXPECT_EQ(yaml_encode(variant(uint8_t{255})), "255");

    // int16_t测试
    EXPECT_EQ(yaml_encode(variant(int16_t{-32768})), "-32768");
    EXPECT_EQ(yaml_encode(variant(int16_t{32767})), "32767");

    // uint16_t测试
    EXPECT_EQ(yaml_encode(variant(uint16_t{65535})), "65535");

    // int32_t测试
    EXPECT_EQ(yaml_encode(variant(int32_t{-2147483648})), "-2147483648");
    EXPECT_EQ(yaml_encode(variant(int32_t{2147483647})), "2147483647");

    // uint32_t测试
    EXPECT_EQ(yaml_encode(variant(uint32_t{4294967295})), "4294967295");

    // int64_t测试
    EXPECT_EQ(yaml_encode(variant(int64_t{-9223372036854775807LL})), "-9223372036854775807");
    EXPECT_EQ(yaml_encode(variant(int64_t{9223372036854775807LL})), "9223372036854775807");

    // uint64_t测试
    EXPECT_EQ(yaml_encode(variant(uint64_t{18446744073709551615ULL})), "18446744073709551615");

    // double测试
    EXPECT_EQ(yaml_encode(variant(1.23)), "1.23");
    EXPECT_EQ(yaml_encode(variant(-1.23)), "-1.23");
    EXPECT_EQ(yaml_encode(variant(0.0)), "0");
}

// 编码选项测试
TEST(YamlEncodeOptionsTest, SingleQuote) {
    // 创建测试数据
    variant value(std::string("hello\"world"));

    // 默认选项（根据内容决定引号类型）
    std::string default_quote = yaml_encode(value);
    EXPECT_EQ(default_quote, "\"hello\\\"world\"");

    // 使用单引号
    yaml_encode_options options;
    options.single_quote = true;
    std::string single_quote = yaml_encode(value, options);
    EXPECT_EQ(single_quote, "'hello\"world'");

    // 包含单引号的字符串
    variant value2(std::string("It's a test"));
    std::string escaped_quote = yaml_encode(value2, options);
    EXPECT_EQ(escaped_quote, "'It''s a test'");
}

TEST(YamlEncodeOptionsTest, FloatPrecision) {
    // 创建测试数据
    variant value(3.14159265359);
    
    // 默认精度测试（-1，使用最大精度）
    std::string default_precision = yaml_encode(value);

    // 指定不同精度测试
    yaml_encode_options options;
    
    // 精度为0
    options.float_precision = 0;
    EXPECT_EQ(yaml_encode(value, options), "3");
    
    // 精度为2
    options.float_precision = 2;
    EXPECT_EQ(yaml_encode(value, options), "3.14");
    
    // 精度为6
    options.float_precision = 6;
    EXPECT_EQ(yaml_encode(value, options), "3.141593");
}

} // namespace test
} // namespace yaml
} // namespace mc