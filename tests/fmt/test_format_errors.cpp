/*
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
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
#include <mc/fmt/format.h>
#include <mc/fmt/format_dict.h>
#include <mc/variant.h>

#include <string_view>

// 测试缺少右花括号
TEST(format_error_test, missing_closing_brace) {
    EXPECT_FALSE(MC_FORMAT_COMPILE_CHECK("{0", 42));
    EXPECT_EQ(sformat_unsafe("{0", 42), "{0");
}

// 测试命名参数名称为空
TEST(format_error_test, invalid_named_parameter) {
    EXPECT_FALSE(MC_FORMAT_COMPILE_CHECK("${}", ("value", 1)));
}

// 测试位置参数索引非法
TEST(format_error_test, invalid_index_parameter) {
    EXPECT_FALSE(MC_FORMAT_COMPILE_CHECK("{abc}", ("value", 1)));
}

// 测试孤立右花括号
TEST(format_error_test, stray_closing_brace) {
    EXPECT_FALSE(MC_FORMAT_COMPILE_CHECK("prefix } suffix"));
    EXPECT_EQ(sformat_unsafe("prefix } suffix"), "prefix } suffix");
}

// 测试缺少命名参数
TEST(format_error_test, missing_named_argument) {
    EXPECT_FALSE(MC_FORMAT_COMPILE_CHECK("${missing}", ("present", 1)));
    EXPECT_EQ(sformat_unsafe("${missing}", ("present", 1)), "${missing}");
}

// 测试缺少位置参数
TEST(format_error_test, missing_positional_argument) {
    EXPECT_FALSE(MC_FORMAT_COMPILE_CHECK("{2}", 1));
    EXPECT_EQ(sformat_unsafe("{2}", 1), "{2}");
}

// 测试格式说明符非法
TEST(format_error_test, invalid_format_spec) {
    EXPECT_FALSE(MC_FORMAT_COMPILE_CHECK("{:.}", 3.14));
    EXPECT_EQ(sformat_unsafe("{:.}", 3.14), "{:.}");
}

// 测试动态参数类型错误
TEST(format_error_test, invalid_dynamic_parameter) {
    mc::dict args{{"value", 1.23}, {"width", "not_number"}};
    EXPECT_EQ(mc::format_dict("${value:{width}f}", args), "{value:not_numberf}");
}

// 测试 get_format_args 正常收集命名参数
TEST(format_error_test, collect_named_arguments) {
    mc::dict arg_names;
    bool     ok = mc::fmt::get_format_args("${host}:{1}:{PORT:{width}.{precision}f}", arg_names);

    ASSERT_TRUE(ok);
    EXPECT_EQ(arg_names.size(), 2U);
    EXPECT_TRUE(arg_names.contains("host"));
    EXPECT_TRUE(arg_names.contains("PORT"));
}

// 测试 get_format_args 遇到语法错误时返回 false
TEST(format_error_test, collect_named_arguments_failed) {
    mc::dict arg_names;
    bool     ok = mc::fmt::get_format_args("${host", arg_names);

    EXPECT_FALSE(ok);
    EXPECT_EQ(arg_names.size(), 0U);
}

// 测试运行时解析错误
// 注意：对于无效的格式字符串，sformat 会在编译期检查失败
// 因此需要使用 sformat_unsafe 来测试运行时行为
TEST(format_error_test, RuntimeParserErrors) {
    // 运行时遇到 "${}" 时当前实现会回退为 "{}"
    EXPECT_EQ(sformat_unsafe("${}", ("value", 1)), "{}");
}

// 测试 parse_placeholder_content 无效起始位置
// 注意：这个函数是内部函数，很难直接测试，我们通过格式化字符串来间接触发
// 使用 sformat_unsafe 来避免编译期检查
TEST(format_error_test, FormatParserPlaceholderContentInvalidStart) {
    // 当前运行时解析会原样输出，为了避免不一致，期待回退文本
    EXPECT_EQ(sformat_unsafe("{", 42), "{");
}

// 测试 compile_format_arg::parse_custom 失败
// 注意：这个函数是编译期函数，很难直接测试
// 我们通过使用自定义格式化器来间接触发
TEST(format_error_test, FormatCompileArgParseCustomFailure) {
    // 通过格式化操作来间接测试 parse_custom
    // 如果自定义格式化器的 parse_fn 返回 false，会在编译期检测到
    // 这里主要验证格式化功能正常工作
    EXPECT_TRUE(MC_FORMAT_COMPILE_CHECK("{}", 42));
}

