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
 * @file conditional_node.h
 * @brief 定义了表达式语法树的条件表达式节点
 */
#ifndef MC_EXPR_NODE_CONDITIONAL_NODE_H
#define MC_EXPR_NODE_CONDITIONAL_NODE_H

#include <mc/expr/node.h>

#include <memory>

namespace mc::expr {

/**
 * @brief 条件表达式节点 (? :)
 */
class conditional_node : public node {
public:
    conditional_node(node_ptr condition, node_ptr true_branch, node_ptr false_branch)
        : m_condition(std::move(condition)), m_true_branch(std::move(true_branch)),
          m_false_branch(std::move(false_branch)) {
    }

    node_type get_type() const override {
        return node_type::conditional;
    }

    mc::variant evaluate(const context_base& ctx) const override;

    std::string to_string() const override;

    const node& get_condition() const {
        return *m_condition;
    }

    const node& get_true_branch() const {
        return *m_true_branch;
    }

    const node& get_false_branch() const {
        return *m_false_branch;
    }

private:
    node_ptr m_condition;
    node_ptr m_true_branch;
    node_ptr m_false_branch;
};

} // namespace mc::expr

#endif // MC_EXPR_NODE_CONDITIONAL_NODE_H
