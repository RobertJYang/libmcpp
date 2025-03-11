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
 * @file test_dict_construction.cpp
 * @brief 测试 dict 和 mutable_dict 类的各种构造方法
 */
#include <gtest/gtest.h>
#include <mc/dict.h>
#include <mc/variant.h>
#include <vector>
#include <utility>

using namespace mc;

// 测试 dict 默认构造函数
TEST(DictConstructionTest, DefaultConstructor) {
    dict d;
    EXPECT_TRUE(d.empty());
    EXPECT_EQ(d.size(), 0);
}

// 测试 dict 从 vector<entry> 构造
TEST(DictConstructionTest, VectorEntryConstructor) {
    std::vector<dict::entry> entries;
    entries.emplace_back("key1", variant(123));
    entries.emplace_back("key2", variant("value"));
    entries.emplace_back("key3", variant(true));
    
    dict d(entries);
    
    EXPECT_EQ(d.size(), 3);
    EXPECT_EQ(d["key1"].as<int>(), 123);
    EXPECT_EQ(d["key2"].as<std::string>(), "value");
    EXPECT_EQ(d["key3"].as<bool>(), true);
}

// 测试 dict 从 initializer_list 构造
TEST(DictConstructionTest, InitializerListConstructor) {
    dict d({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    EXPECT_EQ(d.size(), 3);
    EXPECT_EQ(d["key1"].as<int>(), 123);
    EXPECT_EQ(d["key2"].as<std::string>(), "value");
    EXPECT_EQ(d["key3"].as<bool>(), true);
}

// 测试 dict 拷贝构造函数
TEST(DictConstructionTest, CopyConstructor) {
    dict d1({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    dict d2(d1);
    
    // 验证 d2 是 d1 的副本
    EXPECT_EQ(d2.size(), 3);
    EXPECT_EQ(d2["key1"].as<int>(), 123);
    EXPECT_EQ(d2["key2"].as<std::string>(), "value");
    EXPECT_EQ(d2["key3"].as<bool>(), true);
    
    // 验证 d1 和 d2 共享数据
    mutable_dict md(d1);
    md["key1"] = 456;
    
    EXPECT_EQ(d1["key1"].as<int>(), 456);
    EXPECT_EQ(d2["key1"].as<int>(), 456);
}

// 测试 dict 移动构造函数
TEST(DictConstructionTest, MoveConstructor) {
    dict d1({
        {"key1", variant(123)},
        {"key2", variant("value")},
        {"key3", variant(true)}
    });
    
    dict d2(std::move(d1));
    
    // 验证 d2 包含正确的数据
    EXPECT_EQ(d2.size(), 3);
    EXPECT_EQ(d2["key1"].as<int>(), 123);
    EXPECT_EQ(d2["key2"].as<std::string>(), "value");
    EXPECT_EQ(d2["key3"].as<bool>(), true);
}

// 测试 dict 赋值运算符
TEST(DictConstructionTest, AssignmentOperator) {
    dict d1({
        {"key1", variant(123)},
        {"key2", variant("value")},
        {"key3", variant(true)}
    });
    
    dict d2;
    d2 = d1;
    
    // 验证 d2 是 d1 的副本
    EXPECT_EQ(d2.size(), 3);
    EXPECT_EQ(d2["key1"].as<int>(), 123);
    EXPECT_EQ(d2["key2"].as<std::string>(), "value");
    EXPECT_EQ(d2["key3"].as<bool>(), true);
    
    // 验证 d1 和 d2 共享数据
    mutable_dict md(d1);
    md["key1"] = 456;
    
    EXPECT_EQ(d1["key1"].as<int>(), 456);
    EXPECT_EQ(d2["key1"].as<int>(), 456);
}

// 测试 dict 移动赋值运算符
TEST(DictConstructionTest, MoveAssignmentOperator) {
    dict d1({
        {"key1", variant(123)},
        {"key2", variant("value")},
        {"key3", variant(true)}
    });
    
    dict d2;
    d2 = std::move(d1);
    
    // 验证 d2 包含正确的数据
    EXPECT_EQ(d2.size(), 3);
    EXPECT_EQ(d2["key1"].as<int>(), 123);
    EXPECT_EQ(d2["key2"].as<std::string>(), "value");
    EXPECT_EQ(d2["key3"].as<bool>(), true);
}

// 测试 mutable_dict 默认构造函数
TEST(DictConstructionTest, MutableDictDefaultConstructor) {
    mutable_dict md;
    EXPECT_TRUE(md.empty());
    EXPECT_EQ(md.size(), 0);
}

// 测试 mutable_dict 从 vector<entry> 构造
TEST(DictConstructionTest, MutableDictVectorEntryConstructor) {
    std::vector<dict::entry> entries;
    entries.emplace_back("key1", variant(123));
    entries.emplace_back("key2", variant("value"));
    entries.emplace_back("key3", variant(true));
    
    mutable_dict md(entries);
    
    EXPECT_EQ(md.size(), 3);
    EXPECT_EQ(md["key1"].as<int>(), 123);
    EXPECT_EQ(md["key2"].as<std::string>(), "value");
    EXPECT_EQ(md["key3"].as<bool>(), true);
}

// 测试 mutable_dict 从 initializer_list 构造
TEST(DictConstructionTest, MutableDictInitializerListConstructor) {
    mutable_dict md({
        {"key1", variant(123)},
        {"key2", variant("value")},
        {"key3", variant(true)}
    });
    
    EXPECT_EQ(md.size(), 3);
    EXPECT_EQ(md["key1"].as<int>(), 123);
    EXPECT_EQ(md["key2"].as<std::string>(), "value");
    EXPECT_EQ(md["key3"].as<bool>(), true);
}

// 测试 mutable_dict 从 dict 构造
TEST(DictConstructionTest, MutableDictFromDictConstructor) {
    dict d({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    mutable_dict md(d);
    
    // 验证 md 包含与 d 相同的数据
    EXPECT_EQ(md.size(), 3);
    EXPECT_EQ(md["key1"].as<int>(), 123);
    EXPECT_EQ(md["key2"].as<std::string>(), "value");
    EXPECT_EQ(md["key3"].as<bool>(), true);
    
    // 验证 md 和 d 共享数据
    md["key1"] = 456;
    EXPECT_EQ(d["key1"].as<int>(), 456);
}

// 测试 mutable_dict 拷贝构造函数
TEST(DictConstructionTest, MutableDictCopyConstructor) {
    mutable_dict md1({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    mutable_dict md2(md1);
    
    // 验证 md2 是 md1 的副本
    EXPECT_EQ(md2.size(), 3);
    EXPECT_EQ(md2["key1"].as<int>(), 123);
    EXPECT_EQ(md2["key2"].as<std::string>(), "value");
    EXPECT_EQ(md2["key3"].as<bool>(), true);
    
    // 验证 md1 和 md2 共享数据
    md1["key1"] = 456;
    EXPECT_EQ(md2["key1"].as<int>(), 456);
}

// 测试 mutable_dict 移动构造函数
TEST(DictConstructionTest, MutableDictMoveConstructor) {
    mutable_dict md1({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    mutable_dict md2(std::move(md1));
    
    // 验证 md2 包含正确的数据
    EXPECT_EQ(md2.size(), 3);
    EXPECT_EQ(md2["key1"].as<int>(), 123);
    EXPECT_EQ(md2["key2"].as<std::string>(), "value");
    EXPECT_EQ(md2["key3"].as<bool>(), true);
}

// 测试 mutable_dict 赋值运算符
TEST(DictConstructionTest, MutableDictAssignmentOperator) {
    mutable_dict md1({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    mutable_dict md2;
    md2 = md1;
    
    // 验证 md2 是 md1 的副本
    EXPECT_EQ(md2.size(), 3);
    EXPECT_EQ(md2["key1"].as<int>(), 123);
    EXPECT_EQ(md2["key2"].as<std::string>(), "value");
    EXPECT_EQ(md2["key3"].as<bool>(), true);
    
    // 验证 md1 和 md2 共享数据
    md1["key1"] = 456;
    EXPECT_EQ(md2["key1"].as<int>(), 456);
}

// 测试 mutable_dict 移动赋值运算符
TEST(DictConstructionTest, MutableDictMoveAssignmentOperator) {
    mutable_dict md1({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    mutable_dict md2;
    md2 = std::move(md1);
    
    // 验证 md2 包含正确的数据
    EXPECT_EQ(md2.size(), 3);
    EXPECT_EQ(md2["key1"].as<int>(), 123);
    EXPECT_EQ(md2["key2"].as<std::string>(), "value");
    EXPECT_EQ(md2["key3"].as<bool>(), true);
}

// 测试 mutable_dict 从 dict 赋值
TEST(DictConstructionTest, MutableDictAssignmentFromDict) {
    dict d({
        {"key1", 123},
        {"key2", "value"},
        {"key3", true}
    });
    
    mutable_dict md;
    md = d;
    
    // 验证 md 包含与 d 相同的数据
    EXPECT_EQ(md.size(), 3);
    EXPECT_EQ(md["key1"].as<int>(), 123);
    EXPECT_EQ(md["key2"].as<std::string>(), "value");
    EXPECT_EQ(md["key3"].as<bool>(), true);
    
    // 验证 md 和 d 共享数据
    md["key1"] = 456;
    EXPECT_EQ(d["key1"].as<int>(), 456);
} 