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
 * @file test_dict_conversion.cpp
 * @brief 测试 dict 和 mutable_dict 类与其他类型之间的转换
 */
#include <gtest/gtest.h>
#include <mc/dict.h>
#include <mc/variant.h>
#include <map>
#include <unordered_map>
#include <string>

using namespace mc;

// 测试 dict 转换为 variant
TEST(DictConversionTest, DictToVariant) {
    dict d({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    // 将 dict 转换为 variant
    variant v = to_variant(d);
    
    // 验证 variant 类型
    EXPECT_TRUE(v.is_object());
    
    // 验证 variant 内容
    dict d2 = v.as<dict>();
    EXPECT_EQ(d2["key1"].as<int>(), 123);
    EXPECT_EQ(d2["key2"].as<std::string>(), "value");
    EXPECT_EQ(d2["key3"].as<bool>(), true);
}

// 测试 mutable_dict 转换为 variant
TEST(DictConversionTest, MutableDictToVariant) {
    mutable_dict md({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    // 将 mutable_dict 转换为 variant
    variant v = to_variant(md);
    
    // 验证 variant 类型
    EXPECT_TRUE(v.is_object());
    
    // 验证 variant 内容
    dict d = v.as<dict>();
    EXPECT_EQ(d["key1"].as<int>(), 123);
    EXPECT_EQ(d["key2"].as<std::string>(), "value");
    EXPECT_EQ(d["key3"].as<bool>(), true);
}

// 测试 variant 转换为 dict
TEST(DictConversionTest, VariantToDict) {
    dict d1({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    // 将 dict 转换为 variant
    variant v = to_variant(d1);
    
    // 将 variant 转换为 dict
    dict d2 = v.as<dict>();
    
    // 验证 dict 内容
    EXPECT_EQ(d2.size(), 3);
    EXPECT_EQ(d2["key1"].as<int>(), 123);
    EXPECT_EQ(d2["key2"].as<std::string>(), "value");
    EXPECT_EQ(d2["key3"].as<bool>(), true);
}

// 测试 variant 转换为 mutable_dict
TEST(DictConversionTest, VariantToMutableDict) {
    mutable_dict md1({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    // 将 mutable_dict 转换为 variant
    variant v = to_variant(md1);
    
    // 将 variant 转换为 mutable_dict
    mutable_dict md2 = v.as<mutable_dict>();
    
    // 验证 mutable_dict 内容
    EXPECT_EQ(md2.size(), 3);
    EXPECT_EQ(md2["key1"].as<int>(), 123);
    EXPECT_EQ(md2["key2"].as<std::string>(), "value");
    EXPECT_EQ(md2["key3"].as<bool>(), true);
}

// 测试 std::map 转换为 dict
TEST(DictConversionTest, StdMapToDict) {
    // 创建一个 std::map
    std::map<std::string, variant> m;
    m["key1"] = 123;
    m["key2"] = "value";
    m["key3"] = true;
    
    // 将 std::map 转换为 dict
    dict d;
    for (const auto& pair : m) {
        mutable_dict md(d);
        md(pair.first, pair.second);
        d = md;
    }
    
    // 验证 dict 内容
    EXPECT_EQ(d.size(), 3);
    EXPECT_EQ(d["key1"].as<int>(), 123);
    EXPECT_EQ(d["key2"].as<std::string>(), "value");
    EXPECT_EQ(d["key3"].as<bool>(), true);
}

// 测试 std::unordered_map 转换为 dict
TEST(DictConversionTest, StdUnorderedMapToDict) {
    // 创建一个 std::unordered_map
    std::unordered_map<std::string, variant> m;
    m["key1"] = 123;
    m["key2"] = "value";
    m["key3"] = true;
    
    // 将 std::unordered_map 转换为 dict
    dict d;
    for (const auto& pair : m) {
        mutable_dict md(d);
        md(pair.first, pair.second);
        d = md;
    }
    
    // 验证 dict 内容
    EXPECT_EQ(d.size(), 3);
    EXPECT_EQ(d["key1"].as<int>(), 123);
    EXPECT_EQ(d["key2"].as<std::string>(), "value");
    EXPECT_EQ(d["key3"].as<bool>(), true);
}

// 测试 dict 转换为 std::map
TEST(DictConversionTest, DictToStdMap) {
    // 创建一个 dict
    dict d({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    // 将 dict 转换为 std::map
    std::map<std::string, variant> m;
    for (const auto& entry : d) {
        m[entry.key] = entry.value;
    }
    
    // 验证 std::map 内容
    EXPECT_EQ(m.size(), 3);
    EXPECT_EQ(m["key1"].as<int>(), 123);
    EXPECT_EQ(m["key2"].as<std::string>(), "value");
    EXPECT_EQ(m["key3"].as<bool>(), true);
}

// 测试 dict 转换为 std::unordered_map
TEST(DictConversionTest, DictToStdUnorderedMap) {
    // 创建一个 dict
    dict d({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    // 将 dict 转换为 std::unordered_map
    std::unordered_map<std::string, variant> m;
    for (const auto& entry : d) {
        m[entry.key] = entry.value;
    }
    
    // 验证 std::unordered_map 内容
    EXPECT_EQ(m.size(), 3);
    EXPECT_EQ(m["key1"].as<int>(), 123);
    EXPECT_EQ(m["key2"].as<std::string>(), "value");
    EXPECT_EQ(m["key3"].as<bool>(), true);
}

// 测试嵌套 dict 转换
TEST(DictConversionTest, NestedDictConversion) {
    // 创建嵌套的 dict
    mutable_dict inner_md({
        {"inner1", 123},
        {"inner2", "inner_value"}
    });
    
    dict inner_d({
        {"inner3", true},
        {"inner4", 456.789}
    });
    
    mutable_dict outer_md({
        {"key1", inner_md},
        {"key2", 123},
        {"key3", inner_d}
    });
    
    // 将嵌套的 mutable_dict 转换为 variant
    variant v = to_variant(outer_md);
    
    // 验证 variant 内容
    dict d = v.as<dict>();
    EXPECT_EQ(d["key2"].as<int>(), 123);
    EXPECT_EQ(d["key1"].as<dict>()["inner1"].as<int>(), 123);
    EXPECT_EQ(d["key1"].as<dict>()["inner2"].as<std::string>(), "inner_value");
    EXPECT_EQ(d["key3"].as<dict>()["inner3"].as<bool>(), true);
    EXPECT_DOUBLE_EQ(d["key3"].as<dict>()["inner4"].as<double>(), 456.789);
} 