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
#include <mc/string.h>

namespace mc::expr {

static bool is_digit(char c) {
    return std::isdigit(static_cast<unsigned char>(c));
}

static bool is_alpha(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

static bool is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

static bool is_xdigit(char c) {
    return isxdigit(static_cast<unsigned char>(c));
}

static bool is_binary_digit(char c) {
    return c == '0' || c == '1';
}

static bool is_octal_digit(char c) {
    return c >= '0' && c <= '7';
}

static bool is_whitespace(char c) {
    return c == ' ' || c == '\r' || c == '\t' || c == '\n';
}

lexer::lexer(std::string_view source) : m_source(source), m_current(0), m_start(0) {
}

bool lexer::is_at_end() const {
    return m_current >= m_source.size();
}

char lexer::advance() {
    return m_source[m_current++];
}

bool lexer::match(char expected) {
    if (is_at_end() || m_source[m_current] != expected) {
        return false;
    }

    m_current++;
    return true;
}

char lexer::peek() const {
    if (is_at_end()) {
        return '\0';
    }
    return m_source[m_current];
}

char lexer::peek_next() const {
    if (m_current + 1 >= m_source.size()) {
        return '\0';
    }
    return m_source[m_current + 1];
}

void lexer::skip_while(const std::function<bool(char)>& predicate) {
    while (!is_at_end() && predicate(peek())) {
        advance();
    }
}

void lexer::skip_until(const std::function<bool(char)>& predicate) {
    while (!is_at_end() && !predicate(peek())) {
        advance();
    }
}

// 提取当前词素（从m_start到m_current）
std::string_view lexer::lexeme() const {
    return m_source.substr(m_start, m_current - m_start);
}

// 提取词素（从m_start到指定位置）
std::string_view lexer::lexeme(std::size_t end) const {
    return m_source.substr(m_start, end - m_start);
}

// 提取词素（指定范围）
std::string_view lexer::lexeme(std::size_t start, std::size_t end) const {
    return m_source.substr(start, end - start);
}

std::size_t lexer::lexlen() const {
    return m_current - m_start;
}

// 添加词法单元
void lexer::add_token(token_type type) {
    add_token(type, {});
}

// 添加带有字面值的词法单元
void lexer::add_token(token_type type, mc::variant literal) {
    m_tokens.emplace_back(type, std::string(lexeme()), std::move(literal), m_start);
}

// 处理运算符
void lexer::handle_operator(char c) {
    switch (c) {
    case '(':
        add_token(token_type::left_paren);
        break;
    case ')':
        add_token(token_type::right_paren);
        break;
    case ',':
        add_token(token_type::comma);
        break;
    case '.':
        add_token(token_type::dot);
        break;
    case '-':
        add_token(token_type::minus);
        break;
    case '+':
        add_token(token_type::plus);
        break;
    case ';':
        add_token(token_type::semicolon);
        break;
    case '?':
        add_token(token_type::question);
        break;
    case ':':
        add_token(token_type::colon);
        break;
    case '*':
        add_token(token_type::asterisk);
        break;
    case '%':
        add_token(token_type::percent);
        break;
    case '^':
        add_token(token_type::bit_xor);
        break;
    case '~':
        add_token(token_type::bit_not);
        break;
    case '!':
        add_token(match('=') ? token_type::not_equals : token_type::logical_not);
        break;
    case '=':
        add_token(match('=') ? token_type::equals : token_type::error);
        break;
    case '<':
        if (match('<')) {
            add_token(token_type::lshift);
        } else {
            add_token(match('=') ? token_type::less_equals : token_type::less);
        }
        break;
    case '>':
        if (match('>')) {
            add_token(token_type::rshift);
        } else {
            add_token(match('=') ? token_type::greater_equals : token_type::greater);
        }
        break;
    case '&':
        if (match('&')) {
            add_token(token_type::logical_and);
        } else {
            add_token(token_type::bit_and);
        }
        break;
    case '|':
        if (match('|')) {
            add_token(token_type::logical_or);
        } else {
            add_token(token_type::bit_or);
        }
        break;
    case '/':
        add_token(token_type::slash);
        break;
    default:
        add_token(token_type::error);
        break;
    }
}

// 检查是否是模板字符串
bool lexer::is_template_string(std::string_view source) {
    // 跳过字符直到遇到 $ 或结束引号
    size_t current = 0;
    while (current < source.size()) {
        char c = source[current];

        // 如果碰到结束引号，那不是模板字符串
        if (c == '"' || c == '\'') {
            return false;
        }

        // 如果碰到 $，再判断后面是否跟着 {
        if (c == '$' && current + 1 < source.size() && source[current + 1] == '{') {
            return true;
        }

        // 处理转义字符
        if (c == '\\' && current + 1 < source.size()) {
            current += 2; // 跳过转义字符和被转义的字符
        } else {
            current++;
        }
    }

    return false;
}

bool lexer::is_template_string() {
    if (m_current >= m_source.size()) {
        return false;
    }

    return is_template_string(m_source.substr(m_current));
}

void lexer::scan_template_string(char delimiter) {
    size_t string_start = m_current; // 记录字符串内容开始位置

    // 扫描第一部分文本直到 ${ 或结束引号
    while (m_current < m_source.size()) {
        char c = m_source[m_current];

        // 如果是结束引号，这是一个简单的模板字符串，没有表达式
        if (c == delimiter) {
            auto text = lexeme(string_start, m_current);
            m_current++; // 跳过结束引号
            add_token(token_type::string, text);
            return;
        }

        // 如果找到 ${，开始处理模板表达式
        if (c == '$' && m_current + 1 < m_source.size() && m_source[m_current + 1] == '{') {
            // 添加模板字符串的开始部分
            auto text_part = lexeme(string_start, m_current);
            add_token(token_type::template_start, text_part);

            // 跳过 ${
            m_current += 2;
            m_start = m_current;

            // 扫描表达式直到找到 }
            int brace_count = 1; // 计算嵌套大括号
            while (m_current < m_source.size()) {
                char expr_char = m_source[m_current];

                if (expr_char == '{') {
                    brace_count++;
                } else if (expr_char == '}') {
                    brace_count--;
                    if (brace_count == 0) {
                        break;
                    }
                }

                m_current++;
            }

            if (m_current >= m_source.size() || m_source[m_current] != '}') {
                MC_THROW(parse_error_exception, "表达式解析错误: 未闭合的模板表达式");
            }

            // 标记表达式部分
            add_token(token_type::template_expr, lexeme());

            // 跳过 }
            m_current++;
            m_start      = m_current;
            string_start = m_current;

            // 继续扫描剩余部分作为新的模板字符串部分
            while (m_current < m_source.size()) {
                char next_char = m_source[m_current];

                // 如果找到结束引号，终止模板字符串
                if (next_char == delimiter) {
                    auto end_text = lexeme(string_start, m_current);
                    m_current++; // 跳过结束引号
                    add_token(token_type::template_end, end_text);
                    return;
                }

                // 如果找到下一个 ${，添加中间部分并递归处理
                if (next_char == '$' && m_current + 1 < m_source.size() &&
                    m_source[m_current + 1] == '{') {
                    auto middle_text = lexeme(string_start, m_current);
                    add_token(token_type::template_middle, middle_text);

                    // 跳过 ${
                    m_current += 2;
                    m_start = m_current;

                    // 扫描表达式直到找到 }
                    int brace_count = 1;
                    while (m_current < m_source.size()) {
                        char expr_char = m_source[m_current];

                        if (expr_char == '{') {
                            brace_count++;
                        } else if (expr_char == '}') {
                            brace_count--;
                            if (brace_count == 0) {
                                break;
                            }
                        }

                        m_current++;
                    }

                    if (m_current >= m_source.size() || m_source[m_current] != '}') {
                        MC_THROW(parse_error_exception, "表达式解析错误: 未闭合的模板表达式");
                    }

                    // 标记表达式部分
                    add_token(token_type::template_expr, lexeme());

                    // 跳过 }
                    m_current++;
                    m_start      = m_current;
                    string_start = m_current;
                } else {
                    m_current++;
                }
            }

            // 如果 delimiter 为 0，则表示扫描的是一个裸字符串，不是用 delimiter 包裹的
            if (delimiter == 0) {
                auto end_text = lexeme(string_start, m_current);
                add_token(token_type::template_end, end_text);
                return;
            }

            MC_THROW(parse_error_exception, "表达式解析错误: 未闭合的模板字符串");
        }

        // 处理转义字符
        if (c == '\\' && m_current + 1 < m_source.size()) {
            m_current += 2;
        } else {
            m_current++;
        }
    }

    MC_THROW(parse_error_exception, "表达式解析错误: 未闭合的模板字符串");
}

// 修改扫描词法单元的方法
void lexer::scan_token() {
    char c = advance();

    if (c == '"' || c == '\'') {
        // 检查是否是模板字符串
        if (is_template_string()) {
            // 回退光标，重新扫描为模板字符串
            m_current = m_start + 1; // 跳过引号
            scan_template_string(m_source[m_start]);
        } else {
            scan_string();
        }
    } else if (is_digit(c)) {
        scan_number();
    } else if (is_alpha(c)) {
        scan_identifier();
    } else if (is_whitespace(c)) {
        return;
    } else {
        handle_operator(c);
    }
}

// 扫描字符串字面值
void lexer::scan_string() {
    char delimiter = m_source[m_start]; // 获取字符串的开始引号类型（单引号或双引号）

    // 跳过字符直到遇到未转义的结束引号
    skip_until([delimiter, this](char c) {
        return c == delimiter ||
               (c == '\\' && this->peek_next() == delimiter && this->advance() && false);
    });

    if (is_at_end()) {
        MC_THROW(parse_error_exception, "表达式解析错误: 未闭合的字符串");
    }

    // 跳过结束引号
    advance();

    process_string_literal(delimiter);
}

// 处理字符串字面值内容（处理转义序列）
void lexer::process_string_literal(char delimiter) {
    auto value = lexeme(m_start + 1, m_current - 1);

    std::string result;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            if (value[i + 1] == delimiter) {
                result.push_back(delimiter);
                ++i; // 跳过下一个字符
            } else if (value[i + 1] == 'u' && i + 5 < value.size()) {
                // 处理 Unicode 转义序列 \uXXXX
                std::string hex_str = std::string(value.substr(i + 2, 4));
                try {
                    unsigned int code_point = std::stoul(hex_str, nullptr, 16);
                    // 将 Unicode 码点转换为 UTF-8
                    if (code_point <= 0x7F) {
                        result.push_back(static_cast<char>(code_point));
                    } else if (code_point <= 0x7FF) {
                        result.push_back(static_cast<char>(0xC0 | (code_point >> 6)));
                        result.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
                    } else if (code_point <= 0xFFFF) {
                        result.push_back(static_cast<char>(0xE0 | (code_point >> 12)));
                        result.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
                        result.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
                    } else {
                        result.push_back(static_cast<char>(0xF0 | (code_point >> 18)));
                        result.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
                        result.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
                        result.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
                    }
                    i += 5; // 跳过 \uXXXX
                } catch (...) {
                    // 如果解析失败，保留原始字符
                    result.push_back(value[i]);
                }
            } else {
                result.push_back(value[i]);
            }
        } else {
            result.push_back(value[i]);
        }
    }

    add_token(token_type::string, std::move(result));
}

// 检测数字的进制
// 返回值: 0 = 十进制, 16 = 十六进制, 8 = 八进制, 2 = 二进制，-1 = 浮点数
int lexer::detect_number_radix() {
    if (m_source[m_start] == '0' && m_current < m_source.size()) {
        auto pos = m_current - m_start;
        if (pos == 1 && (peek() == 'x' || peek() == 'X')) {
            advance(); // 十六进制数字，消费 'x' 或 'X'
            skip_while(is_xdigit);
            return 16;
        } else if (pos == 1 && (peek() == 'b' || peek() == 'B')) {
            advance(); // 二进制数字，消费 'b' 或 'B'
            skip_while(is_binary_digit);
            return 2;
        } else if (pos == 1 && peek() >= '0' && peek() <= '7') {
            skip_while(is_octal_digit);
            return 8;
        }
    }

    skip_while(is_digit);
    if (!is_at_end() && peek() == '.' && m_current + 1 < m_source.size() &&
        is_digit(m_source[m_current + 1])) {
        advance(); // 消费 '.'
        skip_while(is_digit);
        return -1; // 浮点数
    }

    return 0; // 默认十进制整数
}

// 扫描数字字面值
void lexer::scan_number() {
    int              radix   = detect_number_radix();
    std::string_view num_str = lexeme();
    if ((radix == 16 || radix == 2)) {
        // 只有前缀(如'0x'/'0b')但没有数字部分
        MC_ASSERT_THROW(lexlen() > 2, parse_error_exception, "表达式解析错误: 无效的${type}数字",
                        ("type", radix == 16 ? "十六进制" : "二进制"));
        num_str = num_str.substr(2); // 去掉前缀'0x'/'0b'
    } else if (radix == 8) {
        // 只有前缀(如'0')但没有数字部分
        MC_ASSERT_THROW(lexlen() > 1, parse_error_exception, "表达式解析错误: 无效的八进制数字");
        num_str = num_str.substr(1); // 去掉前缀'0'
    }

    // 检查数字后面的字符是否合法
    if (!is_at_end()) {
        char next               = peek();
        bool is_valid_delimiter = is_whitespace(next) || !is_alnum(next);
        MC_ASSERT_THROW(is_valid_delimiter, parse_error_exception,
                        "表达式解析错误: 数字 ${num} 后面跟随了非法字符 '${char}'",
                        ("num", lexeme())("char", std::string(1, next)));
    }

    // 如果是浮点数，直接处理
    if (radix == -1) {
        double value;
        MC_ASSERT_THROW(mc::string::try_to_number<double>(lexeme(), value), parse_error_exception,
                        "表达式解析错误: 无法解析浮点数 ${num}", ("num", lexeme()));
        add_token(token_type::number, mc::variant(value));
        return;
    }

    int64_t value;
    MC_ASSERT_THROW(mc::string::try_to_number<int64_t>(num_str, value, radix),
                    parse_error_exception, "表达式解析错误: 无法解析数字 ${num}",
                    ("num", lexeme()));
    add_token(token_type::number, mc::variant(value));
}

void lexer::scan_identifier() {
    skip_while(is_alnum);

    std::string_view text = lexeme();

    // 特殊处理布尔字面值
    if (text == "true") {
        add_token(token_type::number, mc::variant(true));
        return;
    } else if (text == "false") {
        add_token(token_type::number, mc::variant(false));
        return;
    }

    add_token(token_type::identifier, text);
}

std::vector<token> lexer::scan_tokens() {
    m_tokens.clear();

    while (!is_at_end()) {
        m_start = m_current;
        scan_token();
    }

    m_tokens.emplace_back(token_type::end_of_file, "", mc::variant(), m_source.size());
    return m_tokens;
}

std::vector<token> lexer::scan_template_string_tokens() {
    m_tokens.clear();

    while (!is_at_end()) {
        m_start = m_current;
        scan_template_string(0);
    }

    m_tokens.emplace_back(token_type::end_of_file, "", mc::variant(), m_source.size());
    return m_tokens;
}

} // namespace mc::expr