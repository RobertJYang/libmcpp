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

using namespace mc::fmt;

class named_args_test : public testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

// 测试基本的命名参数功能
TEST(named_args, basic_named_args) {
    using namespace mc::fmt;

    // 测试 ${name} 语法
    auto result1 = sformat("Hello ${name}!", ("name", "World"));
    EXPECT_EQ(result1, "Hello World!");

    // 测试 ${name:sformat} 语法
    auto result2 = sformat("Value: ${num:.2f}", ("num", 3.14159));
    EXPECT_EQ(result2, "Value: 3.14");

    // 测试多个命名参数
    auto result3 = sformat("${first} ${second}", ("first", "Hello"), ("second", "World"));
    EXPECT_EQ(result3, "Hello World");
}

// 测试错误处理
TEST(named_args, error_handling) {
    using namespace mc::fmt;

    // 命名参数名称不能为空
    EXPECT_FALSE(MC_FORMAT_COMPILE_CHECK("${}", ("test", "value")));
    // EXPECT_THROW(sformat("${}", ("test", "value")), mc::invalid_arg_exception);

    // 找不到命名参数
    EXPECT_FALSE(MC_FORMAT_COMPILE_CHECK("${missing}", ("present", "value")));
    // EXPECT_THROW(sformat("${missing}", ("present", "value")), mc::invalid_arg_exception);

    // 位置参数的错误处理
    EXPECT_FALSE(MC_FORMAT_COMPILE_CHECK("{1}", "first"));
    // EXPECT_THROW(sformat("{1}", "first"), mc::invalid_arg_exception);
}

// 测试嵌套{}处理
TEST(named_args, nested_braces) {
    using namespace mc::fmt;

    // 测试命名参数中的动态格式说明符
    auto result1 = sformat("${value:{width}.{precision}f}",
                           ("value", 3.14159),
                           ("width", 8),
                           ("precision", 3));
    EXPECT_EQ(result1, "   3.142");

    // 测试位置参数中的动态格式说明符
    auto result2 = sformat("{0:{1}.{2}f}", (3.14159), (8), 3);
    EXPECT_EQ(result2, "   3.142");

    // 测试分离使用
    auto result3 = sformat("${name}: ${value:6.2f}", ("name", "Pi"), ("value", 3.14159));
    EXPECT_EQ(result3, "Pi:   3.14");
}

// 测试带格式规范的命名参数
TEST_F(named_args_test, test_named_args_with_format_spec) {
    auto result = sformat("${name} has ${balance:.2f} dollars",
                          ("name", "David"),
                          ("balance", 123.456));
    EXPECT_EQ(result, "David has 123.46 dollars");
}

// 测试多种数据类型
TEST_F(named_args_test, test_various_types) {
    auto result = sformat("${name}: int=${int_val}, float=${float_val:.1f}, bool=${bool_val}",
                          ("name", "Types"),
                          ("int_val", 42),
                          ("float_val", 3.14159),
                          ("bool_val", true));
    EXPECT_EQ(result, "Types: int=42, float=3.1, bool=true");
}

// 测试字符串类型
TEST_F(named_args_test, test_string_types) {
    std::string std_str = "std::string";
    const char* c_str   = "C string";

    auto result = sformat("${std_str} and ${c_str}",
                          ("std_str", std_str),
                          ("c_str", c_str));
    EXPECT_EQ(result, "std::string and C string");
}

// // 测试指针类型
// TEST_F(named_args_test, test_pointer_type) {
//     int  value  = 42;
//     auto result = sformat("Value at ${ptr}", ("ptr", &value));

//     // 指针地址会变化，所以只检查格式是否正确
//     EXPECT_TRUE(result.find("Value at ") == 0);
//     EXPECT_TRUE(result.find("0x") != std::string::npos || result.find("0X") != std::string::npos);
// }

// 测试字符类型
TEST_F(named_args_test, test_char_type) {
    auto result = sformat("Character: ${ch}", ("ch", 'A'));
    EXPECT_EQ(result, "Character: A");
}

// 测试转义的大括号
TEST_F(named_args_test, test_escaped_braces) {
    auto result = sformat("{{name}} is ${name}", ("name", "literal"));
    EXPECT_EQ(result, "{name} is literal");
}

TEST_F(named_args_test, test_nocopyable) {
    using namespace mc::fmt;

    // 测试不可拷贝类型用作命名参数
    auto ptr1 = std::make_unique<int>(42);
    auto ptr2 = std::make_unique<int>(24);

    // 分离使用：纯位置参数
    auto result1 = sformat("{}", ptr1);
    EXPECT_EQ(result1, "ptr(42)");

    // 分离使用：纯命名参数
    auto result2 = sformat("${value}", ("value", ptr2));
    EXPECT_EQ(result2, "ptr(24)");

    // 验证原始指针没有被移动
    EXPECT_NE(ptr1, nullptr);
    EXPECT_EQ(*ptr1, 42);
    EXPECT_NE(ptr2, nullptr);
    EXPECT_EQ(*ptr2, 24);
}

// 测试命名和非命名参数混合格式
TEST_F(named_args_test, test_mixed_format) {
    auto result = sformat("${name} has ${balance:.2f} dollars, " // 命名参数
                          "{2} has {3:.2f} dollars, "            // 显示位置参数
                          "{} has {:.2f} dollars, "              // 隐式位置参数
                          "{} has ${balance3:.2f} dollars",      // 隐式位置+命名参数

                          ("name", "David")("balance", 12.12), // 链式命名参数
                          ("John")(34.34),                     // 链式位置参数
                          "Alice", 45.45,                      // 普通参数
                          ("Bob")("balance3", 56.56));         // 链式位置+命名参数

    EXPECT_EQ(result, "David has 12.12 dollars, "
                      "John has 34.34 dollars, "
                      "Alice has 45.45 dollars, "
                      "Bob has 56.56 dollars");
}
