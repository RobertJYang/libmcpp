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

TEST_F(StringInteropTest, FindFirstNotOfSupportsStringLikeNeedles)
{
    const mc::string source = "aaabca";
    const auto       expect = std::string("aaabca").find_first_not_of("ab");

    mc::string_view  mc_set      = "ab";
    mc::string       mc_string   = "ab";
    std::string      std_string  = "ab";
    std::string_view std_view    = "ab";
    const char*      cstr_set    = "ab";
    const auto       count_match = source.find_first_not_of("abx", 0, 2);

    EXPECT_EQ(source.find_first_not_of(mc_set), expect);
    EXPECT_EQ(source.find_first_not_of(mc_string), expect);
    EXPECT_EQ(source.find_first_not_of(std_string), expect);
    EXPECT_EQ(source.find_first_not_of(std_view), expect);
    EXPECT_EQ(source.find_first_not_of(cstr_set), expect);
    EXPECT_EQ(count_match, expect);

    static_assert(
        std::is_same_v<decltype(std::declval<const mc::string&>().find_first_not_of(std::declval<mc::string_view>())),
                       mc::string::size_type>,
        "mc::string::find_first_not_of(mc::string_view) 应返回 size_type");
    static_assert(
        std::is_same_v<decltype(std::declval<const mc::string&>().find_first_not_of(std::declval<const mc::string&>())),
                       mc::string::size_type>,
        "mc::string::find_first_not_of(mc::string) 应返回 size_type");
    static_assert(std::is_same_v<decltype(std::declval<const mc::string&>().find_first_not_of(
                                     std::declval<const std::string&>())),
                                 mc::string::size_type>,
                  "mc::string::find_first_not_of(std::string) 应返回 size_type");
    static_assert(
        std::is_same_v<decltype(std::declval<const mc::string&>().find_first_not_of(std::declval<std::string_view>())),
                       mc::string::size_type>,
        "mc::string::find_first_not_of(std::string_view) 应返回 size_type");
    static_assert(
        std::is_same_v<decltype(std::declval<const mc::string&>().find_first_not_of(std::declval<const char*>())),
                       mc::string::size_type>,
        "mc::string::find_first_not_of(const char*) 应返回 size_type");
    static_assert(
        std::is_same_v<decltype(std::declval<const mc::string&>().find_first_not_of(std::declval<const char*>(), 0, 0)),
                       mc::string::size_type>,
        "mc::string::find_first_not_of(const char*, pos, count) 应返回 size_type");
}

TEST_F(StringInteropTest, FindFirstOfSupportsStringLikeNeedles)
{
    const mc::string source = "cab42";
    const auto       expect = std::string("cab42").find_first_of("x4a");
    const auto       view   = source.view();

    mc::string_view  mc_set       = "x4a";
    mc::string       mc_string    = "x4a";
    std::string      std_string   = "x4a";
    std::string_view std_view     = "x4a";
    const char*      cstr_set     = "x4a";
    const auto       count_expect = std::string("cab42").find_first_of("x4yz", 0, 2);
    const auto       count_match  = source.find_first_of("x4yz", 0, 2);

    EXPECT_EQ(source.find_first_of(mc_set), expect);
    EXPECT_EQ(source.find_first_of(mc_string), expect);
    EXPECT_EQ(source.find_first_of(std_string), expect);
    EXPECT_EQ(source.find_first_of(std_view), expect);
    EXPECT_EQ(source.find_first_of(cstr_set), expect);
    EXPECT_EQ(count_match, count_expect);

    EXPECT_EQ(view.find_first_of(mc_set), expect);
    EXPECT_EQ(view.find_first_of(mc_string), expect);
    EXPECT_EQ(view.find_first_of(std_string), expect);
    EXPECT_EQ(view.find_first_of(std_view), expect);
    EXPECT_EQ(view.find_first_of(cstr_set), expect);
    EXPECT_EQ(view.find_first_of("x4yz", 0, 2), count_expect);
}

TEST_F(StringInteropTest, FindLastOfSupportsStringLikeNeedles)
{
    const mc::string source = "abca42";
    const auto       expect = std::string("abca42").find_last_of("x2a");
    const auto       view   = source.view();

    mc::string_view  mc_set      = "x2a";
    mc::string       mc_string   = "x2a";
    std::string      std_string  = "x2a";
    std::string_view std_view    = "x2a";
    const char*      cstr_set    = "x2a";
    const auto       count_match = source.find_last_of("x2yz", mc::string::npos, 2);

    EXPECT_EQ(source.find_last_of(mc_set), expect);
    EXPECT_EQ(source.find_last_of(mc_string), expect);
    EXPECT_EQ(source.find_last_of(std_string), expect);
    EXPECT_EQ(source.find_last_of(std_view), expect);
    EXPECT_EQ(source.find_last_of(cstr_set), expect);
    EXPECT_EQ(count_match, expect);

    EXPECT_EQ(view.find_last_of(mc_set), expect);
    EXPECT_EQ(view.find_last_of(mc_string), expect);
    EXPECT_EQ(view.find_last_of(std_string), expect);
    EXPECT_EQ(view.find_last_of(std_view), expect);
    EXPECT_EQ(view.find_last_of(cstr_set), expect);
    EXPECT_EQ(view.find_last_of("x2yz", mc::string::npos, 2), expect);
}

TEST_F(StringInteropTest, FindLastNotOfSupportsStringLikeNeedles)
{
    const mc::string source = "42abba";
    const auto       expect = std::string("42abba").find_last_not_of("ab");
    const auto       view   = source.view();

    mc::string_view  mc_set      = "ab";
    mc::string       mc_string   = "ab";
    std::string      std_string  = "ab";
    std::string_view std_view    = "ab";
    const char*      cstr_set    = "ab";
    const auto       count_match = source.find_last_not_of("abx", mc::string::npos, 2);

    EXPECT_EQ(source.find_last_not_of(mc_set), expect);
    EXPECT_EQ(source.find_last_not_of(mc_string), expect);
    EXPECT_EQ(source.find_last_not_of(std_string), expect);
    EXPECT_EQ(source.find_last_not_of(std_view), expect);
    EXPECT_EQ(source.find_last_not_of(cstr_set), expect);
    EXPECT_EQ(count_match, expect);

    EXPECT_EQ(view.find_last_not_of(mc_set), expect);
    EXPECT_EQ(view.find_last_not_of(mc_string), expect);
    EXPECT_EQ(view.find_last_not_of(std_string), expect);
    EXPECT_EQ(view.find_last_not_of(std_view), expect);
    EXPECT_EQ(view.find_last_not_of(cstr_set), expect);
    EXPECT_EQ(view.find_last_not_of("abx", mc::string::npos, 2), expect);
}

TEST_F(StringInteropTest, RfindSupportsStringLikeNeedles)
{
    const mc::string source = "aba42aba";
    const auto       expect = std::string("aba42aba").rfind("aba");
    const auto       view   = source.view();

    mc::string_view  mc_set       = "aba";
    mc::string       mc_string    = "aba";
    std::string      std_string   = "aba";
    std::string_view std_view     = "aba";
    const char*      cstr_set     = "aba";
    const auto       count_expect = std::string("aba42aba").rfind("abaxy", mc::string::npos, 3);
    const auto       count_match  = source.rfind("abaxy", mc::string::npos, 3);

    EXPECT_EQ(source.rfind(mc_set), expect);
    EXPECT_EQ(source.rfind(mc_string), expect);
    EXPECT_EQ(source.rfind(std_string), expect);
    EXPECT_EQ(source.rfind(std_view), expect);
    EXPECT_EQ(source.rfind(cstr_set), expect);
    EXPECT_EQ(count_match, count_expect);
    EXPECT_EQ(source.rfind('4'), std::string("aba42aba").rfind('4'));

    EXPECT_EQ(view.rfind(mc_set), expect);
    EXPECT_EQ(view.rfind(mc_string), expect);
    EXPECT_EQ(view.rfind(std_string), expect);
    EXPECT_EQ(view.rfind(std_view), expect);
    EXPECT_EQ(view.rfind(cstr_set), expect);
    EXPECT_EQ(view.rfind("abaxy", mc::string::npos, 3), count_expect);
    EXPECT_EQ(view.rfind('4'), std::string("aba42aba").rfind('4'));
}

TEST_F(StringInteropTest, FindSupportsStringLikeNeedlesAndCountOverload)
{
    const mc::string source = "xyab42ab";
    const auto       expect = std::string("xyab42ab").find("ab");
    const auto       view   = source.view();

    mc::string_view  mc_set       = "ab";
    mc::string       mc_string    = "ab";
    std::string      std_string   = "ab";
    std::string_view std_view     = "ab";
    const char*      cstr_set     = "ab";
    const auto       count_expect = std::string("xyab42ab").find("abxyz", 0, 2);
    const auto       count_match  = source.find("abxyz", 0, 2);

    EXPECT_EQ(source.find(mc_set), expect);
    EXPECT_EQ(source.find(mc_string), expect);
    EXPECT_EQ(source.find(std_string), expect);
    EXPECT_EQ(source.find(std_view), expect);
    EXPECT_EQ(source.find(cstr_set), expect);
    EXPECT_EQ(count_match, count_expect);
    EXPECT_EQ(source.find('4'), std::string("xyab42ab").find('4'));

    EXPECT_EQ(view.find(mc_set), expect);
    EXPECT_EQ(view.find(mc_string), expect);
    EXPECT_EQ(view.find(std_string), expect);
    EXPECT_EQ(view.find(std_view), expect);
    EXPECT_EQ(view.find(cstr_set), expect);
    EXPECT_EQ(view.find("abxyz", 0, 2), count_expect);
    EXPECT_EQ(view.find('4'), std::string("xyab42ab").find('4'));
}

TEST_F(StringInteropTest, CompareSupportsStringLikeNeedlesAndSliceOverloads)
{
    const mc::string source = "prefix_body_tail";
    const auto       view   = source.view();
    const auto       signum = [](int value) {
        return (value > 0) - (value < 0);
    };
    const auto full_expect = signum(std::string_view("prefix_body_tail").compare(std::string_view("body")));

    mc::string_view  mc_text      = "body";
    mc::string       mc_string    = "body";
    std::string      std_string   = "body";
    std::string_view std_view     = "body";
    const char*      cstr_text    = "body";
    const auto       slice_expect = std::string("prefix_body_tail").compare(7, 4, "bodyXYZ", 4);

    EXPECT_EQ(signum(source.compare(mc_text)), signum(std::string("prefix_body_tail").compare("body")));
    EXPECT_EQ(signum(source.compare(mc_string)), signum(std::string("prefix_body_tail").compare("body")));
    EXPECT_EQ(signum(source.compare(std_string)), signum(std::string("prefix_body_tail").compare("body")));
    EXPECT_EQ(signum(source.compare(std_view)), signum(std::string("prefix_body_tail").compare("body")));
    EXPECT_EQ(signum(source.compare(cstr_text)), signum(std::string("prefix_body_tail").compare("body")));
    EXPECT_EQ(source.compare(7, 4, mc_text), 0);
    EXPECT_EQ(source.compare(7, 4, mc_string), 0);
    EXPECT_EQ(source.compare(7, 4, std_string), 0);
    EXPECT_EQ(source.compare(7, 4, std_view), 0);
    EXPECT_EQ(source.compare(7, 4, cstr_text), 0);
    EXPECT_EQ(source.compare(7, 4, "bodyXYZ", 4), slice_expect);

    EXPECT_EQ(signum(view.compare(mc_text)), full_expect);
    EXPECT_EQ(signum(view.compare(mc_string)), full_expect);
    EXPECT_EQ(signum(view.compare(std_string)), full_expect);
    EXPECT_EQ(signum(view.compare(std_view)), full_expect);
    EXPECT_EQ(signum(view.compare(cstr_text)), full_expect);
    EXPECT_EQ(view.compare(7, 4, mc_text), 0);
    EXPECT_EQ(view.compare(7, 4, mc_string), 0);
    EXPECT_EQ(view.compare(7, 4, std_string), 0);
    EXPECT_EQ(view.compare(7, 4, std_view), 0);
    EXPECT_EQ(view.compare(7, 4, cstr_text), 0);
    EXPECT_EQ(view.compare(7, 4, "bodyXYZ", 4), slice_expect);
}

TEST_F(StringInteropTest, CompareSupportsDualSliceStringLikeNeedles)
{
    const mc::string source = "prefix_body_tail";
    const auto       view   = source.view();
    const auto       signum = [](int value) {
        return (value > 0) - (value < 0);
    };

    mc::string_view  mc_text    = "xxbodyyy";
    mc::string       mc_string  = "xxbodyyy";
    std::string      std_string = "xxbodyyy";
    std::string_view std_view   = "xxbodyyy";
    const auto slice_expect     = signum(std::string("prefix_body_tail").compare(7, 4, std::string("xxbodyyy"), 2, 4));

    EXPECT_EQ(signum(source.compare(7, 4, mc_text, 2, 4)), slice_expect);
    EXPECT_EQ(signum(source.compare(7, 4, mc_string, 2, 4)), slice_expect);
    EXPECT_EQ(signum(source.compare(7, 4, std_string, 2, 4)), slice_expect);
    EXPECT_EQ(signum(source.compare(7, 4, std_view, 2, 4)), slice_expect);

    EXPECT_EQ(signum(view.compare(7, 4, mc_text, 2, 4)), slice_expect);
    EXPECT_EQ(signum(view.compare(7, 4, mc_string, 2, 4)), slice_expect);
    EXPECT_EQ(signum(view.compare(7, 4, std_string, 2, 4)), slice_expect);
    EXPECT_EQ(signum(view.compare(7, 4, std_view, 2, 4)), slice_expect);
}

TEST_F(StringInteropTest, StartsEndsContainsSupportStringLikeNeedlesAndChar)
{
    const mc::string source = "prefix_body_tail";
    const auto       view   = source.view();

    mc::string_view  prefix     = "pre";
    mc::string       prefix_str = "pre";
    std::string      std_prefix = "pre";
    std::string_view sv_prefix  = "pre";

    mc::string_view  suffix     = "tail";
    mc::string       suffix_str = "tail";
    std::string      std_suffix = "tail";
    std::string_view sv_suffix  = "tail";

    mc::string_view  needle     = "body";
    mc::string       needle_str = "body";
    std::string      std_needle = "body";
    std::string_view sv_needle  = "body";

    EXPECT_TRUE(source.starts_with(prefix));
    EXPECT_TRUE(source.starts_with(prefix_str));
    EXPECT_TRUE(source.starts_with(std_prefix));
    EXPECT_TRUE(source.starts_with(sv_prefix));
    EXPECT_TRUE(source.starts_with("pre"));
    EXPECT_TRUE(source.starts_with('p'));

    EXPECT_TRUE(source.ends_with(suffix));
    EXPECT_TRUE(source.ends_with(suffix_str));
    EXPECT_TRUE(source.ends_with(std_suffix));
    EXPECT_TRUE(source.ends_with(sv_suffix));
    EXPECT_TRUE(source.ends_with("tail"));
    EXPECT_TRUE(source.ends_with('l'));

    EXPECT_TRUE(source.contains(needle));
    EXPECT_TRUE(source.contains(needle_str));
    EXPECT_TRUE(source.contains(std_needle));
    EXPECT_TRUE(source.contains(sv_needle));
    EXPECT_TRUE(source.contains("body"));
    EXPECT_TRUE(source.contains('b'));

    EXPECT_TRUE(view.starts_with(prefix));
    EXPECT_TRUE(view.starts_with(prefix_str));
    EXPECT_TRUE(view.starts_with(std_prefix));
    EXPECT_TRUE(view.starts_with(sv_prefix));
    EXPECT_TRUE(view.starts_with("pre"));
    EXPECT_TRUE(view.starts_with('p'));

    EXPECT_TRUE(view.ends_with(suffix));
    EXPECT_TRUE(view.ends_with(suffix_str));
    EXPECT_TRUE(view.ends_with(std_suffix));
    EXPECT_TRUE(view.ends_with(sv_suffix));
    EXPECT_TRUE(view.ends_with("tail"));
    EXPECT_TRUE(view.ends_with('l'));

    EXPECT_TRUE(view.contains(needle));
    EXPECT_TRUE(view.contains(needle_str));
    EXPECT_TRUE(view.contains(std_needle));
    EXPECT_TRUE(view.contains(sv_needle));
    EXPECT_TRUE(view.contains("body"));
    EXPECT_TRUE(view.contains('b'));
}

TEST_F(StringInteropTest, InsertSupportsStringLikeNeedles)
{
    mc::string       from_mc     = "pre_tail";
    mc::string       from_string = "pre_tail";
    mc::string       from_std    = "pre_tail";
    mc::string       from_view   = "pre_tail";
    mc::string       from_cstr   = "pre_tail";
    mc::string_view  mc_text     = "body_";
    mc::string       mc_string   = "body_";
    std::string      std_string  = "body_";
    std::string_view std_view    = "body_";

    from_mc.insert(4, mc_text);
    from_string.insert(4, mc_string);
    from_std.insert(4, std_string);
    from_view.insert(4, std_view);
    from_cstr.insert(4, "body_");

    EXPECT_EQ(from_mc, "pre_body_tail");
    EXPECT_EQ(from_string, "pre_body_tail");
    EXPECT_EQ(from_std, "pre_body_tail");
    EXPECT_EQ(from_view, "pre_body_tail");
    EXPECT_EQ(from_cstr, "pre_body_tail");
}

TEST_F(StringInteropTest, ReplaceSupportsDualSliceStringLikeNeedles)
{
    const auto expected = std::string("pre_body_tail").replace(4, 4, std::string("xxCOREyy"), 2, 4);

    mc::string       from_mc     = "pre_body_tail";
    mc::string       from_string = "pre_body_tail";
    mc::string       from_std    = "pre_body_tail";
    mc::string       from_view   = "pre_body_tail";
    mc::string_view  mc_text     = "xxCOREyy";
    mc::string       mc_string   = "xxCOREyy";
    std::string      std_string  = "xxCOREyy";
    std::string_view std_view    = "xxCOREyy";

    from_mc.replace(4, 4, mc_text, 2, 4);
    from_string.replace(4, 4, mc_string, 2, 4);
    from_std.replace(4, 4, std_string, 2, 4);
    from_view.replace(4, 4, std_view, 2, 4);

    EXPECT_EQ(static_cast<std::string>(from_mc), expected);
    EXPECT_EQ(static_cast<std::string>(from_string), expected);
    EXPECT_EQ(static_cast<std::string>(from_std), expected);
    EXPECT_EQ(static_cast<std::string>(from_view), expected);
}

TEST_F(StringInteropTest, AppendSupportsStringLikeSliceOverloads)
{
    const auto expected = std::string("pre_").append(std::string("xxBODYyy"), 2, 4);

    mc::string       from_mc     = "pre_";
    mc::string       from_string = "pre_";
    mc::string       from_std    = "pre_";
    mc::string       from_view   = "pre_";
    mc::string_view  mc_text     = "xxBODYyy";
    mc::string       mc_string   = "xxBODYyy";
    std::string      std_string  = "xxBODYyy";
    std::string_view std_view    = "xxBODYyy";

    from_mc.append(mc_text, 2, 4);
    from_string.append(mc_string, 2, 4);
    from_std.append(std_string, 2, 4);
    from_view.append(std_view, 2, 4);

    EXPECT_EQ(static_cast<std::string>(from_mc), expected);
    EXPECT_EQ(static_cast<std::string>(from_string), expected);
    EXPECT_EQ(static_cast<std::string>(from_std), expected);
    EXPECT_EQ(static_cast<std::string>(from_view), expected);
}

TEST_F(StringInteropTest, InsertAndReplaceSupportCStringCountOverloads)
{
    mc::string inserted = "pre_tail";
    inserted.insert(4, "body_xx", 5);
    EXPECT_EQ(inserted, "pre_body_tail");

    mc::string replaced = "pre_body_tail";
    replaced.replace(4, 4, "COREDROP", 4);
    EXPECT_EQ(replaced, "pre_CORE_tail");
}

} // namespace test
} // namespace mc
