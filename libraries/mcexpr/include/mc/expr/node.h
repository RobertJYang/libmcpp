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

#include <mc/array.h>
#include <mc/memory.h>
#include <mc/string.h>
#include <mc/variant.h>
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
class node : public mc::shared_base {
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
    virtual mc::string to_string() const = 0;
};

using node_ptr  = mc::shared_ptr<node>;

class node_list {
public:
    using storage_type = std::vector<node_ptr>;
    using value_type = storage_type::value_type;
    using iterator = storage_type::iterator;
    using const_iterator = storage_type::const_iterator;

    node_list() = default;
    node_list(std::initializer_list<value_type> init) : m_nodes(init)
    {}

    bool empty() const noexcept
    {
        return m_nodes.empty();
    }

    std::size_t size() const noexcept
    {
        return m_nodes.size();
    }

    void reserve(std::size_t count)
    {
        m_nodes.reserve(count);
    }

    void push_back(value_type value)
    {
        m_nodes.push_back(std::move(value));
    }

    value_type& operator[](std::size_t index)
    {
        return m_nodes[index];
    }

    const value_type& operator[](std::size_t index) const
    {
        return m_nodes[index];
    }

    iterator begin() noexcept
    {
        return m_nodes.begin();
    }

    const_iterator begin() const noexcept
    {
        return m_nodes.begin();
    }

    iterator end() noexcept
    {
        return m_nodes.end();
    }

    const_iterator end() const noexcept
    {
        return m_nodes.end();
    }

private:
    storage_type m_nodes;
};

using node_ptrs = node_list;

node_ptr make_literal(mc::variant value);
node_ptr make_variable(mc::string name);
node_ptr make_unary_op(operator_type op, node_ptr operand);
node_ptr make_binary_op(operator_type op, node_ptr left, node_ptr right);
node_ptr make_function_call(mc::string name, node_ptrs args);
node_ptr make_conditional(node_ptr condition, node_ptr true_branch, node_ptr false_branch);
node_ptr make_property_access(node_ptr object, mc::string property);
node_ptr make_object_method_call(node_ptr object, mc::string method_name, node_ptrs args);
node_ptr make_template_string(mc::array<mc::string> text_parts, node_ptrs expressions);

} // namespace mc::expr

#endif // MC_EXPR_NODE_H