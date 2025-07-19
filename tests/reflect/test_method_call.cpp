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
 * @file test_method_call.cpp
 * @brief 测试方法调用
 */
#include <gtest/gtest.h>
#include <mc/exception.h>
#include <mc/reflect.h>
#include <mc/variant.h>
#include <string>
#include <vector>

using namespace mc;

// 测试类
class Calculator {
public:
    MC_REFLECTABLE();

    int                      m_base_value;
    std::string              m_last_operation;
    std::vector<std::string> m_history;

    Calculator() : m_base_value(0) {
    }

    // 无参数方法
    int get_base() const {
        return m_base_value;
    }

    // 一个参数方法
    void set_base(int value) {
        m_base_value = value;
        record_operation("set_base");
    }

    // 两个参数方法
    int add(int a, int b) {
        int result = a + b + m_base_value;
        record_operation("add");
        return result;
    }

    // 三个参数方法
    int add3(int a, int b, int c) {
        int result = a + b + c + m_base_value;
        record_operation("add3");
        return result;
    }

    // 多参数字符串方法
    std::string format_result(int value, std::string prefix, std::string suffix) {
        std::string result = prefix + ": " + std::to_string(value) + suffix;
        record_operation("format");
        return result;
    }

private:
    void record_operation(const std::string& op) {
        m_last_operation = op;
        m_history.push_back(op);
    }
};

// 反射Calculator类
MC_REFLECT(
    Calculator,
    (m_base_value)(m_last_operation)(m_history)(get_base)(set_base)(add)(add3)(format_result))

// 测试精确参数方法调用
TEST(MethodCallTest, ExactArgsMethodCall) {
    Calculator calc;
    calc.m_base_value = 10;

    // 查找add方法
    auto add_method = mc::reflect::get_method_info<Calculator>("add");
    ASSERT_NE(add_method, nullptr);

    // 测试精确参数调用
    {
        variant result = add_method->invoke(calc, {5, 7});
        EXPECT_EQ(result, 22); // 5 + 7 + 10(base)
        EXPECT_EQ(calc.m_last_operation, "add");
    }

    // 测试参数不足的情况（应该抛出异常）
    {
        EXPECT_THROW(add_method->invoke(calc, {5}), mc::bad_function_call_exception);
    }
}

// 测试字符串参数
TEST(MethodCallTest, StringArgsMethodCall) {
    Calculator calc;

    // 查找format_result方法
    auto format_method = mc::reflect::get_method_info<Calculator>("format_result");
    ASSERT_NE(format_method, nullptr);

    // 测试精确参数调用
    {
        variant result = format_method->invoke(calc, {42, "Output", " (calculated)"});
        EXPECT_EQ(result, "Output: 42 (calculated)");
        EXPECT_EQ(calc.m_last_operation, "format");
    }

    // 测试参数不足的情况（应该抛出异常）
    {
        EXPECT_THROW(format_method->invoke(calc, {42, "Output"}), mc::bad_function_call_exception);
    }
}

// 测试无参数和单参数方法
TEST(MethodCallTest, SimpleMethodCall) {
    Calculator calc;
    calc.m_base_value = 5;

    // 查找get_base和set_base方法
    auto get_method = mc::reflect::get_method_info<Calculator>("get_base");
    auto set_method = mc::reflect::get_method_info<Calculator>("set_base");

    ASSERT_NE(get_method, nullptr);
    ASSERT_NE(set_method, nullptr);

    // 测试无参数方法调用
    {
        variant result = get_method->invoke(calc, {});
        EXPECT_EQ(result, 5);
    }

    // 测试单参数方法调用
    {
        set_method->invoke(calc, {10});
        EXPECT_EQ(calc.m_base_value, 10);
        EXPECT_EQ(calc.m_last_operation, "set_base");
    }
}

// 测试三参数方法
TEST(MethodCallTest, ThreeArgsMethodCall) {
    Calculator calc;
    calc.m_base_value = 10;

    // 查找add3方法
    auto add3_method = mc::reflect::get_method_info<Calculator>("add3");
    ASSERT_NE(add3_method, nullptr);

    // 测试精确参数调用
    {
        variant result = add3_method->invoke(calc, {5, 7, 3});
        EXPECT_EQ(result, 25); // 5 + 7 + 3 + 10(base)
        EXPECT_EQ(calc.m_last_operation, "add3");
    }
}

// 测试反射方法调用便捷函数
// 这是反射方法调用便捷函数，不需要用户手动查找方法信息
TEST(MethodCallTest, InvokeMethodCall) {
    Calculator calc;
    calc.m_base_value = 10;

    // 调用无参数方法
    {
        variant result = mc::reflect::invoke(calc, "get_base");
        EXPECT_EQ(result, 10);
    }

    // 调用单参数方法
    {
        mc::reflect::invoke(calc, "set_base", {20});
        EXPECT_EQ(calc.m_base_value, 20);
        EXPECT_EQ(calc.m_last_operation, "set_base");
    }

    // 调用多参数方法
    {
        variant result = mc::reflect::invoke(calc, "add", {5, 7});
        EXPECT_EQ(result, 32); // 5 + 7 + 20(base)
        EXPECT_EQ(calc.m_last_operation, "add");
    }

    // 调用带字符串参数的方法
    {
        variant result = mc::reflect::invoke(calc, "format_result", {42, "测试", "!"});
        EXPECT_EQ(result, "测试: 42!");
        EXPECT_EQ(calc.m_last_operation, "format");
    }

    // 测试参数不足的情况
    {
        EXPECT_THROW(mc::reflect::invoke(calc, "add", {5}), mc::bad_function_call_exception);
    }

    // 测试不存在的方法
    {
        EXPECT_THROW(mc::reflect::invoke(calc, "不存在的方法"), mc::bad_function_call_exception);
    }
}