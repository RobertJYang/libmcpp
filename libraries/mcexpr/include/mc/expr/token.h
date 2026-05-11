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

#ifndef MC_EXPR_TOKEN_H
#define MC_EXPR_TOKEN_H

#include <mc/string.h>
#include <mc/variant.h>

namespace mc::expr {

/**
 * @brief 词法单元类型
 */
enum class token_type {
    end_of_file,
    number,
    string,
    identifier,
    plus,           // +
    minus,          // -
    asterisk,       // *
    slash,          // /
    percent,        // %
    equals,         // ==
    not_equals,     // !=
    less,           // <
    less_equals,    // <=
    greater,        // >
    greater_equals, // >=
    logical_and,    // &&
    logical_or,     // ||
    logical_not,    // !
    left_paren,     // (
    right_paren,    // )
    comma,          // ,
    dot,            // .
    semicolon,      // ;
    question,       // ?
    colon,          // :

    // 位操作运算符
    bit_and, // &
    bit_or,  // |
    bit_xor, // ^
    bit_not, // ~
    lshift,  // <<
    rshift,  // >>

    // 模板字符串相关
    template_start,  // 模板字符串开始部分
    template_middle, // 模板字符串中间部分
    template_end,    // 模板字符串结束部分
    template_expr,   // 模板表达式 ${...}

    error
};

/**
 * @brief 词法单元
 */
struct token {
    token_type  type;
    mc::string  lexeme;
    mc::variant literal;
    std::size_t position;

    token() : type(token_type::error), position(0)
    {}

    token(token_type t, mc::string lex, mc::variant lit = {}, std::size_t pos = 0)
        : type(t), lexeme(std::move(lex)), literal(std::move(lit)), position(pos)
    {}
};

} // namespace mc::expr
#endif
