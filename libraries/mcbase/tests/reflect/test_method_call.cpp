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
 * @brief 测试反射方法调用
 */
#include <gtest/gtest.h>
#include <mc/exception.h>
#include <mc/reflect.h>
#include <mc/variant.h>
#include <string>
#include <vector>

using namespace mc;

namespace {

// 辅助函数：显式构造 mc::variants，避免花括号初始化列表歧义
template <typename... Args>
mc::variants make_args(Args&&... args)
{
    mc::variants values;
    values.reserve(sizeof...(args));
    (values.push_back(mc::variant(std::forward<Args>(args))), ...);
    return values;
}

} // namespace

// 测试类
class Calculator {
public:
    MC_REFLECTABLE("mc.test.Calculator");

    int                      m_base_value;
    mc::string               m_last_operation;
    std::vector<mc::string>  m_history;

    Calculator() : m_base_value(0)
    {}

    int get_base() const
    {
        return m_base_value;
    }

    void set_base(int value)
    {
        m_base_value = value;
        record_operation("set_base");
    }

    int add(int a, int b)
    {
        int result = a + b + m_base_value;
        record_operation("add");
        return result;
    }

    int add3(int a, int b, int c)
    {
        int result = a + b + c + m_base_value;
        record_operation("add3");
        return result;
    }

    mc::string format_result(int value, mc::string prefix, mc::string suffix)
    {
        record_operation("format");
        return prefix + ": " + mc::string(std::to_string(value).c_str()) + suffix;
    }

private:
    void record_operation(mc::string_view op)
    {
        m_last_operation = mc::string(op);
        m_history.push_back(mc::string(op));
    }
};

MC_REFLECT(Calculator, (m_base_value)(get_base)(set_base)(add)(add3)(format_result))

// 测试两参数方法调用
TEST(MethodCallTest, ExactArgsMethodCall)
{
    Calculator calc;
    calc.m_base_value = 10;

    auto add_method = mc::reflect::get_method_info<Calculator>("add");
    ASSERT_NE(add_method, nullptr);
    EXPECT_EQ(add_method->arg_count(), 2U);

    auto args = make_args(5, 7);
    ASSERT_EQ(args.size(), 2U);

    variant result = add_method->invoke(calc, args);
    EXPECT_EQ(result, 22); // 5 + 7 + 10(base)
    EXPECT_EQ(calc.m_last_operation, "add");
}

// 测试字符串参数方法调用
TEST(MethodCallTest, StringArgsMethodCall)
{
    Calculator calc;

    auto format_method = mc::reflect::get_method_info<Calculator>("format_result");
    ASSERT_NE(format_method, nullptr);

    variant result = format_method->invoke(calc, make_args(42, "Output", " (calculated)"));
    EXPECT_EQ(result, "Output: 42 (calculated)");
    EXPECT_EQ(calc.m_last_operation, "format");
}

// 测试不同参数数量的方法
TEST(MethodCallTest, VariousArgCountMethodCall)
{
    Calculator calc;
    calc.m_base_value = 5;

    // 无参数
    {
        auto    get_method = mc::reflect::get_method_info<Calculator>("get_base");
        variant result     = get_method->invoke(calc, make_args());
        EXPECT_EQ(result, 5);
    }

    // 单参数
    {
        auto set_method = mc::reflect::get_method_info<Calculator>("set_base");
        set_method->invoke(calc, make_args(10));
        EXPECT_EQ(calc.m_base_value, 10);
        EXPECT_EQ(calc.m_last_operation, "set_base");
    }

    // 三参数
    {
        auto    add3_method = mc::reflect::get_method_info<Calculator>("add3");
        variant result      = add3_method->invoke(calc, make_args(5, 7, 3));
        EXPECT_EQ(result, 25); // 5 + 7 + 3 + 10(base)
        EXPECT_EQ(calc.m_last_operation, "add3");
    }
}

// 测试通过反射名称调用
TEST(MethodCallTest, InvokeByName)
{
    Calculator calc;
    calc.m_base_value = 20;

    // 单参数
    mc::reflect::invoke(calc, "set_base", make_args(20));
    EXPECT_EQ(calc.m_base_value, 20);
    EXPECT_EQ(calc.m_last_operation, "set_base");

    // 多参数
    variant result = mc::reflect::invoke(calc, "add", make_args(5, 7));
    EXPECT_EQ(result, 32); // 5 + 7 + 20(base)
    EXPECT_EQ(calc.m_last_operation, "add");

    // 字符串参数
    variant fmt_result = mc::reflect::invoke(calc, "format_result", make_args(42, "测试", "!"));
    EXPECT_EQ(fmt_result, "测试: 42!");
    EXPECT_EQ(calc.m_last_operation, "format");
}

// 测试 create_runtime_method_ptr
TEST(MethodCallTest, CreateRuntimeMethodPtr)
{
    const mc::reflect::method_registration_info* add_method_info = nullptr;
    auto                                         methods = mc::reflect::get_static_methods<Calculator>();
    mc::traits::tuple_for_each(methods, [&](const auto* method) {
        if (method->name == "add") {
            add_method_info = method;
        }
    });

    ASSERT_NE(add_method_info, nullptr);

    auto runtime_method = add_method_info->create_runtime_method_ptr();
    ASSERT_NE(runtime_method, nullptr);

    Calculator calc;
    calc.m_base_value = 3;

    EXPECT_EQ(runtime_method->invoke(calc, make_args(4, 5)), 12);
    EXPECT_EQ(calc.m_last_operation, "add");
}

// 测试参数不足时抛出异常
TEST(MethodCallTest, InsufficientArgsThrows)
{
    Calculator calc;
    calc.m_base_value = 10;

    auto add_method = mc::reflect::get_method_info<Calculator>("add");
    ASSERT_NE(add_method, nullptr);

    auto bad_args = make_args(5);
    EXPECT_THROW(add_method->invoke(calc, bad_args), mc::bad_function_call_exception);
}

// 测试通过名称调用不存在的方法时抛出异常
TEST(MethodCallTest, NonexistentMethodThrows)
{
    Calculator calc;
    EXPECT_THROW(mc::reflect::invoke(calc, "不存在的方法"), mc::bad_function_call_exception);
}
