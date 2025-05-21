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
#include <mc/expr/parser.h>

namespace mc::expr {

parser::parser(std::vector<token> tokens) : m_tokens(std::move(tokens)), m_current(0) {
}

bool parser::match(std::initializer_list<token_type> types) {
    for (auto type : types) {
        if (check(type)) {
            advance();
            return true;
        }
    }
    return false;
}

bool parser::check(token_type type) const {
    if (is_at_end()) {
        return false;
    }
    return peek().type == type;
}

const token& parser::advance() {
    if (!is_at_end()) {
        m_current++;
    }
    return previous();
}

const token& parser::peek() const {
    return m_tokens[m_current];
}

const token& parser::previous() const {
    return m_tokens[m_current - 1];
}

bool parser::is_at_end() const {
    return peek().type == token_type::end_of_file;
}

node_ptr parser::expression() {
    return conditional();
}

// 解析条件表达式 (? :)
node_ptr parser::conditional() {
    auto expr = logical_or();

    // 如果遇到问号，则解析为条件表达式
    if (match({token_type::question})) {
        // 解析真分支
        auto true_branch = logical_or();

        // 必须有冒号
        if (!match({token_type::colon})) {
            MC_THROW(parse_error_exception, "表达式解析错误: 期望冒号");
        }

        // 解析假分支
        auto false_branch = conditional();

        // 创建条件表达式节点
        expr = make_conditional(expr, true_branch, false_branch);
    }

    return expr;
}

// 解析逻辑或表达式
node_ptr parser::logical_or() {
    auto expr = logical_and();

    while (match({token_type::logical_or})) {
        auto right = logical_and();
        expr       = make_binary_op(operator_type::or_op, expr, right);
    }

    return expr;
}

// 解析逻辑与表达式
node_ptr parser::logical_and() {
    auto expr = bit_or();

    while (match({token_type::logical_and})) {
        auto right = bit_or();
        expr       = make_binary_op(operator_type::and_op, expr, right);
    }

    return expr;
}

// 解析位或表达式
node_ptr parser::bit_or() {
    auto expr = bit_xor();

    while (match({token_type::bit_or})) {
        auto right = bit_xor();
        expr       = make_binary_op(operator_type::bit_or, expr, right);
    }

    return expr;
}

// 解析位异或表达式
node_ptr parser::bit_xor() {
    auto expr = bit_and();

    while (match({token_type::bit_xor})) {
        auto right = bit_and();
        expr       = make_binary_op(operator_type::bit_xor, expr, right);
    }

    return expr;
}

// 解析位与表达式
node_ptr parser::bit_and() {
    auto expr = shift();

    while (match({token_type::bit_and})) {
        auto right = shift();
        expr       = make_binary_op(operator_type::bit_and, expr, right);
    }

    return expr;
}

// 解析位移表达式
node_ptr parser::shift() {
    auto expr = equality();

    while (match({token_type::lshift, token_type::rshift})) {
        operator_type op =
            previous().type == token_type::lshift ? operator_type::lshift : operator_type::rshift;
        auto right = equality();
        expr       = make_binary_op(op, expr, right);
    }

    return expr;
}

// 解析相等性表达式
node_ptr parser::equality() {
    auto expr = comparison();

    while (match({token_type::equals, token_type::not_equals})) {
        operator_type op =
            previous().type == token_type::equals ? operator_type::eq : operator_type::ne;
        auto right = comparison();
        expr       = make_binary_op(op, expr, right);
    }

    return expr;
}

// 解析比较表达式
node_ptr parser::comparison() {
    auto expr = term();

    while (match({token_type::less, token_type::less_equals, token_type::greater,
                  token_type::greater_equals})) {
        operator_type op;
        switch (previous().type) {
        case token_type::less:
            op = operator_type::lt;
            break;
        case token_type::less_equals:
            op = operator_type::le;
            break;
        case token_type::greater:
            op = operator_type::gt;
            break;
        case token_type::greater_equals:
            op = operator_type::ge;
            break;
        default:
            op = operator_type::eq;
            break; // 不应该到达这里
        }

        auto right = term();
        expr       = make_binary_op(op, expr, right);
    }

    return expr;
}

// 解析项表达式（加法和减法）
node_ptr parser::term() {
    auto expr = factor();

    while (match({token_type::plus, token_type::minus})) {
        operator_type op =
            previous().type == token_type::plus ? operator_type::add : operator_type::sub;
        auto right = factor();
        expr       = make_binary_op(op, expr, right);
    }

    return expr;
}

// 解析因子表达式（乘法、除法和求模）
node_ptr parser::factor() {
    auto expr = unary();

    while (match({token_type::asterisk, token_type::slash, token_type::percent})) {
        operator_type op;
        switch (previous().type) {
        case token_type::asterisk:
            op = operator_type::mul;
            break;
        case token_type::slash:
            op = operator_type::div;
            break;
        case token_type::percent:
            op = operator_type::mod;
            break;
        default:
            op = operator_type::mul;
            break; // 不应该到达这里
        }

        auto right = unary();
        expr       = make_binary_op(op, expr, right);
    }

    return expr;
}

// 解析一元表达式
node_ptr parser::unary() {
    if (match({token_type::minus, token_type::logical_not, token_type::bit_not})) {
        operator_type op;
        switch (previous().type) {
        case token_type::minus:
            op = operator_type::neg;
            break;
        case token_type::logical_not:
            op = operator_type::not_op;
            break;
        case token_type::bit_not:
            op = operator_type::bit_not;
            break;
        default:
            op = operator_type::neg; // 不应该到达这里
            break;
        }
        auto right = unary();
        return make_unary_op(op, right);
    }

    return primary();
}

// 解析标识符（变量、函数调用或属性访问）
node_ptr parser::parse_identifier() {
    std::string identifier = previous().lexeme;

    // 如果后面跟着左括号，则是函数调用
    if (check(token_type::left_paren)) {
        return function_call();
    }

    // 如果后面跟着点号，则是属性访问或对象方法调用
    if (check(token_type::dot)) {
        return parse_property_access(make_variable(identifier));
    }

    // 否则是变量引用
    return make_variable(identifier);
}

// 解析对象属性访问和方法调用
node_ptr parser::parse_property_access(node_ptr object) {
    node_ptr expr = object;

    // 处理链式属性访问，如 obj.prop1.prop2
    while (match({token_type::dot})) {
        if (!match({token_type::identifier})) {
            MC_THROW(parse_error_exception, "表达式解析错误: 点号后期望属性名");
        }
        std::string property = previous().lexeme;

        // 检查是否是对象方法调用 (obj.method(...))
        if (check(token_type::left_paren)) {
            expr = parse_method_call(expr, property);
        } else {
            // 否则是普通属性访问
            expr = make_property_access(expr, property);
        }

        // 如果属性后面还有点号，继续处理链式访问
        if (!check(token_type::dot)) {
            break;
        }
    }

    return expr;
}

// 解析对象方法调用
node_ptr parser::parse_method_call(node_ptr object, const std::string& method_name) {
    // 匹配左括号
    match({token_type::left_paren});

    node_ptrs arguments;

    // 解析参数列表
    if (!check(token_type::right_paren)) {
        do {
            arguments.push_back(expression());
        } while (match({token_type::comma}));
    }

    // 匹配右括号
    if (!match({token_type::right_paren})) {
        MC_THROW(parse_error_exception, "表达式解析错误: 期望右括号");
    }

    // 创建对象方法调用节点
    return make_object_method_call(object, method_name, std::move(arguments));
}

// 解析基本表达式
node_ptr parser::primary() {
    // 数字字面值
    if (match({token_type::number})) {
        return make_literal(previous().literal);
    }

    // 字符串字面值
    if (match({token_type::string})) {
        return make_literal(previous().literal);
    }

    // 标识符（变量、函数调用或属性访问）
    if (match({token_type::identifier})) {
        return parse_identifier();
    }

    // 带括号的表达式
    if (match({token_type::left_paren})) {
        auto expr = expression();

        if (!match({token_type::right_paren})) {
            MC_THROW(parse_error_exception, "表达式解析错误: 期望右括号");
        }

        return expr;
    }

    MC_THROW(parse_error_exception, "表达式解析错误: 期望表达式");
}

// 解析函数调用
node_ptr parser::function_call() {
    std::string function_name = previous().lexeme;

    // 匹配左括号
    if (!match({token_type::left_paren})) {
        MC_THROW(parse_error_exception, "表达式解析错误: 期望左括号");
    }

    node_ptrs arguments;

    // 解析参数列表
    if (!check(token_type::right_paren)) {
        do {
            arguments.push_back(expression());
        } while (match({token_type::comma}));
    }

    // 匹配右括号
    if (!match({token_type::right_paren})) {
        MC_THROW(parse_error_exception, "表达式解析错误: 期望右括号");
    }

    return make_function_call(function_name, std::move(arguments));
}

// 语法分析入口函数
node_ptr parser::parse() {
    try {
        return expression();
    } catch (const parse_error_exception&) {
        throw;
    } catch (const std::exception& e) {
        MC_THROW(parse_error_exception, "表达式解析错误: ${message}", ("message", e.what()));
    }
}

} // namespace mc::expr