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

#include "expr/node/include/property_access_node.h"
#include "expr/node/include/variable_node.h"

namespace mc::expr {

mc::variant property_access_node::evaluate(const context_base& ctx) const {
    if (m_object->get_type() == node_type::variable) {
        const variable_node& var_node = static_cast<const variable_node&>(*m_object);
        if (ctx.has_variable(m_property, var_node.get_name())) {
            return ctx.get_variable(m_property, var_node.get_name());
        }
    }

    MC_THROW(invalid_arg_exception, "表达式求值错误: 无法访问属性 '${prop}'", ("prop", m_property));
}

std::string property_access_node::to_string() const {
    return m_object->to_string() + "." + m_property;
}

node_ptr make_property_access(node_ptr object, const std::string& property) {
    return std::make_shared<property_access_node>(std::move(object), property);
}

} // namespace mc::expr
