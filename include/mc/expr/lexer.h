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

/**
 * @file lexer.h
 * @brief 定义词法分析器类
 * @note 此头文件尚未启用，词法分析器的实现当前在engine.cpp中，此文件仅为未来拆分做准备
 */
#ifndef MC_EXPR_INTERNAL_LEXER_H
#define MC_EXPR_INTERNAL_LEXER_H

#include <functional>
#include <mc/expr/token.h>
#include <vector>

namespace mc::expr {

/**
 * @brief 词法分析器类
 */
class MC_API lexer {
public:
    explicit lexer(std::string_view source);

    std::vector<token> scan_tokens();
    std::vector<token> scan_template_string_tokens();

    static bool is_template_string(std::string_view source);

private:
    bool is_at_end() const;
    char advance();
    bool match(char expected);
    char peek() const;
    char peek_next() const;

    // 字符跳过工具函数
    void skip_while(const std::function<bool(char)>& predicate);
    void skip_until(const std::function<bool(char)>& predicate);

    // 提取词素
    std::string_view lexeme() const;                                   // 提取从m_start到m_current的子串
    std::string_view lexeme(std::size_t end) const;                    // 提取从m_start到指定位置的子串
    std::string_view lexeme(std::size_t start, std::size_t end) const; // 提取指定范围的子串
    std::size_t      lexlen() const;

    // 添加token
    void add_token(token_type type);
    void add_token(token_type type, mc::variant literal);

    void scan_token();
    void handle_operator(char c);
    void scan_string();
    void scan_template_string(char delimiter);
    void process_string_literal(char delimiter);
    void scan_number();
    void scan_identifier();

    int  detect_number_radix();
    bool is_template_string();

    std::string_view   m_source;
    std::size_t        m_current;
    std::size_t        m_start;
    std::vector<token> m_tokens;
};

} // namespace mc::expr

#endif // MC_EXPR_INTERNAL_LEXER_H