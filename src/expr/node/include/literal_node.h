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
 * @file literal_node.h
 * @brief 定义了表达式语法树的字面值节点
 */
#ifndef MC_EXPR_NODE_LITERAL_NODE_H
#define MC_EXPR_NODE_LITERAL_NODE_H

#include <mc/expr/node.h>

#include <mc/variant.h>

namespace mc::expr {

/**
 * @brief 字面值节点
 */
class literal_node : public node {
public:
    explicit literal_node(mc::variant value) : m_value(std::move(value))
    {}

    node_type get_type() const override
    {
        return node_type::literal;
    }

    mc::variant evaluate(const context_base& ctx) const override;

    std::string to_string() const override;

private:
    mc::variant m_value;
};

} // namespace mc::expr

#endif // MC_EXPR_NODE_LITERAL_NODE_H