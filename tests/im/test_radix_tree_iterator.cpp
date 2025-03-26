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
#include <mc/im/radix_tree.h>
#include <mc/im/transaction.h>
#include <memory>
#include <string>
#include <algorithm>
#include <utility>
#include <vector>
#include <random>

namespace mc::im::tests {

// 简单值类型，用于测试
struct test_value {
    std::string text;
    int         number;

    test_value() : text(), number(0) {
    }
    test_value(const std::string& t, int n) : text(t), number(n) {
    }

    bool operator==(const test_value& other) const {
        return text == other.text && number == other.number;
    }
};

// 基础迭代器测试 - 验证基本遍历功能
TEST(RadixTreeIteratorTest, BasicIteration) {
    using tree_type = radix_tree<tree_config<test_value>>;
    
    // 创建事务并填充数据
    transaction<tree_config<test_value>> tx;

    // 插入数据
    for (int i = 0; i < 10; i++) {
        tx.insert("key" + std::to_string(i), test_value("值" + std::to_string(i), i));
    }

    // 提交事务
    auto tree = tx.commit();

    // 使用迭代器遍历树
    std::vector<std::string> keys;
    std::vector<int>         values;

    for (auto it = tree.begin(); it != tree.end(); ++it) {
        auto        key_buffer = it->first;
        std::string key(key_buffer.data(), key_buffer.size());
        keys.push_back(key);
        values.push_back(it->second.number);
    }

    // 验证结果
    EXPECT_EQ(keys.size(), 10);
    EXPECT_EQ(values.size(), 10);

    // 验证键是按顺序排列的（基数树迭代器按字典序遍历）
    std::vector<std::string> expected_keys;
    for (int i = 0; i < 10; i++) {
        expected_keys.push_back("key" + std::to_string(i));
    }
    std::sort(expected_keys.begin(), expected_keys.end());

    EXPECT_EQ(keys, expected_keys);

    // 测试范围for循环
    int count = 0;
    for (const auto& item : tree) {
        count++;
    }
    EXPECT_EQ(count, 10);
}

// 测试用例：可修改迭代器
TEST(RadixTreeIteratorTest, MutableIterator) {
    using tree_type = radix_tree<tree_config<test_value>>;
    
    // 创建事务并填充数据
    transaction<tree_config<test_value>> tx;
    
    // 插入测试数据
    for (int i = 0; i < 5; i++) {
        tx.insert("key" + std::to_string(i), test_value("原始值" + std::to_string(i), i));
    }
    
    // 提交事务
    auto tree = tx.commit();
    
    // 使用可修改迭代器修改值
    for (auto it = tree.begin(); it != tree.end(); ++it) {
        // 只修改值部分，不修改键
        it->second.text = "修改后的值";
        it->second.number *= 10;
    }
    
    // 验证修改是否生效
    for (auto it = tree.begin(); it != tree.end(); ++it) {
        EXPECT_EQ(it->second.text, "修改后的值");
        int original_number = std::stoi(std::string(it->first.data() + 3, it->first.size() - 3));
        EXPECT_EQ(it->second.number, original_number * 10);
    }
    
    // 使用 const_iterator 进行验证
    for (auto it = tree.cbegin(); it != tree.cend(); ++it) {
        EXPECT_EQ(it->second.text, "修改后的值");
    }
    
    // 通过引用修改方式
    auto  it         = tree.begin();
    auto& item       = *it;
    item.second.text = "通过引用修改";
    
    // 验证引用修改是否生效
    EXPECT_EQ(tree.begin()->second.text, "通过引用修改");
    
    // 非const树使用非const迭代器
    radix_tree<tree_config<test_value>> non_const_tree = tree;
    auto                                nc_it          = non_const_tree.begin();
    nc_it->second.text                                 = "非const迭代器";
    EXPECT_EQ(non_const_tree.begin()->second.text, "非const迭代器");
    
    // const树必须使用const迭代器
    const radix_tree<tree_config<test_value>> const_tree = tree;
    auto                                      const_iter = const_tree.begin();
    // 下面这行如果取消注释应该无法编译通过，因为const树只能使用const迭代器
    // const_iter->second.text = "这行应该编译失败";
}

// 测试用例：空树迭代器
TEST(RadixTreeIteratorTest, EmptyTreeIterator) {
    using tree_type = radix_tree<tree_config<test_value>>;
    
    // 创建空树
    tree_type tree;
    
    // 验证begin等于end
    EXPECT_EQ(tree.begin(), tree.end());
    EXPECT_EQ(tree.cbegin(), tree.cend());
    
    // 空树不应该有任何元素
    int count = 0;
    for (const auto& item : tree) {
        count++;
    }
    EXPECT_EQ(count, 0);
}

// 测试用例：迭代器比较和复制
TEST(RadixTreeIteratorTest, IteratorComparisonAndCopy) {
    using tree_type = radix_tree<tree_config<test_value>>;
    
    // 创建事务并填充数据
    transaction<tree_config<test_value>> tx;
    for (int i = 0; i < 5; i++) {
        tx.insert("key" + std::to_string(i), test_value("值" + std::to_string(i), i));
    }
    
    // 提交事务
    auto tree = tx.commit();
    
    // 比较迭代器
    auto it1 = tree.begin();
    auto it2 = tree.begin();
    auto it3 = tree.end();
    
    EXPECT_EQ(it1, it2);
    EXPECT_NE(it1, it3);
    
    // 复制迭代器
    auto it4 = it1;
    EXPECT_EQ(it1, it4);
    
    // 递增迭代器并比较
    ++it1;
    EXPECT_NE(it1, it2);
    
    // 后置递增
    auto it5 = it2++;
    EXPECT_EQ(it5, tree.begin());
    EXPECT_EQ(it2, it1);
}

// 测试用例：成员访问操作符
TEST(RadixTreeIteratorTest, MemberAccessOperator) {
    using tree_type = radix_tree<tree_config<test_value>>;
    
    // 创建事务并填充数据
    transaction<tree_config<test_value>> tx;
    tx.insert("test_key", test_value("测试文本", 100));
    
    // 提交事务
    auto tree = tx.commit();
    
    // 使用箭头操作符访问成员
    auto it = tree.begin();
    EXPECT_EQ(it->second.text, "测试文本");
    EXPECT_EQ(it->second.number, 100);
    
    // 修改成员
    it->second.text = "新文本";
    it->second.number = 200;
    
    // 验证修改
    EXPECT_EQ(tree.begin()->second.text, "新文本");
    EXPECT_EQ(tree.begin()->second.number, 200);
    
    // 获取引用并修改
    auto& value = it->second;
    value.text = "通过引用修改";
    value.number = 300;
    
    // 验证修改
    EXPECT_EQ(tree.begin()->second.text, "通过引用修改");
    EXPECT_EQ(tree.begin()->second.number, 300);
}

// 测试用例：随机访问测试
TEST(RadixTreeIteratorTest, RandomAccess) {
    using tree_type = radix_tree<tree_config<test_value>>;
    
    // 创建事务并填充数据
    transaction<tree_config<test_value>> tx;
    
    // 插入大量数据
    const int DATA_SIZE = 100;
    for (int i = 0; i < DATA_SIZE; i++) {
        tx.insert(
            "key" + std::to_string(i), 
            test_value("值" + std::to_string(i), i)
        );
    }
    
    // 提交事务
    auto tree = tx.commit();
    
    // 随机访问元素
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, DATA_SIZE - 1);
    
    // 收集所有键以便后续查找
    std::vector<std::string> all_keys;
    for (auto it = tree.begin(); it != tree.end(); ++it) {
        std::string key(it->first.data(), it->first.size());
        all_keys.push_back(key);
    }
    std::sort(all_keys.begin(), all_keys.end());
    
    // 随机查找并验证键存在
    for (int i = 0; i < 20; i++) {
        int idx = dis(gen);
        std::string key = all_keys[idx];
        
        // 使用迭代器查找该键
        bool found = false;
        for (auto it = tree.begin(); it != tree.end(); ++it) {
            std::string cur_key(it->first.data(), it->first.size());
            if (cur_key == key) {
                found = true;
                break;
            }
        }
        
        EXPECT_TRUE(found) << "未找到键: " << key;
    }
}

// 未来增加的测试可以放在这里
// TODO: 测试 upper_bound, lower_bound 等函数

} // namespace mc::im::tests 