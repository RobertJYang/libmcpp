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

#include <iomanip>
#include <mc/expr/context.h>
#include <mc/expr/node.h>
#include <sstream>

#include "expr/node/include/template_string_node.h"

namespace mc::expr {

// 模板字符串节点求值
mc::variant template_string_node::evaluate(const context_base& ctx) const
{
    // 确保文本部分比表达式部分多1个（文本、表达式、文本、表达式...文本）
    if (m_text_parts.size() != m_expressions.size() + 1) {
        MC_THROW(invalid_arg_exception, "表达式求值错误: 模板字符串结构不正确");
    }

    mc::variant result(m_text_parts[0]);
    for (size_t i = 0; i < m_expressions.size(); ++i) {
        mc::variant expr_val = m_expressions[i]->evaluate(ctx);
        result += expr_val;
        result += m_text_parts[i + 1];
    }

    return result;
}

std::string template_string_node::to_string() const
{
    std::string result = "\"" + m_text_parts[0];

    for (size_t i = 0; i < m_expressions.size(); ++i) {
        result += "${" + m_expressions[i]->to_string() + "}";
        result += m_text_parts[i + 1];
    }

    result += "\"";
    return result;
}

node_ptr make_template_string(std::vector<std::string> text_parts, node_ptrs expressions)
{
    return std::make_shared<template_string_node>(std::move(text_parts), std::move(expressions));
}

} // namespace mc::expr