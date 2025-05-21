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

#include "expr/node/include/binary_op_node.h"

namespace mc::expr {

mc::variant binary_op_node::evaluate(const context_base& ctx) const {
    mc::variant left_val = m_left->evaluate(ctx);

    // 短路逻辑操作符
    if (m_operator == operator_type::and_op) {
        bool left_bool = left_val.as_bool();
        if (!left_bool) {
            return false;
        }
        return m_right->evaluate(ctx).as_bool();
    }

    if (m_operator == operator_type::or_op) {
        bool left_bool = left_val.as_bool();
        if (left_bool) {
            return true;
        }
        return m_right->evaluate(ctx).as_bool();
    }

    // 对于其它操作符，先计算右侧值
    mc::variant right_val = m_right->evaluate(ctx);
    switch (m_operator) {
    case operator_type::add:
        return left_val + right_val;
    case operator_type::sub:
        return left_val - right_val;
    case operator_type::mul:
        return left_val * right_val;
    case operator_type::div:
        return left_val / right_val;
    case operator_type::mod:
        return left_val % right_val;
    case operator_type::eq:
        return left_val == right_val;
    case operator_type::ne:
        return left_val != right_val;
    case operator_type::lt:
        return left_val < right_val;
    case operator_type::le:
        return left_val <= right_val;
    case operator_type::gt:
        return left_val > right_val;
    case operator_type::ge:
        return left_val >= right_val;
    case operator_type::bit_and:
        return left_val & right_val;
    case operator_type::bit_or:
        return left_val | right_val;
    case operator_type::bit_xor:
        return left_val ^ right_val;
    case operator_type::lshift:
        return left_val << right_val;
    case operator_type::rshift:
        return left_val >> right_val;
    default:
        MC_THROW(invalid_arg_exception, "表达式求值错误: 不支持的二元操作符");
    }
}

std::string binary_op_node::to_string() const {
    std::string op_str;
    switch (m_operator) {
    case operator_type::add:
        op_str = "+";
        break;
    case operator_type::sub:
        op_str = "-";
        break;
    case operator_type::mul:
        op_str = "*";
        break;
    case operator_type::div:
        op_str = "/";
        break;
    case operator_type::mod:
        op_str = "%";
        break;
    case operator_type::eq:
        op_str = "==";
        break;
    case operator_type::ne:
        op_str = "!=";
        break;
    case operator_type::lt:
        op_str = "<";
        break;
    case operator_type::le:
        op_str = "<=";
        break;
    case operator_type::gt:
        op_str = ">";
        break;
    case operator_type::ge:
        op_str = ">=";
        break;
    case operator_type::and_op:
        op_str = "&&";
        break;
    case operator_type::or_op:
        op_str = "||";
        break;
    case operator_type::bit_and:
        op_str = "&";
        break;
    case operator_type::bit_or:
        op_str = "|";
        break;
    case operator_type::bit_xor:
        op_str = "^";
        break;
    case operator_type::lshift:
        op_str = "<<";
        break;
    case operator_type::rshift:
        op_str = ">>";
        break;
    default:
        op_str = "?";
        break;
    }
    return "(" + m_left->to_string() + " " + op_str + " " + m_right->to_string() + ")";
}

node_ptr make_binary_op(operator_type op, node_ptr left, node_ptr right) {
    return std::make_shared<binary_op_node>(op, std::move(left), std::move(right));
}

} // namespace mc::expr