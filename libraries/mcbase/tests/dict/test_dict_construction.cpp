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
 * @brief 测试 dict 和 dict 类的各种构造方法
 */
#include <gtest/gtest.h>
#include <mc/dict.h>
#include <mc/variant.h>
#include <utility>
#include <vector>

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
    entries.emplace_back("key1", 123);
    entries.emplace_back("key2", "value");
    entries.emplace_back("key3", true);

    dict d(entries);

    EXPECT_EQ(d.size(), 3);
    EXPECT_EQ(d["key1"].as<int>(), 123);
    EXPECT_EQ(d["key2"].as<std::string>(), "value");
    EXPECT_EQ(d["key3"].as<bool>(), true);
}

// 测试 dict 从 initializer_list 构造
TEST(DictConstructionTest, InitializerListConstructor) {
    dict d({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    EXPECT_EQ(d.size(), 3);
    EXPECT_EQ(d["key1"].as<int>(), 123);
    EXPECT_EQ(d["key2"].as<std::string>(), "value");
    EXPECT_EQ(d["key3"].as<bool>(), true);
}

// 测试 dict 拷贝构造函数
TEST(DictConstructionTest, CopyConstructor) {
    dict d1({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    dict d2(d1);

    // 验证 d2 是 d1 的副本
    EXPECT_EQ(d2.size(), 3);
    EXPECT_EQ(d2["key1"].as<int>(), 123);
    EXPECT_EQ(d2["key2"].as<std::string>(), "value");
    EXPECT_EQ(d2["key3"].as<bool>(), true);

    // 验证 d1 和 d2 共享数据
    dict md(d1);
    md["key1"] = 456;

    EXPECT_EQ(d1["key1"].as<int>(), 456);
    EXPECT_EQ(d2["key1"].as<int>(), 456);
}

// 测试 dict 移动构造函数
TEST(DictConstructionTest, MoveConstructor) {
    dict d1({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    dict d2(std::move(d1));

    // 验证 d2 包含正确的数据
    EXPECT_EQ(d2.size(), 3);
    EXPECT_EQ(d2["key1"].as<int>(), 123);
    EXPECT_EQ(d2["key2"].as<std::string>(), "value");
    EXPECT_EQ(d2["key3"].as<bool>(), true);
}

// 测试 dict 赋值运算符
TEST(DictConstructionTest, AssignmentOperator) {
    dict d1({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    dict d2;
    d2 = d1;

    // 验证 d2 是 d1 的副本
    EXPECT_EQ(d2.size(), 3);
    EXPECT_EQ(d2["key1"].as<int>(), 123);
    EXPECT_EQ(d2["key2"].as<std::string>(), "value");
    EXPECT_EQ(d2["key3"].as<bool>(), true);

    // 验证 d1 和 d2 共享数据
    dict md(d1);
    md["key1"] = 456;

    EXPECT_EQ(d1["key1"].as<int>(), 456);
    EXPECT_EQ(d2["key1"].as<int>(), 456);
}

// 测试 dict 移动赋值运算符
TEST(DictConstructionTest, MoveAssignmentOperator) {
    dict d1({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    dict d2;
    d2 = std::move(d1);

    // 验证 d2 包含正确的数据
    EXPECT_EQ(d2.size(), 3);
    EXPECT_EQ(d2["key1"].as<int>(), 123);
    EXPECT_EQ(d2["key2"].as<std::string>(), "value");
    EXPECT_EQ(d2["key3"].as<bool>(), true);
}

// 测试 dict 默认构造函数
TEST(DictConstructionTest, MutableDictDefaultConstructor) {
    dict md;
    EXPECT_TRUE(md.empty());
    EXPECT_EQ(md.size(), 0);
}

// 测试 dict 从 vector<entry> 构造
TEST(DictConstructionTest, MutableDictVectorEntryConstructor) {
    std::vector<dict::entry> entries;
    entries.emplace_back("key1", 123);
    entries.emplace_back("key2", "value");
    entries.emplace_back("key3", true);

    dict md(entries);

    EXPECT_EQ(md.size(), 3);
    EXPECT_EQ(md["key1"].as<int>(), 123);
    EXPECT_EQ(md["key2"].as<std::string>(), "value");
    EXPECT_EQ(md["key3"].as<bool>(), true);
}

// 测试 dict 从 initializer_list 构造
TEST(DictConstructionTest, MutableDictInitializerListConstructor) {
    dict md({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    EXPECT_EQ(md.size(), 3);
    EXPECT_EQ(md["key1"].as<int>(), 123);
    EXPECT_EQ(md["key2"].as<std::string>(), "value");
    EXPECT_EQ(md["key3"].as<bool>(), true);
}

// 测试 dict 从 dict 构造
TEST(DictConstructionTest, MutableDictFromDictConstructor) {
    dict d({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    dict md(d);

    // 验证 md 包含与 d 相同的数据
    EXPECT_EQ(md.size(), 3);
    EXPECT_EQ(md["key1"].as<int>(), 123);
    EXPECT_EQ(md["key2"].as<std::string>(), "value");
    EXPECT_EQ(md["key3"].as<bool>(), true);

    // 验证 md 和 d 共享数据
    md["key1"] = 456;
    EXPECT_EQ(d["key1"].as<int>(), 456);
}

// 测试 dict 拷贝构造函数
TEST(DictConstructionTest, MutableDictCopyConstructor) {
    dict md1({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    dict md2(md1);

    // 验证 md2 是 md1 的副本
    EXPECT_EQ(md2.size(), 3);
    EXPECT_EQ(md2["key1"].as<int>(), 123);
    EXPECT_EQ(md2["key2"].as<std::string>(), "value");
    EXPECT_EQ(md2["key3"].as<bool>(), true);

    // 验证 md1 和 md2 共享数据
    md1["key1"] = 456;
    EXPECT_EQ(md2["key1"].as<int>(), 456);
}

// 测试 dict 移动构造函数
TEST(DictConstructionTest, MutableDictMoveConstructor) {
    dict md1({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    dict md2(std::move(md1));

    // 验证 md2 包含正确的数据
    EXPECT_EQ(md2.size(), 3);
    EXPECT_EQ(md2["key1"].as<int>(), 123);
    EXPECT_EQ(md2["key2"].as<std::string>(), "value");
    EXPECT_EQ(md2["key3"].as<bool>(), true);
}

// 测试 dict 赋值运算符
TEST(DictConstructionTest, MutableDictAssignmentOperator) {
    dict md1({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    dict md2;
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

// 测试 dict 移动赋值运算符
TEST(DictConstructionTest, MutableDictMoveAssignmentOperator) {
    dict md1({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    dict md2;
    md2 = std::move(md1);

    // 验证 md2 包含正确的数据
    EXPECT_EQ(md2.size(), 3);
    EXPECT_EQ(md2["key1"].as<int>(), 123);
    EXPECT_EQ(md2["key2"].as<std::string>(), "value");
    EXPECT_EQ(md2["key3"].as<bool>(), true);
}

// 测试 dict 从 dict 赋值
TEST(DictConstructionTest, MutableDictAssignmentFromDict) {
    dict d({{"key1", 123}, {"key2", "value"}, {"key3", true}});

    dict md;
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

// 测试 dict 链式调用构造函数
TEST(DictConstructionTest, MutableDictChainedConstruction) {
    // 使用单键值对构造函数
    dict md1("key1", 123);
    EXPECT_EQ(md1.size(), 1);
    EXPECT_EQ(md1["key1"].as<int>(), 123);

    // 使用链式调用添加更多键值对
    dict md2("key1", 123);
    md2("key2", "value")("key3", true);
    EXPECT_EQ(md2.size(), 3);
    EXPECT_EQ(md2["key1"].as<int>(), 123);
    EXPECT_EQ(md2["key2"].as<std::string>(), "value");
    EXPECT_EQ(md2["key3"].as<bool>(), true);

    // 使用不同类型的键
    std::string      key1 = "key1";
    std::string_view key2 = "key2";
    const char*      key3 = "key3";

    dict md3(key1, 123);
    md3(key2, "value")(key3, true);
    EXPECT_EQ(md3.size(), 3);
    EXPECT_EQ(md3["key1"].as<int>(), 123);
    EXPECT_EQ(md3["key2"].as<std::string>(), "value");
    EXPECT_EQ(md3["key3"].as<bool>(), true);

    // 测试混合使用链式调用和其他方法
    dict md4("key1", 123);
    md4["key2"] = "value";
    md4("key3", true);
    EXPECT_EQ(md4.size(), 3);
    EXPECT_EQ(md4["key1"].as<int>(), 123);
    EXPECT_EQ(md4["key2"].as<std::string>(), "value");
    EXPECT_EQ(md4["key3"].as<bool>(), true);
}

// 测试从 std::vector<entry> 构造时重复键的处理
TEST(DictConstructionTest, DictConstructorFromVectorWithDuplicateKeys) {
    std::vector<dict::entry> entries;
    entries.emplace_back("key1", 100);  // 第一个值
    entries.emplace_back("key2", "value1");
    entries.emplace_back("key1", 200);  // 重复键，应该覆盖第一个值
    entries.emplace_back("key3", true);
    entries.emplace_back("key2", "value2");  // 重复键，应该覆盖第一个值

    dict d(entries);

    // 验证重复键的值被覆盖（后一个值覆盖前一个值）
    EXPECT_EQ(d.size(), 3);  // 只有3个唯一的键
    EXPECT_EQ(d["key1"].as<int>(), 200);  // 应该是后一个值
    EXPECT_EQ(d["key2"].as<std::string>(), "value2");  // 应该是后一个值
    EXPECT_EQ(d["key3"].as<bool>(), true);
}

// ========== 默认构造后拷贝共享语义测试 ==========

// 测试默认构造后拷贝构造函数的共享语义
TEST(DictConstructionTest, CopyConstructorFromDefaultConstructed) {
    dict d1;  // 默认构造，m_data 为 nullptr
    dict d2 = d1;  // 拷贝构造，应该共享数据

    // 在拷贝时，d1.ensure_data() 会被调用，创建共享数据块
    // d2 应该与 d1 共享同一个数据块

    // 验证两个对象都指向同一个空数据块
    EXPECT_TRUE(d1.empty());
    EXPECT_TRUE(d2.empty());

    // 修改 d1 应该影响 d2
    d1["key1"] = 100;
    EXPECT_EQ(d1.size(), 1);
    EXPECT_EQ(d1["key1"].as<int>(), 100);
    EXPECT_EQ(d2.size(), 1);  // d2 也应该看到变化
    EXPECT_EQ(d2["key1"].as<int>(), 100);

    // 修改 d2 应该影响 d1
    d2["key2"] = 200;
    EXPECT_EQ(d1.size(), 2);
    EXPECT_EQ(d1["key2"].as<int>(), 200);
    EXPECT_EQ(d2.size(), 2);
    EXPECT_EQ(d2["key2"].as<int>(), 200);
}

// 测试默认构造后拷贝赋值运算符的共享语义
TEST(DictConstructionTest, CopyAssignmentFromDefaultConstructed) {
    dict d1;  // 默认构造，m_data 为 nullptr
    dict d2;
    d2 = d1;  // 拷贝赋值，应该共享数据

    // 在赋值时，d1.ensure_data() 会被调用，创建共享数据块
    // d2 应该与 d1 共享同一个数据块

    // 验证两个对象都指向同一个空数据块
    EXPECT_TRUE(d1.empty());
    EXPECT_TRUE(d2.empty());

    // 修改 d1 应该影响 d2
    d1["key1"] = 300;
    EXPECT_EQ(d1.size(), 1);
    EXPECT_EQ(d1["key1"].as<int>(), 300);
    EXPECT_EQ(d2.size(), 1);  // d2 也应该看到变化
    EXPECT_EQ(d2["key1"].as<int>(), 300);
}

// 测试多个默认构造对象之间的拷贝共享
TEST(DictConstructionTest, MultipleCopiesFromDefaultConstructed) {
    dict d1;  // 默认构造
    dict d2 = d1;  // 第一次拷贝
    dict d3 = d1;  // 第二次拷贝

    // 所有三个对象应该共享同一个数据块

    // 通过 d2 添加数据
    d2["key1"] = 1;
    d2["key2"] = 2;
    d2["key3"] = 3;

    // 验证所有对象都看到相同的数据
    EXPECT_EQ(d1.size(), 3);
    EXPECT_EQ(d2.size(), 3);
    EXPECT_EQ(d3.size(), 3);

    EXPECT_EQ(d1["key1"].as<int>(), 1);
    EXPECT_EQ(d1["key2"].as<int>(), 2);
    EXPECT_EQ(d1["key3"].as<int>(), 3);

    EXPECT_EQ(d2["key1"].as<int>(), 1);
    EXPECT_EQ(d2["key2"].as<int>(), 2);
    EXPECT_EQ(d2["key3"].as<int>(), 3);

    EXPECT_EQ(d3["key1"].as<int>(), 1);
    EXPECT_EQ(d3["key2"].as<int>(), 2);
    EXPECT_EQ(d3["key3"].as<int>(), 3);

    // 通过 d3 修改数据
    d3["key1"] = 999;

    // 验证所有对象都看到修改
    EXPECT_EQ(d1["key1"].as<int>(), 999);
    EXPECT_EQ(d2["key1"].as<int>(), 999);
    EXPECT_EQ(d3["key1"].as<int>(), 999);
}

// 测试默认构造对象的链式拷贝
TEST(DictConstructionTest, ChainedCopyFromDefaultConstructed) {
    dict d1;  // 默认构造
    dict d2 = d1;
    dict d3 = d2;
    dict d4 = d3;

    // 所有对象应该共享同一个数据块

    // 通过 d4 添加数据
    d4["key"] = 42;

    // 验证所有对象都看到数据
    EXPECT_EQ(d1.size(), 1);
    EXPECT_EQ(d1["key"].as<int>(), 42);

    EXPECT_EQ(d2.size(), 1);
    EXPECT_EQ(d2["key"].as<int>(), 42);

    EXPECT_EQ(d3.size(), 1);
    EXPECT_EQ(d3["key"].as<int>(), 42);

    EXPECT_EQ(d4.size(), 1);
    EXPECT_EQ(d4["key"].as<int>(), 42);
}

// 测试默认构造对象拷贝后再赋值
TEST(DictConstructionTest, DefaultConstructedCopyThenReassign) {
    dict d1;  // 默认构造
    dict d2 = d1;  // 共享空数据块

    // 给 d1 添加一些数据
    d1["key1"] = 1;
    d1["key2"] = 2;

    EXPECT_EQ(d1.size(), 2);
    EXPECT_EQ(d2.size(), 2);  // d2 也应该看到

    // 创建一个新的有数据的字典
    dict d3({{"key3", 100}, {"key4", 200}, {"key5", 300}});

    // 将 d3 赋值给 d1
    d1 = d3;

    // 现在 d1 和 d3 应该共享数据
    // d2 应该不再与 d1 共享
    EXPECT_EQ(d1.size(), 3);
    EXPECT_EQ(d3.size(), 3);
    EXPECT_EQ(d2.size(), 2);  // d2 保持原来的数据

    EXPECT_EQ(d1["key3"].as<int>(), 100);
    EXPECT_EQ(d3["key3"].as<int>(), 100);

    // 修改 d1 应该影响 d3，但不影响 d2
    d1["key3"] = 999;
    EXPECT_EQ(d1["key3"].as<int>(), 999);
    EXPECT_EQ(d3["key3"].as<int>(), 999);
    EXPECT_EQ(d2["key1"].as<int>(), 1);  // d2 不受影响
}