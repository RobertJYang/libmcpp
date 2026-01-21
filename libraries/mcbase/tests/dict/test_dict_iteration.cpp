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

#include <algorithm>
#include <gtest/gtest.h>
#include <mc/dict.h>
#include <mc/variant.h>
#include <vector>

namespace {

// 测试 dict 的正确迭代方式（修复后）
TEST(DictIterationTest, CorrectIteration) {
    mc::dict dict;
    dict["key1"] = "value1";
    dict["key2"] = "value2";
    dict["key3"] = "value3";

    // 测试正确的迭代方式：使用 entry.key 和 entry.value
    std::map<std::string, std::string> result;
    for (const auto& entry : dict) {
        result[entry.key] = entry.value.as_string();
    }

    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result["key1"], "value1");
    EXPECT_EQ(result["key2"], "value2");
    EXPECT_EQ(result["key3"], "value3");
}

// 测试不应该使用的结构化绑定方式
TEST(DictIterationTest, StructuredBindingNotSupported) {
    mc::dict dict;
    dict["key1"] = "value1";
    dict["key2"] = "value2";

    // 这种方式不应该编译通过，因为 mc::dict::entry 不支持结构化绑定
    // for (auto& [key, value] : dict) {  // 这会导致编译错误
    //     // 处理 key 和 value
    // }

    // 正确的方式：
    std::vector<std::pair<std::string, std::string>> pairs;
    for (const auto& entry : dict) {
        pairs.emplace_back(entry.key, entry.value.as_string());
    }

    EXPECT_EQ(pairs.size(), 2);
    EXPECT_EQ(pairs[0].first, "key1");
    EXPECT_EQ(pairs[0].second, "value1");
}

// 测试 dict 的迭代
TEST(DictIterationTest, MutableDictIteration) {
    mc::dict dict;
    dict["item1"] = 10;
    dict["item2"] = 20;
    dict["item3"] = 30;

    // 测试修改值的迭代
    for (auto& entry : dict) {
        if (entry.key == "item2") {
            entry.value = 200; // 修改值
        }
    }

    EXPECT_EQ(dict["item1"].as_int32(), 10);
    EXPECT_EQ(dict["item2"].as_int32(), 200); // 值应该被修改
    EXPECT_EQ(dict["item3"].as_int32(), 30);
}

// 测试 const dict 的迭代
TEST(DictIterationTest, ConstDictIteration) {
    mc::dict dict;
    dict["const1"] = "value1";
    dict["const2"] = "value2";

    const mc::dict& const_dict = dict;

    // 测试 const 迭代
    std::vector<std::string> keys;
    std::vector<std::string> values;

    for (const auto& entry : const_dict) {
        keys.push_back(entry.key);
        values.push_back(entry.value.as_string());
    }

    EXPECT_EQ(keys.size(), 2);
    EXPECT_EQ(values.size(), 2);

    // 验证包含期望的键值
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "const1") != keys.end());
    EXPECT_TRUE(std::find(keys.begin(), keys.end(), "const2") != keys.end());
    EXPECT_TRUE(std::find(values.begin(), values.end(), "value1") != values.end());
    EXPECT_TRUE(std::find(values.begin(), values.end(), "value2") != values.end());
}

// 测试复杂对象的迭代
TEST(DictIterationTest, ComplexObjectIteration) {
    mc::dict dict;

    // 添加不同类型的值
    dict["string_val"] = std::string("text");
    dict["int_val"]    = 42;
    dict["bool_val"]   = true;
    dict["double_val"] = 3.14;

    // 嵌套字典
    mc::dict nested;
    nested["nested_key"] = "nested_value";
    dict["nested_dict"]  = nested;

    // 迭代并验证类型
    std::map<std::string, mc::type_id> type_map;
    for (const auto& entry : dict) {
        type_map[entry.key] = entry.value.get_type();
    }

    EXPECT_EQ(type_map["string_val"], mc::type_id::string_type);
    EXPECT_EQ(type_map["int_val"], mc::type_id::int_type);
    EXPECT_EQ(type_map["bool_val"], mc::type_id::bool_type);
    EXPECT_EQ(type_map["double_val"], mc::type_id::double_type);
    EXPECT_EQ(type_map["nested_dict"], mc::type_id::object_type);
}

// 测试空字典的迭代
TEST(DictIterationTest, EmptyDictIteration) {
    mc::dict empty_dict;

    int count = 0;
    for (const auto& entry : empty_dict) {
        count++;
        // 这里不应该执行
        FAIL() << "Empty dict should not iterate";
    }

    EXPECT_EQ(count, 0);
}

// 测试迭代器的性能和正确性
TEST(DictIterationTest, IteratorPerformance) {
    mc::dict large_dict;

    // 创建一个较大的字典
    const int size = 1000;
    for (int i = 0; i < size; ++i) {
        large_dict["key" + std::to_string(i)] = i;
    }

    // 测试迭代计数
    int count = 0;
    for (const auto& entry : large_dict) {
        count++;
        // 验证键值对的正确性
        std::string expected_key = "key" + std::to_string(entry.value.as_int32());
        EXPECT_EQ(entry.key, expected_key);
    }

    EXPECT_EQ(count, size);
}

// 测试迭代过程中的查找操作
TEST(DictIterationTest, IterationWithLookup) {
    mc::dict dict;
    dict["apple"]  = 10;
    dict["banana"] = 20;
    dict["cherry"] = 30;

    // 在迭代过程中进行查找操作
    for (const auto& entry : dict) {
        // 使用迭代器的值进行查找验证
        auto found_value = dict[entry.key];
        EXPECT_EQ(found_value.as_int32(), entry.value.as_int32());

        // 验证contains方法
        EXPECT_TRUE(dict.contains(entry.key));
    }
}

// 测试 const dict 的反向迭代器（rbegin() 和 rend()）
TEST(DictIterationTest, DictConstReverseIterator) {
    const dict d({{"key1", 1}, {"key2", 2}, {"key3", 3}});

    // 使用反向迭代器遍历
    std::vector<std::string> reverse_keys;
    for (auto it = d.rbegin(); it != d.rend(); ++it) {
        reverse_keys.push_back(it->key.as_string());
    }

    // 验证反向顺序
    EXPECT_EQ(reverse_keys.size(), 3);
    // 注意：反向迭代器的顺序应该与正向相反
    // 正向顺序可能是 key1, key2, key3，反向应该是 key3, key2, key1
    // 但实际顺序取决于插入顺序，这里主要验证反向迭代器能正常工作
    EXPECT_TRUE(std::find(reverse_keys.begin(), reverse_keys.end(), "key1") != reverse_keys.end());
    EXPECT_TRUE(std::find(reverse_keys.begin(), reverse_keys.end(), "key2") != reverse_keys.end());
    EXPECT_TRUE(std::find(reverse_keys.begin(), reverse_keys.end(), "key3") != reverse_keys.end());

    // 验证反向迭代器与正向迭代器的顺序相反
    std::vector<std::string> forward_keys;
    for (const auto& entry : d) {
        forward_keys.push_back(entry.key.as_string());
    }

    std::reverse(reverse_keys.begin(), reverse_keys.end());
    EXPECT_EQ(forward_keys, reverse_keys);
}

// 测试 find(nullptr) 异常
TEST(DictIterationTest, DictFindWithNullPointerKey) {
    const dict d({{"key1", 123}});
    EXPECT_THROW(d.find(nullptr), std::invalid_argument);
}

// 测试 find(const variant&)
TEST(DictIterationTest, DictFindWithVariantKey) {
    dict d({{"key1", 123}, {"key2", "value"}});
    const dict& const_d = d;

    variant key1("key1");
    variant key2("key2");
    variant key3("key3");

    auto it1 = const_d.find(key1);
    ASSERT_NE(it1, const_d.end());
    EXPECT_EQ(it1->value.as<int>(), 123);

    auto it2 = const_d.find(key2);
    ASSERT_NE(it2, const_d.end());
    EXPECT_EQ(it2->value.as<std::string>(), "value");

    auto it3 = const_d.find(key3);
    EXPECT_EQ(it3, const_d.end());
}

// 测试非 const find(nullptr) 异常
TEST(DictIterationTest, MutableDictFindWithNullPointerKey) {
    dict md;
    md["key1"] = 123;
    EXPECT_THROW(md.find(nullptr), std::invalid_argument);
}

// 测试非 const find(const variant&)
TEST(DictIterationTest, MutableDictFindWithVariantKey) {
    dict md;
    md["key1"] = 123;
    md["key2"] = "value";

    variant key1("key1");
    variant key2("key2");
    variant key3("key3");

    auto it1 = md.find(key1);
    ASSERT_NE(it1, md.end());
    EXPECT_EQ(it1->value.as<int>(), 123);
    it1->value = 456;  // 可以修改
    EXPECT_EQ(md["key1"].as<int>(), 456);

    auto it2 = md.find(key2);
    ASSERT_NE(it2, md.end());
    EXPECT_EQ(it2->value.as<std::string>(), "value");

    auto it3 = md.find(key3);
    EXPECT_EQ(it3, md.end());
}

// 测试非 const find 键不存在返回 end()
TEST(DictIterationTest, MutableDictFindWithNonExistentKey) {
    dict md;
    md["key1"] = 123;
    md["key2"] = "value";

    auto it = md.find("key3");
    EXPECT_EQ(it, md.end());

    // 验证 end() 迭代器不能解引用
    // 注意：解引用 end() 迭代器是未定义行为，这里只验证它等于 end()
    EXPECT_TRUE(it == md.end());
}

} // namespace