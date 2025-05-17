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

#include <mc/variant.h>
#include <string_view>

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

    // 位操作运算符
    bit_and, // &
    bit_or,  // |
    bit_xor, // ^
    bit_not, // ~
    lshift,  // <<
    rshift,  // >>

    error
};

/**
 * @brief 词法单元
 */
struct token {
    token_type  type;
    std::string lexeme;
    mc::variant literal;
    std::size_t position;

    token(token_type t, std::string lex, mc::variant lit = {}, std::size_t pos = 0)
        : type(t), lexeme(std::move(lex)), literal(std::move(lit)), position(pos) {
    }
};

} // namespace mc::expr
#endif
