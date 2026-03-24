/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
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
 * @file test_string_interop.cpp
 * @brief 测试 mc::string / mc::string_view 与标准库字符串的互操作
 */

#include <gtest/gtest.h>

#include <mc/string.h>
#include <mc/string_utils.h>

#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace mc {
namespace test {

class StringInteropTest : public ::testing::Test {};

TEST_F(StringInteropTest, StdStringConstructionCopiesBytes)
{
    std::string source("ab\0cd", 5);
    mc::string  value(source);

    source[0] = 'X';

    EXPECT_EQ(value.size(), 5U);
    EXPECT_EQ(value.view(), mc::string_view("ab\0cd", 5));
}

TEST_F(StringInteropTest, StdStringViewConstructionPreservesSliceBytes)
{
    std::string      source = "prefix_value";
    std::string_view slice(source.data() + 3, 4);
    mc::string       copied(slice);
    mc::string_view  borrowed(slice);

    EXPECT_EQ(copied.view(), mc::string_view("fix_", 4));
    EXPECT_EQ(borrowed, mc::string_view("fix_", 4));

    source[4] = 'X';
    EXPECT_EQ(borrowed, mc::string_view("fXx_", 4));
    EXPECT_EQ(copied.view(), mc::string_view("fix_", 4));
}

TEST_F(StringInteropTest, ImplicitStdConversionsPreserveEmbeddedNull)
{
    mc::string      value("ab\0cd", 5);
    mc::string_view view("xy\0z", 4);

    std::string      copied_string    = value;
    std::string_view borrowed_view    = value;
    std::string      copied_from_view = view;
    std::string_view std_view         = view;

    EXPECT_EQ(copied_string.size(), 5U);
    EXPECT_EQ(copied_string, std::string("ab\0cd", 5));
    EXPECT_EQ(borrowed_view.size(), 5U);
    EXPECT_EQ(std::string(borrowed_view.data(), borrowed_view.size()), std::string("ab\0cd", 5));

    EXPECT_EQ(copied_from_view, std::string("xy\0z", 4));
    EXPECT_EQ(std_view.size(), 4U);
    EXPECT_EQ(std::string(std_view.data(), std_view.size()), std::string("xy\0z", 4));
}

TEST_F(StringInteropTest, ComparisonWithStdStringAndStdStringViewUsesByteSemantics)
{
    mc::string       value("ab\0cd", 5);
    mc::string_view  view = value.view();
    std::string      same("ab\0cd", 5);
    std::string      other("ab\0ce", 5);
    std::string_view same_view(same.data(), same.size());
    std::string_view other_view(other.data(), other.size());

    EXPECT_TRUE(value == same);
    EXPECT_FALSE(value != same);
    EXPECT_TRUE(value == same_view);
    EXPECT_FALSE(value != same_view);

    EXPECT_TRUE(view == same);
    EXPECT_FALSE(view != same);
    EXPECT_TRUE(view == same_view);
    EXPECT_FALSE(view != same_view);
    EXPECT_TRUE(view < other_view);
    EXPECT_TRUE(view <= same_view);
    EXPECT_TRUE(view > mc::string_view("aa", 2));
    EXPECT_TRUE(view >= same_view);
}

TEST_F(StringInteropTest, AppendAndConcatSupportStdStringInputs)
{
    mc::string value = "pre";

    value.append(std::string("fix"));
    value += std::string("_mid");
    value += std::string_view("_tail");
    EXPECT_EQ(value, "prefix_mid_tail");

    mc::string combined = value + std::string("_std");
    EXPECT_EQ(combined, "prefix_mid_tail_std");

    combined = std::string("head_") + mc::string("tail");
    EXPECT_EQ(combined, "head_tail");

    combined = mc::string::concat(std::string("one"), std::string_view("_two"), mc::string_view("_three"));
    EXPECT_EQ(combined, "one_two_three");
}

TEST_F(StringInteropTest, JoinAndConcatPreserveStdStringContainerBytes)
{
    std::vector<std::string> parts = {"aa", std::string("b\0c", 3), "dd"};

    mc::string joined = mc::string::join(parts, "|");
    mc::string merged = mc::string::concat(parts);

    EXPECT_EQ(joined.view(), mc::string_view("aa|b\0c|dd", 9));
    EXPECT_EQ(merged.view(), mc::string_view("aab\0cdd", 7));
}

TEST_F(StringInteropTest, NullptrContractProducesEmptyStringAndView)
{
    const char*     null_cstr = nullptr;
    mc::string_view view(null_cstr);
    mc::string      value(null_cstr);

    EXPECT_TRUE(view.empty());
    EXPECT_EQ(view.size(), 0U);
    EXPECT_TRUE(value.empty());
    EXPECT_EQ(value.size(), 0U);
    EXPECT_TRUE(value == null_cstr);
    EXPECT_EQ(std::string_view(view).size(), 0U);
    EXPECT_EQ(std::string(value), std::string());
}

TEST_F(StringInteropTest, TypeTraitsExposeStdInteropEntrypoints)
{
    static_assert(std::is_constructible_v<mc::string, std::string>, "mc::string 应支持从 std::string 构造");
    static_assert(std::is_constructible_v<mc::string, std::string_view>, "mc::string 应支持从 std::string_view 构造");
    static_assert(std::is_constructible_v<mc::string_view, std::string>, "mc::string_view 应支持从 std::string 构造");
    static_assert(std::is_constructible_v<mc::string_view, std::string_view>,
                  "mc::string_view 应支持从 std::string_view 构造");
    static_assert(std::is_convertible_v<mc::string, std::string>, "mc::string 应可隐式转换为 std::string");
    static_assert(std::is_convertible_v<mc::string, std::string_view>, "mc::string 应可隐式转换为 std::string_view");
    static_assert(std::is_convertible_v<mc::string_view, std::string>, "mc::string_view 应可隐式转换为 std::string");
    static_assert(std::is_convertible_v<mc::string_view, std::string_view>,
                  "mc::string_view 应可隐式转换为 std::string_view");

    SUCCEED();
}

TEST_F(StringInteropTest, OstreamOutputsPreserveByteSequences)
{
    mc::string         value("ab\0cd", 5);
    mc::string_view    view("xy\0z", 4);
    std::ostringstream stream;

    stream << value << "|" << view;

    std::string output = stream.str();
    EXPECT_EQ(output.size(), 10U);
    EXPECT_EQ(output, std::string("ab\0cd|xy\0z", 10));
}

TEST_F(StringInteropTest, HashUsesStableBytesAcrossStringRepresentations)
{
    std::string      std_text("ab\0cd", 5);
    mc::string       value(std_text);
    mc::string_view  borrowed(std_text);
    std::string_view std_view(std_text.data(), std_text.size());

    std::size_t string_hash = std::hash<mc::string>{}(value);
    std::size_t view_hash   = std::hash<mc::string_view>{}(value.view());

    EXPECT_EQ(string_hash, std::hash<mc::string_view>{}(borrowed));
    EXPECT_EQ(string_hash, std::hash<mc::string_view>{}(mc::string_view(std_view)));
    EXPECT_EQ(view_hash, std::hash<mc::string_view>{}(borrowed));
    EXPECT_EQ(view_hash, std::hash<mc::string_view>{}(mc::string_view(std_view)));
}

TEST_F(StringInteropTest, StdStringLeftMutationSupportsMcStringOperands)
{
    std::string     value  = "pre";
    mc::string      suffix = "fix";
    mc::string_view tail   = "_tail";

    value += suffix;
    EXPECT_EQ(value, "prefix");

    value += tail;
    EXPECT_EQ(value, "prefix_tail");

    value.append(mc::string("_append"));
    value.append(mc::string_view("_view"));
    EXPECT_EQ(value, "prefix_tail_append_view");
}

TEST_F(StringInteropTest, StdStringAndStdStringViewLeftComparisonSupportMcStringTypes)
{
    mc::string       mc_text    = "alpha";
    mc::string_view  mc_view    = "alpha";
    std::string      std_text   = "alpha";
    std::string      lower      = "aardvark";
    std::string      upper      = "beta";
    std::string_view std_view   = std_text;
    std::string_view lower_view = "aardvark";
    std::string_view upper_view = "beta";

    EXPECT_TRUE(std_text == mc_text);
    EXPECT_FALSE(std_text != mc_text);
    EXPECT_TRUE(std_text == mc_view);
    EXPECT_FALSE(std_text != mc_view);
    EXPECT_TRUE(std_text <= mc_text);
    EXPECT_TRUE(std_text >= mc_text);
    EXPECT_TRUE(lower < mc_text);
    EXPECT_TRUE(upper > mc_text);
    EXPECT_TRUE(lower < mc_view);
    EXPECT_TRUE(upper > mc_view);

    EXPECT_TRUE(std_view == mc_text);
    EXPECT_FALSE(std_view != mc_text);
    EXPECT_TRUE(std_view == mc_view);
    EXPECT_FALSE(std_view != mc_view);
    EXPECT_TRUE(std_view <= mc_text);
    EXPECT_TRUE(std_view >= mc_text);
    EXPECT_TRUE(std_view <= mc_view);
    EXPECT_TRUE(std_view >= mc_view);
    EXPECT_TRUE(lower_view < mc_text);
    EXPECT_TRUE(upper_view > mc_text);
    EXPECT_TRUE(lower_view < mc_view);
    EXPECT_TRUE(upper_view > mc_view);
}

TEST_F(StringInteropTest, StdStringLeftPlusSupportsMcStringAndView)
{
    std::string     prefix = "head_";
    mc::string      tail   = "tail";
    mc::string_view view   = "_view";

    mc::string combined = prefix + tail;
    EXPECT_EQ(combined, "head_tail");

    combined = prefix + view;
    EXPECT_EQ(combined, "head__view");

    combined = std::string("tmp_") + mc::string("value");
    EXPECT_EQ(combined, "tmp_value");
}

TEST_F(StringInteropTest, StdStringViewLeftPlusSupportsMcStringAndView)
{
    std::string_view prefix = "head_";
    mc::string       tail   = "tail";
    mc::string_view  view   = "_view";

    mc::string combined = prefix + tail;
    EXPECT_EQ(combined, "head_tail");

    combined = prefix + view;
    EXPECT_EQ(combined, "head__view");
}

TEST_F(StringInteropTest, OperatorExpressionTypeTraitsCoverStdLeftOperands)
{
    static_assert(std::is_same_v<decltype(std::declval<std::string&>() += std::declval<mc::string>()), std::string&>,
                  "std::string += mc::string 应返回 std::string&");
    static_assert(
        std::is_same_v<decltype(std::declval<std::string&>() += std::declval<mc::string_view>()), std::string&>,
        "std::string += mc::string_view 应返回 std::string&");
    static_assert(
        std::is_same_v<decltype(std::declval<std::string&>().append(std::declval<mc::string>())), std::string&>,
        "std::string::append(mc::string) 应返回 std::string&");
    static_assert(
        std::is_same_v<decltype(std::declval<std::string&>().append(std::declval<mc::string_view>())), std::string&>,
        "std::string::append(mc::string_view) 应返回 std::string&");

    static_assert(std::is_same_v<decltype(std::declval<std::string>() + std::declval<mc::string>()), mc::string>,
                  "std::string + mc::string 应返回 mc::string");
    static_assert(std::is_same_v<decltype(std::declval<std::string>() + std::declval<mc::string_view>()), mc::string>,
                  "std::string + mc::string_view 应返回 mc::string");
    static_assert(std::is_same_v<decltype(std::declval<std::string_view>() + std::declval<mc::string>()), mc::string>,
                  "std::string_view + mc::string 应返回 mc::string");
    static_assert(
        std::is_same_v<decltype(std::declval<std::string_view>() + std::declval<mc::string_view>()), mc::string>,
        "std::string_view + mc::string_view 应返回 mc::string");

    static_assert(std::is_same_v<decltype(std::declval<std::string>() == std::declval<mc::string>()), bool>,
                  "std::string == mc::string 应返回 bool");
    static_assert(std::is_same_v<decltype(std::declval<std::string>() == std::declval<mc::string_view>()), bool>,
                  "std::string == mc::string_view 应返回 bool");
    static_assert(std::is_same_v<decltype(std::declval<std::string_view>() < std::declval<mc::string>()), bool>,
                  "std::string_view < mc::string 应返回 bool");
    static_assert(std::is_same_v<decltype(std::declval<std::string_view>() >= std::declval<mc::string_view>()), bool>,
                  "std::string_view >= mc::string_view 应返回 bool");

    SUCCEED();
}

TEST_F(StringInteropTest, StdLeftOperatorsPreserveEmbeddedNullBytes)
{
    std::string     prefix("ab\0", 3);
    mc::string      tail("cd\0e", 4);
    mc::string_view view("xy\0z", 4);

    prefix += tail;
    EXPECT_EQ(prefix, std::string("ab\0cd\0e", 7));

    prefix += view;
    EXPECT_EQ(prefix, std::string("ab\0cd\0exy\0z", 11));

    std::string left("L\0", 2);
    mc::string  combined = left + mc::string_view("R\0T", 3);
    EXPECT_EQ(combined.view(), mc::string_view("L\0R\0T", 5));

    std::string_view view_left("Q\0", 2);
    combined = view_left + tail;
    EXPECT_EQ(combined.view(), mc::string_view("Q\0cd\0e", 6));
}

TEST_F(StringInteropTest, StringUtilsStdStringBufferCompatibility)
{
    std::string result = "prefix:";

    mc::strings::append(result, std::string("A"), std::string_view("B"), mc::string("C"), mc::string_view("D"));
    EXPECT_EQ(result, "prefix:ABCD");

    mc::strings::to_string(result, 42);
    EXPECT_EQ(result, "prefix:ABCD42");

    mc::strings::to_string(result, true);
    EXPECT_EQ(result, "prefix:ABCD42true");
}

TEST_F(StringInteropTest, StringUtilsStdStringHelpersPreserveEmbeddedNull)
{
    std::vector<std::string> parts  = {std::string("a\0b", 3), "cd"};
    mc::string               merged = mc::strings::concat(parts);
    EXPECT_EQ(merged.view(), mc::string_view("a\0bcd", 5));

    std::string fixed_width = "x";
    mc::strings::fixed_width_append(fixed_width, 5, mc::string_view("a\0b", 3), false);
    EXPECT_EQ(fixed_width, std::string("x  a\0b", 6));

    std::string replace_target("ab\0ab", 5);
    mc::strings::replace_all_inplace(replace_target, mc::string_view("b\0a", 3), mc::string_view("XYZ", 3));
    EXPECT_EQ(replace_target, std::string("aXYZb", 5));
}

} // namespace test
} // namespace mc
