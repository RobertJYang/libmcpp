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
 * @file parser.h
 * @brief 定义语法分析器类
 * @note 此头文件尚未启用，语法分析器的实现当前在engine.cpp中，此文件仅为未来拆分做准备
 * @note 支持点号访问语法 (obj.property) 用于访问对象属性和方法
 */
#ifndef MC_EXPR_INTERNAL_PARSER_H
#define MC_EXPR_INTERNAL_PARSER_H

#include <mc/expr/node.h>
#include <mc/expr/token.h>

#include <initializer_list>
#include <memory>

namespace mc::expr {

/**
 * @brief 语法分析器类
 */
class parser {
public:
    explicit parser(std::vector<token> tokens);

    std::shared_ptr<node> parse();

private:
    std::shared_ptr<node> expression();
    std::shared_ptr<node> logical_or();
    std::shared_ptr<node> logical_and();
    std::shared_ptr<node> bit_or();
    std::shared_ptr<node> bit_xor();
    std::shared_ptr<node> bit_and();
    std::shared_ptr<node> shift();
    std::shared_ptr<node> equality();
    std::shared_ptr<node> comparison();
    std::shared_ptr<node> term();
    std::shared_ptr<node> factor();
    std::shared_ptr<node> unary();
    std::shared_ptr<node> primary();
    std::shared_ptr<node> function_call();

    bool         match(std::initializer_list<token_type> types);
    bool         check(token_type type) const;
    const token& advance();
    const token& peek() const;
    const token& previous() const;
    bool         is_at_end() const;

    std::vector<token> m_tokens;
    std::size_t        m_current;
};

} // namespace mc::expr

#endif // MC_EXPR_INTERNAL_PARSER_H