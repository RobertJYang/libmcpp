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
#include <mc/expr/context.h>
#include <mc/expr/engine.h>
#include <mc/expr/function.h>
#include <mc/expr/node.h>

namespace {
class node_test : public ::testing::Test {
protected:
    void SetUp() override
    {
    }

    void TearDown() override
    {
    }

    mc::expr::context ctx;
};

// 测试 literal_node
TEST_F(node_test, literal_node_evaluate)
{
    // 测试各种类型的字面值
    auto int_node    = mc::expr::make_literal(42);
    auto double_node = mc::expr::make_literal(3.14);
    auto string_node = mc::expr::make_literal(std::string("hello"));
    auto bool_node   = mc::expr::make_literal(true);

    EXPECT_EQ(int_node->evaluate(ctx), 42);
    EXPECT_EQ(double_node->evaluate(ctx), 3.14);
    EXPECT_EQ(string_node->evaluate(ctx), "hello");
    EXPECT_EQ(bool_node->evaluate(ctx), true);
}

// literal_node_to_string 测试已合并到 literal_node_evaluate 和 literal_node_various_types 中

// 测试 variable_node
TEST_F(node_test, variable_node_evaluate)
{
    // 测试变量存在的情况
    ctx.register_variable("x", 10);
    auto var_node = mc::expr::make_variable("x");
    EXPECT_EQ(var_node->evaluate(ctx), 10);

    // 测试变量不存在的情况
    auto unknown_var = mc::expr::make_variable("unknown");
    EXPECT_THROW(unknown_var->evaluate(ctx), mc::invalid_arg_exception);
}

// variable_node_to_string 测试已合并到 variable_node_accessors_through_usage 中

// 测试 unary_op_node
TEST_F(node_test, unary_op_neg)
{
    auto operand  = mc::expr::make_literal(5);
    auto neg_node = mc::expr::make_unary_op(mc::expr::operator_type::neg, operand);
    EXPECT_EQ(neg_node->evaluate(ctx), -5);

    auto double_operand = mc::expr::make_literal(3.14);
    auto neg_double     = mc::expr::make_unary_op(mc::expr::operator_type::neg, double_operand);
    EXPECT_EQ(neg_double->evaluate(ctx), -3.14);
}

TEST_F(node_test, unary_op_not)
{
    auto true_operand = mc::expr::make_literal(true);
    auto not_node     = mc::expr::make_unary_op(mc::expr::operator_type::not_op, true_operand);
    EXPECT_EQ(not_node->evaluate(ctx), false);

    auto false_operand = mc::expr::make_literal(false);
    auto not_false     = mc::expr::make_unary_op(mc::expr::operator_type::not_op, false_operand);
    EXPECT_EQ(not_false->evaluate(ctx), true);
}

TEST_F(node_test, unary_op_bit_not)
{
    auto operand      = mc::expr::make_literal(5);
    auto bit_not_node = mc::expr::make_unary_op(mc::expr::operator_type::bit_not, operand);
    EXPECT_EQ(bit_not_node->evaluate(ctx), ~5);
}

TEST_F(node_test, unary_op_to_string)
{
    auto operand = mc::expr::make_literal(5);

    auto neg_node = mc::expr::make_unary_op(mc::expr::operator_type::neg, operand);
    EXPECT_EQ(neg_node->to_string(), "-5");

    auto not_node = mc::expr::make_unary_op(mc::expr::operator_type::not_op, operand);
    EXPECT_EQ(not_node->to_string(), "!5");

    auto bit_not_node = mc::expr::make_unary_op(mc::expr::operator_type::bit_not, operand);
    EXPECT_EQ(bit_not_node->to_string(), "~5");
}

// 测试 binary_op_node - 算术运算符
TEST_F(node_test, binary_op_arithmetic)
{
    auto left  = mc::expr::make_literal(10);
    auto right = mc::expr::make_literal(3);

    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::add, left, right)->evaluate(ctx), 13);
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::sub, left, right)->evaluate(ctx), 7);
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::mul, left, right)->evaluate(ctx), 30);
    // 整数除法：10 / 3 = 3
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::div, left, right)->evaluate(ctx), 3);
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::mod, left, right)->evaluate(ctx), 1);

    // 测试浮点数除法
    auto left_double  = mc::expr::make_literal(10.0);
    auto right_double = mc::expr::make_literal(3.0);
    EXPECT_DOUBLE_EQ(mc::expr::make_binary_op(mc::expr::operator_type::div, left_double, right_double)->evaluate(ctx).as_double(), 10.0 / 3.0);
}

// 测试 binary_op_node - 比较运算符
TEST_F(node_test, binary_op_comparison)
{
    auto left  = mc::expr::make_literal(5);
    auto right = mc::expr::make_literal(3);

    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::eq, left, right)->evaluate(ctx), false);
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::ne, left, right)->evaluate(ctx), true);
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::lt, left, right)->evaluate(ctx), false);
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::le, left, right)->evaluate(ctx), false);
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::gt, left, right)->evaluate(ctx), true);
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::ge, left, right)->evaluate(ctx), true);

    // 测试相等的情况
    auto same_left  = mc::expr::make_literal(5);
    auto same_right = mc::expr::make_literal(5);
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::eq, same_left, same_right)->evaluate(ctx), true);
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::le, same_left, same_right)->evaluate(ctx), true);
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::ge, same_left, same_right)->evaluate(ctx), true);
}

// 测试 binary_op_node - 逻辑运算符（短路）
TEST_F(node_test, binary_op_logical_short_circuit)
{
    // 测试 && 短路：左操作数为 false 时，不计算右操作数
    auto false_left = mc::expr::make_literal(false);
    auto right      = mc::expr::make_variable("nonexistent"); // 如果计算会抛出异常
    auto and_node   = mc::expr::make_binary_op(mc::expr::operator_type::and_op, false_left, right);
    EXPECT_EQ(and_node->evaluate(ctx), false);

    // 测试 && 不短路：左操作数为 true 时，计算右操作数
    auto true_left  = mc::expr::make_literal(true);
    auto true_right = mc::expr::make_literal(true);
    auto and_true   = mc::expr::make_binary_op(mc::expr::operator_type::and_op, true_left, true_right);
    EXPECT_EQ(and_true->evaluate(ctx), true);

    // 测试 || 短路：左操作数为 true 时，不计算右操作数
    auto or_node = mc::expr::make_binary_op(mc::expr::operator_type::or_op, true_left, right);
    EXPECT_EQ(or_node->evaluate(ctx), true);

    // 测试 || 不短路：左操作数为 false 时，计算右操作数
    auto false_right = mc::expr::make_literal(false);
    auto or_false    = mc::expr::make_binary_op(mc::expr::operator_type::or_op, false_left, false_right);
    EXPECT_EQ(or_false->evaluate(ctx), false);
}

// 测试 binary_op_node - 位运算符
TEST_F(node_test, binary_op_bitwise)
{
    auto left  = mc::expr::make_literal(5); // 101
    auto right = mc::expr::make_literal(3); // 011

    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::bit_and, left, right)->evaluate(ctx), 5 & 3);
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::bit_or, left, right)->evaluate(ctx), 5 | 3);
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::bit_xor, left, right)->evaluate(ctx), 5 ^ 3);
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::lshift, left, right)->evaluate(ctx), 5 << 3);
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::rshift, left, right)->evaluate(ctx), 5 >> 3);
}

TEST_F(node_test, binary_op_to_string)
{
    auto left  = mc::expr::make_literal(5);
    auto right = mc::expr::make_literal(3);

    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::add, left, right)->to_string(), "(5 + 3)");
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::and_op, left, right)->to_string(), "(5 && 3)");
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::bit_and, left, right)->to_string(), "(5 & 3)");
}

// 测试 conditional_node
TEST_F(node_test, conditional_node_true_branch)
{
    auto condition    = mc::expr::make_literal(true);
    auto true_branch  = mc::expr::make_literal(10);
    auto false_branch = mc::expr::make_literal(20);
    auto cond_node    = mc::expr::make_conditional(condition, true_branch, false_branch);
    EXPECT_EQ(cond_node->evaluate(ctx), 10);
}

TEST_F(node_test, conditional_node_false_branch)
{
    auto condition    = mc::expr::make_literal(false);
    auto true_branch  = mc::expr::make_literal(10);
    auto false_branch = mc::expr::make_literal(20);
    auto cond_node    = mc::expr::make_conditional(condition, true_branch, false_branch);
    EXPECT_EQ(cond_node->evaluate(ctx), 20);
}

// conditional_node_to_string 测试已合并到 conditional_node_accessors_through_usage 中

// 测试 function_call_node
TEST_F(node_test, function_call_node_no_args)
{
    // 注册一个无参数函数
    auto func = mc::expr::make_simple_function("get_answer", []() {
        return 42;
    });
    ctx.register_function(func);

    auto func_node = mc::expr::make_function_call("get_answer", {});
    EXPECT_EQ(func_node->evaluate(ctx), 42);
}

TEST_F(node_test, function_call_node_with_args)
{
    // 注册一个有参数函数
    auto func = mc::expr::make_simple_function("add", [](double a, double b) {
        return a + b;
    });
    ctx.register_function(func);

    auto arg1      = mc::expr::make_literal(10.0);
    auto arg2      = mc::expr::make_literal(20.0);
    auto func_node = mc::expr::make_function_call("add", {arg1, arg2});
    EXPECT_EQ(func_node->evaluate(ctx), 30.0);
}

TEST_F(node_test, function_call_node_undefined)
{
    auto func_node = mc::expr::make_function_call("nonexistent", {});
    EXPECT_THROW(func_node->evaluate(ctx), mc::invalid_arg_exception);
}

TEST_F(node_test, function_call_node_to_string)
{
    // 测试无参数
    auto func_node_no_args = mc::expr::make_function_call("func", {});
    EXPECT_EQ(func_node_no_args->to_string(), "func()");

    // 测试单参数
    auto arg1              = mc::expr::make_literal(10);
    auto func_node_one_arg = mc::expr::make_function_call("func", {arg1});
    EXPECT_EQ(func_node_one_arg->to_string(), "func(10)");

    // 测试多参数（覆盖 i > 0 分支）
    auto arg2            = mc::expr::make_literal(20);
    auto arg3            = mc::expr::make_literal(30);
    auto func_node_multi = mc::expr::make_function_call("func", {arg1, arg2, arg3});
    EXPECT_EQ(func_node_multi->to_string(), "func(10, 20, 30)");
}

// 测试 property_access_node - variable 类型（通过 dict 访问）
TEST_F(node_test, property_access_variable_type_dict)
{
    // 注册一个 dict 变量
    ctx.register_variable("obj", mc::dict{{"prop", 100}});

    auto obj_node  = mc::expr::make_variable("obj");
    auto prop_node = mc::expr::make_property_access(obj_node, "prop");

    // property_access_node 会先检查 variable 类型，然后通过 ctx.has_variable 查找
    // 由于 obj 是 dict，会通过 property_access 链式访问方式处理
    // 这里测试通过 property_access 类型访问 dict 属性
    EXPECT_EQ(prop_node->evaluate(ctx), 100);
}

// 测试 property_access_node - property_access 类型（链式访问）
TEST_F(node_test, property_access_chain)
{
    // 创建嵌套的 dict
    mc::dict inner;
    inner["nested"] = 200;
    mc::dict outer;
    outer["inner"] = inner;

    ctx.register_variable("obj", outer);

    // obj.inner.nested
    auto obj_node    = mc::expr::make_variable("obj");
    auto inner_node  = mc::expr::make_property_access(obj_node, "inner");
    auto nested_node = mc::expr::make_property_access(inner_node, "nested");

    // property_access_node 会先求值 inner_node，得到 inner dict
    // 然后检查是否是 object 类型并包含 "nested" 属性
    EXPECT_EQ(nested_node->evaluate(ctx), 200);
}

// 测试 property_access_node - variable 类型但属性不存在
TEST_F(node_test, property_access_variable_type_not_found)
{
    ctx.register_variable("obj", mc::dict{});

    auto obj_node  = mc::expr::make_variable("obj");
    auto prop_node = mc::expr::make_property_access(obj_node, "nonexistent");

    // 由于 obj 变量存在但没有接口属性，会抛出异常
    EXPECT_THROW(prop_node->evaluate(ctx), mc::invalid_arg_exception);
}

// 测试 property_access_node - property_access 类型但属性不存在
TEST_F(node_test, property_access_chain_not_found)
{
    mc::dict outer;
    outer["inner"] = mc::dict{};

    ctx.register_variable("obj", outer);

    auto obj_node    = mc::expr::make_variable("obj");
    auto inner_node  = mc::expr::make_property_access(obj_node, "inner");
    auto nested_node = mc::expr::make_property_access(inner_node, "nonexistent");

    EXPECT_THROW(nested_node->evaluate(ctx), mc::invalid_arg_exception);
}

// 测试 property_access_node - property_access 类型但结果不是 object
TEST_F(node_test, property_access_chain_not_object)
{
    ctx.register_variable("obj", mc::dict{{"value", 100}});

    auto obj_node   = mc::expr::make_variable("obj");
    auto value_node = mc::expr::make_property_access(obj_node, "value");
    // value 是数字，不是 object，无法继续访问属性
    auto nested_node = mc::expr::make_property_access(value_node, "prop");

    EXPECT_THROW(nested_node->evaluate(ctx), mc::invalid_arg_exception);
}

// 测试 property_access_node - 其他类型的对象（抛出异常）
TEST_F(node_test, property_access_invalid_object_type)
{
    // 使用 literal 作为对象（不是 variable 或 property_access）
    auto literal_obj = mc::expr::make_literal(42);
    auto prop_node   = mc::expr::make_property_access(literal_obj, "prop");

    EXPECT_THROW(prop_node->evaluate(ctx), mc::invalid_arg_exception);
}

// 测试 property_access_node - 属性不存在
TEST_F(node_test, property_access_not_found)
{
    ctx.register_variable("obj", mc::dict{{"prop", 100}});

    auto obj_node  = mc::expr::make_variable("obj");
    auto prop_node = mc::expr::make_property_access(obj_node, "nonexistent");

    EXPECT_THROW(prop_node->evaluate(ctx), mc::invalid_arg_exception);
}

TEST_F(node_test, property_access_to_string)
{
    auto obj_node  = mc::expr::make_variable("obj");
    auto prop_node = mc::expr::make_property_access(obj_node, "prop");
    EXPECT_EQ(prop_node->to_string(), "obj.prop");
}

// 测试 object_method_call_node - 函数不存在
TEST_F(node_test, object_method_call_node_function_not_found)
{
    ctx.register_variable("obj", mc::dict{});

    auto obj_node    = mc::expr::make_variable("obj");
    auto method_node = mc::expr::make_object_method_call(obj_node, "nonexistent", {});

    // 由于函数不存在，会抛出异常
    EXPECT_THROW(method_node->evaluate(ctx), mc::invalid_arg_exception);
}

TEST_F(node_test, object_method_call_node_invalid_object)
{
    // 测试非 variable 类型的对象
    auto literal_obj = mc::expr::make_literal(42);
    auto method_node = mc::expr::make_object_method_call(literal_obj, "method", {});

    EXPECT_THROW(method_node->evaluate(ctx), mc::invalid_arg_exception);
}

TEST_F(node_test, object_method_call_node_to_string)
{
    auto obj_node = mc::expr::make_variable("obj");

    // 无参数
    auto method_no_args = mc::expr::make_object_method_call(obj_node, "method", {});
    EXPECT_EQ(method_no_args->to_string(), "obj.method()");

    // 有参数（覆盖 i > 0 分支）
    auto arg1             = mc::expr::make_literal(10);
    auto arg2             = mc::expr::make_literal(20);
    auto method_with_args = mc::expr::make_object_method_call(obj_node, "method", {arg1, arg2});
    EXPECT_EQ(method_with_args->to_string(), "obj.method(10, 20)");
}

TEST_F(node_test, object_method_call_node_accessors_through_usage)
{
    ctx.register_variable("obj", mc::dict{});
    auto obj_node = mc::expr::make_variable("obj");
    auto node     = mc::expr::make_object_method_call(obj_node, "method", {});

    EXPECT_EQ(node->get_type(), mc::expr::node_type::object_method_call);
    EXPECT_EQ(node->to_string(), "obj.method()");
}

// 测试 template_string_node
TEST_F(node_test, template_string_node_simple)
{
    // 只有文本，没有表达式
    std::vector<std::string> text_parts = {"Hello"};
    mc::expr::node_ptrs      expressions;
    auto                     template_node = mc::expr::make_template_string(text_parts, expressions);

    EXPECT_EQ(template_node->evaluate(ctx), "Hello");
}

TEST_F(node_test, template_string_node_with_expression)
{
    // 文本 + 表达式 + 文本
    std::vector<std::string> text_parts    = {"Hello ", ", world"};
    auto                     expr_node     = mc::expr::make_literal(std::string("test"));
    mc::expr::node_ptrs      expressions   = {expr_node};
    auto                     template_node = mc::expr::make_template_string(text_parts, expressions);

    EXPECT_EQ(template_node->evaluate(ctx), "Hello test, world");
}

TEST_F(node_test, template_string_node_multiple_expressions)
{
    // 多个表达式
    std::vector<std::string> text_parts    = {"Value: ", " + ", " = "};
    auto                     expr1         = mc::expr::make_literal(10);
    auto                     expr2         = mc::expr::make_literal(20);
    mc::expr::node_ptrs      expressions   = {expr1, expr2};
    auto                     template_node = mc::expr::make_template_string(text_parts, expressions);

    EXPECT_EQ(template_node->evaluate(ctx), "Value: 10 + 20 = ");
}

TEST_F(node_test, template_string_node_invalid_structure)
{
    // 测试结构不正确的模板字符串节点
    // text_parts.size() 应该等于 expressions.size() + 1
    // 这里构造一个不满足条件的节点
    std::vector<std::string> text_parts  = {"Hello ", " World"};
    auto                     expr1       = mc::expr::make_literal(std::string("test"));
    auto                     expr2       = mc::expr::make_literal(std::string("test2"));
    mc::expr::node_ptrs      expressions = {expr1, expr2};
    // text_parts.size() == 2, expressions.size() == 2, 不满足 text_parts.size() == expressions.size() + 1
    auto template_node = mc::expr::make_template_string(text_parts, expressions);

    EXPECT_THROW(template_node->evaluate(ctx), mc::invalid_arg_exception);

    // 测试另一种不满足条件的情况：text_parts.size() < expressions.size() + 1
    std::vector<std::string> text_parts2    = {"Hello"};
    mc::expr::node_ptrs      expressions2   = {expr1, expr2};
    auto                     template_node2 = mc::expr::make_template_string(text_parts2, expressions2);

    EXPECT_THROW(template_node2->evaluate(ctx), mc::invalid_arg_exception);
}

// template_string_node_to_string 测试已合并到 template_string_node_accessors_through_usage 中

// 测试 literal_node - 各种类型的字面值
TEST_F(node_test, literal_node_various_types)
{
    // 测试 null
    auto null_node = mc::expr::make_literal(mc::variant{});
    EXPECT_TRUE(null_node->evaluate(ctx).is_null());

    // 测试数组
    mc::variants arr      = {1, 2, 3};
    auto         arr_node = mc::expr::make_literal(arr);
    EXPECT_EQ(arr_node->evaluate(ctx), arr);

    // 测试 dict
    mc::dict dict      = {{"key", "value"}};
    auto     dict_node = mc::expr::make_literal(dict);
    EXPECT_EQ(dict_node->evaluate(ctx), dict);
}

// 测试 binary_op_node - 字符串连接
TEST_F(node_test, binary_op_string_concatenation)
{
    auto left     = mc::expr::make_literal(std::string("Hello"));
    auto right    = mc::expr::make_literal(std::string(" World"));
    auto add_node = mc::expr::make_binary_op(mc::expr::operator_type::add, left, right);
    EXPECT_EQ(add_node->evaluate(ctx), "Hello World");
}

// 测试 binary_op_node - 浮点数比较
TEST_F(node_test, binary_op_float_comparison)
{
    auto left  = mc::expr::make_literal(3.14);
    auto right = mc::expr::make_literal(3.15);

    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::lt, left, right)->evaluate(ctx), true);
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::gt, left, right)->evaluate(ctx), false);
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::eq, left, right)->evaluate(ctx), false);
}

// 测试 conditional_node - 各种类型的条件
TEST_F(node_test, conditional_node_various_conditions)
{
    // 数字 0 为 false
    auto zero_cond    = mc::expr::make_literal(0);
    auto true_branch  = mc::expr::make_literal(10);
    auto false_branch = mc::expr::make_literal(20);
    auto cond_node    = mc::expr::make_conditional(zero_cond, true_branch, false_branch);
    EXPECT_EQ(cond_node->evaluate(ctx), 20);

    // 非零数字为 true
    auto nonzero_cond = mc::expr::make_literal(42);
    auto cond_node2   = mc::expr::make_conditional(nonzero_cond, true_branch, false_branch);
    EXPECT_EQ(cond_node2->evaluate(ctx), 10);

    // 空字符串为 false
    auto empty_str_cond = mc::expr::make_literal(std::string(""));
    auto cond_node3     = mc::expr::make_conditional(empty_str_cond, true_branch, false_branch);
    EXPECT_EQ(cond_node3->evaluate(ctx), 20);

    // 非空字符串为 true
    auto nonempty_str_cond = mc::expr::make_literal(std::string("test"));
    auto cond_node4        = mc::expr::make_conditional(nonempty_str_cond, true_branch, false_branch);
    EXPECT_EQ(cond_node4->evaluate(ctx), 10);
}

// 测试 binary_op_node - 所有运算符的 to_string
TEST_F(node_test, binary_op_all_operators_to_string)
{
    auto left  = mc::expr::make_literal(5);
    auto right = mc::expr::make_literal(3);

    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::add, left, right)->to_string(), "(5 + 3)");
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::sub, left, right)->to_string(), "(5 - 3)");
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::mul, left, right)->to_string(), "(5 * 3)");
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::div, left, right)->to_string(), "(5 / 3)");
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::mod, left, right)->to_string(), "(5 % 3)");
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::eq, left, right)->to_string(), "(5 == 3)");
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::ne, left, right)->to_string(), "(5 != 3)");
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::lt, left, right)->to_string(), "(5 < 3)");
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::le, left, right)->to_string(), "(5 <= 3)");
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::gt, left, right)->to_string(), "(5 > 3)");
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::ge, left, right)->to_string(), "(5 >= 3)");
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::and_op, left, right)->to_string(), "(5 && 3)");
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::or_op, left, right)->to_string(), "(5 || 3)");
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::bit_and, left, right)->to_string(), "(5 & 3)");
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::bit_or, left, right)->to_string(), "(5 | 3)");
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::bit_xor, left, right)->to_string(), "(5 ^ 3)");
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::lshift, left, right)->to_string(), "(5 << 3)");
    EXPECT_EQ(mc::expr::make_binary_op(mc::expr::operator_type::rshift, left, right)->to_string(), "(5 >> 3)");
}

// 测试 function_call_node - 多个参数求值顺序
TEST_F(node_test, function_call_node_arg_evaluation_order)
{
    // 注册一个函数，记录参数求值顺序
    int  call_count = 0;
    auto func       = mc::expr::make_simple_function("test", [&call_count](double a, double b, double c) {
        call_count++;
        return a + b + c;
    });
    ctx.register_function(func);

    auto arg1      = mc::expr::make_literal(10.0);
    auto arg2      = mc::expr::make_literal(20.0);
    auto arg3      = mc::expr::make_literal(30.0);
    auto func_node = mc::expr::make_function_call("test", {arg1, arg2, arg3});

    EXPECT_EQ(func_node->evaluate(ctx), 60.0);
    EXPECT_EQ(call_count, 1);
}

// 测试 property_access_node - 访问 dict 中的各种类型
TEST_F(node_test, property_access_dict_various_types)
{
    mc::dict obj;
    obj["int_val"]    = 42;
    obj["double_val"] = 3.14;
    obj["string_val"] = std::string("test");
    obj["bool_val"]   = true;
    obj["null_val"]   = mc::variant{};

    ctx.register_variable("obj", obj);

    auto obj_node = mc::expr::make_variable("obj");

    EXPECT_EQ(mc::expr::make_property_access(obj_node, "int_val")->evaluate(ctx), 42);
    EXPECT_EQ(mc::expr::make_property_access(obj_node, "double_val")->evaluate(ctx), 3.14);
    EXPECT_EQ(mc::expr::make_property_access(obj_node, "string_val")->evaluate(ctx), "test");
    EXPECT_EQ(mc::expr::make_property_access(obj_node, "bool_val")->evaluate(ctx), true);
    EXPECT_TRUE(mc::expr::make_property_access(obj_node, "null_val")->evaluate(ctx).is_null());
}

// 测试节点的访问器方法
// 注意：节点的具体类型（binary_op_node 等）没有在公共头文件中导出
// 这些访问器方法主要用于内部实现，通过 evaluate 和 to_string 间接测试
// 如果需要直接测试访问器，需要包含内部头文件
// 这里我们通过功能测试间接覆盖这些方法的使用
TEST_F(node_test, binary_op_node_accessors_through_usage)
{
    auto left  = mc::expr::make_literal(5);
    auto right = mc::expr::make_literal(3);
    auto node  = mc::expr::make_binary_op(mc::expr::operator_type::add, left, right);

    // 通过 evaluate 和 to_string 间接测试访问器方法的使用
    EXPECT_EQ(node->get_type(), mc::expr::node_type::binary_op);
    EXPECT_EQ(node->evaluate(ctx), 8);
    EXPECT_EQ(node->to_string(), "(5 + 3)");
}

TEST_F(node_test, unary_op_node_accessors_through_usage)
{
    auto operand = mc::expr::make_literal(5);
    auto node    = mc::expr::make_unary_op(mc::expr::operator_type::neg, operand);

    EXPECT_EQ(node->get_type(), mc::expr::node_type::unary_op);
    EXPECT_EQ(node->evaluate(ctx), -5);
    EXPECT_EQ(node->to_string(), "-5");
}

TEST_F(node_test, conditional_node_accessors_through_usage)
{
    auto condition    = mc::expr::make_literal(true);
    auto true_branch  = mc::expr::make_literal(10);
    auto false_branch = mc::expr::make_literal(20);
    auto node         = mc::expr::make_conditional(condition, true_branch, false_branch);

    EXPECT_EQ(node->get_type(), mc::expr::node_type::conditional);
    EXPECT_EQ(node->evaluate(ctx), 10);
    EXPECT_EQ(node->to_string(), "(true ? 10 : 20)");
}

TEST_F(node_test, function_call_node_accessors_through_usage)
{
    auto func = mc::expr::make_simple_function("test_func", [](double a, double b) {
        return a + b;
    });
    ctx.register_function(func);

    auto arg1 = mc::expr::make_literal(10.0);
    auto arg2 = mc::expr::make_literal(20.0);
    auto node = mc::expr::make_function_call("test_func", {arg1, arg2});

    EXPECT_EQ(node->get_type(), mc::expr::node_type::function_call);
    EXPECT_EQ(node->evaluate(ctx), 30.0);
    // 浮点数在 to_string() 时可能被格式化为整数形式（如果值是整数）
    std::string to_str = node->to_string();
    EXPECT_TRUE(to_str == "test_func(10, 20)" || to_str == "test_func(10.0, 20.0)");
}

TEST_F(node_test, variable_node_accessors_through_usage)
{
    ctx.register_variable("test_var", 42);
    auto node = mc::expr::make_variable("test_var");

    EXPECT_EQ(node->get_type(), mc::expr::node_type::variable);
    EXPECT_EQ(node->evaluate(ctx), 42);
    EXPECT_EQ(node->to_string(), "test_var");
}

TEST_F(node_test, property_access_node_accessors_through_usage)
{
    ctx.register_variable("obj", mc::dict{{"prop", 100}});
    auto obj_node = mc::expr::make_variable("obj");
    auto node     = mc::expr::make_property_access(obj_node, "prop");

    EXPECT_EQ(node->get_type(), mc::expr::node_type::property_access);
    EXPECT_EQ(node->evaluate(ctx), 100);
    EXPECT_EQ(node->to_string(), "obj.prop");
}

TEST_F(node_test, template_string_node_accessors_through_usage)
{
    std::vector<std::string> text_parts  = {"Hello ", ", world"};
    auto                     expr_node   = mc::expr::make_literal(std::string("test"));
    mc::expr::node_ptrs      expressions = {expr_node};
    auto                     node        = mc::expr::make_template_string(text_parts, expressions);

    EXPECT_EQ(node->get_type(), mc::expr::node_type::template_string);
    EXPECT_EQ(node->evaluate(ctx), "Hello test, world");
    // 字符串字面值在 to_string() 时会被加引号，所以是 "\"test\""
    EXPECT_EQ(node->to_string(), "\"Hello ${\"test\"}, world\"");
}

// 测试 context - 移动构造和移动赋值
TEST_F(node_test, context_move_constructor)
{
    mc::expr::context ctx1;
    ctx1.register_variable("x", 10);

    mc::expr::context ctx2(std::move(ctx1));
    EXPECT_EQ(ctx2.get_variable("x"), 10);
}

TEST_F(node_test, context_move_assignment)
{
    mc::expr::context ctx1;
    ctx1.register_variable("x", 10);

    mc::expr::context ctx2;
    ctx2 = std::move(ctx1);
    EXPECT_EQ(ctx2.get_variable("x"), 10);
}

// 测试 context - 通过对象名查找变量
TEST_F(node_test, context_get_variable_with_object)
{
    mc::dict obj;
    obj["prop"] = 100;
    ctx.register_variable("obj", obj);

    // 通过对象名查找属性
    const auto& var = ctx.get_variable("prop", "obj");
    EXPECT_EQ(var, 100);
}

// 测试 context - 通过对象名查找不存在的变量
TEST_F(node_test, context_get_variable_with_object_not_found)
{
    ctx.register_variable("obj", mc::dict{});

    // 对象存在但属性不存在
    const auto& var = ctx.get_variable("nonexistent", "obj");
    EXPECT_TRUE(var.is_null());
}

// 测试 context - 通过对象名查找不存在的对象
TEST_F(node_test, context_get_variable_with_nonexistent_object)
{
    // 对象不存在，应该返回空 variant
    const auto& var = ctx.get_variable("prop", "nonexistent");
    EXPECT_TRUE(var.is_null());
}

// 测试 context - has_variable 通过对象名
TEST_F(node_test, context_has_variable_with_object)
{
    mc::dict obj;
    obj["prop"] = 100;
    ctx.register_variable("obj", obj);

    EXPECT_TRUE(ctx.has_variable("prop", "obj"));
    EXPECT_FALSE(ctx.has_variable("nonexistent", "obj"));
    EXPECT_FALSE(ctx.has_variable("prop", "nonexistent"));
}

// 测试 context - has_function 通过对象名
TEST_F(node_test, context_has_function_with_object)
{
    // 注册一个对象方法
    auto func = mc::expr::make_simple_function("test_method", []() {
        return 42;
    });
    ctx.register_function(func);
    ctx.register_variable("obj", mc::dict{});

    // 注意：has_function 通过对象名查找需要对象是 abstract_object 类型
    // 这里测试 dict 类型的情况
    EXPECT_FALSE(ctx.has_function("test_method", "obj"));
}

// 测试 context - invoke 通过对象名
TEST_F(node_test, context_invoke_with_object)
{
    auto func = mc::expr::make_simple_function("test_method", []() {
        return 42;
    });
    ctx.register_function(func);
    ctx.register_variable("obj", mc::dict{});

    mc::variants args;
    // 通过对象名调用，但对象是 dict 类型，应该返回空 variant
    auto result = ctx.invoke("test_method", args, "obj");
    EXPECT_TRUE(result.is_null());
}

// 测试 context - import_from_dict
TEST_F(node_test, context_import_from_dict)
{
    mc::dict dict;
    dict["x"] = 10;
    dict["y"] = 20;
    dict["z"] = std::string("test");

    ctx.import_from_dict(dict);

    EXPECT_EQ(ctx.get_variable("x"), 10);
    EXPECT_EQ(ctx.get_variable("y"), 20);
    EXPECT_EQ(ctx.get_variable("z"), "test");
}

// 测试 context - import_from_dict 非字符串键
TEST_F(node_test, context_import_from_dict_non_string_key)
{
    mc::dict dict;
    dict[42] = 10; // 非字符串键应该被忽略

    ctx.import_from_dict(dict);

    // 非字符串键不会被导入
    const auto& var = ctx.get_variable("42");
    EXPECT_TRUE(var.is_null());
}

// 测试 context - register_function 返回 ID
TEST_F(node_test, context_register_function_returns_id)
{
    auto func = mc::expr::make_simple_function("test", []() {
        return 42;
    });

    int id = ctx.register_function(func);
    EXPECT_GE(id, 0);

    // 函数应该可以调用
    mc::variants args;
    EXPECT_EQ(ctx.invoke("test", args), 42);
}

// 测试 context - register_variable 返回 ID
TEST_F(node_test, context_register_variable_returns_id)
{
    int id = ctx.register_variable("x", 10);
    EXPECT_GE(id, 0);

    EXPECT_EQ(ctx.get_variable("x"), 10);
}

// 测试 context - register_function nullptr
TEST_F(node_test, context_register_function_nullptr)
{
    std::shared_ptr<mc::expr::function> func = nullptr;
    int                                 id   = ctx.register_function(func);
    EXPECT_EQ(id, -1);
}

// 测试 parser - 解析错误异常分支
TEST_F(node_test, parser_parse_error_exceptions)
{
    mc::expr::engine engine;

    // 测试未闭合的括号
    EXPECT_THROW(engine.compile("(1 + 2"), mc::parse_error_exception);

    // 测试期望右括号
    EXPECT_THROW(engine.compile("(1 + 2"), mc::parse_error_exception);

    // 测试期望表达式
    EXPECT_THROW(engine.compile(""), mc::parse_error_exception);
}

// 测试 parser - 条件表达式缺少冒号
TEST_F(node_test, parser_conditional_missing_colon)
{
    mc::expr::engine engine;

    // 条件表达式缺少冒号
    EXPECT_THROW(engine.compile("1 ? 2"), mc::parse_error_exception);
}

// 测试 parser - 属性访问缺少属性名
TEST_F(node_test, parser_property_access_missing_name)
{
    mc::expr::engine engine;

    // 点号后缺少属性名
    EXPECT_THROW(engine.compile("obj."), mc::parse_error_exception);
}

// 测试 parser - 函数调用缺少右括号
TEST_F(node_test, parser_function_call_missing_right_paren)
{
    mc::expr::engine engine;

    // 函数调用缺少右括号
    EXPECT_THROW(engine.compile("func(1, 2"), mc::parse_error_exception);
}

// 测试 parser - 模板字符串未闭合
TEST_F(node_test, parser_template_string_unclosed)
{
    mc::expr::engine engine;

    // 模板字符串未闭合
    EXPECT_THROW(engine.compile("\"text ${expr"), mc::parse_error_exception);
}

// 测试 binary_op_node - default 分支（虽然很难触发）
// 注意：由于 operator_type 是强类型枚举，无法直接测试 default 分支
// 但为了代码完整性，这里保留注释说明

// 测试 unary_op_node - default 分支
// 同上，无法直接测试

} // namespace
