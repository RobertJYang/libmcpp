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

/**
 * @file test_string.cpp
 * @brief 测试字符串处理库
 */
#include <gtest/gtest.h>
#include <iostream>
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/string.h>
#include <mc/string_utils.h>
#include <mc/time.h>
#include <string>
#include <string_view>
#include <type_traits>

using namespace mc::strings;

namespace mc {
namespace test {

class StringTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 在每个测试前执行
    }

    void TearDown() override
    {
        // 在每个测试后执行
    }
};

template <typename T, typename = void>
struct has_mutable_data : std::false_type {};

template <typename T>
struct has_mutable_data<T, std::void_t<decltype(std::declval<T&>().data())>>
    : std::bool_constant<std::is_same_v<decltype(std::declval<T&>().data()), char*>> {};

template <typename T, typename = void>
struct has_capacity : std::false_type {};

template <typename T>
struct has_capacity<T, std::void_t<decltype(std::declval<const T&>().capacity())>> : std::true_type {};

/**
 * @brief 测试忽略大小写比较函数
 */
TEST_F(StringTest, IEqualsTest)
{
    // 测试相同字符串
    ASSERT_TRUE(iequals("test", "test")) << "相同字符串应该相等";

    // 测试大小写不同的字符串
    ASSERT_TRUE(iequals("TEST", "test")) << "大小写不同的字符串应该相等";
    ASSERT_TRUE(iequals("Test", "tEsT")) << "大小写混合的字符串应该相等";

    // 测试不同字符串
    ASSERT_FALSE(iequals("test", "test1")) << "不同字符串不应该相等";

    // 测试空字符串
    ASSERT_TRUE(iequals("", "")) << "空字符串应该相等";
    ASSERT_FALSE(iequals("test", "")) << "非空字符串与空字符串不应该相等";

    // 测试 mc::string / mc::string_view 版本（走 string_view 比较，避免匹配非 constexpr 的 const char* 构造）
    ASSERT_TRUE(iequals(mc::string("test").view(), mc::string("TEST").view())) << "mc::string 版本应该正常工作";
}

/**
 * @brief 测试字符串转换函数
 */
TEST_F(StringTest, StringConversionTest)
{
    // 测试转换为小写
    ASSERT_EQ(to_lower("TEST"), "test") << "转换为小写应该正确";
    ASSERT_EQ(to_lower("Test"), "test") << "转换为小写应该正确";

    // 测试转换为大写
    ASSERT_EQ(to_upper("test"), "TEST") << "转换为大写应该正确";
    ASSERT_EQ(to_upper("Test"), "TEST") << "转换为大写应该正确";

    mc::string s1 = "TEST";
    s1.to_lower_inplace();
    ASSERT_EQ(s1, "test") << "原地转换为小写应该正确";

    mc::string s2 = "test";
    s2.to_upper_inplace();
    ASSERT_EQ(s2, "TEST") << "原地转换为大写应该正确";
}

/**
 * @brief 测试字符串修剪函数
 */
TEST_F(StringTest, StringTrimTest)
{
    // 测试修剪两端空白
    ASSERT_EQ(trim("  test  "), "test") << "修剪两端空白应该正确";
    ASSERT_EQ(trim("\t\ntest\r\n"), "test") << "修剪两端空白应该正确";

    // 测试修剪左侧空白
    ASSERT_EQ(ltrim("  test  "), "test  ") << "修剪左侧空白应该正确";

    // 测试修剪右侧空白
    ASSERT_EQ(rtrim("  test  "), "  test") << "修剪右侧空白应该正确";

    mc::string s1 = "  test  ";
    s1.trim_inplace();
    ASSERT_EQ(s1, "test") << "原地修剪两端空白应该正确";

    mc::string s2 = "  test  ";
    s2.ltrim_inplace();
    ASSERT_EQ(s2, "test  ") << "原地修剪左侧空白应该正确";

    mc::string s3 = "  test  ";
    s3.rtrim_inplace();
    ASSERT_EQ(s3, "  test") << "原地修剪右侧空白应该正确";
}

/**
 * @brief 测试字符串分割和连接函数
 */
TEST_F(StringTest, StringSplitJoinTest)
{
    std::vector<mc::string> parts1 = split("a,b,c", ',');
    ASSERT_EQ(parts1.size(), 3) << "分割后应该有3个元素";
    ASSERT_EQ(parts1[0], "a") << "第一个元素应该是a";
    ASSERT_EQ(parts1[1], "b") << "第二个元素应该是b";
    ASSERT_EQ(parts1[2], "c") << "第三个元素应该是c";

    std::vector<mc::string> parts2 = split("a::b::c", "::");
    ASSERT_EQ(parts2.size(), 3) << "分割后应该有3个元素";
    ASSERT_EQ(parts2[0], "a") << "第一个元素应该是a";
    ASSERT_EQ(parts2[1], "b") << "第二个元素应该是b";
    ASSERT_EQ(parts2[2], "c") << "第三个元素应该是c";

    // 测试连接
    ASSERT_EQ(join(parts1, ","), "a,b,c") << "连接应该正确";
    ASSERT_EQ(join(parts2, "::"), "a::b::c") << "连接应该正确";
}

/**
 * @brief 测试类似Lua的子字符串函数
 */
TEST_F(StringTest, SubstrLuaStyleTest)
{
    const mc::string test_str = "Hello, World!";
    const auto       test_sv  = test_str.view();

    ASSERT_EQ(substr(test_sv, 0, 4), "Hello") << "从开头提取子字符串应该正确";
    ASSERT_EQ(substr(test_sv, 7, 11), "World") << "从中间提取子字符串应该正确";

    ASSERT_EQ(substr(test_sv, -6, -2), "World") << "使用负数索引提取子字符串应该正确";
    ASSERT_EQ(substr(test_sv, -13, -8), "Hello,") << "使用负数索引从开头提取子字符串应该正确";

    ASSERT_EQ(substr(test_sv, 7), "World!") << "省略结束位置应该提取到字符串末尾";
    ASSERT_EQ(substr(test_sv, -6), "World!") << "使用负数起始索引并省略结束位置应该正确";

    ASSERT_EQ(substr(test_sv, 0, -1), "Hello, World!") << "提取完整字符串应该正确";
    ASSERT_EQ(substr(test_sv, 0, test_str.length() - 1), "Hello, World!") << "使用字符串长度作为结束位置应该正确";

    ASSERT_EQ(substr(test_sv, 0, 0), "H") << "提取单个字符应该正确";
    ASSERT_EQ(substr(test_sv, -1, -1), "!") << "提取最后一个字符应该正确";

    ASSERT_EQ(substr(test_sv, 100, 200), "") << "超出范围的索引应该返回空字符串";
    ASSERT_EQ(substr(test_sv, -100, -50), "") << "超出范围的负数索引应该返回空字符串";
    ASSERT_EQ(substr(test_sv, 5, 2), "") << "结束索引小于起始索引应该返回空字符串";

    ASSERT_EQ(substr("", 0, 5), "") << "空字符串应该返回空字符串";
}

/**
 * @brief 测试以长度为参数的子字符串函数
 */
TEST_F(StringTest, SubstringTest)
{
    const mc::string       test_str = "Hello, World!";
    const mc::string_view  test_sv(test_str.view());

    ASSERT_EQ(substring(test_sv, 0, 5), "Hello") << "从开头提取指定长度的子字符串应该正确";
    ASSERT_EQ(substring(test_sv, 7, 5), "World") << "从中间提取指定长度的子字符串应该正确";

    ASSERT_EQ(substring(test_sv, -6, 5), "World") << "使用负数索引提取指定长度的子字符串应该正确";
    ASSERT_EQ(substring(test_sv, -13, 6), "Hello,") << "使用负数索引从开头提取指定长度的子字符串应该正确";

    ASSERT_EQ(substring(test_sv, 7), "World!") << "默认长度应该提取到字符串末尾";
    ASSERT_EQ(substring(test_sv, -6), "World!") << "使用负数起始索引并使用默认长度应该正确";

    ASSERT_EQ(substring(test_sv, 0), "Hello, World!") << "从开头提取整个字符串应该正确";
    ASSERT_EQ(substring(test_sv, 0, test_str.length()), "Hello, World!") << "使用字符串长度作为长度应该正确";

    ASSERT_EQ(substring(test_sv, 0, 100), "Hello, World!") << "超出范围的长度应该被调整";
    ASSERT_EQ(substring(test_sv, 7, 100), "World!") << "从中间开始超出范围的长度应该被调整";
    ASSERT_EQ(substring(test_sv, -6, 100), "World!") << "使用负索引时超出范围的长度应该被调整";

    ASSERT_EQ(substring(test_sv, 0, 0), "") << "零长度应该返回空字符串";
    ASSERT_EQ(substring(test_sv, 5, 0), "") << "从中间开始的零长度应该返回空字符串";

    ASSERT_EQ(substring(test_sv, 100, 5), "") << "超出范围的索引应该返回空字符串";
    ASSERT_EQ(substring(test_sv, -100, 5), "") << "超出范围的负数索引应该返回空字符串";

    ASSERT_EQ(substring("", 0, 5), "") << "空字符串应该返回空字符串";
}

/**
 * @brief 测试字符串前缀和后缀检查函数
 */
TEST_F(StringTest, StringPrefixSuffixTest)
{
    // 测试前缀检查
    ASSERT_TRUE(starts_with("test", "te")) << "前缀检查应该正确";
    ASSERT_FALSE(starts_with("test", "es")) << "前缀检查应该正确";

    // 测试后缀检查
    ASSERT_TRUE(ends_with("test", "st")) << "后缀检查应该正确";
    ASSERT_FALSE(ends_with("test", "es")) << "后缀检查应该正确";
}

/**
 * @brief 测试字符串替换函数
 */
TEST_F(StringTest, StringReplaceTest)
{
    // 测试替换
    ASSERT_EQ(replace_all("test test", "test", "abc"), "abc abc") << "替换应该正确";

    mc::string s = "test test";
    s.replace_all_inplace("test", "abc");
    ASSERT_EQ(s, "abc abc") << "原地替换应该正确";
}

/**
 * @brief 测试字符串包含函数
 */
TEST_F(StringTest, StringContainsTest)
{
    // 测试包含
    ASSERT_TRUE(contains("test", "es")) << "包含检查应该正确";
    ASSERT_FALSE(contains("test", "abc")) << "包含检查应该正确";

    // 测试忽略大小写包含
    ASSERT_TRUE(icontains("TEST", "es")) << "忽略大小写包含检查应该正确";
    ASSERT_FALSE(icontains("TEST", "abc")) << "忽略大小写包含检查应该正确";
}

TEST_F(StringTest, JoinTest)
{
    ASSERT_EQ(join(",", "a", "b", "c"), "a,b,c");
    ASSERT_EQ(join(",", "a", "b", "c", "d"), "a,b,c,d");
    ASSERT_EQ(join(",", "a", "b", "c", "d", "e"), "a,b,c,d,e");

    ASSERT_EQ(join(std::vector<mc::string>{"a", "b", "c"}, ","), "a,b,c");
    ASSERT_EQ(join(std::vector<mc::string>{"a", "b", "c", "d"}, ","), "a,b,c,d");
    ASSERT_EQ(join(std::vector<mc::string>{"a", "b", "c", "d", "e"}, ","), "a,b,c,d,e");
}

TEST_F(StringTest, ConcatTest)
{
    ASSERT_EQ(concat("a", "b", "c"), "abc");
    ASSERT_EQ(concat("a", "b", "c", "d"), "abcd");
    ASSERT_EQ(concat("a", "b", "c", "d", "e"), "abcde");

    ASSERT_EQ(concat(std::vector<mc::string>{"a", "b", "c"}), "abc");
    ASSERT_EQ(concat(std::vector<mc::string>{"a", "b", "c", "d"}), "abcd");
    ASSERT_EQ(concat(std::vector<mc::string>{"a", "b", "c", "d", "e"}), "abcde");
}

TEST_F(StringTest, SplitIteratorTest)
{
    // 测试空字符串
    {
        mc::string_view str;
        auto            it = split_iterator(str, ".");
        EXPECT_EQ(it, split_iterator::end());
    }

    // 测试单个分隔符
    {
        mc::string_view              str = "a.b.c";
        std::vector<mc::string_view> parts;
        for (auto it = split_iterator(str, "."); it != split_iterator::end(); ++it) {
            parts.push_back(*it);
        }
        ASSERT_EQ(parts.size(), 3);
        EXPECT_EQ(parts[0], "a");
        EXPECT_EQ(parts[1], "b");
        EXPECT_EQ(parts[2], "c");
    }

    // 测试多个分隔符
    {
        mc::string_view              str = "a::b.c::d";
        std::vector<mc::string_view> parts;
        for (auto it = split_iterator(str, ".:"); it != split_iterator::end(); ++it) {
            parts.push_back(*it);
        }
        ASSERT_EQ(parts.size(), 4);
        EXPECT_EQ(parts[0], "a");
        EXPECT_EQ(parts[1], "b");
        EXPECT_EQ(parts[2], "c");
        EXPECT_EQ(parts[3], "d");
    }

    // 测试连续分隔符
    {
        mc::string_view              str = "a...b::c";
        std::vector<mc::string_view> parts;
        for (auto it = split_iterator(str, ".:"); it != split_iterator::end(); ++it) {
            parts.push_back(*it);
        }
        ASSERT_EQ(parts.size(), 3);
        EXPECT_EQ(parts[0], "a");
        EXPECT_EQ(parts[1], "b");
        EXPECT_EQ(parts[2], "c");
    }

    // 测试边界情况
    {
        mc::string_view              str = "..a..b..";
        std::vector<mc::string_view> parts;
        for (auto it = split_iterator(str, "."); it != split_iterator::end(); ++it) {
            parts.push_back(*it);
        }
        ASSERT_EQ(parts.size(), 2);
        EXPECT_EQ(parts[0], "a");
        EXPECT_EQ(parts[1], "b");
    }
}

TEST(string, split_iterator)
{
    // 基本分割测试
    {
        mc::string                   str = "a.b.c";
        std::vector<mc::string_view> parts;
        for (auto it = split_iterator(str, "."); it != split_iterator::end(); ++it) {
            parts.push_back(*it);
        }
        ASSERT_EQ(parts.size(), 3);
        ASSERT_EQ(parts[0], "a");
        ASSERT_EQ(parts[1], "b");
        ASSERT_EQ(parts[2], "c");
    }

    // 多个分隔符测试
    {
        mc::string                   str = "a.b:c";
        std::vector<mc::string_view> parts;
        for (auto it = split_iterator(str, ".:"); it != split_iterator::end(); ++it) {
            parts.push_back(*it);
        }
        ASSERT_EQ(parts.size(), 3);
        ASSERT_EQ(parts[0], "a");
        ASSERT_EQ(parts[1], "b");
        ASSERT_EQ(parts[2], "c");
    }

    // 连续分隔符测试
    {
        mc::string                   str = "a..b:::c";
        std::vector<mc::string_view> parts;
        for (auto it = split_iterator(str, ".:"); it != split_iterator::end(); ++it) {
            parts.push_back(*it);
        }
        ASSERT_EQ(parts.size(), 3);
        ASSERT_EQ(parts[0], "a");
        ASSERT_EQ(parts[1], "b");
        ASSERT_EQ(parts[2], "c");
    }

    // 空字符串测试
    {
        mc::string                   str = "";
        std::vector<mc::string_view> parts;
        for (auto it = split_iterator(str, "."); it != split_iterator::end(); ++it) {
            parts.push_back(*it);
        }
        ASSERT_TRUE(parts.empty());
    }

    // 只有分隔符的字符串测试
    {
        mc::string                   str = "...:::";
        std::vector<mc::string_view> parts;
        for (auto it = split_iterator(str, ".:"); it != split_iterator::end(); ++it) {
            parts.push_back(*it);
        }
        ASSERT_TRUE(parts.empty());
    }

    // 前后有分隔符的字符串测试
    {
        mc::string                   str = "..a:b..c::";
        std::vector<mc::string_view> parts;
        for (auto it = split_iterator(str, ".:"); it != split_iterator::end(); ++it) {
            parts.push_back(*it);
        }
        ASSERT_EQ(parts.size(), 3);
        ASSERT_EQ(parts[0], "a");
        ASSERT_EQ(parts[1], "b");
        ASSERT_EQ(parts[2], "c");
    }
}

/**
 * @brief 测试字符串转数字的安全性
 */
TEST_F(StringTest, TestStringToNumberSafety)
{
    // 测试 string_view 的安全性
    {
        // 构造一个较长的字符串，包含合法数字后跟其他字符
        mc::string long_str = "42000hello";

        // 创建一个只包含数字部分的 string_view
        mc::string_view num_view(long_str.data(), 2); // 只取 "42"

        // 验证转换
        int result = 0;
        ASSERT_TRUE(mc::strings::try_to_number(num_view, result));
        ASSERT_EQ(result, 42);

        // 验证 to_number
        ASSERT_EQ(mc::to_number<int>(num_view), 42);
    }

    // 测试 mc::string 的安全性
    {
        mc::string str    = "42";
        int        result = 0;
        ASSERT_TRUE(mc::strings::try_to_number(str.view(), result));
        ASSERT_EQ(result, 42);

        // 验证 to_number
        ASSERT_EQ(mc::to_number<int>(str.view()), 42);
    }

    // 测试 C 风格字符串的安全性
    {
        const char* str    = "42";
        int         result = 0;
        ASSERT_TRUE(mc::strings::try_to_number(str, result));
        ASSERT_EQ(result, 42);

        // 验证 to_number
        ASSERT_EQ(mc::to_number<int>(str), 42);
    }

    // 测试不同进制的边界情况
    {
        // 二进制
        mc::string      bin_str(64, '1'); // 64个1
        mc::string_view bin_view(bin_str);
        uint64_t        result = 0;
        ASSERT_TRUE(mc::strings::try_to_number(bin_view, result, 2));
        ASSERT_EQ(result, std::numeric_limits<uint64_t>::max());

        // 八进制
        mc::string      oct_str = "1777777777777777777777"; // 最大64位八进制
        mc::string_view oct_view(oct_str);
        ASSERT_TRUE(mc::strings::try_to_number(oct_view, result, 8));
        ASSERT_EQ(result, std::numeric_limits<uint64_t>::max());

        // 十六进制
        mc::string      hex_str = "FFFFFFFFFFFFFFFF"; // 最大64位十六进制
        mc::string_view hex_view(hex_str);
        ASSERT_TRUE(mc::strings::try_to_number(hex_view, result, 16));
        ASSERT_EQ(result, std::numeric_limits<uint64_t>::max());
    }

    // 测试超长字符串
    {
        // 二进制超长
        mc::string too_long_bin(65, '1');
        int        result = 0;
        ASSERT_FALSE(mc::strings::try_to_number(too_long_bin.view(), result, 2));

        // 八进制超长
        mc::string too_long_oct(23, '7');
        ASSERT_FALSE(mc::strings::try_to_number(too_long_oct.view(), result, 8));

        // 十六进制超长
        mc::string too_long_hex(17, 'F');
        ASSERT_FALSE(mc::strings::try_to_number(too_long_hex.view(), result, 16));

        // 十进制超长
        mc::string too_long_dec(21, '9');
        ASSERT_FALSE(mc::strings::try_to_number(too_long_dec.view(), result));
    }

    // 测试字符串后面有非数字字符的情况
    {
        // string_view 情况
        mc::string      str1 = "42abc";
        mc::string_view view1(str1.data(), 2); // 只取 "42"
        int             result = 0;
        ASSERT_TRUE(mc::strings::try_to_number(view1, result));
        ASSERT_EQ(result, 42);

        // string 情况
        mc::string str2 = "42abc";
        ASSERT_FALSE(mc::strings::try_to_number(str2.view(), result));

        // C 字符串情况
        const char* str3 = "42abc";
        ASSERT_FALSE(mc::strings::try_to_number(str3, result));
    }

    // 测试空字符串
    {
        int result = 0;
        ASSERT_FALSE(mc::strings::try_to_number(mc::string_view{}, result));
        ASSERT_FALSE(mc::strings::try_to_number(mc::string{}, result));
        ASSERT_FALSE(mc::strings::try_to_number("", result));
    }

    // 测试带符号数
    {
        mc::string str    = "-42";
        int        result = 0;
        ASSERT_TRUE(mc::strings::try_to_number(str, result));
        ASSERT_EQ(result, -42);

        // 验证 string_view
        mc::string      long_str = "-42000";
        mc::string_view view(long_str.data(), 3); // 只取 "-42"
        ASSERT_TRUE(mc::strings::try_to_number(view, result));
        ASSERT_EQ(result, -42);
    }

    // 测试浮点数
    {
        mc::string str    = "3.14159";
        double     result = 0;
        ASSERT_TRUE(mc::strings::try_to_number(str, result));
        ASSERT_DOUBLE_EQ(result, 3.14159);

        // 验证 string_view
        mc::string      long_str = "3.14159265359";
        mc::string_view view(long_str.data(), 7); // 只取 "3.14159"
        ASSERT_TRUE(mc::strings::try_to_number(view, result));
        ASSERT_DOUBLE_EQ(result, 3.14159);
    }

    // 测试溢出情况
    {
        mc::string str    = "999999999999999999999"; // 超过 int64_t 范围
        int64_t    result = 0;
        ASSERT_FALSE(mc::strings::try_to_number(str, result));
        // 当数值溢出时，std::strtoll 会设置 errno = ERANGE
        ASSERT_EQ(errno, ERANGE);

        // 验证 to_number 抛出异常
        EXPECT_THROW(mc::to_number<int64_t>(str), mc::overflow_exception);
    }
}

/**
 * @brief 测试字符串转数字的默认值功能
 */
TEST_F(StringTest, TestStringToNumberWithDefault)
{
    // 测试正常转换
    ASSERT_EQ(mc::to_number_with_default<int>("42", -1), 42);
    ASSERT_EQ(mc::to_number_with_default<int>(mc::string("42"), -1), 42);
    ASSERT_EQ(mc::to_number_with_default<int>(mc::string_view("42"), -1), 42);

    // 测试转换失败返回默认值
    ASSERT_EQ(mc::to_number_with_default<int>("abc", -1), -1);
    ASSERT_EQ(mc::to_number_with_default<int>(mc::string("abc"), -1), -1);
    ASSERT_EQ(mc::to_number_with_default<int>(mc::string_view("abc"), -1), -1);

    // 测试空字符串返回默认值
    ASSERT_EQ(mc::to_number_with_default<int>("", -1), -1);
    ASSERT_EQ(mc::to_number_with_default<int>(mc::string(), -1), -1);
    ASSERT_EQ(mc::to_number_with_default<int>(mc::string_view(), -1), -1);

    // 测试溢出返回默认值
    ASSERT_EQ(mc::to_number_with_default<int>("999999999999999999999", -1), -1);
}

// 测试 prepare_number_string 缓冲区大小不足的情况
TEST_F(StringTest, PrepareNumberStringBufferTooSmall)
{
    char buffer[1]; // 非常小的缓冲区
    auto result = mc::strings::detail::prepare_number_string("123", 10, buffer, 1);
    // 当缓冲区太小时，应该返回空 string_view
    EXPECT_TRUE(result.empty());
}

// 测试 try_to_bool 空字符串情况
TEST_F(StringTest, TryToBoolEmptyString)
{
    bool result = true;
    EXPECT_TRUE(mc::strings::try_to_bool("", result));
    EXPECT_FALSE(result);
}

// 测试 to_bool 函数
TEST_F(StringTest, ToBoolTest)
{
    // 测试 "true" 返回 true
    EXPECT_TRUE(mc::strings::to_bool("true"));
    EXPECT_TRUE(mc::strings::to_bool("TRUE"));
    EXPECT_TRUE(mc::strings::to_bool("True"));
    EXPECT_TRUE(mc::strings::to_bool("1"));

    // 测试 "false" 返回 false
    EXPECT_FALSE(mc::strings::to_bool("false"));
    EXPECT_FALSE(mc::strings::to_bool("FALSE"));
    EXPECT_FALSE(mc::strings::to_bool("False"));
    EXPECT_FALSE(mc::strings::to_bool("0"));

    // 测试空字符串返回 false
    EXPECT_FALSE(mc::strings::to_bool(""));

    // 测试无效字符串抛出异常
    EXPECT_THROW(mc::strings::to_bool("invalid"), mc::invalid_arg_exception);
}

// 测试 iequals 的空指针处理
TEST_F(StringTest, IEqualsNullPointer)
{
    // 测试两个 nullptr 返回 true
    EXPECT_TRUE(mc::strings::iequals(nullptr, nullptr));

    // 测试 nullptr 与非空字符串返回 false
    EXPECT_FALSE(mc::strings::iequals(nullptr, "test"));
    EXPECT_FALSE(mc::strings::iequals("test", nullptr));
}

// 测试空分隔符的 split
TEST_F(StringTest, SplitEmptyDelimiter)
{
    auto result = split("abc", "");
    // 当分隔符为空时，应该返回包含整个字符串的向量
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "abc");
}

// 测试空前缀的 starts_with
TEST_F(StringTest, StartsWithEmptyPrefix)
{
    // 当前缀为空时，应该返回 true
    EXPECT_TRUE(starts_with("test", ""));
}

// 测试 longest_common_prefix 函数
TEST_F(StringTest, LongestCommonPrefixTest)
{
    // 测试有公共前缀的情况
    EXPECT_EQ(longest_common_prefix("abc", "ab"), "ab");

    // 测试没有公共前缀的情况
    EXPECT_EQ(longest_common_prefix("abc", "def"), "");

    // 测试空字符串的情况
    EXPECT_EQ(longest_common_prefix("", "abc"), "");
    EXPECT_EQ(longest_common_prefix("abc", ""), "");
}

// 测试空 from 的 replace_all
TEST_F(StringTest, ReplaceAllEmptyFrom)
{
    // 当 from 为空时，应该返回原字符串的副本
    EXPECT_EQ(replace_all("test", "", "x"), "test");
}

// 测试空 from 的 replace_all_inplace
TEST_F(StringTest, ReplaceAllInplaceEmptyFrom)
{
    mc::string s = "test";
    // 当 from 为空时，应该直接返回，不修改字符串
    replace_all_inplace(s, "", "x");
    EXPECT_EQ(s, "test");
}

// 测试 icontains 的边界情况
TEST_F(StringTest, IContainsEdgeCases)
{
    // 测试空字符串
    EXPECT_FALSE(icontains("", "test"));

    // 测试空子字符串
    EXPECT_TRUE(icontains("test", ""));

    // 测试 s.size() < substring.size()
    EXPECT_FALSE(icontains("ab", "abc"));
}

// 测试 icontains 的大字符串处理
TEST_F(StringTest, IContainsLargeString)
{
    // 创建一个长度超过 1024 的字符串
    mc::string large_string(2000, 'a');
    large_string += "TEST";
    large_string += mc::string(2000, 'b');

    // 测试在大字符串中查找
    EXPECT_TRUE(icontains(large_string, "test"));
    EXPECT_FALSE(icontains(large_string, "xyz"));
}

// 测试 substr 的结束位置超出长度
TEST_F(StringTest, SubstrEndExceedsLength)
{
    // 当结束位置超出长度时，应该调整到字符串末尾
    EXPECT_EQ(substr("test", 0, 100), "test");
}

// 测试 fixed_width_append 的右对齐
TEST_F(StringTest, FixedWidthAppendRightAlign)
{
    mc::string result;
    fixed_width_append(result, 10, "test", false); // left_align = false 表示右对齐
    // 右对齐时，应该先添加空格，再添加字符串
    EXPECT_EQ(result, "      test");
    EXPECT_EQ(result.size(), 10);
}

// 测试无效的双字节 UTF-8 字符
TEST_F(StringTest, IsValidUtf8InvalidTwoByte)
{
    // 测试不完整的双字节字符（只有 1 个字节）
    mc::string invalid1 = "\xC2"; // 只有起始字节，缺少后续字节
    EXPECT_FALSE(is_valid_utf8(invalid1));

    // 测试第二个字节不符合格式的双字节字符
    mc::string invalid2 = "\xC2\x20"; // 第二个字节应该是 0x80-0xBF，但这里是 0x20
    EXPECT_FALSE(is_valid_utf8(invalid2));

    // 测试过长编码的双字节字符（实际上双字节字符最多 2 字节，这里测试边界情况）
    // 双字节字符范围是 U+0080 到 U+07FF
    mc::string invalid3 = "\xC2\xC0"; // 第二个字节不符合格式
    EXPECT_FALSE(is_valid_utf8(invalid3));
}

// 测试无效的三字节 UTF-8 字符
TEST_F(StringTest, IsValidUtf8InvalidThreeByte)
{
    // 测试不完整的三字节字符（只有 2 个字节）
    mc::string invalid1 = "\xE0\xA0"; // 只有前两个字节，缺少第三个字节
    EXPECT_FALSE(is_valid_utf8(invalid1));

    // 测试第二、三个字节不符合格式的三字节字符
    mc::string invalid2 = "\xE0\x20\xA0"; // 第二个字节应该是 0xA0-0xBF，但这里是 0x20
    EXPECT_FALSE(is_valid_utf8(invalid2));

    mc::string invalid3 = "\xE0\xA0\x20"; // 第三个字节应该是 0x80-0xBF，但这里是 0x20
    EXPECT_FALSE(is_valid_utf8(invalid3));

    // 测试代理区间的三字节字符（U+D800 到 U+DFFF 是代理区间，不应该出现在 UTF-8 中）
    mc::string invalid4 = "\xED\xA0\x80"; // U+D800，代理区间
    EXPECT_FALSE(is_valid_utf8(invalid4));
}

// 测试四字节 UTF-8 字符
TEST_F(StringTest, IsValidUtf8FourByte)
{
    // 测试有效的四字节字符
    mc::string valid = "\xF0\x90\x80\x80"; // U+10000
    EXPECT_TRUE(is_valid_utf8(valid));

    // 测试不完整的四字节字符（只有 3 个字节）
    mc::string invalid1 = "\xF0\x90\x80"; // 缺少第四个字节
    EXPECT_FALSE(is_valid_utf8(invalid1));

    // 测试第二、三、四个字节不符合格式的四字节字符
    mc::string invalid2 = "\xF0\x20\x80\x80"; // 第二个字节应该是 0x90-0xBF，但这里是 0x20
    EXPECT_FALSE(is_valid_utf8(invalid2));

    mc::string invalid3 = "\xF0\x90\x20\x80"; // 第三个字节应该是 0x80-0xBF，但这里是 0x20
    EXPECT_FALSE(is_valid_utf8(invalid3));

    mc::string invalid4 = "\xF0\x90\x80\x20"; // 第四个字节应该是 0x80-0xBF，但这里是 0x20
    EXPECT_FALSE(is_valid_utf8(invalid4));

    // 测试超出 Unicode 范围的四字节字符（U+10FFFF 是最大值）
    mc::string invalid5 = "\xF4\x90\x80\x80"; // U+110000，超出范围
    EXPECT_FALSE(is_valid_utf8(invalid5));
}

// 测试无效的 UTF-8 起始字节
TEST_F(StringTest, IsValidUtf8InvalidStartByte)
{
    // 测试包含无效起始字节的字符串
    mc::string invalid1 = "\xFF"; // 0xFF 不是有效的 UTF-8 起始字节
    EXPECT_FALSE(is_valid_utf8(invalid1));

    mc::string invalid2 = "\xFE"; // 0xFE 不是有效的 UTF-8 起始字节
    EXPECT_FALSE(is_valid_utf8(invalid2));

    mc::string invalid3 = "test\xFFtest"; // 中间包含无效字节
    EXPECT_FALSE(is_valid_utf8(invalid3));
}

// 测试 to_string(double) 移除末尾小数点
TEST_F(StringTest, ToStringDoubleRemoveTrailingDot)
{
    // 当浮点数的小数部分全为 0 时，应该移除末尾的小数点
    mc::string result = to_string(123.0);
    EXPECT_EQ(result, "123"); // 不包含小数点
}

// 测试 to_string(bool) 函数
TEST_F(StringTest, ToStringBoolTest)
{
    // 测试 true 返回 "true"
    EXPECT_EQ(to_string(true), "true");

    // 测试 false 返回 "false"
    EXPECT_EQ(to_string(false), "false");
}

// 测试 to_string(mc::string&, bool) 函数
TEST_F(StringTest, ToStringBoolAppendTest)
{
    mc::string result;

    // 测试 to_string(result, true) 将 "true" 追加到 result
    to_string(result, true);
    EXPECT_EQ(result, "true");

    // 测试 to_string(result, false) 将 "false" 追加到 result
    result.clear();
    to_string(result, false);
    EXPECT_EQ(result, "false");
}

TEST_F(StringTest, StringContractSubstrMatchesStdString)
{
    mc::string value = "abcdef";

    constexpr bool returns_string =
        std::is_same_v<decltype(std::declval<const mc::string&>().substr(1, 3)), mc::string>;
    EXPECT_TRUE(returns_string) << "mc::string::substr 应返回 owning mc::string";

    EXPECT_EQ(value.substr(1, 3), "bcd") << "mc::string::substr 应遵循标准字符串的 pos/count 语义";
    EXPECT_EQ(value.substr(2), "cdef") << "省略 count 时应返回到末尾";
}

TEST_F(StringTest, StringContractSubstrProducesIndependentString)
{
    mc::string value = "abcdef";
    mc::string slice = value.substr(1, 3);

    value[2] = 'Z';

    EXPECT_EQ(slice, "bcd") << "substr 返回结果不应在原字符串修改后跟着变化";
}

TEST_F(StringTest, StringContractExposesMutableData)
{
    EXPECT_TRUE(has_mutable_data<mc::string>::value) << "mc::string 应提供 char* data() noexcept";
}

TEST_F(StringTest, StringContractExposesCapacity)
{
    EXPECT_TRUE(has_capacity<mc::string>::value) << "mc::string 应提供 capacity() 以兼容标准字符串";
}

TEST_F(StringTest, StringContractCStrIsNullTerminated)
{
    mc::string value = "compatibility";

    EXPECT_EQ(std::char_traits<char>::length(value.c_str()), value.size())
        << "mc::string::c_str() 应始终返回以 '\\0' 结尾的 C 字符串";
}

TEST_F(StringTest, StringContractCopyOnWritePreservesOtherCopies)
{
    mc::string original = "hello";
    mc::string copy     = original;

    copy.push_back('!');

    EXPECT_EQ(original, "hello");
    EXPECT_EQ(copy, "hello!");
}

TEST_F(StringTest, StringContractPreservesEmbeddedNull)
{
    mc::string value("ab\0cd", 5);

    value.append(mc::string_view("ef", 2));

    EXPECT_EQ(value.size(), 7U);
    EXPECT_EQ(value[2], '\0');
    EXPECT_EQ(value.view(), mc::string_view("ab\0cdef", 7));
}

TEST_F(StringTest, StringStdInteropCompatibility)
{
    mc::string value = "hello";

    std::string_view std_view = value;
    EXPECT_EQ(std_view, "hello");

    constexpr std::string_view source = "world";
    constexpr mc::string_view  compat(source);
    static_assert(compat.size() == 5, "mc::string_view 应可在 constexpr 上下文从 std::string_view 构造");

    EXPECT_EQ(compat, "world");
}

TEST_F(StringTest, StringLegacyStaticHelperCompatibility)
{
    std::vector<std::string> parts = {"a", "b", "c"};
    std::string              mutable_text = "  hello  ";

    EXPECT_EQ(mc::string::join(parts, ","), "a,b,c");
    EXPECT_EQ(mc::string::concat("pre", std::string("_mid"), mc::string_view("_tail")), "pre_mid_tail");
    EXPECT_EQ(mc::string::substring("abcdef", 1, 3), "bcd");
    EXPECT_EQ(mc::string::to_upper("hello"), "HELLO");
    EXPECT_EQ(mc::string::to_lower("HELLO"), "hello");
    EXPECT_EQ(mc::string::trim("  hello  "), "hello");
    mc::string::trim_inplace(mutable_text);
    EXPECT_EQ(mutable_text, "hello");
    EXPECT_TRUE(mc::string::starts_with(std::string_view("prefix_value"), "prefix"));
    EXPECT_TRUE(mc::string::contains(std::string_view("prefix_value"), "value"));
    EXPECT_EQ(mc::string::replace_all(std::string_view("prefix_body_tail"), "body", "core"), "prefix_core_tail");
}

TEST_F(StringTest, StringStdViewComparisonCompatibility)
{
    std::string_view lhs = "value";
    mc::string_view  rhs = "value";

    EXPECT_TRUE(lhs == "value");
    EXPECT_FALSE(lhs != "value");
    EXPECT_TRUE(lhs == rhs);
    EXPECT_FALSE(lhs != rhs);
}

TEST_F(StringTest, TimePointImplicitStdStringViewCompatibility)
{
    std::string_view now = mc::time_point::now();
    EXPECT_FALSE(now.empty());
}

} // namespace test
} // namespace mc