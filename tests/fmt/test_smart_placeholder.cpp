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
#include <mc/fmt/format.h>
#include <mc/fmt/format_arg.h>

/* 测试智能识别索引占位符还是命名占位符

之前格式化字符串要求 {} 为索引占位符，${} 为命名占位符，为了使用上更简便，
让 {} 即支持索引也支持占位，旧的 ${} 命名占位仍然可以使用，但推荐后续在
格式化字符串中统一使用 {} 占位符
*/

using namespace mc::fmt;

// 测试智能占位符的基本功能
TEST(smart_placeholder_test, basic_smart_placeholder)
{
    // 测试自增索引 {}
    auto result1 = sformat("{} {}", "Hello", "World");
    EXPECT_EQ(result1, "Hello World");

    // 测试显式索引 {0}, {1}
    auto result2 = sformat("{1} {0}", "World", "Hello");
    EXPECT_EQ(result2, "Hello World");

    // 测试命名参数 {name}
    auto result3 = sformat("{name} {value}", ("name", "Answer"), ("value", 42));
    EXPECT_EQ(result3, "Answer 42");
}

// 测试混合使用不同类型的占位符
TEST(smart_placeholder_test, mixed_placeholder_types)
{
    // 混合自增索引和命名参数
    auto result1 = sformat("{} is {age} years old", "Alice", ("age", 25));
    EXPECT_EQ(result1, "Alice is 25 years old");

    // 混合显式索引和命名参数
    auto result2 = sformat("{0} has {balance:.2f} dollars", "Bob", ("balance", 123.456));
    EXPECT_EQ(result2, "Bob has 123.46 dollars");

    // 混合所有类型
    auto result3 = sformat("{} {name} {2}", "Hello", ("name", "Test"), "World"); // 命名参数也会增加索引
    EXPECT_EQ(result3, "Hello Test World");
}

// 测试带格式说明符的智能占位符
TEST(smart_placeholder_test, format_specifications)
{
    // 自增索引带格式
    auto result1 = sformat("{:.2f} {}", 3.14159, "pi");
    EXPECT_EQ(result1, "3.14 pi");

    // 显式索引带格式
    auto result2 = sformat("{0:.2f} {1:>10}", 3.14159, "right");
    EXPECT_EQ(result2, "3.14      right");

    // 命名参数带格式
    auto result3 = sformat("{name:<10} {value:04d}", ("name", "Number"), ("value", 42));
    EXPECT_EQ(result3, "Number     0042");
}

// 测试动态格式参数
TEST(smart_placeholder_test, dynamic_format_parameters)
{
    // 使用命名参数作为动态宽度和精度
    auto result1 = sformat("{value:{width}.{precision}f}", ("value", 3.14159), ("width", 10), ("precision", 3));
    EXPECT_EQ(result1, "     3.142");

    // 混合索引和命名参数的动态格式
    auto result2 = sformat("{0:{width}.2f}", 3.14159, ("width", 8));
    EXPECT_EQ(result2, "    3.14");
}

// 测试错误处理
TEST(smart_placeholder_test, error_handling)
{
    // 无效的标识符名称
    EXPECT_FALSE(MC_FORMAT_COMPILE_CHECK("{123abc}", ("123abc", "test")));
    EXPECT_FALSE(MC_FORMAT_COMPILE_CHECK("{-name}", ("name", "test")));
    EXPECT_FALSE(MC_FORMAT_COMPILE_CHECK("{name space}", ("name", "test")));

    // 找不到命名参数
    EXPECT_FALSE(MC_FORMAT_COMPILE_CHECK("{missing_arg}", ("present", "value")));

    // 索引超出范围
    EXPECT_FALSE(MC_FORMAT_COMPILE_CHECK("{5}", "only_one"));
}

// 测试空字符串索引
TEST(smart_placeholder_test, SmartPlaceholderEmptyIndexString)
{
    // 测试 is_numeric_string("") 返回 true 的情况
    // 空字符串应该被视为有效的自增索引
    auto result = sformat("{} {}", "first", "second");
    EXPECT_EQ(result, "first second");
}

// 测试边界情况
TEST(smart_placeholder_test, edge_cases)
{
    // 单个字符的命名参数
    auto result1 = sformat("{x} {y}", ("x", 10), ("y", 20));
    EXPECT_EQ(result1, "10 20");

    // 长命名参数
    auto result2 = sformat("{very_long_parameter_name}", ("very_long_parameter_name", "value"));
    EXPECT_EQ(result2, "value");

    // 提供了3个参数但格式化字符串只用了一部分，正常编译期报错，这里使用不安全版本
    auto result3 = sformat_unsafe("{2}", "first", "second", "target_value");
    EXPECT_EQ(result3, "target_value");

    // 格式化字符串不是字面值常量而是变量，只能使用不安全版本
    const char* fmt     = "{x} {y}";
    auto        result4 = sformat_unsafe(fmt, ("x", 10), ("y", 20));
    EXPECT_EQ(result4, "10 20");
}

// 测试嵌套大括号
TEST(smart_placeholder_test, nested_braces)
{
    // 命名参数中的嵌套大括号格式
    auto result1 = sformat("{value:{width}.{precision}f}", ("value", 123.456), ("width", 12), ("precision", 2));
    EXPECT_EQ(result1, "      123.46");

    // 索引参数中的嵌套大括号格式
    auto result2 = sformat("{0:{1}.{2}f}", 123.456, 12, 2);
    EXPECT_EQ(result2, "      123.46");
}

// 测试转义字符
TEST(smart_placeholder_test, escaped_braces)
{
    // 转义的大括号与智能占位符混合
    auto result1 = sformat("{{}} {name} {{}}", ("name", "test"));
    EXPECT_EQ(result1, "{} test {}");

    auto result2 = sformat("{{0}} {0} {{name}}", "value");
    EXPECT_EQ(result2, "{0} value {name}");
}
