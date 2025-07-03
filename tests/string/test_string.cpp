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

using namespace mc::string;

namespace mc {
namespace test {

class StringTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 在每个测试前执行
    }

    void TearDown() override {
        // 在每个测试后执行
    }
};

/**
 * @brief 测试忽略大小写比较函数
 */
TEST_F(StringTest, IEqualsTest) {
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

    // 测试 std::string 版本
    ASSERT_TRUE(iequals(std::string("test"), std::string("TEST")))
        << "std::string 版本应该正常工作";

    // 测试 std::string_view 版本
    ASSERT_TRUE(iequals(std::string_view("test"), std::string_view("TEST")))
        << "std::string_view 版本应该正常工作";
}

/**
 * @brief 测试字符串转换函数
 */
TEST_F(StringTest, StringConversionTest) {
    // 测试转换为小写
    ASSERT_EQ(to_lower("TEST"), "test") << "转换为小写应该正确";
    ASSERT_EQ(to_lower("Test"), "test") << "转换为小写应该正确";

    // 测试转换为大写
    ASSERT_EQ(to_upper("test"), "TEST") << "转换为大写应该正确";
    ASSERT_EQ(to_upper("Test"), "TEST") << "转换为大写应该正确";

    // 测试原地转换为小写
    std::string s1 = "TEST";
    to_lower_inplace(s1);
    ASSERT_EQ(s1, "test") << "原地转换为小写应该正确";

    // 测试原地转换为大写
    std::string s2 = "test";
    to_upper_inplace(s2);
    ASSERT_EQ(s2, "TEST") << "原地转换为大写应该正确";
}

/**
 * @brief 测试字符串修剪函数
 */
TEST_F(StringTest, StringTrimTest) {
    // 测试修剪两端空白
    ASSERT_EQ(trim("  test  "), "test") << "修剪两端空白应该正确";
    ASSERT_EQ(trim("\t\ntest\r\n"), "test") << "修剪两端空白应该正确";

    // 测试修剪左侧空白
    ASSERT_EQ(ltrim("  test  "), "test  ") << "修剪左侧空白应该正确";

    // 测试修剪右侧空白
    ASSERT_EQ(rtrim("  test  "), "  test") << "修剪右侧空白应该正确";

    // 测试原地修剪
    std::string s1 = "  test  ";
    trim_inplace(s1);
    ASSERT_EQ(s1, "test") << "原地修剪两端空白应该正确";

    std::string s2 = "  test  ";
    ltrim_inplace(s2);
    ASSERT_EQ(s2, "test  ") << "原地修剪左侧空白应该正确";

    std::string s3 = "  test  ";
    rtrim_inplace(s3);
    ASSERT_EQ(s3, "  test") << "原地修剪右侧空白应该正确";
}

/**
 * @brief 测试字符串分割和连接函数
 */
TEST_F(StringTest, StringSplitJoinTest) {
    // 测试按字符分割
    std::vector<std::string> parts1 = split("a,b,c", ',');
    ASSERT_EQ(parts1.size(), 3) << "分割后应该有3个元素";
    ASSERT_EQ(parts1[0], "a") << "第一个元素应该是a";
    ASSERT_EQ(parts1[1], "b") << "第二个元素应该是b";
    ASSERT_EQ(parts1[2], "c") << "第三个元素应该是c";

    // 测试按字符串分割
    std::vector<std::string> parts2 = split("a::b::c", "::");
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
TEST_F(StringTest, SubstrLuaStyleTest) {
    const std::string test_str = "Hello, World!";

    // 测试正常情况下的子字符串提取（正数索引）
    ASSERT_EQ(substr(test_str, 0, 4), "Hello") << "从开头提取子字符串应该正确";
    ASSERT_EQ(substr(test_str, 7, 11), "World") << "从中间提取子字符串应该正确";

    // 测试负数索引
    ASSERT_EQ(substr(test_str, -6, -2), "World") << "使用负数索引提取子字符串应该正确";
    ASSERT_EQ(substr(test_str, -13, -8), "Hello,") << "使用负数索引从开头提取子字符串应该正确";

    // 测试默认结束位置
    ASSERT_EQ(substr(test_str, 7), "World!") << "省略结束位置应该提取到字符串末尾";
    ASSERT_EQ(substr(test_str, -6), "World!") << "使用负数起始索引并省略结束位置应该正确";

    // 测试完整字符串
    ASSERT_EQ(substr(test_str, 0, -1), "Hello, World!") << "提取完整字符串应该正确";
    ASSERT_EQ(substr(test_str, 0, test_str.length() - 1), "Hello, World!") << "使用字符串长度作为结束位置应该正确";

    // 测试边界情况
    ASSERT_EQ(substr(test_str, 0, 0), "H") << "提取单个字符应该正确";
    ASSERT_EQ(substr(test_str, -1, -1), "!") << "提取最后一个字符应该正确";

    // 测试超出范围的情况
    ASSERT_EQ(substr(test_str, 100, 200), "") << "超出范围的索引应该返回空字符串";
    ASSERT_EQ(substr(test_str, -100, -50), "") << "超出范围的负数索引应该返回空字符串";
    ASSERT_EQ(substr(test_str, 5, 2), "") << "结束索引小于起始索引应该返回空字符串";

    // 测试空字符串
    ASSERT_EQ(substr("", 0, 5), "") << "空字符串应该返回空字符串";
}

/**
 * @brief 测试以长度为参数的子字符串函数
 */
TEST_F(StringTest, SubstringTest) {
    const std::string test_str = "Hello, World!";

    // 测试正常情况下的子字符串提取（正数索引）
    ASSERT_EQ(substring(test_str, 0, 5), "Hello") << "从开头提取指定长度的子字符串应该正确";
    ASSERT_EQ(substring(test_str, 7, 5), "World") << "从中间提取指定长度的子字符串应该正确";

    // 测试负数索引
    ASSERT_EQ(substring(test_str, -6, 5), "World") << "使用负数索引提取指定长度的子字符串应该正确";
    ASSERT_EQ(substring(test_str, -13, 6), "Hello,") << "使用负数索引从开头提取指定长度的子字符串应该正确";

    // 测试默认长度（提取到末尾）
    ASSERT_EQ(substring(test_str, 7), "World!") << "默认长度应该提取到字符串末尾";
    ASSERT_EQ(substring(test_str, -6), "World!") << "使用负数起始索引并使用默认长度应该正确";

    // 测试完整字符串
    ASSERT_EQ(substring(test_str, 0), "Hello, World!") << "从开头提取整个字符串应该正确";
    ASSERT_EQ(substring(test_str, 0, test_str.length()), "Hello, World!") << "使用字符串长度作为长度应该正确";

    // 测试长度超出字符串范围
    ASSERT_EQ(substring(test_str, 0, 100), "Hello, World!") << "超出范围的长度应该被调整";
    ASSERT_EQ(substring(test_str, 7, 100), "World!") << "从中间开始超出范围的长度应该被调整";
    ASSERT_EQ(substring(test_str, -6, 100), "World!") << "使用负索引时超出范围的长度应该被调整";

    // 测试零长度
    ASSERT_EQ(substring(test_str, 0, 0), "") << "零长度应该返回空字符串";
    ASSERT_EQ(substring(test_str, 5, 0), "") << "从中间开始的零长度应该返回空字符串";

    // 测试超出范围的索引
    ASSERT_EQ(substring(test_str, 100, 5), "") << "超出范围的索引应该返回空字符串";
    ASSERT_EQ(substring(test_str, -100, 5), "") << "超出范围的负数索引应该返回空字符串";

    // 测试空字符串
    ASSERT_EQ(substring("", 0, 5), "") << "空字符串应该返回空字符串";
}

/**
 * @brief 测试字符串前缀和后缀检查函数
 */
TEST_F(StringTest, StringPrefixSuffixTest) {
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
TEST_F(StringTest, StringReplaceTest) {
    // 测试替换
    ASSERT_EQ(replace_all("test test", "test", "abc"), "abc abc") << "替换应该正确";

    // 测试原地替换
    std::string s = "test test";
    replace_all_inplace(s, "test", "abc");
    ASSERT_EQ(s, "abc abc") << "原地替换应该正确";
}

/**
 * @brief 测试字符串包含函数
 */
TEST_F(StringTest, StringContainsTest) {
    // 测试包含
    ASSERT_TRUE(contains("test", "es")) << "包含检查应该正确";
    ASSERT_FALSE(contains("test", "abc")) << "包含检查应该正确";

    // 测试忽略大小写包含
    ASSERT_TRUE(icontains("TEST", "es")) << "忽略大小写包含检查应该正确";
    ASSERT_FALSE(icontains("TEST", "abc")) << "忽略大小写包含检查应该正确";
}

/**
 * @brief 测试字符串格式化函数
 */
TEST_F(StringTest, FormatWithDictTest) {
    // 测试使用dict进行格式化
    mc::dict args{{"host", "example.com"},
                  {"port", 8080},
                  {"protocol", "https"},
                  {"enabled", true},
                  {"ratio", 0.75}};

    // 基本替换测试
    std::string result = mc::format("连接到 ${host}:${port}", args);
    ASSERT_EQ(result, "连接到 example.com:8080") << "基本替换应该正确";

    // 测试不同类型的值
    result = mc::format("协议: ${protocol}, 端口: ${port}, 启用: ${enabled}, 比率: ${ratio}", args);
    ASSERT_TRUE(contains(result, "协议: https")) << "字符串值替换应该正确";
    ASSERT_TRUE(contains(result, "端口: 8080")) << "整数值替换应该正确";
    ASSERT_TRUE(contains(result, "启用: true")) << "布尔值替换应该正确";
    ASSERT_TRUE(contains(result, "比率: 0.")) << "浮点值替换应该正确";

    // 测试不存在的键
    result = mc::format("${host}:${missing}", args);
    ASSERT_TRUE(contains(result, "example.com")) << "存在的键应该被替换";
    ASSERT_TRUE(contains(result, "${missing}")) << "不存在的键应该保留原始占位符";

    // 测试格式错误
    result = mc::format("${host:${port}", args);
    // 我们不确定具体的输出格式，但应该包含原始输入的一部分或替换后的值
    ASSERT_TRUE(contains(result, "example.com") || contains(result, "${host:") ||
                contains(result, "8080"))
        << "格式错误应该处理部分占位符或保留原始文本";

    // 测试空字符串
    result = mc::format("", args);
    ASSERT_EQ(result, "") << "空字符串应该返回空字符串";

    // 测试没有占位符的字符串
    result = mc::format("没有占位符的字符串", args);
    ASSERT_EQ(result, "没有占位符的字符串") << "没有占位符的字符串应该保持不变";

    // 测试简单字符串值
    mc::dict simple_args{{"simple_key", "simple_value"}};
    result = mc::format("简单替换: ${simple_key}", simple_args);
    ASSERT_EQ(result, "简单替换: simple_value") << "简单替换应该正确";

    // 测试新format重载函数，将结果追加到现有字符串
    std::string append_result = "前缀：";
    mc_format_append(append_result, "连接到 ${host}:${port}",
                     ("host", "example.com")("port", "8080"));
    ASSERT_EQ(append_result, "前缀：连接到 example.com:8080") << "追加格式化结果应该正确";

    // 测试新format重载函数，使用空字符串
    std::string empty_result;
    mc_format_append(empty_result, "协议: ${protocol}", ("protocol", "https"));
    ASSERT_EQ(empty_result, "协议: https") << "从空字符串开始格式化应该正确";

    // 测试宏的其他功能
    // 测试无参数情况
    result = mc_format("无参数字符串");
    ASSERT_EQ(result, "无参数字符串") << "无参数调用应该返回原始字符串";

    // 测试多个参数
    result = mc_format("${a}-${b}-${c}", ("a", 1)("b", 2.5)("c", "文本"));
    ASSERT_EQ(result, "1-2.5-文本") << "多参数调用应该正确格式化";
}

TEST_F(StringTest, FormatIcaseTest) {
    mc::dict args{{"host", "example.com"},
                  {"port", 8080},
                  {"protocol", "https"},
                  {"enabled", true},
                  {"ratio", 0.75}};

    std::string result = mc::format_icase("${host}:${port}", args);
    ASSERT_EQ(result, "example.com:8080") << "大小写不敏感格式化应该正确";

    result = mc::format_icase("${HOST}:${port}", args);
    ASSERT_EQ(result, "example.com:8080") << "大小写不敏感格式化应该正确";

    result = mc::format_icase("${host}:${PORT}", args);
    ASSERT_EQ(result, "example.com:8080") << "大小写不敏感格式化应该正确";

    result = mc::format_icase("${HOST}:${PORT}", args);
    ASSERT_EQ(result, "example.com:8080") << "大小写不敏感格式化应该正确";

    result = mc::format("${HOST}:${PORT}", args);
    ASSERT_EQ(result, "${HOST}:${PORT}") << "大小写敏感格式化应该失败";

    result = mc::format("${HOST}:${port}", args);
    ASSERT_EQ(result, "${HOST}:8080") << "大小写敏感格式化应该失败";
}

TEST_F(StringTest, JoinTest) {
    ASSERT_EQ(join(",", "a", "b", "c"), "a,b,c");
    ASSERT_EQ(join(",", "a", "b", "c", "d"), "a,b,c,d");
    ASSERT_EQ(join(",", "a", "b", "c", "d", "e"), "a,b,c,d,e");

    ASSERT_EQ(join(std::vector<std::string>{"a", "b", "c"}, ","), "a,b,c");
    ASSERT_EQ(join(std::vector<std::string>{"a", "b", "c", "d"}, ","), "a,b,c,d");
    ASSERT_EQ(join(std::vector<std::string>{"a", "b", "c", "d", "e"}, ","), "a,b,c,d,e");
}

TEST_F(StringTest, ConcatTest) {
    ASSERT_EQ(concat("a", "b", "c"), "abc");
    ASSERT_EQ(concat("a", "b", "c", "d"), "abcd");
    ASSERT_EQ(concat("a", "b", "c", "d", "e"), "abcde");

    ASSERT_EQ(concat(std::vector<std::string>{"a", "b", "c"}), "abc");
    ASSERT_EQ(concat(std::vector<std::string>{"a", "b", "c", "d"}), "abcd");
    ASSERT_EQ(concat(std::vector<std::string>{"a", "b", "c", "d", "e"}), "abcde");
}

TEST_F(StringTest, SplitIteratorTest) {
    // 测试空字符串
    {
        std::string_view str;
        auto             it = split_iterator(str, ".");
        EXPECT_EQ(it, split_iterator::end());
    }

    // 测试单个分隔符
    {
        std::string_view              str = "a.b.c";
        std::vector<std::string_view> parts;
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
        std::string_view              str = "a::b.c::d";
        std::vector<std::string_view> parts;
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
        std::string_view              str = "a...b::c";
        std::vector<std::string_view> parts;
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
        std::string_view              str = "..a..b..";
        std::vector<std::string_view> parts;
        for (auto it = split_iterator(str, "."); it != split_iterator::end(); ++it) {
            parts.push_back(*it);
        }
        ASSERT_EQ(parts.size(), 2);
        EXPECT_EQ(parts[0], "a");
        EXPECT_EQ(parts[1], "b");
    }
}

TEST(string, split_iterator) {
    // 基本分割测试
    {
        std::string                   str = "a.b.c";
        std::vector<std::string_view> parts;
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
        std::string                   str = "a.b:c";
        std::vector<std::string_view> parts;
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
        std::string                   str = "a..b:::c";
        std::vector<std::string_view> parts;
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
        std::string                   str = "";
        std::vector<std::string_view> parts;
        for (auto it = split_iterator(str, "."); it != split_iterator::end(); ++it) {
            parts.push_back(*it);
        }
        ASSERT_TRUE(parts.empty());
    }

    // 只有分隔符的字符串测试
    {
        std::string                   str = "...:::";
        std::vector<std::string_view> parts;
        for (auto it = split_iterator(str, ".:"); it != split_iterator::end(); ++it) {
            parts.push_back(*it);
        }
        ASSERT_TRUE(parts.empty());
    }

    // 前后有分隔符的字符串测试
    {
        std::string                   str = "..a:b..c::";
        std::vector<std::string_view> parts;
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
TEST_F(StringTest, TestStringToNumberSafety) {
    // 测试 string_view 的安全性
    {
        // 构造一个较长的字符串，包含合法数字后跟其他字符
        std::string long_str = "42000hello";

        // 创建一个只包含数字部分的 string_view
        std::string_view num_view(long_str.data(), 2); // 只取 "42"

        // 验证转换
        int result = 0;
        ASSERT_TRUE(mc::try_to_number(num_view, result));
        ASSERT_EQ(result, 42);

        // 验证 to_number
        ASSERT_EQ(mc::to_number<int>(num_view), 42);
    }

    // 测试 std::string 的安全性
    {
        std::string str    = "42";
        int         result = 0;
        ASSERT_TRUE(mc::try_to_number(str, result));
        ASSERT_EQ(result, 42);

        // 验证 to_number
        ASSERT_EQ(mc::to_number<int>(str), 42);
    }

    // 测试 C 风格字符串的安全性
    {
        const char* str    = "42";
        int         result = 0;
        ASSERT_TRUE(mc::try_to_number(str, result));
        ASSERT_EQ(result, 42);

        // 验证 to_number
        ASSERT_EQ(mc::to_number<int>(str), 42);
    }

    // 测试不同进制的边界情况
    {
        // 二进制
        std::string      bin_str(64, '1'); // 64个1
        std::string_view bin_view(bin_str);
        uint64_t         result = 0;
        ASSERT_TRUE(mc::try_to_number(bin_view, result, 2));
        ASSERT_EQ(result, std::numeric_limits<uint64_t>::max());

        // 八进制
        std::string      oct_str = "1777777777777777777777"; // 最大64位八进制
        std::string_view oct_view(oct_str);
        ASSERT_TRUE(mc::try_to_number(oct_view, result, 8));
        ASSERT_EQ(result, std::numeric_limits<uint64_t>::max());

        // 十六进制
        std::string      hex_str = "FFFFFFFFFFFFFFFF"; // 最大64位十六进制
        std::string_view hex_view(hex_str);
        ASSERT_TRUE(mc::try_to_number(hex_view, result, 16));
        ASSERT_EQ(result, std::numeric_limits<uint64_t>::max());
    }

    // 测试超长字符串
    {
        // 二进制超长
        std::string too_long_bin(65, '1');
        int         result = 0;
        ASSERT_FALSE(mc::try_to_number(too_long_bin, result, 2));

        // 八进制超长
        std::string too_long_oct(23, '7');
        ASSERT_FALSE(mc::try_to_number(too_long_oct, result, 8));

        // 十六进制超长
        std::string too_long_hex(17, 'F');
        ASSERT_FALSE(mc::try_to_number(too_long_hex, result, 16));

        // 十进制超长
        std::string too_long_dec(21, '9');
        ASSERT_FALSE(mc::try_to_number(too_long_dec, result));
    }

    // 测试字符串后面有非数字字符的情况
    {
        // string_view 情况
        std::string      str1 = "42abc";
        std::string_view view1(str1.data(), 2); // 只取 "42"
        int              result = 0;
        ASSERT_TRUE(mc::try_to_number(view1, result));
        ASSERT_EQ(result, 42);

        // string 情况
        std::string str2 = "42abc";
        ASSERT_FALSE(mc::try_to_number(str2, result));

        // C 字符串情况
        const char* str3 = "42abc";
        ASSERT_FALSE(mc::try_to_number(str3, result));
    }

    // 测试空字符串
    {
        int result = 0;
        ASSERT_FALSE(mc::try_to_number(std::string_view{}, result));
        ASSERT_FALSE(mc::try_to_number(std::string{}, result));
        ASSERT_FALSE(mc::try_to_number("", result));
    }

    // 测试带符号数
    {
        std::string str    = "-42";
        int         result = 0;
        ASSERT_TRUE(mc::try_to_number(str, result));
        ASSERT_EQ(result, -42);

        // 验证 string_view
        std::string      long_str = "-42000";
        std::string_view view(long_str.data(), 3); // 只取 "-42"
        ASSERT_TRUE(mc::try_to_number(view, result));
        ASSERT_EQ(result, -42);
    }

    // 测试浮点数
    {
        std::string str    = "3.14159";
        double      result = 0;
        ASSERT_TRUE(mc::try_to_number(str, result));
        ASSERT_DOUBLE_EQ(result, 3.14159);

        // 验证 string_view
        std::string      long_str = "3.14159265359";
        std::string_view view(long_str.data(), 7); // 只取 "3.14159"
        ASSERT_TRUE(mc::try_to_number(view, result));
        ASSERT_DOUBLE_EQ(result, 3.14159);
    }

    // 测试溢出情况
    {
        std::string str    = "999999999999999999999"; // 超过 int64_t 范围
        int64_t     result = 0;
        ASSERT_FALSE(mc::try_to_number(str, result));
        ASSERT_EQ(errno, ERANGE);

        // 验证 to_number 抛出异常
        EXPECT_THROW(mc::to_number<int64_t>(str), mc::overflow_exception);
    }
}

/**
 * @brief 测试字符串转数字的默认值功能
 */
TEST_F(StringTest, TestStringToNumberWithDefault) {
    // 测试正常转换
    ASSERT_EQ(mc::to_number_with_default<int>("42", -1), 42);
    ASSERT_EQ(mc::to_number_with_default<int>(std::string("42"), -1), 42);
    ASSERT_EQ(mc::to_number_with_default<int>(std::string_view("42"), -1), 42);

    // 测试转换失败返回默认值
    ASSERT_EQ(mc::to_number_with_default<int>("abc", -1), -1);
    ASSERT_EQ(mc::to_number_with_default<int>(std::string("abc"), -1), -1);
    ASSERT_EQ(mc::to_number_with_default<int>(std::string_view("abc"), -1), -1);

    // 测试空字符串返回默认值
    ASSERT_EQ(mc::to_number_with_default<int>("", -1), -1);
    ASSERT_EQ(mc::to_number_with_default<int>(std::string(), -1), -1);
    ASSERT_EQ(mc::to_number_with_default<int>(std::string_view(), -1), -1);

    // 测试溢出返回默认值
    ASSERT_EQ(mc::to_number_with_default<int>("999999999999999999999", -1), -1);
}

} // namespace test
} // namespace mc