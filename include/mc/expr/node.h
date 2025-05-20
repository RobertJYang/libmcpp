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
 * @file node.h
 * @brief 定义了表达式语法树的节点类型
 */
#ifndef MC_EXPR_NODE_H
#define MC_EXPR_NODE_H

#include <mc/variant.h>
#include <memory>
#include <vector>

namespace mc::expr {

// 前置声明
class context_base;

/**
 * @brief 表达式节点类型
 */
enum class node_type {
    literal,        // 字面值（数字、字符串等）
    variable,       // 变量引用
    unary_op,       // 一元操作
    binary_op,      // 二元操作
    function_call,  // 函数调用
    conditional,    // 条件表达式 (? :)
    property_access // 属性访问 (obj.property)
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
};

/**
 * @brief 字面值节点
 */
class literal_node : public node {
public:
    explicit literal_node(mc::variant value) : m_value(std::move(value)) {
    }

    node_type get_type() const override {
        return node_type::literal;
    }

    mc::variant evaluate(const context_base& ctx) const override;

private:
    mc::variant m_value;
};

/**
 * @brief 变量引用节点
 */
class variable_node : public node {
public:
    explicit variable_node(std::string name) : m_name(std::move(name)) {
    }

    node_type get_type() const override {
        return node_type::variable;
    }

    mc::variant evaluate(const context_base& ctx) const override;

    const std::string& get_name() const {
        return m_name;
    }

private:
    std::string m_name;
};

/**
 * @brief 一元操作节点
 */
class unary_op_node : public node {
public:
    unary_op_node(operator_type op, std::shared_ptr<node> operand)
        : m_operator(op), m_operand(std::move(operand)) {
    }

    node_type get_type() const override {
        return node_type::unary_op;
    }

    mc::variant evaluate(const context_base& ctx) const override;

    operator_type get_operator() const {
        return m_operator;
    }

    const node& get_operand() const {
        return *m_operand;
    }

private:
    operator_type         m_operator;
    std::shared_ptr<node> m_operand;
};

/**
 * @brief 二元操作节点
 */
class binary_op_node : public node {
public:
    binary_op_node(operator_type op, std::shared_ptr<node> left, std::shared_ptr<node> right)
        : m_operator(op), m_left(std::move(left)), m_right(std::move(right)) {
    }

    node_type get_type() const override {
        return node_type::binary_op;
    }

    mc::variant evaluate(const context_base& ctx) const override;

    operator_type get_operator() const {
        return m_operator;
    }

    const node& get_left() const {
        return *m_left;
    }

    const node& get_right() const {
        return *m_right;
    }

private:
    operator_type         m_operator;
    std::shared_ptr<node> m_left;
    std::shared_ptr<node> m_right;
};

/**
 * @brief 函数调用节点
 */
class function_call_node : public node {
public:
    function_call_node(std::string name, std::vector<std::shared_ptr<node>> args)
        : m_name(std::move(name)), m_args(std::move(args)) {
    }

    node_type get_type() const override {
        return node_type::function_call;
    }

    mc::variant evaluate(const context_base& ctx) const override;

    const std::string& get_name() const {
        return m_name;
    }

    const std::vector<std::shared_ptr<node>>& get_args() const {
        return m_args;
    }

private:
    std::string                        m_name;
    std::vector<std::shared_ptr<node>> m_args;
};

/**
 * @brief 属性访问节点
 */
class property_access_node : public node {
public:
    property_access_node(std::shared_ptr<node> object, std::string property)
        : m_object(std::move(object)), m_property(std::move(property)) {
    }

    node_type get_type() const override {
        return node_type::property_access;
    }

    mc::variant evaluate(const context_base& ctx) const override;

    const node& get_object() const {
        return *m_object;
    }

    const std::string& get_property() const {
        return m_property;
    }

private:
    std::shared_ptr<node> m_object;
    std::string           m_property;
};

// 节点类型辅助函数
std::shared_ptr<node> make_literal(mc::variant value);
std::shared_ptr<node> make_variable(const std::string& name);
std::shared_ptr<node> make_unary_op(operator_type op, std::shared_ptr<node> operand);
std::shared_ptr<node> make_binary_op(operator_type op, std::shared_ptr<node> left,
                                     std::shared_ptr<node> right);
std::shared_ptr<node> make_function_call(const std::string&                 name,
                                         std::vector<std::shared_ptr<node>> args);
std::shared_ptr<node> make_property_access(std::shared_ptr<node> object,
                                           const std::string&    property);

} // namespace mc::expr

#endif // MC_EXPR_NODE_H