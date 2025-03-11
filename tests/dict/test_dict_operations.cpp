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
 * @file test_dict_operations.cpp
 * @brief 测试 dict 和 mutable_dict 类的各种操作方法
 */
#include <gtest/gtest.h>
#include <mc/dict.h>
#include <mc/variant.h>
#include <vector>
#include <algorithm>

using namespace mc;

// 测试 dict 的基本访问操作
TEST(DictOperationsTest, DictBasicAccess) {
    dict d({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    // 测试 operator[]
    EXPECT_EQ(d["key1"].as<int>(), 123);
    EXPECT_EQ(d["key2"].as<std::string>(), "value");
    EXPECT_EQ(d["key3"].as<bool>(), true);
    EXPECT_THROW(d["key4"], std::out_of_range);
    
    // 测试 get 方法
    EXPECT_EQ(d.get("key1", 0).as<int>(), 123);
    EXPECT_EQ(d.get("key4", 456).as<int>(), 456);
    
    // 测试 contains 方法
    EXPECT_TRUE(d.contains("key1"));
    EXPECT_TRUE(d.contains("key2"));
    EXPECT_TRUE(d.contains("key3"));
    EXPECT_FALSE(d.contains("key4"));
    
    // 测试 size 和 empty 方法
    EXPECT_EQ(d.size(), 3);
    EXPECT_FALSE(d.empty());
    
    // 测试 at 方法
    EXPECT_EQ(d.at(0).key, "key1");
    EXPECT_EQ(d.at(0).value.as<int>(), 123);
    EXPECT_EQ(d.at(1).key, "key2");
    EXPECT_EQ(d.at(1).value.as<std::string>(), "value");
    EXPECT_EQ(d.at(2).key, "key3");
    EXPECT_EQ(d.at(2).value.as<bool>(), true);
    EXPECT_THROW(d.at(3), std::out_of_range);
    
    // 测试 find_index 方法
    EXPECT_EQ(d.find_index("key1"), 0);
    EXPECT_EQ(d.find_index("key2"), 1);
    EXPECT_EQ(d.find_index("key3"), 2);
    EXPECT_EQ(d.find_index("key4"), -1);
}

// 测试 dict 的迭代器操作
TEST(DictOperationsTest, DictIterators) {
    dict d({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    // 测试迭代器遍历
    std::vector<std::string> keys;
    std::vector<variant> values;
    
    for (const auto& entry : d) {
        keys.push_back(entry.key);
        values.push_back(entry.value);
    }
    
    EXPECT_EQ(keys.size(), 3);
    EXPECT_EQ(values.size(), 3);
    
    // 验证键的顺序
    EXPECT_EQ(keys[0], "key1");
    EXPECT_EQ(keys[1], "key2");
    EXPECT_EQ(keys[2], "key3");
    
    // 验证值的内容
    EXPECT_EQ(values[0].as<int>(), 123);
    EXPECT_EQ(values[1].as<std::string>(), "value");
    EXPECT_EQ(values[2].as<bool>(), true);
    
    // 测试 begin 和 end 方法
    auto it = d.begin();
    EXPECT_EQ(it->key, "key1");
    EXPECT_EQ(it->value.as<int>(), 123);
    
    ++it;
    EXPECT_EQ(it->key, "key2");
    EXPECT_EQ(it->value.as<std::string>(), "value");
    
    ++it;
    EXPECT_EQ(it->key, "key3");
    EXPECT_EQ(it->value.as<bool>(), true);
    
    ++it;
    EXPECT_EQ(it, d.end());
}

// 测试 dict 的 find 方法
TEST(DictOperationsTest, DictFind) {
    dict d({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    // 测试 find 方法 (std::string 版本)
    auto it1 = d.find(std::string("key1"));
    EXPECT_NE(it1, d.end());
    EXPECT_EQ(it1->key, "key1");
    EXPECT_EQ(it1->value.as<int>(), 123);
    
    // 测试 find 方法 (std::string_view 版本)
    auto it2 = d.find(std::string_view("key2"));
    EXPECT_NE(it2, d.end());
    EXPECT_EQ(it2->key, "key2");
    EXPECT_EQ(it2->value.as<std::string>(), "value");
    
    // 测试 find 方法 (const char* 版本)
    auto it3 = d.find("key3");
    EXPECT_NE(it3, d.end());
    EXPECT_EQ(it3->key, "key3");
    EXPECT_EQ(it3->value.as<bool>(), true);
    
    // 测试查找不存在的键
    auto it4 = d.find("key4");
    EXPECT_EQ(it4, d.end());
}

// 测试 dict 的 keys 和 values 方法
TEST(DictOperationsTest, DictKeysAndValues) {
    dict d({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    // 测试 keys 方法
    std::vector<std::string> keys = d.keys();
    EXPECT_EQ(keys.size(), 3);
    EXPECT_EQ(keys[0], "key1");
    EXPECT_EQ(keys[1], "key2");
    EXPECT_EQ(keys[2], "key3");
    
    // 测试 values 方法
    std::vector<variant> values = d.values();
    EXPECT_EQ(values.size(), 3);
    EXPECT_EQ(values[0].as<int>(), 123);
    EXPECT_EQ(values[1].as<std::string>(), "value");
    EXPECT_EQ(values[2].as<bool>(), true);
}

// 测试 dict 的比较操作
TEST(DictOperationsTest, DictComparison) {
    dict d1({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    dict d2({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    dict d3({
        {"key1", 456},
        {"key2", "value"},
        {"key3", true}
    });
    
    dict d4({
        {"key1", 123},
        {"key2", "value"}
    });
    
    dict d5({
        {"key1", 123},
        {"key2", "value"},
        {"key4", false}
    });
    
    // 测试相等比较
    EXPECT_TRUE(d1 == d2);
    EXPECT_FALSE(d1 == d3);
    EXPECT_FALSE(d1 == d4);
    EXPECT_FALSE(d1 == d5);
    
    // 测试不等比较
    EXPECT_FALSE(d1 != d2);
    EXPECT_TRUE(d1 != d3);
    EXPECT_TRUE(d1 != d4);
    EXPECT_TRUE(d1 != d5);
}

// 测试 mutable_dict 的基本修改操作
TEST(DictOperationsTest, MutableDictBasicModification) {
    mutable_dict md({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    // 测试 operator[] 修改现有键值对
    md["key1"] = 456;
    EXPECT_EQ(md["key1"].as<int>(), 456);
    
    // 测试 operator[] 添加新键值对
    md["key4"] = "new value";
    EXPECT_EQ(md.size(), 4);
    EXPECT_EQ(md["key4"].as<std::string>(), "new value");
    
    // 测试 operator() 修改现有键值对
    md("key2", "modified value");
    EXPECT_EQ(md["key2"].as<std::string>(), "modified value");
    
    // 测试 operator() 添加新键值对
    md("key5", false);
    EXPECT_EQ(md.size(), 5);
    EXPECT_EQ(md["key5"].as<bool>(), false);
    
    // 测试链式调用 operator()
    md("key6", 789)("key7", "chain")("key8", true);
    EXPECT_EQ(md.size(), 8);
    EXPECT_EQ(md["key6"].as<int>(), 789);
    EXPECT_EQ(md["key7"].as<std::string>(), "chain");
    EXPECT_EQ(md["key8"].as<bool>(), true);
}

// 测试 mutable_dict 的 erase 和 clear 方法
TEST(DictOperationsTest, MutableDictEraseAndClear) {
    mutable_dict md({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    // 测试 erase 方法
    EXPECT_TRUE(md.erase("key1"));
    EXPECT_EQ(md.size(), 2);
    EXPECT_FALSE(md.contains("key1"));
    
    // 测试 erase 不存在的键
    EXPECT_FALSE(md.erase("key1"));
    EXPECT_EQ(md.size(), 2);
    
    // 测试 clear 方法
    md.clear();
    EXPECT_EQ(md.size(), 0);
    EXPECT_TRUE(md.empty());
}

// 测试 mutable_dict 的迭代器操作
TEST(DictOperationsTest, MutableDictIterators) {
    mutable_dict md({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    // 测试迭代器遍历并修改值
    for (auto& entry : md) {
        if (entry.key == "key1") {
            entry.value = 456;
        } else if (entry.key == "key2") {
            entry.value = "modified";
        }
    }
    
    EXPECT_EQ(md["key1"].as<int>(), 456);
    EXPECT_EQ(md["key2"].as<std::string>(), "modified");
    EXPECT_EQ(md["key3"].as<bool>(), true);
    
    // 测试 begin 和 end 方法
    auto it = md.begin();
    EXPECT_EQ(it->key, "key1");
    EXPECT_EQ(it->value.as<int>(), 456);
    
    ++it;
    EXPECT_EQ(it->key, "key2");
    EXPECT_EQ(it->value.as<std::string>(), "modified");
    
    ++it;
    EXPECT_EQ(it->key, "key3");
    EXPECT_EQ(it->value.as<bool>(), true);
    
    ++it;
    EXPECT_EQ(it, md.end());
}

// 测试 mutable_dict 的 at 方法
TEST(DictOperationsTest, MutableDictAt) {
    mutable_dict md({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    // 测试 at 方法读取
    EXPECT_EQ(md.at(0).key, "key1");
    EXPECT_EQ(md.at(0).value.as<int>(), 123);
    EXPECT_EQ(md.at(1).key, "key2");
    EXPECT_EQ(md.at(1).value.as<std::string>(), "value");
    EXPECT_EQ(md.at(2).key, "key3");
    EXPECT_EQ(md.at(2).value.as<bool>(), true);
    EXPECT_THROW(md.at(3), std::out_of_range);
    
    // 测试 at 方法修改
    md.at(0).value = 456;
    EXPECT_EQ(md["key1"].as<int>(), 456);
    
    md.at(1).value = "modified";
    EXPECT_EQ(md["key2"].as<std::string>(), "modified");
}

// 测试 dict 和 mutable_dict 之间的数据共享
TEST(DictOperationsTest, DataSharing) {
    dict d({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    mutable_dict md1({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    // 从 mutable_dict 创建 dict
    dict d2 = md1;
    
    // 从 dict 创建另一个 mutable_dict
    mutable_dict md2 = d2;
    
    // 修改 md1，验证 d 和 md2 也被修改
    md1["key1"] = 456;
    EXPECT_EQ(d2["key1"].as<int>(), 456);
    EXPECT_EQ(md2["key1"].as<int>(), 456);
    
    // 修改 md2，验证 d 和 md1 也被修改
    md2["key2"] = "modified";
    EXPECT_EQ(d2["key2"].as<std::string>(), "modified");
    EXPECT_EQ(md1["key2"].as<std::string>(), "modified");
    
    // 添加新键值对到 md1，验证 d 和 md2 也被修改
    md1["key4"] = 789;
    EXPECT_EQ(d2.size(), 4);
    EXPECT_EQ(d2["key4"].as<int>(), 789);
    EXPECT_EQ(md2.size(), 4);
    EXPECT_EQ(md2["key4"].as<int>(), 789);
    
    // 从 md2 删除键值对，验证 d 和 md1 也被修改
    md2.erase("key3");
    EXPECT_EQ(d2.size(), 3);
    EXPECT_FALSE(d2.contains("key3"));
    EXPECT_EQ(md1.size(), 3);
    EXPECT_FALSE(md1.contains("key3"));
}

// 测试 mutable_dict 的 find 方法
TEST(DictOperationsTest, MutableDictFind) {
    mutable_dict md({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    // 测试 find 方法 (std::string 版本)
    auto it1 = md.find(std::string("key1"));
    EXPECT_NE(it1, md.end());
    EXPECT_EQ(it1->key, "key1");
    EXPECT_EQ(it1->value.as<int>(), 123);
    
    // 修改找到的元素
    it1->value = 456;
    EXPECT_EQ(md["key1"].as<int>(), 456);
    
    // 测试 find 方法 (std::string_view 版本)
    auto it2 = md.find(std::string_view("key2"));
    EXPECT_NE(it2, md.end());
    EXPECT_EQ(it2->key, "key2");
    EXPECT_EQ(it2->value.as<std::string>(), "value");
    
    // 修改找到的元素
    it2->value = "modified";
    EXPECT_EQ(md["key2"].as<std::string>(), "modified");
    
    // 测试 find 方法 (const char* 版本)
    auto it3 = md.find("key3");
    EXPECT_NE(it3, md.end());
    EXPECT_EQ(it3->key, "key3");
    EXPECT_EQ(it3->value.as<bool>(), true);
    
    // 修改找到的元素
    it3->value = false;
    EXPECT_EQ(md["key3"].as<bool>(), false);
    
    // 测试查找不存在的键
    auto it4 = md.find("key4");
    EXPECT_EQ(it4, md.end());
    
    // 测试 const 版本的 find 方法
    const mutable_dict& cmd = md;
    
    auto it5 = cmd.find("key1");
    EXPECT_NE(it5, cmd.end());
    EXPECT_EQ(it5->key, "key1");
    EXPECT_EQ(it5->value.as<int>(), 456);
    
    auto it6 = cmd.find("key4");
    EXPECT_EQ(it6, cmd.end());
} 