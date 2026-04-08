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

#include <iostream>
#include <limits>

#include <mc/fmt/format.h>
#include <mc/fmt/format_context.h>
#include <mc/fmt/format_dict.h>
#include <mc/exception.h>

using namespace mc::fmt;

TEST(format_dict_test, named_format_use_dict) {
    mc::dict args = {
        {"first", "first"},
        {"second", "second"}};

    EXPECT_EQ(mc::format_dict("${first} ${second}", args), "first second");
    EXPECT_EQ(mc::format_dict("{} {}", args), "first second"); // 字典也可以按索引访问
    EXPECT_EQ(mc::format_dict("{0:{1}.{2}f}", mc::dict{
                                                  {"value", 1.23456},
                                                  {"width", 8},
                                                  {"precision", 3}}),
              "   1.235"); // 动态参数支持从字典读取
}

static bool contains(std::string_view str, std::string_view substr) {
    return str.find(substr) != std::string_view::npos;
}

/**
 * @brief 测试字符串格式化函数
 */
TEST(format_dict_test, FormatWithDictTest) {
    // 测试使用dict进行格式化
    mc::dict args{{"host", "example.com"},
                  {"port", 8080},
                  {"protocol", "https"},
                  {"enabled", true},
                  {"ratio", 0.75}};

    // 基本替换测试
    std::string result = mc::format_dict("连接到 ${host}:${port}", args);
    ASSERT_EQ(result, "连接到 example.com:8080") << "基本替换应该正确";

    // 测试不同类型的值
    result = mc::format_dict("协议: ${protocol}, 端口: ${port}, 启用: ${enabled}, 比率: ${ratio}", args);
    ASSERT_TRUE(contains(result, "协议: https")) << "字符串值替换应该正确";
    ASSERT_TRUE(contains(result, "端口: 8080")) << "整数值替换应该正确";
    ASSERT_TRUE(contains(result, "启用: true")) << "布尔值替换应该正确";
    ASSERT_TRUE(contains(result, "比率: 0.")) << "浮点值替换应该正确";

    // 测试不存在的键
    ASSERT_EQ(mc::format_dict("${host}:${missing}", args), "example.com:${missing}");

    // 测试格式错误
    ASSERT_EQ(mc::format_dict("${host:${port}", args), "${host:8080");

    // 测试空字符串
    result = mc::format_dict("", args);
    ASSERT_EQ(result, "") << "空字符串应该返回空字符串";

    // 测试没有占位符的字符串
    result = mc::format_dict("没有占位符的字符串", args);
    ASSERT_EQ(result, "没有占位符的字符串") << "没有占位符的字符串应该保持不变";

    // 测试简单字符串值
    mc::dict simple_args{{"simple_key", "simple_value"}};
    result = mc::format_dict("简单替换: ${simple_key}", simple_args);
    ASSERT_EQ(result, "简单替换: simple_value") << "简单替换应该正确";

    // 测试新format重载函数，将结果追加到现有字符串
    std::string append_result = "前缀：";
    mc::format_dict(append_result, "连接到 ${host}:${port}",
                    mc::dict()("host", "example.com")("port", "8080"));
    ASSERT_EQ(append_result, "前缀：连接到 example.com:8080") << "追加格式化结果应该正确";

    // 测试新format重载函数，使用空字符串
    std::string empty_result;
    mc::format_dict(empty_result, "协议: ${protocol}", mc::dict()("protocol", "https"));
    ASSERT_EQ(empty_result, "协议: https") << "从空字符串开始格式化应该正确";

    // 测试宏的其他功能
    // 测试无参数情况
    result = mc::format_dict("无参数字符串", mc::dict());
    ASSERT_EQ(result, "无参数字符串") << "无参数调用应该返回原始字符串";

    // 测试多个参数
    result = mc::format_dict("${a}-${b}-${c}", mc::dict()("a", 1)("b", 2.5)("c", "文本"));
    ASSERT_EQ(result, "1-2.5-文本") << "多参数调用应该正确格式化";
}

TEST(format_dict_test, FormatIcaseTest) {
    mc::dict args{{"host", "example.com"},
                  {"port", 8080},
                  {"protocol", "https"},
                  {"enabled", true},
                  {"ratio", 0.75}};

    std::string result = mc::format_dict_icase("${host}:${port}", args);
    ASSERT_EQ(result, "example.com:8080") << "大小写不敏感格式化应该正确";

    result = mc::format_dict_icase("${HOST}:${port}", args);
    ASSERT_EQ(result, "example.com:8080") << "大小写不敏感格式化应该正确";

    result = mc::format_dict_icase("${host}:${PORT}", args);
    ASSERT_EQ(result, "example.com:8080") << "大小写不敏感格式化应该正确";

    result = mc::format_dict_icase("${HOST}:${PORT}", args);
    ASSERT_EQ(result, "example.com:8080") << "大小写不敏感格式化应该正确";

    ASSERT_EQ(mc::format_dict("${HOST}:${PORT}", args), "${HOST}:${PORT}");
    ASSERT_EQ(mc::format_dict("${HOST}:${port}", args), "${HOST}:8080");
}

// 测试动态宽度和精度参数（包含大小写不敏感匹配）
TEST(format_dict_test, DynamicWidthAndPrecisionFromDict) {
    mc::dict args{{"value", 3.14159},
                  {"WIDTH", 10},
                  {"precision", 4}};

    std::string result = mc::format_dict_icase("${value:{WIDTH}.{precision}f}", args);
    EXPECT_EQ(result, "    3.1416");
}

// 测试动态参数类型错误时的处理行为
TEST(format_dict_test, DynamicParameterTypeError) {
    mc::dict args{{"value", 1.23}, {"width", "not_number"}};

    // 当前实现会保留转换失败的动态参数文本，并把占位符中的内容直接拼接。
    EXPECT_EQ(mc::format_dict("${value:{width}f}", args), "{value:not_numberf}");
}

// 验证 format_dict_icase 追加重载覆盖率，并确保大小写忽略逻辑生效
TEST(format_dict_test, AppendIcaseOverload) {
    mc::dict args{{"path", "/tmp/FILE"}};
    std::string buffer = "prefix:";
    mc::format_dict_icase(buffer, "${PATH}", args);
    EXPECT_EQ(buffer, "prefix:/tmp/FILE");
    mc::format_dict_icase(buffer, " ${path}", args);
    EXPECT_EQ(buffer, "prefix:/tmp/FILE /tmp/FILE");
}

// dict 中的动态宽度参数由浮点数提供，覆盖 try_as<int>() 分支
TEST(format_dict_test, DynamicWidthFromFloatingEntry) {
    mc::dict args{{"value", 3.14159}, {"width", 6.0}, {"precision", 2}};
    EXPECT_EQ(mc::format_dict("${value:{width}.{precision}f}", args), "  3.14");
}

// 直接验证 runtime_arg_store 在 dict 模式下的大小写匹配与缺失参数分支
TEST(format_dict_test, RuntimeStoreFallbacks) {
    mc::dict args;
    args.insert(mc::variant(42), "answer");

    mc::fmt::detail::runtime_arg_store store;
    store.icase = true;
    store.set_dict_args(&args);

    const auto* matched = store.get_variant("42");
    ASSERT_NE(matched, nullptr);
    EXPECT_EQ(matched->as_string(), "answer");

    int    dynamic_value  = 0;
    size_t invalid_index  = std::numeric_limits<size_t>::max();
    EXPECT_FALSE(store.resolve_dynamic_param(invalid_index, "missing", dynamic_value));
}

// 测试 dict 的 key 非字符串时的 fallback
TEST(format_dict_test, DictNonStringKeyFallback) {
    // 构造一个 dict，键为 int64_t
    mc::dict dict;
    dict.insert(mc::variant(static_cast<int64_t>(123)), "value1");
    dict.insert(mc::variant(static_cast<int64_t>(456)), "value2");

    // 格式化 dict，应该使用 fallback 格式化非字符串键
    std::string result = sformat("{}", dict);
    // 验证输出包含十进制键
    EXPECT_TRUE(result.find("123") != std::string::npos || result.find("value1") != std::string::npos);
    EXPECT_TRUE(result.find("456") != std::string::npos || result.find("value2") != std::string::npos);
}
