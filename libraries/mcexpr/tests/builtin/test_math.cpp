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
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/expr/builtin.h>
#include <mc/expr/engine.h>

namespace {
class math_builtin_test : public ::testing::Test {
protected:
    math_builtin_test() = default;

    void SetUp() override
    {
        auto& builtin = mc::expr::builtin::get_instance();
        auto& ctx     = builtin.get_context();
        context       = engine.make_context(&ctx);
    }

    void TearDown() override
    {}

    mc::expr::engine  engine;
    mc::expr::context context;
};
} // namespace

// 测试 abs 函数
TEST_F(math_builtin_test, AbsFunction)
{
    // 测试正整数
    EXPECT_EQ(engine.evaluate("abs(10)", context), 10);

    // 测试负整数
    EXPECT_EQ(engine.evaluate("abs(-10)", context), 10);

    // 测试正浮点数
    EXPECT_EQ(engine.evaluate("abs(3.14)", context), 3.14);

    // 测试负浮点数
    EXPECT_EQ(engine.evaluate("abs(-3.14)", context), 3.14);

    // 测试零
    EXPECT_EQ(engine.evaluate("abs(0)", context), 0);

    // 测试参数错误
    EXPECT_THROW(engine.evaluate("abs()", context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("abs('string')", context), mc::invalid_arg_exception);

    // 实参个数比形参个数多，允许
    EXPECT_NO_THROW(engine.evaluate("abs(1, 2)", context));
}

// 测试 min 函数
TEST_F(math_builtin_test, MinFunction)
{
    // 测试整数
    EXPECT_EQ(engine.evaluate("min(3, 1, 4, 1, 5)", context), 1);

    // 测试浮点数
    EXPECT_EQ(engine.evaluate("min(3.14, 2.71, 1.41)", context), 1.41);

    // 测试混合类型
    EXPECT_EQ(engine.evaluate("min(5, 2.5, 10)", context), 2.5);

    // 测试单个参数
    EXPECT_EQ(engine.evaluate("min(42)", context), 42);

    // 测试参数错误
    EXPECT_THROW(engine.evaluate("min()", context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("min('a', 'b')", context), mc::invalid_arg_exception);
}

// 测试 max 函数
TEST_F(math_builtin_test, MaxFunction)
{
    // 测试整数
    EXPECT_EQ(engine.evaluate("max(3, 1, 4, 1, 5)", context), 5);

    // 测试浮点数
    EXPECT_EQ(engine.evaluate("max(3.14, 2.71, 1.41)", context), 3.14);

    // 测试混合类型
    EXPECT_EQ(engine.evaluate("max(5, 7.5, 2)", context), 7.5);

    // 测试单个参数
    EXPECT_EQ(engine.evaluate("max(42)", context), 42);

    // 测试参数错误
    EXPECT_THROW(engine.evaluate("max()", context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("max('a', 'b')", context), mc::invalid_arg_exception);
}

// 测试 pow 函数
TEST_F(math_builtin_test, PowFunction)
{
    // 测试整数
    EXPECT_EQ(engine.evaluate("pow(2, 3)", context), 8.0);

    // 测试浮点数
    EXPECT_EQ(engine.evaluate("pow(2.5, 2)", context), 6.25);

    // 测试负指数
    EXPECT_EQ(engine.evaluate("pow(2, -1)", context), 0.5);

    // 测试零次方
    EXPECT_EQ(engine.evaluate("pow(42, 0)", context), 1.0);

    // 测试负底数（应该抛出异常）
    EXPECT_THROW(engine.evaluate("pow(-1, 2)", context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("pow(-2.5, 3)", context), mc::invalid_arg_exception);

    // 测试参数错误
    EXPECT_THROW(engine.evaluate("pow(2)", context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("pow('string', 2)", context), mc::invalid_arg_exception);

    // 实参个数比形参个数多，允许
    EXPECT_NO_THROW(engine.evaluate("pow(2, 3, 4)", context));
}

// 测试 sqrt 函数
TEST_F(math_builtin_test, SqrtFunction)
{
    // 测试整数
    EXPECT_EQ(engine.evaluate("sqrt(4)", context), 2.0);
    EXPECT_EQ(engine.evaluate("sqrt(9)", context), 3.0);

    // 测试浮点数
    EXPECT_EQ(engine.evaluate("sqrt(2)", context), 1.414213562373095);

    // 测试零
    EXPECT_EQ(engine.evaluate("sqrt(0)", context), 0.0);

    // 测试负数（应该抛出异常）
    EXPECT_THROW(engine.evaluate("sqrt(-1)", context), mc::invalid_arg_exception);

    // 测试参数错误
    EXPECT_THROW(engine.evaluate("sqrt()", context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("sqrt('string')", context), mc::invalid_arg_exception);

    // 实参个数比形参个数多，允许
    EXPECT_NO_THROW(engine.evaluate("sqrt(1, 2)", context));
}

// 测试 floor 函数
TEST_F(math_builtin_test, FloorFunction)
{
    // 测试整数
    EXPECT_EQ(engine.evaluate("floor(42)", context), 42.0);

    // 测试浮点数
    EXPECT_EQ(engine.evaluate("floor(3.14)", context), 3.0);
    EXPECT_EQ(engine.evaluate("floor(2.99)", context), 2.0);

    // 测试负浮点数
    EXPECT_EQ(engine.evaluate("floor(-3.14)", context), -4.0);
    EXPECT_EQ(engine.evaluate("floor(-2.0)", context), -2.0);

    // 测试参数错误
    EXPECT_THROW(engine.evaluate("floor()", context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("floor('string')", context), mc::invalid_arg_exception);

    // 实参个数比形参个数多，允许
    EXPECT_NO_THROW(engine.evaluate("floor(1, 2)", context));
}

// 测试 ceil 函数
TEST_F(math_builtin_test, CeilFunction)
{
    // 测试整数
    EXPECT_EQ(engine.evaluate("ceil(42)", context), 42.0);

    // 测试浮点数
    EXPECT_EQ(engine.evaluate("ceil(3.14)", context), 4.0);
    EXPECT_EQ(engine.evaluate("ceil(2.01)", context), 3.0);

    // 测试负浮点数
    EXPECT_EQ(engine.evaluate("ceil(-3.14)", context), -3.0);
    EXPECT_EQ(engine.evaluate("ceil(-2.0)", context), -2.0);

    // 测试参数错误
    EXPECT_THROW(engine.evaluate("ceil()", context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("ceil('string')", context), mc::invalid_arg_exception);

    // 实参个数比形参个数多，允许
    EXPECT_NO_THROW(engine.evaluate("ceil(1, 2)", context));
}

// 测试 round 函数
TEST_F(math_builtin_test, RoundFunction)
{
    // 测试整数
    EXPECT_EQ(engine.evaluate("round(42)", context), 42.0);

    // 测试浮点数
    EXPECT_EQ(engine.evaluate("round(3.14)", context), 3.0);
    EXPECT_EQ(engine.evaluate("round(3.5)", context), 4.0);
    EXPECT_EQ(engine.evaluate("round(2.49)", context), 2.0);

    // 测试负浮点数
    EXPECT_EQ(engine.evaluate("round(-3.14)", context), -3.0);
    EXPECT_EQ(engine.evaluate("round(-3.5)", context), -4.0);

    // 测试参数错误
    EXPECT_THROW(engine.evaluate("round()", context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("round('string')", context), mc::invalid_arg_exception);

    // 实参个数比形参个数多，允许
    EXPECT_NO_THROW(engine.evaluate("round(1, 2)", context));
}

// 测试 log 函数
TEST_F(math_builtin_test, LogFunction)
{
    // 测试正数
    EXPECT_NEAR(engine.evaluate("log(1)", context).as_double(), 0.0, 1e-10);
    EXPECT_NEAR(engine.evaluate("log(2.718281828)", context).as_double(), 1.0, 1e-6);
    EXPECT_NEAR(engine.evaluate("log(10)", context).as_double(), 2.302585093, 1e-6);

    // 测试零（应该抛出异常）
    EXPECT_THROW(engine.evaluate("log(0)", context), mc::invalid_arg_exception);

    // 测试负数（应该抛出异常）
    EXPECT_THROW(engine.evaluate("log(-1)", context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("log(-10.5)", context), mc::invalid_arg_exception);

    // 测试参数错误
    EXPECT_THROW(engine.evaluate("log()", context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("log('string')", context), mc::invalid_arg_exception);

    // 实参个数比形参个数多，允许
    EXPECT_NO_THROW(engine.evaluate("log(2, 3)", context));
}

// 测试 exp 函数
TEST_F(math_builtin_test, ExpFunction)
{
    // 测试零
    EXPECT_EQ(engine.evaluate("exp(0)", context), 1.0);

    // 测试正数
    EXPECT_NEAR(engine.evaluate("exp(1)", context).as_double(), 2.718281828, 1e-6);
    EXPECT_NEAR(engine.evaluate("exp(2)", context).as_double(), 7.389056099, 1e-6);

    // 测试负数
    EXPECT_NEAR(engine.evaluate("exp(-1)", context).as_double(), 0.367879441, 1e-6);

    // 测试参数错误
    EXPECT_THROW(engine.evaluate("exp()", context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("exp('string')", context), mc::invalid_arg_exception);

    // 实参个数比形参个数多，允许
    EXPECT_NO_THROW(engine.evaluate("exp(1, 2)", context));
}