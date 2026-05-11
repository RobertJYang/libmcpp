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
#include <test_utilities/base.h>

#include <mc/fmt/format.h>
#include <mc/fmt/formatter_mc.h>

namespace mc::test {

class mc_formatter_test : public mc::test::TestBase {
protected:
    void SetUp() override {
        // 在每个测试前执行
    }

    void TearDown() override {
        // 在每个测试后执行
    }
};

/**
 * @brief 测试 mc::variant 的格式化
 */
TEST_F(mc_formatter_test, VariantFormatting) {
    // 测试基本类型
    mc::variant v1      = 42;
    mc::string result1 = sformat("{}", v1);
    EXPECT_EQ(result1, "42");

    mc::variant v2      = 3.14;
    mc::string result2 = sformat("{}", v2);
    EXPECT_EQ(result2, "3.14");

    mc::variant v3      = "hello";
    mc::string result3 = sformat("{}", v3);
    EXPECT_EQ(result3, "hello");

    mc::variant v4      = true;
    mc::string result4 = sformat("{}", v4);
    EXPECT_EQ(result4, "true");

    mc::variant v5; // null
    mc::string result5 = sformat("{}", v5);
    EXPECT_EQ(result5, "null");
}

/**
 * @brief 测试 mc::variant 格式化参数
 */
TEST_F(mc_formatter_test, VariantFormatWithSpec) {
    mc::variant v1 = 42;
    mc::variant v2 = "hello";
    mc::variant v3 = 3.14;

    // 测试整数格式化
    EXPECT_EQ(sformat("{:d}", v1), "42");
    EXPECT_EQ(sformat("{:x}", v1), "2a");
    EXPECT_EQ(sformat("{:o}", v1), "52");
    EXPECT_EQ(sformat("{:b}", v1), "101010");

    // 测试字符串格式化
    EXPECT_EQ(sformat("{:s}", v2), "hello");

    // 测试浮点数格式化
    EXPECT_EQ(sformat("{:.2f}", v3), "3.14");
    EXPECT_EQ(sformat("{:e}", v3), "3.140000e+00");

    // 测试类型转换
    mc::variant v4 = 65;
    EXPECT_EQ(sformat("{:c}", v4), "A"); // 整数转字符
}

// 测试 variant 中的 int8_t 作为 char
TEST_F(mc_formatter_test, FormatCharFromVariant) {
    // variant 中存放 int8_t('A')
    mc::variant v      = static_cast<int8_t>('A');
    mc::string result = sformat("{}", v);
    // 默认行为：按整数输出
    EXPECT_EQ(result, "65");
}

// 测试 blob 格式化
TEST_F(mc_formatter_test, BlobFormatterOutputsSize) {
    // 创建 mc::blob
    mc::blob blob;
    blob.data          = std::vector<char>{static_cast<char>(1), static_cast<char>(2), static_cast<char>(3)};
    mc::string result = sformat("{}", blob);
    // 期望输出 "blob(3 bytes)"
    EXPECT_EQ(result, "blob(3 bytes)");
}

// 测试 extension 格式化
TEST_F(mc_formatter_test, ExtensionFormatterUsesTypeName) {
    // 定义一个继承 variant_extension_base 的伪 extension
    class custom_ext : public mc::variant_extension_base {
    public:
        mc::string_view get_type_name() const override {
            return "custom_ext";
        }

        mc::shared_ptr<variant_extension_base> copy() const override {
            return mc::make_shared<custom_ext>(*this);
        }

        bool equals(const variant_extension_base& other) const override {
            return get_type_name() == other.get_type_name();
        }
    };

    auto        ext = mc::make_shared<custom_ext>();
    mc::variant v(ext);
    mc::string result = sformat("{}", v);
    // 期望输出 "extension(custom_ext)"
    EXPECT_EQ(result, "extension(custom_ext)");
}

/**
 * @brief 测试 mc::variant 格式化参数失败回退
 */
TEST_F(mc_formatter_test, VariantFormatFallback) {
    mc::variant v1 = "hello";
    mc::variant v2 = true;

    // 字符串无法转换为整数，应该回退到默认格式化
    EXPECT_EQ(sformat("{:d}", v1), "hello");
    EXPECT_EQ(sformat("{:s}", v2), "true");
}

/**
 * @brief 测试 mc::variants 的格式化
 */
TEST_F(mc_formatter_test, VariantsFormatting) {
    mc::variants arr;
    arr.push_back(1);
    arr.push_back(2.5);
    arr.push_back("test");
    arr.push_back(true);

    mc::string result = sformat("{}", arr);
    EXPECT_EQ(result, "[1,2.5,\"test\",true]");
}

/**
 * @brief 测试 mc::dict 的格式化
 */
TEST_F(mc_formatter_test, DictFormatting) {
    mc::dict dict;
    dict["name"]       = "张三";
    dict["age"]        = 30;
    dict["is_student"] = false;

    mc::string result = sformat("{}", dict);
    EXPECT_EQ(result, "{\"name\":张三,\"age\":30,\"is_student\":false}");
}

/**
 * @brief 测试 mc::dict 的格式化
 */
TEST_F(mc_formatter_test, MutableDictFormatting) {
    mc::dict dict;
    dict["city"]       = "北京";
    dict["population"] = 21540000;
    dict["is_capital"] = true;

    mc::string result = sformat("{}", dict);
    EXPECT_EQ(result, "{\"city\":北京,\"population\":21540000,\"is_capital\":true}");
}

/**
 * @brief 测试嵌套结构的格式化
 */
TEST_F(mc_formatter_test, NestedStructureFormatting) {
    // 创建嵌套的 dict
    mc::dict inner_dict;
    inner_dict["x"] = 10;
    inner_dict["y"] = 20;

    mc::variants arr;
    arr.push_back(1);
    arr.push_back(2);
    arr.push_back(3);

    mc::dict outer_dict;
    outer_dict["point"]   = inner_dict;
    outer_dict["numbers"] = arr;
    outer_dict["name"]    = "test";

    mc::string result = sformat("{}", outer_dict);
    EXPECT_EQ(result, "{\"point\":{\"x\":10,\"y\":20},\"numbers\":[1,2,3],\"name\":test}");
}

/**
 * @brief 测试 mc::variant 包含数组的格式化
 */
TEST_F(mc_formatter_test, VariantWithArrayFormatting) {
    mc::variants arr;
    arr.push_back("a");
    arr.push_back("b");
    arr.push_back("c");

    mc::variant v      = arr;
    mc::string result = sformat("{}", v);
    EXPECT_EQ(result, "[\"a\",\"b\",\"c\"]");
}

/**
 * @brief 测试 mc::variant 包含 dict 的格式化
 */
TEST_F(mc_formatter_test, VariantWithDictFormatting) {
    mc::dict dict;
    dict["key1"] = "value1";
    dict["key2"] = 42;

    mc::variant v      = dict;
    mc::string result = sformat("{}", v);
    EXPECT_EQ(result, "{\"key1\":value1,\"key2\":42}");
}

/**
 * @brief 测试复杂嵌套结构的格式化
 */
TEST_F(mc_formatter_test, ComplexNestedFormatting) {
    // 创建复杂的嵌套结构
    mc::variants inner_arr;
    inner_arr.push_back("nested1");
    inner_arr.push_back("nested2");

    mc::dict inner_dict;
    inner_dict["nested_key"] = inner_arr;

    mc::variants outer_arr;
    outer_arr.push_back(1);
    outer_arr.push_back(inner_dict);
    outer_arr.push_back("end");

    mc::variant v      = outer_arr;
    mc::string result = sformat("{}", v);
    EXPECT_EQ(result, "[1,{\"nested_key\":[\"nested1\",\"nested2\"]},\"end\"]");
}

} // namespace mc::test