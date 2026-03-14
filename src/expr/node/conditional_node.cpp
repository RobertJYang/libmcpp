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

#include <mc/expr/context.h>
#include <mc/expr/node.h>

#include "expr/node/include/conditional_node.h"

namespace mc::expr {

mc::variant conditional_node::evaluate(const context_base& ctx) const
{
    mc::variant condition_val = m_condition->evaluate(ctx);
    if (condition_val.as_bool()) {
        return m_true_branch->evaluate(ctx);
    } else {
        return m_false_branch->evaluate(ctx);
    }
}

std::string conditional_node::to_string() const
{
    return "(" + m_condition->to_string() + " ? " + m_true_branch->to_string() + " : " + m_false_branch->to_string() +
           ")";
}

node_ptr make_conditional(node_ptr condition, node_ptr true_branch, node_ptr false_branch)
{
    return std::make_shared<conditional_node>(std::move(condition), std::move(true_branch), std::move(false_branch));
}

} // namespace mc::expr
