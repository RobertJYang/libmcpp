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

#include "node/include/variable_node.h"

namespace mc::expr {

mc::variant variable_node::evaluate(const context_base& ctx) const
{
    if (!ctx.has_variable(m_name)) {
        MC_THROW(invalid_arg_exception, "表达式求值错误: 未定义的变量 '${name}'", ("name", m_name));
    }
    return ctx.get_variable(m_name);
}

mc::string variable_node::to_string() const
{
    return m_name;
}

node_ptr make_variable(mc::string name)
{
    return mc::make_shared<variable_node>(std::move(name));
}

} // namespace mc::expr