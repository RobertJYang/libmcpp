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
class string_builtin_test : public ::testing::Test {
protected:
    string_builtin_test() = default;

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

// 测试 length 函数
TEST_F(string_builtin_test, LengthFunction) {
    // 测试空字符串
    EXPECT_EQ(engine.evaluate("length('')", m_context), 0);

    // 测试普通字符串
    EXPECT_EQ(engine.evaluate("length('hello')", m_context), 5);

    // 测试含有特殊字符的字符串
    EXPECT_EQ(engine.evaluate("length('你好，世界')", m_context),
              std::string_view("你好，世界").length());

    // 测试参数错误
    EXPECT_THROW(engine.evaluate("length()", m_context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("length('a', 'b')", m_context), mc::invalid_arg_exception);

    // 测试数值类型 - 应该尝试转换
    EXPECT_THROW(engine.evaluate("length(123)", m_context), mc::invalid_arg_exception);
}

// 测试 concat 函数
TEST_F(string_builtin_test, ConcatFunction) {
    // 测试空字符串
    EXPECT_EQ(engine.evaluate("concat()", m_context), "");

    // 测试单个字符串
    EXPECT_EQ(engine.evaluate("concat('hello')", m_context), "hello");

    // 测试多个字符串
    EXPECT_EQ(engine.evaluate("concat('hello', ' ', 'world')", m_context), "hello world");

    // 测试混合类型
    EXPECT_EQ(engine.evaluate("concat('Value: ', 42)", m_context), "Value: 42");
    EXPECT_EQ(engine.evaluate("concat('Pi: ', 3.14159)", m_context), "Pi: 3.14159");
    EXPECT_EQ(engine.evaluate("concat('Boolean: ', true)", m_context), "Boolean: true");

    // 测试多种类型混合
    EXPECT_EQ(engine.evaluate("concat('混合类型: ', 42, ', ', 3.14, ', ', true)", m_context),
              "混合类型: 42, 3.14, true");
}

// 测试 substring 函数
TEST_F(string_builtin_test, SubstringFunction) {
    // 测试基本功能
    EXPECT_EQ(engine.evaluate("substring('hello world', 0, 5)", m_context), "hello");
    EXPECT_EQ(engine.evaluate("substring('hello world', 6, 5)", m_context), "world");

    // 测试边界情况
    EXPECT_EQ(engine.evaluate("substring('hello', 0, 0)", m_context), "");
    EXPECT_EQ(engine.evaluate("substring('hello', 5, 0)", m_context), "");

    // 测试超出范围
    EXPECT_EQ(engine.evaluate("substring('hello', 0, 10)", m_context), "hello");
    EXPECT_EQ(engine.evaluate("substring('hello', 10, 5)", m_context), "");

    // 测试负索引（从末尾开始）
    EXPECT_EQ(engine.evaluate("substring('hello world', -5, 5)", m_context), "world");

    // 测试参数错误
    EXPECT_THROW(engine.evaluate("substring()", m_context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("substring('hello')", m_context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("substring('hello', 0)", m_context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("substring(123, 0, 5)", m_context), mc::invalid_arg_exception);
}

// 测试 to_upper 函数
TEST_F(string_builtin_test, UpperFunction) {
    // 测试基本功能
    EXPECT_EQ(engine.evaluate("to_upper('hello')", m_context), "HELLO");

    // 测试混合大小写
    EXPECT_EQ(engine.evaluate("to_upper('Hello World')", m_context), "HELLO WORLD");

    // 测试已经全是大写
    EXPECT_EQ(engine.evaluate("to_upper('HELLO')", m_context), "HELLO");

    // 测试含有非字母字符
    EXPECT_EQ(engine.evaluate("to_upper('hello123!@#')", m_context), "HELLO123!@#");

    // 测试空字符串
    EXPECT_EQ(engine.evaluate("to_upper('')", m_context), "");

    // 测试参数错误
    EXPECT_THROW(engine.evaluate("to_upper()", m_context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("to_upper('a', 'b')", m_context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("to_upper(123)", m_context), mc::invalid_arg_exception);
}

// 测试 to_lower 函数
TEST_F(string_builtin_test, LowerFunction) {
    // 测试基本功能
    EXPECT_EQ(engine.evaluate("to_lower('HELLO')", m_context), "hello");

    // 测试混合大小写
    EXPECT_EQ(engine.evaluate("to_lower('Hello World')", m_context), "hello world");

    // 测试已经全是小写
    EXPECT_EQ(engine.evaluate("to_lower('hello')", m_context), "hello");

    // 测试含有非字母字符
    EXPECT_EQ(engine.evaluate("to_lower('HELLO123!@#')", m_context), "hello123!@#");

    // 测试空字符串
    EXPECT_EQ(engine.evaluate("to_lower('')", m_context), "");

    // 测试参数错误
    EXPECT_THROW(engine.evaluate("to_lower()", m_context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("to_lower('a', 'b')", m_context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("to_lower(123)", m_context), mc::invalid_arg_exception);
}

// 测试 trim 函数
TEST_F(string_builtin_test, TrimFunction) {
    // 测试基本功能
    EXPECT_EQ(engine.evaluate("trim('  hello  ')", m_context), "hello");

    // 测试只有前导空格
    EXPECT_EQ(engine.evaluate("trim('  hello')", m_context), "hello");

    // 测试只有尾随空格
    EXPECT_EQ(engine.evaluate("trim('hello  ')", m_context), "hello");

    // 测试无空格
    EXPECT_EQ(engine.evaluate("trim('hello')", m_context), "hello");

    // 测试只有空格
    EXPECT_EQ(engine.evaluate("trim('   ')", m_context), "");

    // 测试空字符串
    EXPECT_EQ(engine.evaluate("trim('')", m_context), "");

    // 测试参数错误
    EXPECT_THROW(engine.evaluate("trim()", m_context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("trim('a', 'b')", m_context), mc::invalid_arg_exception);
    EXPECT_THROW(engine.evaluate("trim(123)", m_context), mc::invalid_arg_exception);
}