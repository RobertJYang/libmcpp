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
 * @file unary_op_node.h
 * @brief 定义了表达式语法树的一元操作节点
 */
#ifndef MC_EXPR_NODE_UNARY_OP_NODE_H
#define MC_EXPR_NODE_UNARY_OP_NODE_H

#include <mc/expr/node.h>

#include <memory>

namespace mc::expr {

/**
 * @brief 一元操作节点
 */
class unary_op_node : public node {
public:
    unary_op_node(operator_type op, node_ptr operand) : m_operator(op), m_operand(std::move(operand))
    {}

    node_type get_type() const override
    {
        return node_type::unary_op;
    }

    mc::variant evaluate(const context_base& ctx) const override;

    mc::string to_string() const override;

    operator_type get_operator() const
    {
        return m_operator;
    }

    const node& get_operand() const
    {
        return *m_operand;
    }

private:
    operator_type m_operator;
    node_ptr      m_operand;
};

} // namespace mc::expr

#endif // MC_EXPR_NODE_UNARY_OP_NODE_H