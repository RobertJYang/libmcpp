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
#include <mc/expr/lexer.h>
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
    if (m_current >= m_tokens.size()) {
        MC_THROW(parse_error_exception, "表达式解析错误: 解析器索引越界");
    }
    return m_tokens[m_current];
}

const token& parser::previous() const {
    if (m_current <= 0 || m_current > m_tokens.size()) {
        MC_THROW(parse_error_exception, "表达式解析错误: 解析器索引越界");
    }
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
// 示例：解析表达式 -a * b + c
// 解析过程：
// 1. term() 调用 factor() 解析 -a * b
// 2. factor() 调用 unary() 解析 -a
// 3. unary() 解析 -a 形成AST: -a
// 4. factor() 解析 * b 形成AST: (-a) * b
// 5. term() 解析 + c 形成AST: ((-a) * b) + c
//
// AST结构：
//        +
//       / \
//      *   c
//     / \
//    -   b
//    |
//    a
node_ptr parser::term() {
    // 1. 声明并初始化变量
    auto expr = factor();  // auto是C++的类型推导关键字，编译器会自动推导expr的类型

    // 2. while循环处理连续的加减运算
    while (match({token_type::plus, token_type::minus})) {
        // 3. 三元运算符：condition ? value_if_true : value_if_false
        operator_type op =
            previous().type == token_type::plus ? operator_type::add : operator_type::sub;
        
        // 4. 解析右操作数
        auto right = factor();
        
        // 5. 创建二元运算符节点
        expr = make_binary_op(op, expr, right);
    }

    return expr;
}

// 解析因子表达式（乘法、除法和求模），优先级高于加减
node_ptr parser::factor() {
    // 1. 首先解析一元表达式
    auto expr = unary();

    // 2. 循环处理连续的乘除和求模运算
    while (match({token_type::asterisk, token_type::slash, token_type::percent})) {
        // 3. 根据运算符类型确定操作类型
        operator_type op;
        switch (previous().type) {
        case token_type::asterisk:  // *
            op = operator_type::mul;
            break;
        case token_type::slash:     // /
            op = operator_type::div;
            break;
        case token_type::percent:   // %
            op = operator_type::mod;
            break;
        default:
            op = operator_type::mul;
            break; // 不应该到达这里
        }

        // 4. 解析右操作数并创建二元运算节点
        auto right = unary();
        expr = make_binary_op(op, expr, right);
    }

    return expr;
}

// 解析一元表达式，优先级最高，一元运算符包括：-、!、~
// 例如：-a * b 会先解析 -a，再解析 * b
node_ptr parser::unary() {
    // 1. 检查是否是一元运算符
    if (match({token_type::minus, token_type::logical_not, token_type::bit_not})) {
        // 2. 根据运算符类型确定操作类型
        operator_type op;
        switch (previous().type) {
        case token_type::minus:         // -
            op = operator_type::neg;
            break;
        case token_type::logical_not:   // !
            op = operator_type::not_op;
            break;
        case token_type::bit_not:       // ~
            op = operator_type::bit_not;
            break;
        default:
            op = operator_type::neg;    // 不应该到达这里
            break;
        }
        // 3. 解析操作数并创建一元运算节点
        auto right = unary();
        return make_unary_op(op, right);
    }

    // 4. 如果不是一元运算符，则解析基本表达式
    return primary();
}

// 解析标识符（变量、函数调用或属性访问）
node_ptr parser::parse_identifier() {
    // 1. 获取标识符的名称
    std::string identifier = previous().lexeme;

    // 2. 根据后续token类型判断标识符的用途
    // 2.1 如果后面是左括号，说明是函数调用，比如 max(1, 2)
    if (check(token_type::left_paren)) {
        return function_call();
    }

    // 2.2 如果后面是点号，说明是属性访问或对象方法调用，比如 obj.prop1
    if (check(token_type::dot)) {
        return parse_property_access(make_variable(identifier));
    }

    // 2.3 如果都不是，说明是普通变量引用，比如 a
    return make_variable(identifier);
}

// 解析对象属性访问和方法调用
// 支持以下语法：
// 1. 属性访问：obj.name, user.address.city
// 2. 方法调用：obj.getValue(), user.getAddress().getCity()
node_ptr parser::parse_property_access(node_ptr object) {
    node_ptr expr = object;

    // 处理链式访问：obj.prop1.prop2.method()
    while (match({token_type::dot})) {
        // 检查点号后的标识符
        if (!match({token_type::identifier})) {
            MC_THROW(parse_error_exception, "表达式解析错误: 点号后期望属性名");
        }
        std::string property = previous().lexeme;

        // 根据是否有左括号区分方法调用和属性访问
        if (check(token_type::left_paren)) {
            expr = parse_method_call(expr, property);     // obj.method(...)
        } else {
            expr = make_property_access(expr, property);  // obj.property
        }

        // 检查是否继续链式访问
        if (!check(token_type::dot)) {
            break;
        }
    }

    return expr;
}

// 解析对象方法调用
// 语法：obj.method(arg1, arg2, ...)
// 示例：user.getName(), list.add(1, 2)
node_ptr parser::parse_method_call(node_ptr object, const std::string& method_name) {
    // 1. 匹配左括号 '('
    match({token_type::left_paren});

    // 2. 存储方法参数列表
    node_ptrs arguments;

    // 3. 解析参数列表：method(expr1, expr2, ...)
    if (!check(token_type::right_paren)) {  // 不是空参数列表
        do {
            arguments.push_back(expression());  // 解析每个参数表达式
        } while (match({token_type::comma}));  // 如果遇到逗号，继续解析下一个参数
    }

    // 4. 检查右括号 ')'
    if (!match({token_type::right_paren})) {
        MC_THROW(parse_error_exception, "表达式解析错误: 期望右括号");
    }

    // 5. 创建方法调用节点
    return make_object_method_call(object, method_name, std::move(arguments));
}

// 解析模板字符串
// 语法：`text ${expr1} text ${expr2} text`
// 示例：`Hello ${name}! Your age is ${age + 1}`
node_ptr parser::parse_template_string() {
    // 1. 存储模板字符串的文本部分和表达式部分
    std::vector<std::string> text_parts;   // 存储普通文本
    node_ptrs expressions;                 // 存储表达式节点

    // 2. 获取起始文本部分
    std::string start_text = previous().literal.as_string();
    text_parts.push_back(start_text);

    while (true) {
        // 3. 解析 ${...} 表达式部分
        if (!match({token_type::template_expr})) {
            MC_THROW(parse_error_exception, "表达式解析错误: 期望模板表达式");
        }

        // 4. 解析表达式内容
        std::string expr_text = previous().lexeme;
        // 为表达式创建新的解析器
        mc::expr::lexer expr_lexer(expr_text);
        auto tokens = expr_lexer.scan_tokens();
        parser expr_parser(tokens);
        expressions.push_back(expr_parser.parse());

        // 5. 解析表达式后的文本部分
        if (match({token_type::template_middle})) {
            // 还有更多部分要解析
            text_parts.push_back(previous().literal.as_string());
        } else if (match({token_type::template_end})) {
            // 到达模板字符串结尾
            text_parts.push_back(previous().literal.as_string());
            break;
        } else {
            MC_THROW(parse_error_exception, 
                "表达式解析错误: 期望模板字符串中间部分或结束部分");
        }
    }

    // 6. 创建模板字符串节点
    return make_template_string(std::move(text_parts), std::move(expressions));
}

// 解析基本表达式
node_ptr parser::primary() {
    // 1. 解析数字字面值
    if (match({token_type::number})) {
        return make_literal(previous().literal);  // 如：123, 3.14
    }

    // 2. 解析字符串字面值
    if (match({token_type::string})) {
        return make_literal(previous().literal);  // 如："hello", 'world'
    }

    // 3. 解析模板字符串
    if (match({token_type::template_start})) {
        return parse_template_string();  // 如：`Hello ${name}`
    }

    // 4. 解析标识符
    if (match({token_type::identifier})) {
        return parse_identifier();  // 变量、函数调用或属性访问
    }

    // 5. 解析括号表达式：(1 + 2)
    if (match({token_type::left_paren})) {
        auto expr = expression();  // 解析括号内的表达式

        if (!match({token_type::right_paren})) {
            MC_THROW(parse_error_exception, "表达式解析错误: 期望右括号");
        }

        return expr;
    }

    // 无法识别的表达式
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