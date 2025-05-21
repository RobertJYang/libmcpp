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

#include "expr/node/include/object_method_call_node.h"
#include "expr/node/include/variable_node.h"

namespace mc::expr {

mc::variant object_method_call_node::evaluate(const context_base& ctx) const {
    if (m_object->get_type() != node_type::variable) {
        MC_THROW(invalid_arg_exception, "表达式求值错误: 无法调用对象的方法 '${obj}.${method}'",
                 ("obj", m_object->to_string())("method", m_method_name));
    }

    const variable_node& var_node = static_cast<const variable_node&>(*m_object);
    if (!ctx.has_function(m_method_name, var_node.get_name())) {
        MC_THROW(invalid_arg_exception, "表达式求值错误: 无法调用对象的方法 '${obj}.${method}'",
                 ("obj", var_node.get_name())("method", m_method_name));
    }

    mc::variants args;
    args.reserve(m_args.size());
    for (const auto& arg_node : m_args) {
        args.emplace_back(arg_node->evaluate(ctx));
    }

    return ctx.invoke(m_method_name, args, var_node.get_name());
}

std::string object_method_call_node::to_string() const {
    std::string result = m_object->to_string() + "." + m_method_name + "(";
    for (size_t i = 0; i < m_args.size(); ++i) {
        if (i > 0) {
            result += ", ";
        }
        result += m_args[i]->to_string();
    }
    result += ")";
    return result;
}

node_ptr make_object_method_call(node_ptr object, const std::string& method_name, node_ptrs args) {
    return std::make_shared<object_method_call_node>(std::move(object), method_name,
                                                     std::move(args));
}

} // namespace mc::expr
