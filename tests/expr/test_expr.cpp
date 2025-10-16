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
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/expr/builtin.h>
#include <mc/expr/engine.h>

namespace {
class expr_test : public ::testing::Test {
protected:
    expr_test() {
    }

    void SetUp() override {
    }

    void TearDown() override {
    }

    mc::expr::engine engine;
};
} // namespace

TEST_F(expr_test, BasicArithmetic) {
    auto& ctx = engine.get_global_context();

    // 整数算术运算
    EXPECT_EQ(engine.evaluate("1 + 2", ctx), 3);
    EXPECT_EQ(engine.evaluate("5 - 3", ctx), 2);
    EXPECT_EQ(engine.evaluate("2 * 3", ctx), 6);
    EXPECT_EQ(engine.evaluate("10 / 2", ctx), 5.0);
    EXPECT_EQ(engine.evaluate("10 % 3", ctx), 1);

    // 浮点数算术运算
    EXPECT_EQ(engine.evaluate("1.5 + 2.5", ctx), 4.0);
    EXPECT_EQ(engine.evaluate("5.5 - 3.2", ctx), 2.3);
    EXPECT_EQ(engine.evaluate("2.5 * 3.0", ctx), 7.5);
    EXPECT_EQ(engine.evaluate("10.0 / 4.0", ctx), 2.5);

    // 复合算术运算
    EXPECT_EQ(engine.evaluate("1 + 2 * 3", ctx), 7);
    EXPECT_EQ(engine.evaluate("(1 + 2) * 3", ctx), 9);
    EXPECT_EQ(engine.evaluate("1 + 2 * 3.5", ctx), 8.0);
}

TEST_F(expr_test, Comparison) {
    mc::expr::context ctx;

    // 比较运算
    EXPECT_TRUE(engine.evaluate("1 < 2", ctx));
    EXPECT_TRUE(engine.evaluate("2 <= 2", ctx));
    EXPECT_FALSE(engine.evaluate("3 < 2", ctx));
    EXPECT_TRUE(engine.evaluate("3 > 2", ctx));
    EXPECT_TRUE(engine.evaluate("3 >= 3", ctx));
    EXPECT_FALSE(engine.evaluate("2 > 3", ctx));
    EXPECT_TRUE(engine.evaluate("2 == 2", ctx));
    EXPECT_FALSE(engine.evaluate("2 == 3", ctx));
    EXPECT_TRUE(engine.evaluate("2 != 3", ctx));
    EXPECT_FALSE(engine.evaluate("2 != 2", ctx));

    // 复合比较
    EXPECT_TRUE(engine.evaluate("1 < 2 && 3 > 2", ctx));
    EXPECT_FALSE(engine.evaluate("1 > 2 && 3 > 2", ctx));
    EXPECT_TRUE(engine.evaluate("1 > 2 || 3 > 2", ctx));
    EXPECT_FALSE(engine.evaluate("1 > 2 || 3 < 2", ctx));
}

TEST_F(expr_test, StringOperations) {
    auto& ctx = engine.get_global_context();

    // 字符串连接
    EXPECT_EQ(engine.evaluate("'Hello' + ' ' + 'World'", ctx), "Hello World");
    EXPECT_EQ(engine.evaluate("'Value: ' + 42", ctx), "Value: 42");
    EXPECT_EQ(engine.evaluate("'Pi: ' + 3.14", ctx), "Pi: 3.14");

    // 字符串比较
    EXPECT_TRUE(engine.evaluate("'abc' == 'abc'", ctx));
    EXPECT_FALSE(engine.evaluate("'abc' == 'def'", ctx));
    EXPECT_TRUE(engine.evaluate("'abc' != 'def'", ctx));
}

TEST_F(expr_test, Variables) {
    auto& ctx = engine.get_global_context();

    // 设置变量
    ctx.register_variable("x", 10);
    ctx.register_variable("y", 20);
    ctx.register_variable("name", "Alice");

    // 使用变量
    EXPECT_EQ(engine.evaluate("x + y", ctx), 30);
    EXPECT_EQ(engine.evaluate("x * y", ctx), 200);
    EXPECT_EQ(engine.evaluate("'Hello, ' + name", ctx), "Hello, Alice");

    // 从dict导入变量
    mc::dict data;
    data["z"]  = 30;
    data["pi"] = 3.14159;

    ctx.import_from_dict(data);
    EXPECT_EQ(engine.evaluate("x + y + z", ctx), 60);
    EXPECT_EQ(engine.evaluate("pi", ctx), 3.14159);
}

TEST_F(expr_test, Functions) {
    auto ctx = engine.make_context();

    // 测试内置函数
    EXPECT_EQ(engine.evaluate("abs(-10)", ctx), 10);
    EXPECT_EQ(engine.evaluate("abs(-3.14)", ctx), 3.14);

    EXPECT_EQ(engine.evaluate("min(3, 1, 4, 1, 5)", ctx), 1);
    EXPECT_EQ(engine.evaluate("max(3, 1, 4, 1, 5)", ctx), 5);

    EXPECT_EQ(engine.evaluate("length('hello')", ctx), 5);
    EXPECT_EQ(engine.evaluate("concat('a', 'b', 'c')", ctx), "abc");

    // 测试类型转换函数
    EXPECT_EQ(engine.evaluate("to_integer('42')", ctx), 42);
    EXPECT_EQ(engine.evaluate("to_double('42.1')", ctx), 42.1);
    EXPECT_EQ(engine.evaluate("to_string(42)", ctx), "42");
    EXPECT_TRUE(engine.evaluate("to_bool(1)", ctx));
    EXPECT_FALSE(engine.evaluate("to_bool(0)", ctx));
    EXPECT_TRUE(engine.evaluate("to_bool('true')", ctx));
    EXPECT_FALSE(engine.evaluate("to_bool('')", ctx));

    auto square_func = mc::expr::make_simple_function("square", [](double value) {
        return value * value;
    });

    ctx.register_function(square_func);
    EXPECT_EQ(engine.evaluate("square(3)", ctx), 9.0);
    EXPECT_EQ(engine.evaluate("square(4 + 1)", ctx), 25.0);

    // 测试多参数函数
    auto sum_func = mc::expr::make_simple_function("sum", [](double a, double b, double c) {
        return a + b + c;
    });

    ctx.register_function(sum_func);
    EXPECT_EQ(engine.evaluate("sum(1, 2, 3)", ctx), 6.0);

    // 测试返回字符串的函数
    auto greet_func = mc::expr::make_simple_function("greet", [](const std::string& name) {
        return "Hello, " + name + "!";
    });

    ctx.register_function(greet_func);
    EXPECT_EQ(engine.evaluate("greet('World')", ctx), "Hello, World!");

    // 测试参数不足的情况
    EXPECT_THROW(engine.evaluate("sum(1, 2)", ctx), mc::invalid_arg_exception);

    // 测试类型不匹配的情况
    EXPECT_THROW(engine.evaluate("square('not a number')", ctx), mc::invalid_arg_exception);
}

TEST_F(expr_test, Errors) {
    auto ctx = engine.make_context();

    // 语法错误
    EXPECT_THROW(engine.evaluate("1 + ", ctx), mc::parse_error_exception);
    EXPECT_THROW(engine.evaluate("(1 + 2", ctx), mc::parse_error_exception);

    // 类型错误
    EXPECT_THROW(engine.evaluate("'abc' < 123", ctx), mc::invalid_op_exception);

    // 除零错误
    EXPECT_THROW(engine.evaluate("1 / 0", ctx), mc::divide_by_zero_exception);
    EXPECT_THROW(engine.evaluate("1 % 0", ctx), mc::divide_by_zero_exception);

    // 未定义变量
    EXPECT_THROW(engine.evaluate("undefined_var", ctx), mc::invalid_arg_exception);

    // 未定义函数
    EXPECT_THROW(engine.evaluate("undefined_func()", ctx), mc::invalid_arg_exception);

    // 函数参数不匹配
    EXPECT_THROW(engine.evaluate("abs()", ctx), mc::invalid_arg_exception);

    // 实参个数比形参个数多，允许
    EXPECT_NO_THROW(engine.evaluate("abs(1, 2)", ctx));
}

TEST_F(expr_test, BitOperations) {
    auto& ctx = engine.get_global_context();

    // 测试位与操作(&)
    auto result_and = engine.evaluate("0x0F & 0x33", ctx);
    EXPECT_TRUE(result_and.is_int64());
    EXPECT_EQ(result_and, 0x03);

    // 测试位或操作(|)
    auto result_or = engine.evaluate("0x0F00 | 0x00FF", ctx);
    EXPECT_TRUE(result_or.is_int64());
    EXPECT_EQ(result_or, 0x0FFF);

    // 测试位异或操作(^)
    auto result_xor = engine.evaluate("0xF0F0 ^ 0xFFFF", ctx);
    EXPECT_TRUE(result_xor.is_int64());
    EXPECT_EQ(result_xor, 0x0F0F);

    // 测试左移操作(<<)
    auto result_lshift = engine.evaluate("1 << 3", ctx);
    EXPECT_TRUE(result_lshift.is_int64());
    EXPECT_EQ(result_lshift, 8);

    // 测试右移操作(>>)
    auto result_rshift = engine.evaluate("64 >> 2", ctx);
    EXPECT_TRUE(result_rshift.is_int64());
    EXPECT_EQ(result_rshift, 16);

    // 测试位非操作(~)
    auto result_not = engine.evaluate("~0xF0", ctx);
    EXPECT_TRUE(result_not.is_int64());
    EXPECT_EQ(result_not & 0xFF, 0x0F);

    // 测试复合位操作
    auto result_complex = engine.evaluate("(0x0F & 0x33) | (0x0C <<4)", ctx);
    EXPECT_TRUE(result_complex.is_int64());
    EXPECT_EQ(result_complex, 0xC3);

    // 测试不同类型之间的操作 - 应该保留左操作数的类型
    auto result_mixed = engine.evaluate("0x0F & 3", ctx);
    EXPECT_TRUE(result_mixed.is_int64());
    EXPECT_EQ(result_mixed, 3); // 0x03

    // 允许浮点数参与位操作，但结果会转换为整数
    auto result_float = engine.evaluate("3.14 & 2", ctx);
    EXPECT_TRUE(result_float.is_int64());
    EXPECT_EQ(result_float, 2);

    // 测试位操作与非数值类型 - 应该抛出异常
    EXPECT_THROW(engine.evaluate("'abc' | 1", ctx), mc::invalid_op_exception);
}

TEST_F(expr_test, TemplateStrings) {
    auto& ctx = engine.get_global_context();

    // 设置变量
    ctx.register_variable("name", "Alice");
    ctx.register_variable("age", 30);
    ctx.register_variable("pi", 3.14159);

    // 基本模板字符串
    EXPECT_EQ(engine.evaluate("\"Hello, ${name}!\"", ctx), "Hello, Alice!");

    // 包含数值的模板字符串
    EXPECT_EQ(engine.evaluate("\"Age: ${age}\"", ctx), "Age: 30");
    EXPECT_EQ(engine.evaluate("\"Pi: ${pi}\"", ctx), "Pi: 3.14159");

    // 多个表达式的模板字符串
    EXPECT_EQ(engine.evaluate("\"${name} is ${age} years old.\"", ctx), "Alice is 30 years old.");

    // 表达式内的计算
    EXPECT_EQ(engine.evaluate("\"Sum: ${1 + 2 + 3}\"", ctx), "Sum: 6");
    EXPECT_EQ(engine.evaluate("\"Pi squared: ${pi * pi}\"", ctx), "Pi squared: 9.869588");
    EXPECT_EQ(engine.evaluate("\"Next year: ${age + 1}\"", ctx), "Next year: 31");

    // 使用条件表达式
    EXPECT_EQ(engine.evaluate("\"Status: ${age >= 18 ? 'adult' : 'minor'}\"", ctx),
              "Status: adult");

    // 嵌套表达式（字符串拼接）
    EXPECT_EQ(engine.evaluate("\"${name + ' says: \"Hi ' + name + '!\"'}\"", ctx),
              "Alice says: \"Hi Alice!\"");

    // 复杂表达式组合
    EXPECT_EQ(engine.evaluate("\"${1 + 2} plus ${3 + 4} equals ${(1 + 2) + (3 + 4)}\"", ctx),
              "3 plus 7 equals 10");

    // 布尔值
    EXPECT_EQ(engine.evaluate("\"Is adult: ${age >= 18}\"", ctx), "Is adult: true");

    // 函数调用
    auto greeting_func = mc::expr::make_simple_function("greeting", [](const std::string& name) {
        return "Hello, " + name + "!";
    });
    ctx.register_function(greeting_func);

    EXPECT_EQ(engine.evaluate("\"Message: ${greeting(name)}\"", ctx), "Message: Hello, Alice!");

    mc::dict user = {{"name", "Bob"}, {"details", mc::dict{{"city", "Beijing"}}}};

    ctx.register_variable("user", user);
    EXPECT_EQ(engine.evaluate("\"User: ${user.name}\"", ctx), "User: Bob");
    EXPECT_EQ(engine.evaluate("\"City: ${user.details.city}\"", ctx), "City: Beijing");

    // 异常情况：未定义变量
    EXPECT_THROW(engine.evaluate("\"${undefined_var}\"", ctx), mc::invalid_arg_exception);
}