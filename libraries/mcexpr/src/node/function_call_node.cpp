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

#include <mc/exception.h>
#include <mc/expr/context.h>

#include "node/include/function_call_node.h"

namespace mc::expr {

mc::variant function_call_node::evaluate(const context_base& ctx) const
{
    if (!ctx.has_function(m_name)) {
        MC_THROW(invalid_arg_exception, "表达式求值错误: 未定义的函数 '${name}'", ("name", m_name));
    }

    mc::variants args;
    args.reserve(m_args.size());
    for (const auto& arg_node : m_args) {
        args.emplace_back(arg_node->evaluate(ctx));
    }

    return ctx.invoke(m_name, args);
}

mc::string function_call_node::to_string() const
{
    mc::string result = m_name + "(";
    for (size_t i = 0; i < m_args.size(); ++i) {
        if (i > 0) {
            result += ", ";
        }
        result += m_args[i]->to_string();
    }
    result += ")";
    return result;
}

node_ptr make_function_call(mc::string name, node_ptrs args)
{
    return mc::make_shared<function_call_node>(std::move(name), std::move(args));
}

} // namespace mc::expr
