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
 * @file test_variant_standard_containers.cpp
 * @brief 测试 variant 与标准库容器类型的交互
 */
#include "test_variant_helpers.h"
#include <array>
#include <deque>
#include <forward_list>
#include <gtest/gtest.h>
#include <list>
#include <map>
#include <mc/dict.h>
#include <mc/variant.h>
#include <set>
#include <string>
#include <test_utilities/test_base.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mc {
namespace test {

class VariantStdContainersTest : public mc::test::TestBase {
protected:
    void SetUp() override {
        // 在每个测试前执行
    }

    void TearDown() override {
        // 在每个测试后执行
    }
};

/**
 * @brief 测试 std::vector 与 variant 的转换
 */
TEST_F(VariantStdContainersTest, VectorConversion) {
    // 创建整数向量
    std::vector<int> int_vec = {1, 2, 3, 4, 5};
    variant          v;
    to_variant(int_vec, v);

    ASSERT_TRUE(v.is_array()) << "variant 应该是数组类型";
    ASSERT_EQ(v.size(), int_vec.size()) << "数组大小不匹配";

    // 检查数组元素
    for (size_t i = 0; i < int_vec.size(); ++i) {
        ASSERT_EQ(v[i].as<int>(), int_vec[i]) << "数组元素不匹配，位置: " << i;
    }

    // 转换回 std::vector<int>
    std::vector<int> result;
    from_variant(v, result);
    ASSERT_EQ(result, int_vec) << "转换回的向量不匹配";

    // 使用 as 方法转换
    std::vector<int> result2 = v.as<std::vector<int>>();
    ASSERT_EQ(result2, int_vec) << "使用 as 方法转换的向量不匹配";

    // 测试不同类型的向量
    std::vector<std::string> str_vec = {"one", "two", "three"};
    to_variant(str_vec, v);
    std::vector<std::string> str_result = v.as<std::vector<std::string>>();
    ASSERT_EQ(str_result, str_vec) << "字符串向量转换失败";

    // 测试复杂类型的向量
    std::vector<std::vector<int>> nested_vec = {{1, 2}, {3, 4}, {5, 6}};
    to_variant(nested_vec, v);
    std::vector<std::vector<int>> nested_result = v.as<std::vector<std::vector<int>>>();
    ASSERT_EQ(nested_result, nested_vec) << "嵌套向量转换失败";
}

/**
 * @brief 测试 std::map 与 variant 的转换
 */
TEST_F(VariantStdContainersTest, MapConversion) {
    // 创建字符串到整数的映射
    std::map<std::string, int> str_int_map = {{"one", 1}, {"two", 2}, {"three", 3}};

    variant v;
    to_variant(str_int_map, v);

    ASSERT_TRUE(v.is_object()) << "variant 应该是对象类型";
    ASSERT_EQ(v.size(), str_int_map.size()) << "映射大小不匹配";

    // 检查对象元素
    dict d = v.as<dict>();
    for (const auto& [key, value] : str_int_map) {
        ASSERT_TRUE(d.contains(key)) << "映射缺少键: " << key;
        ASSERT_EQ(d[key].as<int>(), value) << "映射值不匹配，键: " << key;
    }

    // 转换回 std::map
    std::map<std::string, int> result;
    from_variant(v, result);
    ASSERT_EQ(result, str_int_map) << "转换回的映射不匹配";

    // 使用 as 方法转换
    std::map<std::string, int> result2 = v.as<std::map<std::string, int>>();
    ASSERT_EQ(result2, str_int_map) << "使用 as 方法转换的映射不匹配";

    // 测试不同类型的映射
    std::map<std::string, std::string> str_str_map = {{"key1", "value1"}, {"key2", "value2"}};
    to_variant(str_str_map, v);
    std::map<std::string, std::string> str_str_result = v.as<std::map<std::string, std::string>>();
    ASSERT_EQ(str_str_result, str_str_map) << "字符串到字符串映射转换失败";

    // 测试复杂类型的映射
    std::map<std::string, std::vector<int>> complex_map = {{"vec1", {1, 2, 3}},
                                                           {"vec2", {4, 5, 6}}};
    to_variant(complex_map, v);
    std::map<std::string, std::vector<int>> complex_result =
        v.as<std::map<std::string, std::vector<int>>>();
    ASSERT_EQ(complex_result, complex_map) << "复杂映射转换失败";
}

/**
 * @brief 测试 std::unordered_map 与 variant 的转换
 */
TEST_F(VariantStdContainersTest, UnorderedMapConversion) {
    // 创建字符串到整数的无序映射
    std::unordered_map<std::string, int> str_int_map = {{"one", 1}, {"two", 2}, {"three", 3}};

    variant v;
    to_variant(str_int_map, v);

    ASSERT_TRUE(v.is_object()) << "variant 应该是对象类型";
    ASSERT_EQ(v.size(), str_int_map.size()) << "映射大小不匹配";

    // 检查对象元素
    dict d = v.as<dict>();
    for (const auto& [key, value] : str_int_map) {
        ASSERT_TRUE(d.contains(key)) << "映射缺少键: " << key;
        ASSERT_EQ(d[key].as<int>(), value) << "映射值不匹配，键: " << key;
    }

    // 转换回 std::unordered_map
    std::unordered_map<std::string, int> result;
    from_variant(v, result);

    // 由于无序映射的顺序不确定，我们只能检查大小和内容
    ASSERT_EQ(result.size(), str_int_map.size()) << "转换回的映射大小不匹配";
    for (const auto& [key, value] : str_int_map) {
        ASSERT_TRUE(result.find(key) != result.end()) << "转换回的映射缺少键: " << key;
        ASSERT_EQ(result[key], value) << "转换回的映射值不匹配，键: " << key;
    }

    // 使用 as 方法转换
    std::unordered_map<std::string, int> result2 = v.as<std::unordered_map<std::string, int>>();
    ASSERT_EQ(result2.size(), str_int_map.size()) << "使用 as 方法转换的映射大小不匹配";
    for (const auto& [key, value] : str_int_map) {
        ASSERT_TRUE(result2.find(key) != result2.end()) << "使用 as 方法转换的映射缺少键: " << key;
        ASSERT_EQ(result2[key], value) << "使用 as 方法转换的映射值不匹配，键: " << key;
    }
}

/**
 * @brief 测试 std::set 与 variant 的转换
 */
TEST_F(VariantStdContainersTest, SetConversion) {
    // 创建整数集合
    std::set<int> int_set = {1, 2, 3, 4, 5};

    variant v;
    to_variant(int_set, v);

    ASSERT_TRUE(v.is_array()) << "variant 应该是数组类型";
    ASSERT_EQ(v.size(), int_set.size()) << "集合大小不匹配";

    // 检查数组元素（集合是有序的）
    int i = 0;
    for (int value : int_set) {
        ASSERT_EQ(v[i].as<int>(), value) << "集合元素不匹配，位置: " << i;
        ++i;
    }

    // 转换回 std::set
    std::set<int> result;
    from_variant(v, result);
    ASSERT_EQ(result, int_set) << "转换回的集合不匹配";

    // 使用 as 方法转换
    std::set<int> result2 = v.as<std::set<int>>();
    ASSERT_EQ(result2, int_set) << "使用 as 方法转换的集合不匹配";

    // 测试字符串集合
    std::set<std::string> str_set = {"apple", "banana", "cherry"};
    to_variant(str_set, v);
    std::set<std::string> str_result = v.as<std::set<std::string>>();
    ASSERT_EQ(str_result, str_set) << "字符串集合转换失败";
}

/**
 * @brief 测试 std::unordered_set 与 variant 的转换
 */
TEST_F(VariantStdContainersTest, UnorderedSetConversion) {
    // 创建整数无序集合
    std::unordered_set<int> int_set = {1, 2, 3, 4, 5};

    variant v;
    to_variant(int_set, v);

    ASSERT_TRUE(v.is_array()) << "variant 应该是数组类型";
    ASSERT_EQ(v.size(), int_set.size()) << "集合大小不匹配";

    // 转换回 std::unordered_set
    std::unordered_set<int> result;
    from_variant(v, result);

    // 由于无序集合的顺序不确定，我们只能检查大小和内容
    ASSERT_EQ(result.size(), int_set.size()) << "转换回的集合大小不匹配";
    for (int value : int_set) {
        ASSERT_TRUE(result.find(value) != result.end()) << "转换回的集合缺少值: " << value;
    }

    // 使用 as 方法转换
    std::unordered_set<int> result2 = v.as<std::unordered_set<int>>();
    ASSERT_EQ(result2.size(), int_set.size()) << "使用 as 方法转换的集合大小不匹配";
    for (int value : int_set) {
        ASSERT_TRUE(result2.find(value) != result2.end())
            << "使用 as 方法转换的集合缺少值: " << value;
    }
}

/**
 * @brief 测试 std::array 与 variant 的转换
 */
TEST_F(VariantStdContainersTest, ArrayConversion) {
    // 创建固定大小的数组
    std::array<int, 5> int_array = {1, 2, 3, 4, 5};

    variant v;
    to_variant(int_array, v);

    ASSERT_TRUE(v.is_array()) << "variant 应该是数组类型";
    ASSERT_EQ(v.size(), int_array.size()) << "数组大小不匹配";

    // 检查数组元素
    for (size_t i = 0; i < int_array.size(); ++i) {
        ASSERT_EQ(v[i].as<int>(), int_array[i]) << "数组元素不匹配，位置: " << i;
    }

    // 转换回 std::array
    std::array<int, 5> result;
    from_variant(v, result);
    ASSERT_EQ(result, int_array) << "转换回的数组不匹配";

    // 使用 as 方法转换
    std::array<int, 5> result2 = v.as<std::array<int, 5>>();
    ASSERT_EQ(result2, int_array) << "使用 as 方法转换的数组不匹配";
}

/**
 * @brief 测试 std::deque 与 variant 的转换
 */
TEST_F(VariantStdContainersTest, DequeConversion) {
    // 创建双端队列
    std::deque<int> int_deque = {1, 2, 3, 4, 5};

    variant v;
    to_variant(int_deque, v);

    ASSERT_TRUE(v.is_array()) << "variant 应该是数组类型";
    ASSERT_EQ(v.size(), int_deque.size()) << "双端队列大小不匹配";

    // 检查数组元素
    for (size_t i = 0; i < int_deque.size(); ++i) {
        ASSERT_EQ(v[i].as<int>(), int_deque[i]) << "双端队列元素不匹配，位置: " << i;
    }

    // 转换回 std::deque
    std::deque<int> result;
    from_variant(v, result);

    // 比较双端队列内容
    ASSERT_EQ(result.size(), int_deque.size()) << "转换回的双端队列大小不匹配";
    for (size_t i = 0; i < int_deque.size(); ++i) {
        ASSERT_EQ(result[i], int_deque[i]) << "转换回的双端队列元素不匹配，位置: " << i;
    }

    // 使用 as 方法转换
    std::deque<int> result2 = v.as<std::deque<int>>();
    ASSERT_EQ(result2.size(), int_deque.size()) << "使用 as 方法转换的双端队列大小不匹配";
    for (size_t i = 0; i < int_deque.size(); ++i) {
        ASSERT_EQ(result2[i], int_deque[i]) << "使用 as 方法转换的双端队列元素不匹配，位置: " << i;
    }
}

/**
 * @brief 测试 std::list 与 variant 的转换
 */
TEST_F(VariantStdContainersTest, ListConversion) {
    // 创建链表
    std::list<int> int_list = {1, 2, 3, 4, 5};

    variant v;
    to_variant(int_list, v);

    ASSERT_TRUE(v.is_array()) << "variant 应该是数组类型";
    ASSERT_EQ(v.size(), int_list.size()) << "链表大小不匹配";

    // 转换回 std::list
    std::list<int> result;
    from_variant(v, result);

    // 比较链表内容
    ASSERT_EQ(result.size(), int_list.size()) << "转换回的链表大小不匹配";
    auto it1 = int_list.begin();
    auto it2 = result.begin();
    for (size_t i = 0; i < int_list.size(); ++i) {
        ASSERT_EQ(*it2, *it1) << "转换回的链表元素不匹配，位置: " << i;
        ++it1;
        ++it2;
    }

    // 使用 as 方法转换
    std::list<int> result2 = v.as<std::list<int>>();
    ASSERT_EQ(result2.size(), int_list.size()) << "使用 as 方法转换的链表大小不匹配";
    it1      = int_list.begin();
    auto it3 = result2.begin();
    for (size_t i = 0; i < int_list.size(); ++i) {
        ASSERT_EQ(*it3, *it1) << "使用 as 方法转换的链表元素不匹配，位置: " << i;
        ++it1;
        ++it3;
    }
}

/**
 * @brief 测试混合容器与 variant 的转换
 */
TEST_F(VariantStdContainersTest, MixedContainerConversion) {
    // 创建复杂的嵌套容器
    std::map<std::string, std::vector<std::pair<int, std::string>>> complex_container = {
        {"group1", {{1, "one"}, {2, "two"}}}, {"group2", {{3, "three"}, {4, "four"}}}};

    variant v;
    to_variant(complex_container, v);

    ASSERT_TRUE(v.is_object()) << "variant 应该是对象类型";
    ASSERT_EQ(v.size(), complex_container.size()) << "容器大小不匹配";

    // 检查对象结构
    dict d = v.as<dict>();
    ASSERT_TRUE(d.contains("group1")) << "缺少键 group1";
    ASSERT_TRUE(d.contains("group2")) << "缺少键 group2";

    ASSERT_TRUE(d["group1"].is_array()) << "group1 应该是数组";
    ASSERT_EQ(d["group1"].size(), 2) << "group1 数组大小不匹配";

    // 转换回原始类型
    std::map<std::string, std::vector<std::pair<int, std::string>>> result =
        v.as<std::map<std::string, std::vector<std::pair<int, std::string>>>>();

    // 比较内容
    ASSERT_EQ(result.size(), complex_container.size()) << "转换回的容器大小不匹配";
    for (const auto& [key, value] : complex_container) {
        ASSERT_TRUE(result.find(key) != result.end()) << "转换回的容器缺少键: " << key;
        ASSERT_EQ(result[key].size(), value.size()) << "转换回的内部向量大小不匹配，键: " << key;

        for (size_t i = 0; i < value.size(); ++i) {
            ASSERT_EQ(result[key][i].first, value[i].first)
                << "转换回的对组 first 不匹配，键: " << key << ", 位置: " << i;
            ASSERT_EQ(result[key][i].second, value[i].second)
                << "转换回的对组 second 不匹配，键: " << key << ", 位置: " << i;
        }
    }
}

/**
 * @brief 测试空容器与 variant 的转换
 */
TEST_F(VariantStdContainersTest, EmptyContainerConversion) {
    // 测试空向量
    std::vector<int> empty_vec;
    variant          v;
    to_variant(empty_vec, v);

    ASSERT_TRUE(v.is_array()) << "variant 应该是数组类型";
    ASSERT_EQ(v.size(), 0) << "空向量大小应该为 0";

    std::vector<int> result_vec = v.as<std::vector<int>>();
    ASSERT_TRUE(result_vec.empty()) << "转换回的向量应该为空";

    // 测试空映射
    std::map<std::string, int> empty_map;
    to_variant(empty_map, v);

    ASSERT_TRUE(v.is_object()) << "variant 应该是对象类型";
    ASSERT_EQ(v.size(), 0) << "空映射大小应该为 0";

    std::map<std::string, int> result_map = v.as<std::map<std::string, int>>();
    ASSERT_TRUE(result_map.empty()) << "转换回的映射应该为空";
}

/**
 * @brief 测试类型不匹配的容器转换
 */
TEST_F(VariantStdContainersTest, TypeMismatchConversion) {
    // 创建整数向量
    std::vector<int> int_vec = {1, 2, 3};
    variant          v;
    to_variant(int_vec, v);

    // 尝试转换为不兼容的类型
    bool exception_thrown = false;
    try {
        auto result = v.as<std::map<std::string, int>>();
    } catch (const std::exception& e) {
        exception_thrown = true;
        // 检查异常消息是否包含有用的信息
        EXPECT_TRUE(std::string(e.what()).find("类型错误") != std::string::npos);
    }
    ASSERT_TRUE(exception_thrown) << "将数组转换为映射应该抛出异常";

    // 创建字符串到整数的映射
    std::map<std::string, int> str_int_map = {{"one", 1}, {"two", 2}};
    to_variant(str_int_map, v);

    // 尝试转换为不兼容的类型
    exception_thrown = false;
    try {
        auto result = v.as<std::vector<int>>();
    } catch (const std::exception& e) {
        exception_thrown = true;
        // 检查异常消息是否包含有用的信息
        EXPECT_TRUE(std::string(e.what()).find("类型错误") != std::string::npos);
    }
    ASSERT_TRUE(exception_thrown) << "将对象转换为向量应该抛出异常";
}

} // namespace test
} // namespace mc