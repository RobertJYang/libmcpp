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
 * @brief 测试 dict 和 dict 类对 mc::string_view 和 const char* 的支持
 */
#include <gtest/gtest.h>
#include <mc/dict.h>
#include <mc/variant.h>
#include <string>
#include <string_view>
#include <type_traits>

using namespace mc;

namespace {

struct dict_api_shape_probe : dict {
    using dict::find_entry;
};

template <typename T, typename = void>
struct has_std_string_find_entry_mutable : std::false_type {};

template <typename T>
struct has_std_string_find_entry_mutable<
    T, std::void_t<decltype(static_cast<dict::entry* (T::*)(const std::string&)>(&T::find_entry))>> : std::true_type {};

template <typename T, typename = void>
struct has_std_string_view_find_entry_mutable : std::false_type {};

template <typename T>
struct has_std_string_view_find_entry_mutable<
    T, std::void_t<decltype(static_cast<dict::entry* (T::*)(std::string_view)>(&T::find_entry))>> : std::true_type {};

template <typename T, typename = void>
struct has_std_string_find_entry_const : std::false_type {};

template <typename T>
struct has_std_string_find_entry_const<
    T, std::void_t<decltype(static_cast<const dict::entry* (T::*)(const std::string&) const>(&T::find_entry))>>
    : std::true_type {};

template <typename T, typename = void>
struct has_std_string_view_find_entry_const : std::false_type {};

template <typename T>
struct has_std_string_view_find_entry_const<
    T, std::void_t<decltype(static_cast<const dict::entry* (T::*)(std::string_view) const>(&T::find_entry))>>
    : std::true_type {};

static_assert(std::is_same_v<decltype(static_cast<dict::iterator (dict::*)(mc::string_view)>(&dict::find)),
                             dict::iterator (dict::*)(mc::string_view)>);
static_assert(std::is_same_v<decltype(static_cast<dict::iterator (dict::*)(const std::string&)>(&dict::find)),
                             dict::iterator (dict::*)(const std::string&)>);
static_assert(std::is_same_v<decltype(static_cast<dict::iterator (dict::*)(std::string_view)>(&dict::find)),
                             dict::iterator (dict::*)(std::string_view)>);
static_assert(!has_std_string_find_entry_mutable<dict_api_shape_probe>::value);
static_assert(!has_std_string_view_find_entry_mutable<dict_api_shape_probe>::value);
static_assert(!has_std_string_find_entry_const<dict_api_shape_probe>::value);
static_assert(!has_std_string_view_find_entry_const<dict_api_shape_probe>::value);

} // namespace

// 测试 dict 对 string_view 的支持
TEST(DictStringViewTest, DictStringViewSupport)
{
    dict d({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    // 测试 mc::string_view 接口
    mc::string_view key1_view = "key1";
    mc::string_view key2_view = "key2";
    mc::string_view key3_view = "key3";
    mc::string_view key4_view = "key4";

    // 测试 contains 方法
    EXPECT_TRUE(d.contains(key1_view));
    EXPECT_TRUE(d.contains(key2_view));
    EXPECT_TRUE(d.contains(key3_view));
    EXPECT_FALSE(d.contains(key4_view));

    // 测试 operator[] 方法
    EXPECT_EQ(d[key1_view].as<int>(), 123);
    EXPECT_EQ(d[key2_view].as<mc::string>(), "value");
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
TEST(DictStringViewTest, DictConstCharSupport)
{
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
    EXPECT_EQ(d[key2_cstr].as<mc::string>(), "value");
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
TEST(DictStringViewTest, MutableDictStringViewSupport)
{
    dict md({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    // 测试 mc::string_view 接口
    mc::string_view key1_view = "key1";
    mc::string_view key2_view = "key2";
    mc::string_view key3_view = "key3";
    mc::string_view key4_view = "key4";

    // 测试 operator[] 方法
    EXPECT_EQ(md[key1_view].as<int>(), 123);
    EXPECT_EQ(md[key2_view].as<mc::string>(), "value");
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
TEST(DictStringViewTest, MutableDictConstCharSupport)
{
    dict md({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    // 测试 const char* 接口
    const char* key1_cstr = "key1";
    const char* key2_cstr = "key2";
    const char* key3_cstr = "key3";
    const char* key4_cstr = "key4";

    // 测试 operator[] 方法
    EXPECT_EQ(md[key1_cstr].as<int>(), 123);
    EXPECT_EQ(md[key2_cstr].as<mc::string>(), "value");
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
TEST(DictStringViewTest, MixedStringTypes)
{
    // 使用不同类型的字符串作为键
    mc::string  key1_str  = "key1";
    mc::string  key2_view = "key2";
    const char* key3_cstr = "key3";

    dict md({{key1_str, 123}, {key2_view, "value"}, {key3_cstr, true}});

    // 使用不同类型的字符串访问键值对
    mc::string      key1_str_copy  = "key1";
    mc::string_view key2_view_copy = "key2";
    const char*     key3_cstr_copy = "key3";

    EXPECT_EQ(md[key1_str_copy].as<int>(), 123);
    EXPECT_EQ(md[mc::string_view(key1_str_copy)].as<int>(), 123);
    EXPECT_EQ(md[key1_str_copy.c_str()].as<int>(), 123);

    EXPECT_EQ(md[key2_view_copy].as<mc::string>(), "value");
    EXPECT_EQ(md[mc::string(key2_view_copy)].as<mc::string>(), "value");
    EXPECT_EQ(md[mc::string(key2_view_copy).c_str()].as<mc::string>(), "value");

    EXPECT_EQ(md[key3_cstr_copy].as<bool>(), true);
    EXPECT_EQ(md[mc::string(key3_cstr_copy)].as<bool>(), true);
    EXPECT_EQ(md[mc::string_view(key3_cstr_copy)].as<bool>(), true);

    // 测试 contains 方法
    EXPECT_TRUE(md.contains(key1_str_copy));
    EXPECT_TRUE(md.contains(mc::string_view(key1_str_copy)));
    EXPECT_TRUE(md.contains(key1_str_copy.c_str()));

    // 测试 erase 方法
    EXPECT_TRUE(md.erase(key1_str_copy));
    EXPECT_FALSE(md.contains(key1_str_copy));

    EXPECT_TRUE(md.erase(mc::string_view(key2_view_copy)));
    EXPECT_FALSE(md.contains(key2_view_copy));

    EXPECT_TRUE(md.erase(key3_cstr_copy));
    EXPECT_FALSE(md.contains(key3_cstr_copy));
}

TEST(DictStringViewTest, DictStdStringViewSupport)
{
    dict d({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    std::string_view key1 = "key1";
    std::string_view key2 = "key2";
    std::string_view key4 = "key4";

    EXPECT_TRUE(d.contains(key1));
    EXPECT_EQ(d.find(key1)->value.as<int>(), 123);
    EXPECT_EQ(d.get(key2, "fallback").as<mc::string>(), "value");
    EXPECT_EQ(d.find_index(key4), -1);

    d[key4] = 456;
    EXPECT_TRUE(d.contains(key4));
    EXPECT_TRUE(d.erase(key4));
    EXPECT_FALSE(d.contains(key4));
}

TEST(DictStringViewTest, DictStdStringSupport)
{
    dict d({{"key1", 123}, {"key2", "value"}});

    std::string key1 = "key1";
    std::string key2 = "key2";
    std::string key3 = "key3";

    EXPECT_TRUE(d.contains(key1));
    EXPECT_EQ(d.find(key2)->value.as<mc::string>(), "value");

    d[key3] = 789;
    EXPECT_TRUE(d.contains(key3));
    EXPECT_TRUE(d.erase(key3));
    EXPECT_FALSE(d.contains(key3));
}

TEST(DictStringViewTest, DictStdStringEmbeddedNullSupport)
{
    std::string embedded_key("a\0b", 3);
    std::string missing_key("a\0c", 3);
    dict        d({{mc::string(embedded_key), 123}});

    EXPECT_TRUE(d.contains(embedded_key));
    EXPECT_EQ(d.find(embedded_key)->value.as<int>(), 123);
    EXPECT_EQ(d.get(embedded_key, 0).as<int>(), 123);
    EXPECT_EQ(d.find_index(embedded_key), 0);

    EXPECT_FALSE(d.contains(missing_key));
    EXPECT_EQ(d.get(missing_key, 456).as<int>(), 456);

    d[embedded_key] = 789;
    EXPECT_EQ(d[embedded_key].as<int>(), 789);
    EXPECT_TRUE(d.erase(embedded_key));
    EXPECT_FALSE(d.contains(embedded_key));
}

TEST(DictStringViewTest, DictStdStringViewEmbeddedNullSupport)
{
    std::string      embedded_key_storage("a\0b", 3);
    std::string_view embedded_key(embedded_key_storage.data(), embedded_key_storage.size());
    std::string      missing_key_storage("a\0c", 3);
    std::string_view missing_key(missing_key_storage.data(), missing_key_storage.size());
    dict             d({{mc::string(embedded_key), 123}});

    EXPECT_TRUE(d.contains(embedded_key));
    EXPECT_EQ(d.find(embedded_key)->value.as<int>(), 123);
    EXPECT_EQ(d.get(embedded_key, 0).as<int>(), 123);
    EXPECT_EQ(d.find_index(embedded_key), 0);

    EXPECT_FALSE(d.contains(missing_key));
    EXPECT_EQ(d.get(missing_key, 456).as<int>(), 456);

    d[embedded_key] = 789;
    EXPECT_EQ(d[embedded_key].as<int>(), 789);
    EXPECT_TRUE(d.erase(embedded_key));
    EXPECT_FALSE(d.contains(embedded_key));
}