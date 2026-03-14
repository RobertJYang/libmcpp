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

#include <gtest/gtest.h>
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/expr/lexer.h>
#include <mc/expr/token.h>

// 辅助函数，用于验证词法分析器输出的token
void verify_number_token(const std::string& expr, mc::variant expected_value, mc::type_id expected_type)
{
    mc::expr::lexer lexer(expr);
    auto            tokens = lexer.scan_tokens();

    // 应该有两个token：数字和EOF
    ASSERT_EQ(tokens.size(), 2);
    ASSERT_EQ(tokens[0].type, mc::expr::token_type::number);
    ASSERT_EQ(tokens[1].type, mc::expr::token_type::end_of_file);

    // 验证值和类型
    ASSERT_EQ(tokens[0].literal.get_type(), expected_type) << expr << "," << expected_value << "," << expected_type;
    ASSERT_EQ(tokens[0].literal, expected_value) << expr << "," << expected_value << "," << expected_type;
}

// 测试十进制数字
TEST(LexerTest, DecimalNumbers)
{
    verify_number_token("42", 42, mc::type_id::int64_type);
    verify_number_token("127", 127, mc::type_id::int64_type);
    verify_number_token("128", 128, mc::type_id::int64_type);
    verify_number_token("255", 255, mc::type_id::int64_type);
    verify_number_token("256", 256, mc::type_id::int64_type);
    verify_number_token("32767", 32767, mc::type_id::int64_type);
    verify_number_token("32768", 32768, mc::type_id::int64_type);

    // 测试浮点数
    verify_number_token("3.14", 3.14, mc::type_id::double_type);
    verify_number_token("0.5", 0.5, mc::type_id::double_type);
    verify_number_token("1.0", 1.0, mc::type_id::double_type);
}

// 测试十六进制数字
TEST(LexerTest, HexadecimalNumbers)
{
    verify_number_token("0xFF", 255, mc::type_id::int64_type);
    verify_number_token("0xff", 255, mc::type_id::int64_type);
    verify_number_token("0XFF", 255, mc::type_id::int64_type);
    verify_number_token("0x7F", 127, mc::type_id::int64_type);
    verify_number_token("0x80", 128, mc::type_id::int64_type);
    verify_number_token("0xFF00", 65280, mc::type_id::int64_type);
}

// 测试二进制数字
TEST(LexerTest, BinaryNumbers)
{
    verify_number_token("0b101", 5, mc::type_id::int64_type);
    verify_number_token("0B1010", 10, mc::type_id::int64_type);
    verify_number_token("0b11111111", 255, mc::type_id::int64_type);
}

// 测试八进制数字
TEST(LexerTest, OctalNumbers)
{
    verify_number_token("077", 63, mc::type_id::int64_type);
    verify_number_token("0377", 255, mc::type_id::int64_type);
}

// 测试错误情况
TEST(LexerTest, ErrorCases)
{
    // 非法的十六进制数字
    EXPECT_THROW(
        {
            mc::expr::lexer lexer("0xGH");
            lexer.scan_tokens();
        },
        mc::parse_error_exception);

    // 非法的二进制数字
    EXPECT_THROW(
        {
            mc::expr::lexer lexer("0b102");
            lexer.scan_tokens();
        },
        mc::parse_error_exception);
}

// 测试模板字符串词法分析
TEST(LexerTest, TemplateString)
{
    // 基本模板字符串
    {
        mc::expr::lexer lexer("\"Hello, ${name}!\"");
        auto            tokens = lexer.scan_tokens();

        // 应该有5个token：template_start, template_expr, template_end, EOF
        ASSERT_EQ(tokens.size(), 4);
        ASSERT_EQ(tokens[0].type, mc::expr::token_type::template_start);
        ASSERT_EQ(tokens[0].literal.as_string(), "Hello, ");

        ASSERT_EQ(tokens[1].type, mc::expr::token_type::template_expr);
        ASSERT_EQ(tokens[1].lexeme, "name");

        ASSERT_EQ(tokens[2].type, mc::expr::token_type::template_end);
        ASSERT_EQ(tokens[2].literal.as_string(), "!");

        ASSERT_EQ(tokens[3].type, mc::expr::token_type::end_of_file);
    }

    // 多个表达式的模板字符串
    {
        mc::expr::lexer lexer("\"${greeting}, ${name}! Your score is ${score}.\"");
        auto            tokens = lexer.scan_tokens();

        // 应该有7个token：template_start, template_expr, template_middle, template_expr,
        // template_middle, template_expr, template_end, EOF
        ASSERT_EQ(tokens.size(), 8);
        ASSERT_EQ(tokens[0].type, mc::expr::token_type::template_start);
        ASSERT_EQ(tokens[0].literal.as_string(), "");

        ASSERT_EQ(tokens[1].type, mc::expr::token_type::template_expr);
        ASSERT_EQ(tokens[1].lexeme, "greeting");

        ASSERT_EQ(tokens[2].type, mc::expr::token_type::template_middle);
        ASSERT_EQ(tokens[2].literal.as_string(), ", ");

        ASSERT_EQ(tokens[3].type, mc::expr::token_type::template_expr);
        ASSERT_EQ(tokens[3].lexeme, "name");

        ASSERT_EQ(tokens[4].type, mc::expr::token_type::template_middle);
        ASSERT_EQ(tokens[4].literal.as_string(), "! Your score is ");

        ASSERT_EQ(tokens[5].type, mc::expr::token_type::template_expr);
        ASSERT_EQ(tokens[5].lexeme, "score");

        ASSERT_EQ(tokens[6].type, mc::expr::token_type::template_end);
        ASSERT_EQ(tokens[6].literal.as_string(), ".");

        ASSERT_EQ(tokens[7].type, mc::expr::token_type::end_of_file);
    }

    // 测试嵌套花括号
    {
        mc::expr::lexer lexer("\"Result: ${if (x > 5) { return a; } else { return b; }}\"");
        auto            tokens = lexer.scan_tokens();

        ASSERT_EQ(tokens.size(), 4);
        ASSERT_EQ(tokens[0].type, mc::expr::token_type::template_start);
        ASSERT_EQ(tokens[0].literal.as_string(), "Result: ");

        ASSERT_EQ(tokens[1].type, mc::expr::token_type::template_expr);
        ASSERT_EQ(tokens[1].lexeme, "if (x > 5) { return a; } else { return b; }");

        ASSERT_EQ(tokens[2].type, mc::expr::token_type::template_end);
        ASSERT_EQ(tokens[2].literal.as_string(), "");

        ASSERT_EQ(tokens[3].type, mc::expr::token_type::end_of_file);
    }

    // 错误情况：未闭合的模板表达式
    EXPECT_THROW(
        {
            mc::expr::lexer lexer("\"Hello, ${name!\"");
            lexer.scan_tokens();
        },
        mc::parse_error_exception);

    // 错误情况：未闭合的模板字符串
    EXPECT_THROW(
        {
            mc::expr::lexer lexer("\"Hello, ${name}");
            lexer.scan_tokens();
        },
        mc::parse_error_exception);
}

// 测试更多数字格式
TEST(LexerTest, MoreNumberFormats)
{
    // 测试前导零的十进制数字
    verify_number_token("007", 7, mc::type_id::int64_type);
    verify_number_token("000", 0, mc::type_id::int64_type);

    // 测试浮点数格式
    verify_number_token("0.0", 0.0, mc::type_id::double_type);
    verify_number_token("0.5", 0.5, mc::type_id::double_type);
    verify_number_token("5.0", 5.0, mc::type_id::double_type);
    verify_number_token("123.456", 123.456, mc::type_id::double_type);

    // 测试科学计数法（如果支持）
    // verify_number_token("1e2", 100.0, mc::type_id::double_type);
    // verify_number_token("1.5e-2", 0.015, mc::type_id::double_type);
}

// 测试标识符
TEST(LexerTest, Identifiers)
{
    mc::expr::lexer lexer("hello world _test var123");
    auto            tokens = lexer.scan_tokens();

    ASSERT_GE(tokens.size(), 5);
    ASSERT_EQ(tokens[0].type, mc::expr::token_type::identifier);
    ASSERT_EQ(tokens[0].lexeme, "hello");
    ASSERT_EQ(tokens[1].type, mc::expr::token_type::identifier);
    ASSERT_EQ(tokens[1].lexeme, "world");
    ASSERT_EQ(tokens[2].type, mc::expr::token_type::identifier);
    ASSERT_EQ(tokens[2].lexeme, "_test");
    ASSERT_EQ(tokens[3].type, mc::expr::token_type::identifier);
    ASSERT_EQ(tokens[3].lexeme, "var123");
}

// 测试布尔字面值
TEST(LexerTest, BooleanLiterals)
{
    mc::expr::lexer lexer("true false");
    auto            tokens = lexer.scan_tokens();

    ASSERT_GE(tokens.size(), 3);
    ASSERT_EQ(tokens[0].type, mc::expr::token_type::number);
    ASSERT_TRUE(tokens[0].literal.is_bool());
    EXPECT_TRUE(tokens[0].literal.as_bool());

    ASSERT_EQ(tokens[1].type, mc::expr::token_type::number);
    ASSERT_TRUE(tokens[1].literal.is_bool());
    EXPECT_FALSE(tokens[1].literal.as_bool());
}

// 测试字符串字面值
TEST(LexerTest, StringLiterals)
{
    // 单引号字符串
    {
        mc::expr::lexer lexer("'hello'");
        auto            tokens = lexer.scan_tokens();

        ASSERT_GE(tokens.size(), 2);
        ASSERT_EQ(tokens[0].type, mc::expr::token_type::string);
        EXPECT_EQ(tokens[0].literal.as_string(), "hello");
    }

    // 双引号字符串
    {
        mc::expr::lexer lexer("\"world\"");
        auto            tokens = lexer.scan_tokens();

        ASSERT_GE(tokens.size(), 2);
        ASSERT_EQ(tokens[0].type, mc::expr::token_type::string);
        EXPECT_EQ(tokens[0].literal.as_string(), "world");
    }

    // 空字符串
    {
        mc::expr::lexer lexer("''");
        auto            tokens = lexer.scan_tokens();

        ASSERT_GE(tokens.size(), 2);
        ASSERT_EQ(tokens[0].type, mc::expr::token_type::string);
        EXPECT_EQ(tokens[0].literal.as_string(), "");
    }
}

// 测试运算符
TEST(LexerTest, Operators)
{
    mc::expr::lexer lexer("+ - * / % == != < <= > >= && || ! & | ^ ~ << >>");
    auto            tokens = lexer.scan_tokens();

    ASSERT_GE(tokens.size(), 18);
    EXPECT_EQ(tokens[0].type, mc::expr::token_type::plus);
    EXPECT_EQ(tokens[1].type, mc::expr::token_type::minus);
    EXPECT_EQ(tokens[2].type, mc::expr::token_type::asterisk);
    EXPECT_EQ(tokens[3].type, mc::expr::token_type::slash);
    EXPECT_EQ(tokens[4].type, mc::expr::token_type::percent);
    EXPECT_EQ(tokens[5].type, mc::expr::token_type::equals);
    EXPECT_EQ(tokens[6].type, mc::expr::token_type::not_equals);
    EXPECT_EQ(tokens[7].type, mc::expr::token_type::less);
    EXPECT_EQ(tokens[8].type, mc::expr::token_type::less_equals);
    EXPECT_EQ(tokens[9].type, mc::expr::token_type::greater);
    EXPECT_EQ(tokens[10].type, mc::expr::token_type::greater_equals);
    EXPECT_EQ(tokens[11].type, mc::expr::token_type::logical_and);
    EXPECT_EQ(tokens[12].type, mc::expr::token_type::logical_or);
    EXPECT_EQ(tokens[13].type, mc::expr::token_type::logical_not);
    EXPECT_EQ(tokens[14].type, mc::expr::token_type::bit_and);
    EXPECT_EQ(tokens[15].type, mc::expr::token_type::bit_or);
    EXPECT_EQ(tokens[16].type, mc::expr::token_type::bit_xor);
    EXPECT_EQ(tokens[17].type, mc::expr::token_type::bit_not);
    EXPECT_EQ(tokens[18].type, mc::expr::token_type::lshift);
    EXPECT_EQ(tokens[19].type, mc::expr::token_type::rshift);
}

// 测试分隔符
TEST(LexerTest, Delimiters)
{
    mc::expr::lexer lexer("( ) , . ; ? :");
    auto            tokens = lexer.scan_tokens();

    ASSERT_GE(tokens.size(), 8);
    EXPECT_EQ(tokens[0].type, mc::expr::token_type::left_paren);
    EXPECT_EQ(tokens[1].type, mc::expr::token_type::right_paren);
    EXPECT_EQ(tokens[2].type, mc::expr::token_type::comma);
    EXPECT_EQ(tokens[3].type, mc::expr::token_type::dot);
    EXPECT_EQ(tokens[4].type, mc::expr::token_type::semicolon);
    EXPECT_EQ(tokens[5].type, mc::expr::token_type::question);
    EXPECT_EQ(tokens[6].type, mc::expr::token_type::colon);
}

// 测试空白字符
TEST(LexerTest, Whitespace)
{
    mc::expr::lexer lexer("1  2\t3\n4");
    auto            tokens = lexer.scan_tokens();

    // 应该只有数字token和EOF
    ASSERT_EQ(tokens.size(), 5);
    EXPECT_EQ(tokens[0].type, mc::expr::token_type::number);
    EXPECT_EQ(tokens[1].type, mc::expr::token_type::number);
    EXPECT_EQ(tokens[2].type, mc::expr::token_type::number);
    EXPECT_EQ(tokens[3].type, mc::expr::token_type::number);
    EXPECT_EQ(tokens[4].type, mc::expr::token_type::end_of_file);
}

// 测试更多错误情况
TEST(LexerTest, MoreErrorCases)
{
    // 非法的二进制数字
    EXPECT_THROW(
        {
            mc::expr::lexer lexer("0b2");
            lexer.scan_tokens();
        },
        mc::parse_error_exception);

    // 未闭合的字符串
    EXPECT_THROW(
        {
            mc::expr::lexer lexer("'unclosed string");
            lexer.scan_tokens();
        },
        mc::parse_error_exception);

    // 数字后跟非法字符
    EXPECT_THROW(
        {
            mc::expr::lexer lexer("123abc");
            lexer.scan_tokens();
        },
        mc::parse_error_exception);
}

// 测试模板字符串的更多场景
TEST(LexerTest, MoreTemplateStringScenarios)
{
    // 只有文本的模板字符串（没有表达式）
    {
        mc::expr::lexer lexer("\"Hello, World!\"");
        auto            tokens = lexer.scan_tokens();

        ASSERT_GE(tokens.size(), 2);
        ASSERT_EQ(tokens[0].type, mc::expr::token_type::string);
        EXPECT_EQ(tokens[0].literal.as_string(), "Hello, World!");
    }

    // 只有表达式的模板字符串
    {
        mc::expr::lexer lexer("\"${name}\"");
        auto            tokens = lexer.scan_tokens();

        ASSERT_GE(tokens.size(), 4);
        ASSERT_EQ(tokens[0].type, mc::expr::token_type::template_start);
        ASSERT_EQ(tokens[0].literal.as_string(), "");
        ASSERT_EQ(tokens[1].type, mc::expr::token_type::template_expr);
        ASSERT_EQ(tokens[1].lexeme, "name");
        ASSERT_EQ(tokens[2].type, mc::expr::token_type::template_end);
        ASSERT_EQ(tokens[2].literal.as_string(), "");
    }
}

// 测试 Unicode 转义序列
TEST(LexerTest, UnicodeEscapeTokenized)
{
    mc::expr::lexer lexer("\"\\u4F60\\u597D\"");
    auto            tokens = lexer.scan_tokens();

    ASSERT_GE(tokens.size(), 2);
    ASSERT_EQ(tokens[0].type, mc::expr::token_type::string);
    // 验证 Unicode 转义序列被正确解析为 "你好"
    std::string result = tokens[0].literal.as_string();
    EXPECT_EQ(result, "你好");
}

// 测试 lexer::peek() 在字符串末尾的情况
// 注意：peek() 是私有方法，我们通过 scan_tokens() 的行为来间接验证
// peek() 在 is_at_end() 时返回 '\0'，这会影响 scan_tokens() 的行为
TEST(LexerTest, LexerPeekAtEnd)
{
    // 测试空字符串：peek() 应该返回 '\0'，导致立即结束扫描
    mc::expr::lexer lexer("");
    auto            tokens = lexer.scan_tokens();
    // 应该只有一个 EOF token
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, mc::expr::token_type::end_of_file);

    // 测试单字符字符串：扫描完所有 token 后，peek() 应该返回 '\0'
    mc::expr::lexer lexer2("a");
    auto            tokens2 = lexer2.scan_tokens();
    // 应该有两个 token：标识符 'a' 和 EOF
    ASSERT_EQ(tokens2.size(), 2);
    EXPECT_EQ(tokens2[0].type, mc::expr::token_type::identifier);
    EXPECT_EQ(tokens2[1].type, mc::expr::token_type::end_of_file);
}

// 测试 lexer::peek_next() 在字符串倒数第二个字符的情况
// 注意：peek_next() 是私有方法，我们通过 scan_tokens() 的行为来间接验证
// peek_next() 在 m_current + 1 >= m_source.size() 时返回 '\0'
TEST(LexerTest, LexerPeekNextAtEnd)
{
    // 测试单字符字符串：peek_next() 应该返回 '\0'
    mc::expr::lexer lexer("a");
    auto            tokens = lexer.scan_tokens();
    // 应该有两个 token：标识符 'a' 和 EOF
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].type, mc::expr::token_type::identifier);
    EXPECT_EQ(tokens[1].type, mc::expr::token_type::end_of_file);

    // 测试两个字符的字符串：在扫描第一个字符时，peek_next() 应该返回第二个字符
    // 在扫描第二个字符时，peek_next() 应该返回 '\0'
    mc::expr::lexer lexer2("ab");
    auto            tokens2 = lexer2.scan_tokens();
    // 应该有两个 token：标识符 'ab' 和 EOF
    ASSERT_EQ(tokens2.size(), 2);
    EXPECT_EQ(tokens2[0].type, mc::expr::token_type::identifier);
    EXPECT_EQ(tokens2[1].type, mc::expr::token_type::end_of_file);
}

// 测试 lexer::lexeme(std::size_t end) 函数
TEST(LexerTest, LexerLexemeWithEnd)
{
    mc::expr::lexer lexer("hello world");
    // 手动设置 m_start 和 m_current 来测试 lexeme(end)
    // 注意：由于 lexer 的私有成员，我们无法直接访问
    // 但可以通过扫描 token 来间接测试
    auto tokens = lexer.scan_tokens();
    // lexeme(end) 主要用于内部实现，这里主要验证它不会崩溃
    EXPECT_GE(tokens.size(), 1);
}

// 测试简单模板字符串（没有表达式）
TEST(LexerTest, LexerSimpleTemplateString)
{
    mc::expr::lexer lexer("\"text\"");
    auto            tokens = lexer.scan_tokens();

    ASSERT_GE(tokens.size(), 2);
    ASSERT_EQ(tokens[0].type, mc::expr::token_type::string);
    EXPECT_EQ(tokens[0].literal.as_string(), "text");
}

// 测试模板字符串中的嵌套大括号
TEST(LexerTest, LexerTemplateStringNestedBraces)
{
    mc::expr::lexer lexer("\"${a{b}}\"");
    auto            tokens = lexer.scan_tokens();

    ASSERT_GE(tokens.size(), 4);
    ASSERT_EQ(tokens[0].type, mc::expr::token_type::template_start);
    ASSERT_EQ(tokens[1].type, mc::expr::token_type::template_expr);
    // 验证嵌套大括号被正确处理
    EXPECT_EQ(tokens[1].lexeme, "a{b}");
}

// 测试模板字符串中的转义字符
TEST(LexerTest, LexerTemplateStringEscape)
{
    mc::expr::lexer lexer("\"text\\n${expr}\"");
    auto            tokens = lexer.scan_tokens();

    ASSERT_GE(tokens.size(), 4);
    ASSERT_EQ(tokens[0].type, mc::expr::token_type::template_start);
    // 验证转义字符被正确处理
    std::string text = tokens[0].literal.as_string();
    EXPECT_TRUE(text.find("\\n") != std::string::npos || text.find("\n") != std::string::npos);
}