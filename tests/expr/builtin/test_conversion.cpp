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
class conversion_builtin_test : public ::testing::Test {
protected:
    conversion_builtin_test() = default;

    void SetUp() override {
        auto& builtin = mc::expr::builtin::get_instance();
        auto& ctx     = builtin.get_context();
        m_context     = engine.create_context(&ctx);
    }

    void TearDown() override {
    }

    mc::expr::engine  engine;
    mc::expr::context m_context;
};
} // namespace

// 测试 to_string 函数
TEST_F(conversion_builtin_test, ToStringFunction) {
    // 测试整数
    EXPECT_EQ(engine.evaluate("to_string(42)", m_context), "42");

    // 测试负整数
    EXPECT_EQ(engine.evaluate("to_string(-42)", m_context), "-42");

    // 测试浮点数
    EXPECT_EQ(engine.evaluate("to_string(3.14)", m_context), "3.14");

    // 测试布尔值
    EXPECT_EQ(engine.evaluate("to_string(true)", m_context), "true");
    EXPECT_EQ(engine.evaluate("to_string(false)", m_context), "false");

    // 测试已经是字符串
    EXPECT_EQ(engine.evaluate("to_string('hello')", m_context), "hello");

    // 测试参数错误
    EXPECT_THROW(engine.evaluate("to_string()", m_context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("to_string(1, 2)", m_context), mc::invalid_arg_exception);
}

// 测试 to_bool 函数
TEST_F(conversion_builtin_test, ToBoolFunction) {
    // 测试数值
    EXPECT_TRUE(engine.evaluate("to_bool(1)", m_context));
    EXPECT_TRUE(engine.evaluate("to_bool(42)", m_context));
    EXPECT_FALSE(engine.evaluate("to_bool(0)", m_context));

    // 测试浮点数
    EXPECT_TRUE(engine.evaluate("to_bool(3.14)", m_context));
    EXPECT_FALSE(engine.evaluate("to_bool(0.0)", m_context));

    // 测试字符串（只有 true 和 1 是 true）
    EXPECT_TRUE(engine.evaluate("to_bool('true')", m_context));
    EXPECT_FALSE(engine.evaluate("to_bool('yes')", m_context));
    EXPECT_TRUE(engine.evaluate("to_bool('1')", m_context));
    EXPECT_FALSE(engine.evaluate("to_bool('on')", m_context));
    EXPECT_FALSE(engine.evaluate("to_bool('enabled')", m_context));

    EXPECT_FALSE(engine.evaluate("to_bool('false')", m_context));
    EXPECT_FALSE(engine.evaluate("to_bool('no')", m_context));
    EXPECT_FALSE(engine.evaluate("to_bool('0')", m_context));
    EXPECT_FALSE(engine.evaluate("to_bool('off')", m_context));
    EXPECT_FALSE(engine.evaluate("to_bool('disabled')", m_context));
    EXPECT_FALSE(engine.evaluate("to_bool('')", m_context));

    // 测试已经是布尔类型
    EXPECT_TRUE(engine.evaluate("to_bool(true)", m_context));
    EXPECT_FALSE(engine.evaluate("to_bool(false)", m_context));

    // 测试参数错误
    EXPECT_THROW(engine.evaluate("to_bool()", m_context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("to_bool(true, false)", m_context), mc::invalid_arg_exception);
}

// 测试 to_int 函数
TEST_F(conversion_builtin_test, ToIntFunction) {
    // 测试整数
    EXPECT_EQ(engine.evaluate("to_integer(42)", m_context), 42);

    // 测试负整数
    EXPECT_EQ(engine.evaluate("to_integer(-42)", m_context), -42);

    // 测试浮点数 (应当截断)
    EXPECT_EQ(engine.evaluate("to_integer(3.14)", m_context), 3);
    EXPECT_EQ(engine.evaluate("to_integer(-3.14)", m_context), -3);

    // 测试字符串
    EXPECT_EQ(engine.evaluate("to_integer('42')", m_context), 42);
    EXPECT_EQ(engine.evaluate("to_integer('-42')", m_context), -42);

    // 测试布尔值
    EXPECT_EQ(engine.evaluate("to_integer(true)", m_context), 1);
    EXPECT_EQ(engine.evaluate("to_integer(false)", m_context), 0);

    // 测试无效转换
    EXPECT_THROW(engine.evaluate("to_integer('not an int')", m_context), mc::invalid_arg_exception);

    // 测试参数错误
    EXPECT_THROW(engine.evaluate("to_integer()", m_context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("to_integer(42, 43)", m_context), mc::invalid_arg_exception);
}

// 测试 to_double 函数
TEST_F(conversion_builtin_test, ToDoubleFunction) {
    // 测试整数
    EXPECT_EQ(engine.evaluate("to_double(42)", m_context), 42.0);

    // 测试负整数
    EXPECT_EQ(engine.evaluate("to_double(-42)", m_context), -42.0);

    // 测试浮点数
    EXPECT_EQ(engine.evaluate("to_double(3.14)", m_context), 3.14);

    // 测试字符串
    EXPECT_EQ(engine.evaluate("to_double('3.14')", m_context), 3.14);
    EXPECT_EQ(engine.evaluate("to_double('-3.14')", m_context), -3.14);
    EXPECT_EQ(engine.evaluate("to_double('1.23e-2')", m_context), 0.0123);

    // 测试布尔值
    EXPECT_EQ(engine.evaluate("to_double(true)", m_context), 1.0);
    EXPECT_EQ(engine.evaluate("to_double(false)", m_context), 0.0);

    // 测试无效转换
    EXPECT_THROW(engine.evaluate("to_double('not a float')", m_context), mc::invalid_arg_exception);

    // 测试参数错误
    EXPECT_THROW(engine.evaluate("to_double()", m_context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("to_double(3.14, 2.71)", m_context), mc::invalid_arg_exception);
}