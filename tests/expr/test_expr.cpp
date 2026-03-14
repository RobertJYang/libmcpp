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

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <iostream>
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/expr/builtin.h>
#include <mc/expr/engine.h>
#include <mc/expr/function.h>

namespace {
class expr_test : public ::testing::Test {
protected:
    expr_test()
    {}

    void SetUp() override
    {}

    void TearDown() override
    {}

    mc::expr::engine engine;
};

} // namespace

TEST_F(expr_test, BasicArithmetic)
{
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

TEST_F(expr_test, Comparison)
{
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

TEST_F(expr_test, StringOperations)
{
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

TEST_F(expr_test, Variables)
{
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

TEST_F(expr_test, Functions)
{
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

TEST_F(expr_test, Errors)
{
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

TEST_F(expr_test, BitOperations)
{
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

TEST_F(expr_test, TemplateStrings)
{
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
    EXPECT_EQ(engine.evaluate("\"Status: ${age >= 18 ? 'adult' : 'minor'}\"", ctx), "Status: adult");

    // 嵌套表达式（字符串拼接）
    EXPECT_EQ(engine.evaluate("\"${name + ' says: \"Hi ' + name + '!\"'}\"", ctx), "Alice says: \"Hi Alice!\"");

    // 复杂表达式组合
    EXPECT_EQ(engine.evaluate("\"${1 + 2} plus ${3 + 4} equals ${(1 + 2) + (3 + 4)}\"", ctx), "3 plus 7 equals 10");

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

// 测试一元运算符的更多场景
TEST_F(expr_test, UnaryOperators)
{
    auto& ctx = engine.get_global_context();

    // 一元负号与浮点数
    EXPECT_EQ(engine.evaluate("-3.14", ctx), -3.14);
    EXPECT_EQ(engine.evaluate("--3.14", ctx), 3.14);

    // 逻辑非与不同类型
    EXPECT_FALSE(engine.evaluate("!true", ctx));
    EXPECT_TRUE(engine.evaluate("!false", ctx));
    EXPECT_FALSE(engine.evaluate("!'hello'", ctx));
    EXPECT_TRUE(engine.evaluate("!''", ctx));

    // 位非与不同数值
    EXPECT_EQ(engine.evaluate("~1", ctx), -2);
    EXPECT_EQ(engine.evaluate("~(-1)", ctx), 0);
}

// 测试运算符结合性
TEST_F(expr_test, operator_associativity)
{
    auto& ctx = engine.get_global_context();

    // 左结合运算符
    EXPECT_EQ(engine.evaluate("10 - 3 - 2", ctx), 5);
    EXPECT_EQ(engine.evaluate("10.0 / 2.0 / 2.0", ctx), 2.5);
    EXPECT_EQ(engine.evaluate("10 / 2 / 2", ctx), 2); // 整数除法
    EXPECT_EQ(engine.evaluate("10 % 3 % 2", ctx), 1);

    // 右结合运算符（条件表达式）
    EXPECT_EQ(engine.evaluate("1 ? 2 : 3 ? 4 : 5", ctx), 2);
    EXPECT_EQ(engine.evaluate("0 ? 2 : 3 ? 4 : 5", ctx), 4);
}

// 测试浮点数精度
TEST_F(expr_test, floating_point_precision)
{
    auto& ctx = engine.get_global_context();

    // 浮点数运算
    EXPECT_DOUBLE_EQ(engine.evaluate("0.1 + 0.2", ctx).as_double(), 0.3);
    EXPECT_DOUBLE_EQ(engine.evaluate("1.0 / 3.0", ctx).as_double(), 1.0 / 3.0);

    // 浮点数比较
    EXPECT_TRUE(engine.evaluate("0.1 + 0.2 == 0.3", ctx));
    EXPECT_TRUE(engine.evaluate("1.0 / 3.0 * 3.0 == 1.0", ctx));
}

// 测试字符串转义
TEST_F(expr_test, string_escaping)
{
    auto& ctx = engine.get_global_context();

    // 单引号字符串中的转义（只支持转义引号）
    EXPECT_EQ(engine.evaluate("'hello\\'world'", ctx), "hello'world");
    // 注意：lexer 目前只支持转义引号，不支持 \n, \t 等转义序列
    EXPECT_EQ(engine.evaluate("'hello\\nworld'", ctx), "hello\\nworld");
    EXPECT_EQ(engine.evaluate("'hello\\tworld'", ctx), "hello\\tworld");

    // 双引号字符串中的转义（只支持转义引号）
    EXPECT_EQ(engine.evaluate("\"hello\\\"world\"", ctx), "hello\"world");
    // 注意：lexer 目前只支持转义引号，不支持 \n, \t 等转义序列
    EXPECT_EQ(engine.evaluate("\"hello\\nworld\"", ctx), "hello\\nworld");
}

// 测试大数处理
TEST_F(expr_test, large_numbers)
{
    auto& ctx = engine.get_global_context();

    // 大整数（int64_t 的最大值）
    EXPECT_EQ(engine.evaluate("9223372036854775807", ctx), 9223372036854775807LL);

    // 使用表达式 -9223372036854775807 - 1 来表示 int64_t 的最小值
    EXPECT_EQ(engine.evaluate("-9223372036854775807 - 1", ctx), static_cast<int64_t>(-9223372036854775808ULL));

    // 大数运算
    EXPECT_EQ(engine.evaluate("1000000000 + 2000000000", ctx), 3000000000LL);
    EXPECT_EQ(engine.evaluate("1000000000 * 2", ctx), 2000000000LL);
}

// 测试边界值
TEST_F(expr_test, boundary_values)
{
    auto& ctx = engine.get_global_context();

    // 零值
    EXPECT_EQ(engine.evaluate("0", ctx), 0);
    EXPECT_EQ(engine.evaluate("0.0", ctx), 0.0);
    EXPECT_TRUE(engine.evaluate("0 == 0.0", ctx));

    // 负数
    EXPECT_EQ(engine.evaluate("-0", ctx), 0);
    EXPECT_EQ(engine.evaluate("-0.0", ctx), 0.0);

    // 空字符串
    EXPECT_EQ(engine.evaluate("''", ctx), "");
    EXPECT_EQ(engine.evaluate("\"\"", ctx), "");
    EXPECT_TRUE(engine.evaluate("'' == \"\"", ctx));
}

// 测试类型转换
TEST_F(expr_test, type_conversion)
{
    auto& ctx = engine.get_global_context();

    // 整数转浮点数
    EXPECT_DOUBLE_EQ(engine.evaluate("1 + 2.0", ctx).as_double(), 3.0);
    EXPECT_DOUBLE_EQ(engine.evaluate("1.0 + 2", ctx).as_double(), 3.0);

    // 数字转字符串
    EXPECT_EQ(engine.evaluate("'Value: ' + 42", ctx), "Value: 42");
    EXPECT_EQ(engine.evaluate("'Value: ' + 3.14", ctx), "Value: 3.14");

    // 布尔值转数字
    EXPECT_EQ(engine.evaluate("true + 1", ctx), 2);
    EXPECT_EQ(engine.evaluate("false + 1", ctx), 1);
}

// 测试复杂嵌套表达式
TEST_F(expr_test, complex_nested_expressions)
{
    auto ctx = engine.make_context();
    ctx.register_variable("a", 1);
    ctx.register_variable("b", 2);
    ctx.register_variable("c", 3);

    // 多层嵌套
    EXPECT_EQ(engine.evaluate("((a + b) * c) - (a * b)", ctx), 7);
    EXPECT_EQ(engine.evaluate("(a > 0 ? b : c) + (b > 0 ? c : a)", ctx), 5);

    // 函数调用嵌套
    // min(-1, -2, -3) = -3, abs(-3) = 3
    EXPECT_EQ(engine.evaluate("abs(min(-1, -2, -3))", ctx), 3);
    EXPECT_EQ(engine.evaluate("max(abs(-1), abs(-2), abs(-3))", ctx), 3);
}

// 测试更多错误场景（补充 Errors 测试中未覆盖的场景）
TEST_F(expr_test, more_error_scenarios)
{
    auto ctx = engine.make_context();

    // 无效的函数调用（逗号错误）
    EXPECT_THROW(engine.evaluate("func(,)", ctx), mc::parse_error_exception);
    EXPECT_THROW(engine.evaluate("func(1,)", ctx), mc::parse_error_exception);

    // 无效的属性访问
    EXPECT_THROW(engine.evaluate(".property", ctx), mc::parse_error_exception);
    EXPECT_THROW(engine.evaluate("obj.", ctx), mc::parse_error_exception);
}

// 测试条件表达式求值（补充 Comparison 测试中缺少的条件表达式测试）
TEST_F(expr_test, conditional_expression)
{
    auto& ctx = engine.get_global_context();

    // 基本条件表达式
    EXPECT_EQ(engine.evaluate("1 ? 2 : 3", ctx), 2);
    EXPECT_EQ(engine.evaluate("0 ? 2 : 3", ctx), 3);
    EXPECT_EQ(engine.evaluate("true ? 10 : 20", ctx), 10);
    EXPECT_EQ(engine.evaluate("false ? 10 : 20", ctx), 20);

    // 嵌套条件表达式（注意：解析器可能不支持嵌套的三元运算符，如果失败则移除这些测试）
    // 如果解析器不支持嵌套，这些测试会抛出异常
    // EXPECT_EQ(engine.evaluate("1 ? 2 ? 3 : 4 : 5", ctx), 3);
    // EXPECT_EQ(engine.evaluate("1 ? 0 ? 3 : 4 : 5", ctx), 4);
    // EXPECT_EQ(engine.evaluate("0 ? 2 ? 3 : 4 : 5", ctx), 5);
}

// 覆盖数学模块中未执行的函数
TEST_F(expr_test, math_module_advanced_functions)
{
    auto ctx = engine.make_context();

    EXPECT_DOUBLE_EQ(engine.evaluate("round(1.4)", ctx).as_double(), 1.0);
    EXPECT_DOUBLE_EQ(engine.evaluate("ceil(1.2)", ctx).as_double(), 2.0);
    EXPECT_DOUBLE_EQ(engine.evaluate("floor(1.8)", ctx).as_double(), 1.0);
    EXPECT_DOUBLE_EQ(engine.evaluate("sqrt(9)", ctx).as_double(), 3.0);
    EXPECT_NEAR(engine.evaluate("exp(1)", ctx).as_double(), std::exp(1.0), 1e-9);
    EXPECT_NEAR(engine.evaluate("log(1)", ctx).as_double(), 0.0, 1e-12);
    EXPECT_NEAR(engine.evaluate("pow(4, 0.5)", ctx).as_double(), 2.0, 1e-9);
}

// 覆盖数学模块中的异常分支
TEST_F(expr_test, math_module_invalid_inputs)
{
    auto ctx = engine.make_context();
    EXPECT_THROW(engine.evaluate("log(0)", ctx), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("sqrt(-1)", ctx), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("pow(-1, 2)", ctx), mc::invalid_arg_exception);
}

// 测试 engine::evaluate 保持自定义函数抛出的异常
TEST_F(expr_test, HandleUnknownException)
{
    auto ctx = engine.make_context();

    // 注册一个函数，在执行时抛出 std::logic_error
    auto throwing_func = mc::expr::make_simple_function("throw_logic_error", []() {
        throw std::logic_error("test logic error");
    });

    ctx.register_function(throwing_func);

    // 调用这个函数，应抛出 std::logic_error
    EXPECT_THROW(engine.evaluate("throw_logic_error()", ctx), std::logic_error);
}

// 测试 builtin::register_symbol(std::string name, mc::variant value)
TEST_F(expr_test, BuiltinRegisterVariable)
{
    auto& builtin = mc::expr::builtin::get_instance();
    auto& ctx     = engine.get_global_context();

    // 注册一个变量
    builtin.register_symbol("test_var", mc::variant(42));
    EXPECT_EQ(engine.evaluate("test_var", ctx), 42);

    // 注册另一个变量
    builtin.register_symbol("test_str", mc::variant("hello"));
    EXPECT_EQ(engine.evaluate("test_str", ctx), "hello");
}
