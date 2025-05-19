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
void verify_number_token(const std::string& expr, mc::variant expected_value,
                         mc::type_id expected_type) {
    mc::expr::lexer lexer(expr);
    auto            tokens = lexer.scan_tokens();

    // 应该有两个token：数字和EOF
    ASSERT_EQ(tokens.size(), 2);
    ASSERT_EQ(tokens[0].type, mc::expr::token_type::number);
    ASSERT_EQ(tokens[1].type, mc::expr::token_type::end_of_file);

    // 验证值和类型
    ASSERT_EQ(tokens[0].literal.get_type(), expected_type)
        << expr << "," << expected_value << "," << expected_type;
    ASSERT_EQ(tokens[0].literal, expected_value)
        << expr << "," << expected_value << "," << expected_type;
}

// 测试十进制数字
TEST(LexerTest, DecimalNumbers) {
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
TEST(LexerTest, HexadecimalNumbers) {
    verify_number_token("0xFF", 255, mc::type_id::int64_type);
    verify_number_token("0xff", 255, mc::type_id::int64_type);
    verify_number_token("0XFF", 255, mc::type_id::int64_type);
    verify_number_token("0x7F", 127, mc::type_id::int64_type);
    verify_number_token("0x80", 128, mc::type_id::int64_type);
    verify_number_token("0xFF00", 65280, mc::type_id::int64_type);
}

// 测试二进制数字
TEST(LexerTest, BinaryNumbers) {
    verify_number_token("0b101", 5, mc::type_id::int64_type);
    verify_number_token("0B1010", 10, mc::type_id::int64_type);
    verify_number_token("0b11111111", 255, mc::type_id::int64_type);
}

// 测试八进制数字
TEST(LexerTest, OctalNumbers) {
    verify_number_token("077", 63, mc::type_id::int64_type);
    verify_number_token("0377", 255, mc::type_id::int64_type);
}

// 测试错误情况
TEST(LexerTest, ErrorCases) {
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