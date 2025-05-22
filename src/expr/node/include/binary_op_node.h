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
 * @file binary_op_node.h
 * @brief 定义了表达式语法树的二元操作节点
 */
#ifndef MC_EXPR_NODE_BINARY_OP_NODE_H
#define MC_EXPR_NODE_BINARY_OP_NODE_H

#include <mc/expr/node.h>
#include <memory>

namespace mc::expr {

/**
 * @brief 二元操作节点
 */
class binary_op_node : public node {
public:
    binary_op_node(operator_type op, node_ptr left, node_ptr right)
        : m_operator(op), m_left(std::move(left)), m_right(std::move(right)) {
    }

    node_type get_type() const override {
        return node_type::binary_op;
    }

    mc::variant evaluate(const context_base& ctx) const override;

    std::string to_string() const override;

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
    operator_type m_operator;
    node_ptr      m_left;
    node_ptr      m_right;
};

} // namespace mc::expr

#endif // MC_EXPR_NODE_BINARY_OP_NODE_H