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

#ifndef MC_EXPR_NODE_H
#define MC_EXPR_NODE_H

#include <mc/variant.h>

#include <memory>
#include <string>
#include <vector>

namespace mc::expr {
class context_base;

/**
 * @brief 表达式节点类型
 */
enum class node_type {
    literal,            // 字面值（数字、字符串等）
    variable,           // 变量引用
    unary_op,           // 一元操作
    binary_op,          // 二元操作
    function_call,      // 函数调用
    conditional,        // 条件表达式 (? :)
    property_access,    // 属性访问 (obj.property)
    object_method_call, // 对象方法调用 (obj.method(args))
    template_string     // 模板字符串 ("text ${expr} text")
};

/**
 * @brief 操作符类型
 */
enum class operator_type {
    // 算术运算符
    add, // +
    sub, // -
    mul, // *
    div, // /
    mod, // %
    neg, // - (一元)

    // 比较运算符
    eq, // ==
    ne, // !=
    lt, // <
    le, // <=
    gt, // >
    ge, // >=

    // 逻辑运算符
    and_op, // &&
    or_op,  // ||
    not_op, // !

    // 位操作运算符
    bit_and, // &
    bit_or,  // |
    bit_xor, // ^
    bit_not, // ~ (一元)
    lshift,  // <<
    rshift   // >>
};

/**
 * @brief 表达式节点基类
 */
class node {
public:
    virtual ~node() = default;

    /**
     * @brief 获取节点类型
     */
    virtual node_type get_type() const = 0;

    /**
     * @brief 求值表达式节点
     * @param ctx 表达式上下文
     * @return 表达式计算结果
     */
    virtual mc::variant evaluate(const context_base& ctx) const = 0;

    /**
     * @brief 将节点转换为字符串
     * @return 节点字符串表示
     */
    virtual std::string to_string() const = 0;
};

using node_ptr  = std::shared_ptr<node>;
using node_ptrs = std::vector<node_ptr>;

node_ptr make_literal(mc::variant value);
node_ptr make_variable(const std::string& name);
node_ptr make_unary_op(operator_type op, node_ptr operand);
node_ptr make_binary_op(operator_type op, node_ptr left, node_ptr right);
node_ptr make_function_call(const std::string& name, node_ptrs args);
node_ptr make_conditional(node_ptr condition, node_ptr true_branch, node_ptr false_branch);
node_ptr make_property_access(node_ptr object, const std::string& property);
node_ptr make_object_method_call(node_ptr object, const std::string& method_name, node_ptrs args);
node_ptr make_template_string(std::vector<std::string> text_parts, node_ptrs expressions);

} // namespace mc::expr

#endif // MC_EXPR_NODE_H