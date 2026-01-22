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
 * @file test_dict_string_view.cpp
 * @brief 测试 dict 和 dict 类对 std::string_view 和 const char* 的支持
 */
#include <gtest/gtest.h>
#include <mc/dict.h>
#include <mc/variant.h>
#include <string_view>

using namespace mc;

// 测试 dict 对 string_view 的支持
TEST(DictStringViewTest, DictStringViewSupport) {
    dict d({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    // 测试 std::string_view 接口
    std::string_view key1_view = "key1";
    std::string_view key2_view = "key2";
    std::string_view key3_view = "key3";
    std::string_view key4_view = "key4";

    // 测试 contains 方法
    EXPECT_TRUE(d.contains(key1_view));
    EXPECT_TRUE(d.contains(key2_view));
    EXPECT_TRUE(d.contains(key3_view));
    EXPECT_FALSE(d.contains(key4_view));

    // 测试 operator[] 方法
    EXPECT_EQ(d[key1_view].as<int>(), 123);
    EXPECT_EQ(d[key2_view].as<std::string>(), "value");
    EXPECT_EQ(d[key3_view].as<bool>(), true);
    // const 版本的 operator[] 访问不存在的键会抛出异常
    const dict& const_d = d;
    EXPECT_THROW(const_d[key4_view], std::out_of_range);

    // 测试 get 方法
    EXPECT_EQ(d.get(key1_view, 0).as<int>(), 123);
    EXPECT_EQ(d.get(key4_view, 456).as<int>(), 456);

    // 测试 find_index 方法
    EXPECT_GE(d.find_index(key1_view), 0);
    EXPECT_GE(d.find_index(key2_view), 0);
    EXPECT_GE(d.find_index(key3_view), 0);
    EXPECT_EQ(d.find_index(key4_view), -1);
}

// 测试 dict 对 const char* 的支持
TEST(DictStringViewTest, DictConstCharSupport) {
    dict d({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    // 测试 const char* 接口
    const char* key1_cstr = "key1";
    const char* key2_cstr = "key2";
    const char* key3_cstr = "key3";
    const char* key4_cstr = "key4";

    // 测试 contains 方法
    EXPECT_TRUE(d.contains(key1_cstr));
    EXPECT_TRUE(d.contains(key2_cstr));
    EXPECT_TRUE(d.contains(key3_cstr));
    EXPECT_FALSE(d.contains(key4_cstr));

    // 测试 operator[] 方法
    EXPECT_EQ(d[key1_cstr].as<int>(), 123);
    EXPECT_EQ(d[key2_cstr].as<std::string>(), "value");
    EXPECT_EQ(d[key3_cstr].as<bool>(), true);
    // const 版本的 operator[] 访问不存在的键会抛出异常
    const dict& const_d = d;
    EXPECT_THROW(const_d[key4_cstr], std::out_of_range);

    // 测试 get 方法
    EXPECT_EQ(d.get(key1_cstr, 0).as<int>(), 123);
    EXPECT_EQ(d.get(key4_cstr, 456).as<int>(), 456);

    // 测试 find_index 方法
    EXPECT_GE(d.find_index(key1_cstr), 0);
    EXPECT_GE(d.find_index(key2_cstr), 0);
    EXPECT_GE(d.find_index(key3_cstr), 0);
    EXPECT_EQ(d.find_index(key4_cstr), -1);
}

// 测试 dict 对 string_view 的支持
TEST(DictStringViewTest, MutableDictStringViewSupport) {
    dict md({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    // 测试 std::string_view 接口
    std::string_view key1_view = "key1";
    std::string_view key2_view = "key2";
    std::string_view key3_view = "key3";
    std::string_view key4_view = "key4";

    // 测试 operator[] 方法
    EXPECT_EQ(md[key1_view].as<int>(), 123);
    EXPECT_EQ(md[key2_view].as<std::string>(), "value");
    EXPECT_EQ(md[key3_view].as<bool>(), true);

    // 测试添加新键值对
    md[key4_view] = 456;
    EXPECT_EQ(md[key4_view].as<int>(), 456);

    // 测试 erase 方法
    EXPECT_TRUE(md.erase(key4_view));
    EXPECT_FALSE(md.contains(key4_view));
    EXPECT_FALSE(md.erase(key4_view));
}

// 测试 dict 对 const char* 的支持
TEST(DictStringViewTest, MutableDictConstCharSupport) {
    dict md({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    // 测试 const char* 接口
    const char* key1_cstr = "key1";
    const char* key2_cstr = "key2";
    const char* key3_cstr = "key3";
    const char* key4_cstr = "key4";

    // 测试 operator[] 方法
    EXPECT_EQ(md[key1_cstr].as<int>(), 123);
    EXPECT_EQ(md[key2_cstr].as<std::string>(), "value");
    EXPECT_EQ(md[key3_cstr].as<bool>(), true);

    // 测试添加新键值对
    md[key4_cstr] = 456;
    EXPECT_EQ(md[key4_cstr].as<int>(), 456);

    // 测试 erase 方法
    EXPECT_TRUE(md.erase(key4_cstr));
    EXPECT_FALSE(md.contains(key4_cstr));
    EXPECT_FALSE(md.erase(key4_cstr));
}

// 测试混合字符串类型
TEST(DictStringViewTest, MixedStringTypes) {
    // 使用不同类型的字符串作为键
    std::string key1_str  = "key1";
    std::string key2_view = "key2";
    const char* key3_cstr = "key3";

    dict md({{key1_str, 123}, {key2_view, "value"}, {key3_cstr, true}});

    // 使用不同类型的字符串访问键值对
    std::string      key1_str_copy  = "key1";
    std::string_view key2_view_copy = "key2";
    const char*      key3_cstr_copy = "key3";

    EXPECT_EQ(md[key1_str_copy].as<int>(), 123);
    EXPECT_EQ(md[std::string_view(key1_str_copy)].as<int>(), 123);
    EXPECT_EQ(md[key1_str_copy.c_str()].as<int>(), 123);

    EXPECT_EQ(md[key2_view_copy].as<std::string>(), "value");
    EXPECT_EQ(md[std::string(key2_view_copy)].as<std::string>(), "value");
    EXPECT_EQ(md[std::string(key2_view_copy).c_str()].as<std::string>(), "value");

    EXPECT_EQ(md[key3_cstr_copy].as<bool>(), true);
    EXPECT_EQ(md[std::string(key3_cstr_copy)].as<bool>(), true);
    EXPECT_EQ(md[std::string_view(key3_cstr_copy)].as<bool>(), true);

    // 测试 contains 方法
    EXPECT_TRUE(md.contains(key1_str_copy));
    EXPECT_TRUE(md.contains(std::string_view(key1_str_copy)));
    EXPECT_TRUE(md.contains(key1_str_copy.c_str()));

    // 测试 erase 方法
    EXPECT_TRUE(md.erase(key1_str_copy));
    EXPECT_FALSE(md.contains(key1_str_copy));

    EXPECT_TRUE(md.erase(std::string_view(key2_view_copy)));
    EXPECT_FALSE(md.contains(key2_view_copy));

    EXPECT_TRUE(md.erase(key3_cstr_copy));
    EXPECT_FALSE(md.contains(key3_cstr_copy));
}