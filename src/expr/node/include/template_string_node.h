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
 * @file template_string_node.h
 * @brief 定义了表达式语法树的模板字符串节点
 */
#ifndef MC_EXPR_NODE_TEMPLATE_STRING_NODE_H
#define MC_EXPR_NODE_TEMPLATE_STRING_NODE_H

#include <mc/expr/node.h>
#include <string>
#include <vector>

namespace mc::expr {

/**
 * @brief 模板字符串节点 ("text ${expr} text")
 */
class template_string_node : public node {
public:
    template_string_node(std::vector<std::string> text_parts, node_ptrs expressions)
        : m_text_parts(std::move(text_parts)), m_expressions(std::move(expressions))
    {}

    node_type get_type() const override
    {
        return node_type::template_string;
    }

    mc::variant evaluate(const context_base& ctx) const override;

    std::string to_string() const override;

    const std::vector<std::string>& get_text_parts() const
    {
        return m_text_parts;
    }

    const node_ptrs& get_expressions() const
    {
        return m_expressions;
    }

private:
    std::vector<std::string> m_text_parts;  // 文本部分
    node_ptrs                m_expressions; // 表达式部分
};

} // namespace mc::expr

#endif // MC_EXPR_NODE_TEMPLATE_STRING_NODE_H