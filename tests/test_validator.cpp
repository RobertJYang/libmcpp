/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
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
 * @file test_validator.cpp
 * @brief mc::validate::validator 决策表测试用例
 */
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <mc/validate/validator.h>

using namespace mc::validate;

namespace mc {
namespace validate {
namespace test {

/**
 * @brief format_name_and_value 决策表测试
 *
 * 决策表：
 * | need_convert | 结果 name | 结果 val |
 * |-------------|-----------|----------|
 * | false       | name      | val_str  |
 * | true        | %name     | %name:val_str |
 */
TEST(ValidatorTest, FormatNameAndValue) {
    // need_convert = false
    {
        auto [name, val] = validator::format_name_and_value("Prop1", "100", false);
        EXPECT_EQ(name, "Prop1");
        EXPECT_EQ(val, "100");
    }

    // need_convert = true
    {
        auto [name, val] = validator::format_name_and_value("Prop1", "100", true);
        EXPECT_EQ(name, "%Prop1");
        EXPECT_EQ(val, "%Prop1:100");
    }

    // 空字符串测试
    {
        auto [name, val] = validator::format_name_and_value("", "", false);
        EXPECT_EQ(name, "");
        EXPECT_EQ(val, "");
    }

    // need_convert = true, 空字符串
    {
        auto [name, val] = validator::format_name_and_value("Prop", "", true);
        EXPECT_EQ(name, "%Prop");
        EXPECT_EQ(val, "%Prop:");
    }
}

/**
 * @brief check_integer 决策表测试
 *
 * 决策表：
 * | val 类型 | val 是否整数 | val 在范围内 | need_convert | 结果 |
 * |---------|------------|------------|-------------|------|
 * | 数字    | 是          | 是          | false       | 通过 |
 * | 数字    | 是          | 是          | true        | 通过 |
 * | 数字    | 是          | 否          | false       | property_value_out_of_range |
 * | 数字    | 是          | 否          | true        | property_value_out_of_range |
 * | 数字    | 否          | -           | false       | property_value_type_error |
 * | 数字    | 否          | -           | true        | property_value_type_error |
 * | NaN     | -           | -           | false       | property_value_type_error |
 * | NaN     | -           | -           | true        | property_value_type_error |
 */
TEST(ValidatorTest, CheckInteger) {
    // 情况1: 整数，范围内，need_convert = false
    EXPECT_NO_THROW(validator::check_integer("Prop1", 50.0, 0.0, 100.0, false));

    // 情况2: 整数，范围内，need_convert = true
    EXPECT_NO_THROW(validator::check_integer("Prop1", 50.0, 0.0, 100.0, true));

    // 情况3: 整数，边界值 min，need_convert = false
    EXPECT_NO_THROW(validator::check_integer("Prop1", 0.0, 0.0, 100.0, false));

    // 情况4: 整数，边界值 max，need_convert = false
    EXPECT_NO_THROW(validator::check_integer("Prop1", 100.0, 0.0, 100.0, false));

    // 情况5: 整数，小于 min，need_convert = false
    EXPECT_THROW(validator::check_integer("Prop1", -1.0, 0.0, 100.0, false),
                 property_value_out_of_range);

    // 情况6: 整数，大于 max，need_convert = false
    EXPECT_THROW(validator::check_integer("Prop1", 101.0, 0.0, 100.0, false),
                 property_value_out_of_range);

    // 情况7: 整数，小于 min，need_convert = true
    EXPECT_THROW(validator::check_integer("Prop1", -1.0, 0.0, 100.0, true),
                 property_value_out_of_range);

    // 情况8: 浮点数（非整数），need_convert = false
    EXPECT_THROW(validator::check_integer("Prop1", 3.14, 0.0, 100.0, false),
                 property_value_type_error);

    // 情况9: 浮点数（非整数），need_convert = true
    EXPECT_THROW(validator::check_integer("Prop1", 3.14, 0.0, 100.0, true),
                 property_value_type_error);

    // 情况10: NaN，need_convert = false
    EXPECT_THROW(validator::check_integer("Prop1", std::nan(""), 0.0, 100.0, false),
                 property_value_type_error);

    // 情况11: NaN，need_convert = true
    EXPECT_THROW(validator::check_integer("Prop1", std::nan(""), 0.0, 100.0, true),
                 property_value_type_error);

    // 情况12: 整数 0.0（边界情况）
    EXPECT_NO_THROW(validator::check_integer("Prop1", 0.0, 0.0, 100.0, false));

    // 情况13: 大整数
    EXPECT_NO_THROW(validator::check_integer("Prop1", 1000000.0, 0.0, 2000000.0, false));
}

/**
 * @brief ranges 决策表测试
 *
 * 决策表：
 * | val 在范围内 | allow_nil | need_convert | 结果 |
 * |------------|----------|-------------|------|
 * | 是          | false    | false       | 通过 |
 * | 是          | false    | true        | 通过 |
 * | 是          | true     | false       | 通过 |
 * | 是          | true     | true        | 通过 |
 * | 否          | false    | false       | property_value_out_of_range |
 * | 否          | false    | true        | property_value_out_of_range |
 * | 否          | true     | false       | property_value_out_of_range |
 * | 否          | true     | true        | property_value_out_of_range |
 */
TEST(ValidatorTest, Ranges) {
    // 情况1: 范围内，allow_nil = false，need_convert = false
    EXPECT_NO_THROW(validator::ranges("Prop1", 50.0, 0.0, 100.0, false, false));

    // 情况2: 范围内，allow_nil = false，need_convert = true
    EXPECT_NO_THROW(validator::ranges("Prop1", 50.0, 0.0, 100.0, true, false));

    // 情况3: 范围内，allow_nil = true，need_convert = false
    EXPECT_NO_THROW(validator::ranges("Prop1", 50.0, 0.0, 100.0, false, true));

    // 情况4: 范围内，allow_nil = true，need_convert = true
    EXPECT_NO_THROW(validator::ranges("Prop1", 50.0, 0.0, 100.0, true, true));

    // 情况5: 边界值 min
    EXPECT_NO_THROW(validator::ranges("Prop1", 0.0, 0.0, 100.0, false, false));

    // 情况6: 边界值 max
    EXPECT_NO_THROW(validator::ranges("Prop1", 100.0, 0.0, 100.0, false, false));

    // 情况7: 小于 min，allow_nil = false，need_convert = false
    EXPECT_THROW(validator::ranges("Prop1", -1.0, 0.0, 100.0, false, false),
                 property_value_out_of_range);

    // 情况8: 大于 max，allow_nil = false，need_convert = false
    EXPECT_THROW(validator::ranges("Prop1", 101.0, 0.0, 100.0, false, false),
                 property_value_out_of_range);

    // 情况9: 小于 min，allow_nil = false，need_convert = true
    EXPECT_THROW(validator::ranges("Prop1", -1.0, 0.0, 100.0, true, false),
                 property_value_out_of_range);

    // 情况10: 大于 max，allow_nil = true，need_convert = false
    EXPECT_THROW(validator::ranges("Prop1", 101.0, 0.0, 100.0, false, true),
                 property_value_out_of_range);

    // 情况11: 浮点数（允许）
    EXPECT_NO_THROW(validator::ranges("Prop1", 3.14, 0.0, 10.0, false, false));

    // 情况12: 负数范围
    EXPECT_NO_THROW(validator::ranges("Prop1", -50.0, -100.0, 0.0, false, false));
}

/**
 * @brief lens 决策表测试
 *
 * 决策表：
 * | 长度关系 | allow_nil | need_convert | 结果 |
 * |---------|----------|-------------|------|
 * | 正常     | false    | false       | 通过 |
 * | 正常     | false    | true        | 通过 |
 * | 正常     | true     | false       | 通过 |
 * | 正常     | true     | true        | 通过 |
 * | 过短     | false    | false       | string_length_error |
 * | 过短     | false    | true        | string_length_error |
 * | 过短     | true     | false       | string_length_error |
 * | 过短     | true     | true        | string_length_error |
 * | 过长     | false    | false       | string_length_error |
 * | 过长     | false    | true        | string_length_error |
 * | 过长     | true     | false       | string_length_error |
 * | 过长     | true     | true        | string_length_error |
 */
TEST(ValidatorTest, Lens) {
    // 情况1: 长度正常，allow_nil = false，need_convert = false
    EXPECT_NO_THROW(validator::lens("Prop1", "hello", 1, 10, false, false));

    // 情况2: 长度正常，allow_nil = false，need_convert = true
    EXPECT_NO_THROW(validator::lens("Prop1", "hello", 1, 10, true, false));

    // 情况3: 长度正常，allow_nil = true，need_convert = false
    EXPECT_NO_THROW(validator::lens("Prop1", "hello", 1, 10, false, true));

    // 情况4: 长度正常，allow_nil = true，need_convert = true
    EXPECT_NO_THROW(validator::lens("Prop1", "hello", 1, 10, true, true));

    // 情况5: 边界值 min
    EXPECT_NO_THROW(validator::lens("Prop1", "a", 1, 10, false, false));

    // 情况6: 边界值 max
    EXPECT_NO_THROW(validator::lens("Prop1", "1234567890", 1, 10, false, false));

    // 情况7: 长度过短，allow_nil = false，need_convert = false
    EXPECT_THROW(validator::lens("Prop1", "", 1, 10, false, false), string_length_error);

    // 情况8: 长度过长，allow_nil = false，need_convert = false
    EXPECT_THROW(validator::lens("Prop1", "12345678901", 1, 10, false, false),
                 string_length_error);

    // 情况9: 长度过短，allow_nil = false，need_convert = true
    EXPECT_THROW(validator::lens("Prop1", "", 1, 10, true, false), string_length_error);

    // 情况10: 长度过长，allow_nil = true，need_convert = false
    EXPECT_THROW(validator::lens("Prop1", "12345678901", 1, 10, false, true),
                 string_length_error);

    // 情况11: 空字符串，min = 0
    EXPECT_NO_THROW(validator::lens("Prop1", "", 0, 10, false, false));

    // 情况12: 中文字符（多字节）
    EXPECT_NO_THROW(validator::lens("Prop1", "测试", 1, 10, false, false));
}

/**
 * @brief regex 决策表测试
 *
 * 决策表：
 * | 正则编译 | 匹配结果 | allow_nil | need_convert | 结果 |
 * |---------|---------|----------|-------------|------|
 * | 成功     | 匹配     | false    | false       | 通过 |
 * | 成功     | 匹配     | false    | true        | 通过 |
 * | 成功     | 匹配     | true     | false       | 通过 |
 * | 成功     | 匹配     | true     | true        | 通过 |
 * | 成功     | 不匹配   | false    | false       | format_error |
 * | 成功     | 不匹配   | false    | true        | format_error |
 * | 成功     | 不匹配   | true     | false       | format_error |
 * | 成功     | 不匹配   | true     | true        | format_error |
 * | 失败     | -        | false    | false       | format_error |
 * | 失败     | -        | false    | true        | format_error |
 * | 失败     | -        | true     | false       | format_error |
 * | 失败     | -        | true     | true        | format_error |
 */
TEST(ValidatorTest, Regex) {
    // 情况1: 匹配，allow_nil = false，need_convert = false
    EXPECT_NO_THROW(validator::regex("Prop1", "hello123", "[a-z0-9]+", false, false));

    // 情况2: 匹配，allow_nil = false，need_convert = true
    EXPECT_NO_THROW(validator::regex("Prop1", "hello123", "[a-z0-9]+", true, false));

    // 情况3: 匹配，allow_nil = true，need_convert = false
    EXPECT_NO_THROW(validator::regex("Prop1", "hello123", "[a-z0-9]+", false, true));

    // 情况4: 匹配，allow_nil = true，need_convert = true
    EXPECT_NO_THROW(validator::regex("Prop1", "hello123", "[a-z0-9]+", true, true));

    // 情况5: 不匹配，allow_nil = false，need_convert = false
    EXPECT_THROW(validator::regex("Prop1", "HELLO", "[a-z]+", false, false), format_error);

    // 情况6: 不匹配，allow_nil = false，need_convert = true
    EXPECT_THROW(validator::regex("Prop1", "HELLO", "[a-z]+", true, false), format_error);

    // 情况7: 不匹配，allow_nil = true，need_convert = false
    EXPECT_THROW(validator::regex("Prop1", "HELLO", "[a-z]+", false, true), format_error);

    // 情况8: 不匹配，allow_nil = true，need_convert = true
    EXPECT_THROW(validator::regex("Prop1", "HELLO", "[a-z]+", true, true), format_error);

    // 情况9: 空字符串匹配
    EXPECT_NO_THROW(validator::regex("Prop1", "", ".*", false, false));

    // 情况10: 邮箱格式匹配
    EXPECT_NO_THROW(validator::regex("Prop1", "test@example.com",
                                     "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$",
                                     false, false));

    // 情况11: 邮箱格式不匹配
    EXPECT_THROW(validator::regex("Prop1", "invalid-email",
                                  "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$", false,
                                  false),
                 format_error);

    // 情况12: 数字匹配
    EXPECT_NO_THROW(validator::regex("Prop1", "12345", "^[0-9]+$", false, false));

    // 情况13: 数字不匹配（包含字母）
    EXPECT_THROW(validator::regex("Prop1", "123a", "^[0-9]+$", false, false), format_error);
}

/**
 * @brief json 决策表测试
 *
 * 决策表：
 * | JSON 有效性 | 结果 |
 * |-----------|------|
 * | 有效       | 通过 |
 * | 无效       | json_error |
 */
TEST(ValidatorTest, Json) {
    // 情况1: 有效 JSON - 对象
    EXPECT_NO_THROW(validator::json(R"({"key": "value"})"));

    // 情况2: 有效 JSON - 数组
    EXPECT_NO_THROW(validator::json(R"([1, 2, 3])"));

    // 情况3: 有效 JSON - 字符串
    EXPECT_NO_THROW(validator::json(R"("hello")"));

    // 情况4: 有效 JSON - 数字
    EXPECT_NO_THROW(validator::json("123"));

    // 情况5: 有效 JSON - 布尔值
    EXPECT_NO_THROW(validator::json("true"));
    EXPECT_NO_THROW(validator::json("false"));

    // 情况6: 有效 JSON - null
    EXPECT_NO_THROW(validator::json("null"));

    // 情况7: 有效 JSON - 嵌套对象
    EXPECT_NO_THROW(validator::json(R"({"a": {"b": {"c": 123}}})"));

    // 情况8: 有效 JSON - 空对象
    EXPECT_NO_THROW(validator::json("{}"));

    // 情况9: 有效 JSON - 空数组
    EXPECT_NO_THROW(validator::json("[]"));

    // 情况10: 无效 JSON - 缺少引号
    EXPECT_THROW(validator::json("{key: value}"), json_error);

    // 情况11: 无效 JSON - 缺少逗号
    EXPECT_THROW(validator::json(R"({"a": 1 "b": 2})"), json_error);

    // 情况12: 无效 JSON - 多余的逗号
    EXPECT_THROW(validator::json(R"({"a": 1,})"), json_error);

    // 情况13: 无效 JSON - 未闭合的括号
    EXPECT_THROW(validator::json(R"({"a": 1)"), json_error);

    // 情况14: 无效 JSON - 空字符串
    EXPECT_THROW(validator::json(""), json_error);

    // 情况15: 无效 JSON - 普通文本
    EXPECT_THROW(validator::json("hello world"), json_error);

    // 情况16: 无效 JSON - 不完整的字符串
    EXPECT_THROW(validator::json(R"("unclosed)"), json_error);
}

/**
 * @brief 异常消息格式测试
 */
TEST(ValidatorTest, ExceptionMessageFormat) {
    // 测试 property_value_type_error 消息格式
    try {
        validator::check_integer("Prop1", 3.14, 0.0, 100.0, false);
        FAIL() << "应该抛出异常";
    } catch (const property_value_type_error& e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("PropertyValueTypeError"), std::string::npos);
        EXPECT_NE(msg.find("Prop1"), std::string::npos);
    }

    // 测试 property_value_out_of_range 消息格式
    try {
        validator::check_integer("Prop1", 150.0, 0.0, 100.0, false);
        FAIL() << "应该抛出异常";
    } catch (const property_value_out_of_range& e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("PropertyValueOutOfRange"), std::string::npos);
        EXPECT_NE(msg.find("Prop1"), std::string::npos);
    }

    // 测试 need_convert = true 时的消息格式
    try {
        validator::check_integer("Prop1", 150.0, 0.0, 100.0, true);
        FAIL() << "应该抛出异常";
    } catch (const property_value_out_of_range& e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("%Prop1"), std::string::npos);
    }

    // 测试 string_length_error 消息格式
    try {
        validator::lens("Prop1", "", 1, 10, false, false);
        FAIL() << "应该抛出异常";
    } catch (const string_length_error& e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("StringValueTooShort"), std::string::npos);
    }

    // 测试 format_error 消息格式
    try {
        validator::regex("Prop1", "HELLO", "[a-z]+", false, false);
        FAIL() << "应该抛出异常";
    } catch (const format_error& e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("PropertyValueFormatError"), std::string::npos);
    }

    // 测试 json_error 消息格式
    try {
        validator::json("invalid json");
        FAIL() << "应该抛出异常";
    } catch (const json_error& e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("MalformedJSON"), std::string::npos);
    }
}

} // namespace test
} // namespace validate
} // namespace mc
