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

#include "expr/node/include/unary_op_node.h"

namespace mc::expr {

mc::variant unary_op_node::evaluate(const context_base& ctx) const {
    mc::variant operand_val = m_operand->evaluate(ctx);

    switch (m_operator) {
    case operator_type::neg:
        return -operand_val;
    case operator_type::not_op:
        return !operand_val;
    case operator_type::bit_not:
        return ~operand_val;
    default:
        MC_THROW(invalid_arg_exception, "表达式求值错误: 不支持的一元操作符");
    }
}

std::string unary_op_node::to_string() const {
    std::string op_str;
    switch (m_operator) {
    case operator_type::neg:
        op_str = "-";
        break;
    case operator_type::not_op:
        op_str = "!";
        break;
    case operator_type::bit_not:
        op_str = "~";
        break;
    default:
        op_str = "?";
        break;
    }
    return op_str + m_operand->to_string();
}

node_ptr make_unary_op(operator_type op, node_ptr operand) {
    return std::make_shared<unary_op_node>(op, std::move(operand));
}

} // namespace mc::expr