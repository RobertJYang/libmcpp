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

#include <cmath>
#include <mc/exception.h>
#include <mc/expr/context.h>
#include <mc/expr/function.h>
#include <mc/expr/node.h>

namespace mc::expr {

// 字面值节点求值
mc::variant literal_node::evaluate(const context& ctx) const {
    return m_value;
}

// 变量节点求值
mc::variant variable_node::evaluate(const context& ctx) const {
    if (!ctx.has_variable(m_name)) {
        MC_THROW(invalid_arg_exception, "表达式求值错误: 未定义的变量 '${name}'", ("name", m_name));
    }
    return ctx.get_variable(m_name);
}

// 一元操作节点求值
mc::variant unary_op_node::evaluate(const context& ctx) const {
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

// 二元操作节点求值
mc::variant binary_op_node::evaluate(const context& ctx) const {
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

// 函数调用节点求值
mc::variant function_call_node::evaluate(const context& ctx) const {
    if (!ctx.has_function(m_name)) {
        MC_THROW(invalid_arg_exception, "表达式求值错误: 未定义的函数 '${name}'", ("name", m_name));
    }

    std::shared_ptr<function> func = ctx.get_function(m_name);

    mc::variants args;
    args.reserve(m_args.size());
    for (const auto& arg_node : m_args) {
        args.emplace_back(arg_node->evaluate(ctx));
    }

    return func->call(args);
}

// 辅助函数实现
std::shared_ptr<node> make_literal(mc::variant value) {
    return std::make_shared<literal_node>(std::move(value));
}

std::shared_ptr<node> make_variable(const std::string& name) {
    return std::make_shared<variable_node>(name);
}

std::shared_ptr<node> make_unary_op(operator_type op, std::shared_ptr<node> operand) {
    return std::make_shared<unary_op_node>(op, std::move(operand));
}

std::shared_ptr<node> make_binary_op(operator_type op, std::shared_ptr<node> left,
                                     std::shared_ptr<node> right) {
    return std::make_shared<binary_op_node>(op, std::move(left), std::move(right));
}

std::shared_ptr<node> make_function_call(const std::string&                 name,
                                         std::vector<std::shared_ptr<node>> args) {
    return std::make_shared<function_call_node>(name, std::move(args));
}

} // namespace mc::expr