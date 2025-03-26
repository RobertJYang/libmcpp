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

#include <algorithm>
#include <gtest/gtest.h>
#include <iostream>
#include <mc/im/radix_tree.h>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

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
        keys.emplace_back(it->first);
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
    it->second.text   = "新文本";
    it->second.number = 200;

    // 验证修改
    EXPECT_EQ(tree.begin()->second.text, "新文本");
    EXPECT_EQ(tree.begin()->second.number, 200);

    // 获取引用并修改
    auto& value  = it->second;
    value.text   = "通过引用修改";
    value.number = 300;

    // 验证修改
    EXPECT_EQ(tree.begin()->second.text, "通过引用修改");
    EXPECT_EQ(tree.begin()->second.number, 300);
}

// 测试用例：随机访问测试
TEST(RadixTreeIteratorTest, RandomAccess) {
    // 创建事务并填充数据
    transaction<tree_config<test_value>> tx;

    // 插入大量数据
    const int DATA_SIZE = 100;
    for (int i = 0; i < DATA_SIZE; i++) {
        tx.insert("key" + std::to_string(i), test_value("值" + std::to_string(i), i));
    }

    // 提交事务
    auto tree = tx.commit();

    // 随机访问元素
    std::random_device              rd;
    std::mt19937                    gen(rd());
    std::uniform_int_distribution<> dis(0, DATA_SIZE - 1);

    // 收集所有键以便后续查找
    std::vector<std::string> all_keys;
    for (auto it = tree.begin(); it != tree.end(); ++it) {
        all_keys.emplace_back(it->first);
    }
    std::sort(all_keys.begin(), all_keys.end());

    // 随机查找并验证键存在
    for (int i = 0; i < 20; i++) {
        int         idx = dis(gen);
        std::string key = all_keys[idx];

        // 使用迭代器查找该键
        bool found = false;
        for (auto it = tree.begin(); it != tree.end(); ++it) {
            if (it->first == key) {
                found = true;
                break;
            }
        }

        EXPECT_TRUE(found) << "未找到键: " << key;
    }
}

// 测试用例：查找具有特定键的元素
TEST(RadixTreeIteratorTest, Find) {
    using tree_type = radix_tree<tree_config<test_value>>;

    // 创建事务并填充数据
    transaction<tree_config<test_value>> tx;

    // 插入测试数据
    for (int i = 0; i < 5; i++) {
        tx.insert("key" + std::to_string(i), test_value("值" + std::to_string(i), i));
    }

    // 提交事务
    auto tree = tx.commit();

    // 测试find函数
    auto it1 = tree.find("key0");
    ASSERT_NE(it1, tree.end());
    EXPECT_EQ(it1->second.number, 0);

    auto it2 = tree.find("key3");
    ASSERT_NE(it2, tree.end());
    EXPECT_EQ(it2->second.number, 3);

    // 测试查找不存在的键
    auto it3 = tree.find("nonexistent");
    EXPECT_EQ(it3, tree.end());

    // 使用const版本的find
    const tree_type& const_tree = tree;
    auto             const_it   = const_tree.find("key2");
    ASSERT_NE(const_it, const_tree.end());
    EXPECT_EQ(const_it->second.number, 2);
}

// 测试用例：lower_bound和upper_bound函数
TEST(RadixTreeIteratorTest, BoundaryFunctions) {
    // 创建事务并填充数据
    transaction<tree_config<test_value>> tx;

    // 插入排序后的测试数据
    tx.insert("a_key", test_value("A值", 1));
    tx.insert("b_key", test_value("B值", 2));
    tx.insert("c_key", test_value("C值", 3));
    tx.insert("d_key", test_value("D值", 4));
    tx.insert("e_key", test_value("E值", 5));

    // 提交事务
    auto tree = tx.commit();

    // 测试lower_bound
    auto lb1 = tree.lower_bound("b_key");
    ASSERT_NE(lb1, tree.end());
    EXPECT_EQ(lb1->second.number, 2);

    auto lb2 = tree.lower_bound("c");
    ASSERT_NE(lb2, tree.end());
    EXPECT_EQ(lb2->second.number, 3);

    ++lb1;
    EXPECT_EQ(lb1->second.number, 3);
    EXPECT_EQ(lb1, lb2);

    auto lb11 = tree.find("b_key");
    ASSERT_NE(lb11, tree.end());
    EXPECT_EQ(lb11->second.number, 2);
    ++lb11;
    EXPECT_EQ(lb11->second.number, 3);
    EXPECT_EQ(lb11, lb2);
    EXPECT_EQ(lb11->first, lb1->first);
    EXPECT_EQ(lb11->first, lb2->first);

    // 测试upper_bound
    auto ub1 = tree.upper_bound("b_key");
    ASSERT_NE(ub1, tree.end());
    EXPECT_EQ(ub1->second.number, 3); // 应该返回c_key

    auto ub2 = tree.upper_bound("d");
    ASSERT_NE(ub2, tree.end());
    EXPECT_EQ(ub2->second.number, 4); // 应该返回d_key

    // 测试超出范围的情况
    auto lb3 = tree.lower_bound("f_key");
    EXPECT_EQ(lb3, tree.end());

    auto ub3 = tree.upper_bound("e_key");
    EXPECT_EQ(ub3, tree.end());

    // 使用const版本
    const auto& const_tree = tree;
    auto const_lb = const_tree.lower_bound("b_key");
    ASSERT_NE(const_lb, const_tree.end());
    EXPECT_EQ(const_lb->second.number, 2);

    auto const_ub = const_tree.upper_bound("b_key");
    ASSERT_NE(const_ub, const_tree.end());
    EXPECT_EQ(const_ub->second.number, 3);
}

// 测试用例：equal_range函数
TEST(RadixTreeIteratorTest, EqualRange) {
    using tree_type = radix_tree<tree_config<test_value>>;

    // 创建事务并填充数据
    transaction<tree_config<test_value>> tx;

    // 插入排序后的测试数据
    tx.insert("a_key", test_value("A值", 1));
    tx.insert("b_key", test_value("B值", 2));
    tx.insert("c_key", test_value("C值", 3));
    tx.insert("d_key", test_value("D值", 4));
    tx.insert("e_key", test_value("E值", 5));

    // 提交事务
    auto tree = tx.commit();

    // 测试equal_range
    auto range1 = tree.equal_range("c_key");
    ASSERT_NE(range1.first, tree.end());
    EXPECT_EQ(range1.first->second.number, 3);  // lower_bound

    if (range1.second != tree.end()) {
        EXPECT_EQ(range1.second->second.number, 4);  // upper_bound (d_key)
    } else {
        FAIL() << "range1.second 不应该是 end()";
    }

    // 测试不存在的键
    auto range2 = tree.equal_range("between_c_d");
    if (range2.first != tree.end()) {
        // 应该指向第一个不小于给定键的元素
        EXPECT_GE(range2.first->first, "between_c_d");
    }

    // 测试超出范围的情况
    auto range3 = tree.equal_range("z_key");
    EXPECT_EQ(range3.first, tree.end());
    EXPECT_EQ(range3.second, tree.end());

    // 使用const版本
    const tree_type& const_tree = tree;
    auto const_range = const_tree.equal_range("b_key");
    ASSERT_NE(const_range.first, const_tree.end());
    EXPECT_EQ(const_range.first->second.number, 2);  // lower_bound

    if (const_range.second != const_tree.end()) {
        EXPECT_EQ(const_range.second->second.number, 3);  // upper_bound (c_key)
    } else {
        FAIL() << "const_range.second 不应该是 end()";
    }
}

// 测试用例：复杂的lower_bound场景
TEST(RadixTreeIteratorTest, ComplexLowerBound) {
    // 创建事务并填充数据
    transaction<tree_config<test_value>> tx;

    // 插入具有不同前缀长度的数据
    // 1. 短前缀
    tx.insert("a", test_value("短前缀1", 1));
    tx.insert("ab", test_value("短前缀2", 2));

    // 2. 中等长度前缀
    tx.insert("abc", test_value("中等前缀1", 3));
    tx.insert("abcd", test_value("中等前缀2", 4));
    tx.insert("abce", test_value("中等前缀3", 5));

    // 3. 长前缀
    tx.insert("abcdef", test_value("长前缀1", 6));
    tx.insert("abcdefg", test_value("长前缀2", 7));
    tx.insert("abcdefgh", test_value("长前缀3", 8));

    // 4. 特殊前缀
    tx.insert("abcde", test_value("特殊前缀1", 9));
    tx.insert("abcde1", test_value("特殊前缀2", 10));
    tx.insert("abcde2", test_value("特殊前缀3", 11));

    // 提交事务
    auto tree = tx.commit();

    // 测试场景1：查找不存在的短前缀
    {
        auto it = tree.lower_bound("aa");
        ASSERT_NE(it, tree.end());
        EXPECT_EQ(it->first, "ab");
        EXPECT_EQ(it->second.number, 2);
    }

    // 测试场景2：查找存在的短前缀
    {
        auto it = tree.lower_bound("ab");
        ASSERT_NE(it, tree.end());
        EXPECT_EQ(it->first, "ab");
        EXPECT_EQ(it->second.number, 2);
    }

    // 测试场景3：查找中等长度前缀
    {
        auto it = tree.lower_bound("abc");
        ASSERT_NE(it, tree.end());
        EXPECT_EQ(it->first, "abc");
        EXPECT_EQ(it->second.number, 3);
    }

    // 测试场景4：查找不存在的长前缀
    {
        auto it = tree.lower_bound("abcdefi");
        ASSERT_NE(it, tree.end());
        EXPECT_EQ(it->first, "abce");
        EXPECT_EQ(it->second.number, 5);
    }

    // 测试场景5：查找特殊前缀
    {
        auto it = tree.lower_bound("abcde");
        ASSERT_NE(it, tree.end());
        EXPECT_EQ(it->first, "abcde");
        EXPECT_EQ(it->second.number, 9);
    }

    // 测试场景6：查找空字符串
    {
        auto it = tree.lower_bound("");
        ASSERT_NE(it, tree.end());
        EXPECT_EQ(it->first, "a");
        EXPECT_EQ(it->second.number, 1);
    }

    // 测试场景7：查找大于所有键的字符串
    {
        auto it = tree.lower_bound("z");
        EXPECT_EQ(it, tree.end());
    }

    // 测试场景8：验证迭代器序列
    {
        auto it = tree.lower_bound("abcde");
        ASSERT_NE(it, tree.end());

        // 验证后续元素
        std::vector<std::string> expected_keys   = {"abcde",  "abcde1",  "abcde2",
                                                    "abcdef", "abcdefg", "abcdefgh"};
        std::vector<int>         expected_values = {9, 10, 11, 6, 7, 8};

        for (size_t i = 0; i < expected_keys.size(); ++i) {
            ASSERT_NE(it, tree.end());
            EXPECT_EQ(it->first, expected_keys[i]);
            EXPECT_EQ(it->second.number, expected_values[i]);
            ++it;
        }
    }

    // 测试场景9：查找部分匹配的前缀
    {
        auto it = tree.lower_bound("abcd");
        ASSERT_NE(it, tree.end());
        EXPECT_EQ(it->first, "abcd");
        EXPECT_EQ(it->second.number, 4);
    }

    // 测试场景10：查找完全匹配的长前缀
    {
        auto it = tree.lower_bound("abcdefgh");
        ASSERT_NE(it, tree.end());
        EXPECT_EQ(it->first, "abcdefgh");
        EXPECT_EQ(it->second.number, 8);
    }

    // 测试场景11：验证find和lower_bound的迭代器行为一致性
    {
        // 使用存在的键，find和lower_bound应返回相同迭代器
        auto find_it = tree.find("abcde");
        auto lb_it = tree.lower_bound("abcde");
        ASSERT_NE(find_it, tree.end());
        ASSERT_NE(lb_it, tree.end());
        EXPECT_EQ(find_it, lb_it);
        
        // 两个迭代器同时递增，应该保持相同位置
        while(find_it != tree.end() && lb_it != tree.end()) {
            EXPECT_EQ(find_it->first, lb_it->first);
            EXPECT_EQ(find_it->second.number, lb_it->second.number);
            ++find_it;
            ++lb_it;
        }
        
        // 两个迭代器应该同时到达末尾
        EXPECT_EQ(find_it, tree.end());
        EXPECT_EQ(lb_it, tree.end());
    }
}

// 测试用例：复杂的upper_bound场景
TEST(RadixTreeIteratorTest, ComplexUpperBound) {
    // 创建事务并填充数据
    transaction<tree_config<test_value>> tx;

    // 插入具有不同前缀长度的数据
    // 1. 短前缀
    tx.insert("a", test_value("短前缀1", 1));
    tx.insert("ab", test_value("短前缀2", 2));

    // 2. 中等长度前缀
    tx.insert("abc", test_value("中等前缀1", 3));
    tx.insert("abcd", test_value("中等前缀2", 4));
    tx.insert("abce", test_value("中等前缀3", 5));

    // 3. 长前缀
    tx.insert("abcdef", test_value("长前缀1", 6));
    tx.insert("abcdefg", test_value("长前缀2", 7));
    tx.insert("abcdefgh", test_value("长前缀3", 8));

    // 4. 特殊前缀
    tx.insert("abcde", test_value("特殊前缀1", 9));
    tx.insert("abcde1", test_value("特殊前缀2", 10));
    tx.insert("abcde2", test_value("特殊前缀3", 11));

    // 提交事务
    auto tree = tx.commit();

    // 测试场景1：查找严格大于短前缀的元素
    {
        auto it = tree.upper_bound("a");
        ASSERT_NE(it, tree.end());
        EXPECT_EQ(it->first, "ab");
        EXPECT_EQ(it->second.number, 2);
    }

    // 测试场景2：查找严格大于存在的短前缀
    {
        auto it = tree.upper_bound("ab");
        ASSERT_NE(it, tree.end());
        EXPECT_EQ(it->first, "abc");
        EXPECT_EQ(it->second.number, 3);
    }

    // 测试场景3：查找严格大于中等长度前缀
    {
        auto it = tree.upper_bound("abc");
        ASSERT_NE(it, tree.end());
        EXPECT_EQ(it->first, "abcd");
        EXPECT_EQ(it->second.number, 4);
    }

    // 测试场景4：查找严格大于不存在的前缀
    {
        auto it = tree.upper_bound("abcc");
        ASSERT_NE(it, tree.end());
        EXPECT_EQ(it->first, "abcd");
        EXPECT_EQ(it->second.number, 4);
    }

    // 测试场景5：查找严格大于特殊前缀
    {
        auto it = tree.upper_bound("abcde");
        ASSERT_NE(it, tree.end());
        EXPECT_EQ(it->first, "abcde1");
        EXPECT_EQ(it->second.number, 10);
    }

    // 测试场景6：查找严格大于特殊前缀的最后一个元素
    {
        auto it = tree.upper_bound("abcde2");
        ASSERT_NE(it, tree.end());
        EXPECT_EQ(it->first, "abcdef");
        EXPECT_EQ(it->second.number, 6);
    }

    // 测试场景7：查找空字符串
    {
        auto it = tree.upper_bound("");
        ASSERT_NE(it, tree.end());
        EXPECT_EQ(it->first, "a");
        EXPECT_EQ(it->second.number, 1);
    }

    // 测试场景8：查找大于所有键的字符串
    {
        auto it = tree.upper_bound("z");
        EXPECT_EQ(it, tree.end());
    }

    // 测试场景9：验证迭代器序列
    {
        auto it = tree.upper_bound("abcde");
        ASSERT_NE(it, tree.end());

        // 验证后续元素
        std::vector<std::string> expected_keys   = {"abcde1", "abcde2", "abcdef", "abcdefg", "abcdefgh"};
        std::vector<int>         expected_values = {10, 11, 6, 7, 8};

        for (size_t i = 0; i < expected_keys.size(); ++i) {
            ASSERT_NE(it, tree.end());
            EXPECT_EQ(it->first, expected_keys[i]);
            EXPECT_EQ(it->second.number, expected_values[i]);
            ++it;
        }
    }

    // 测试场景10：查找严格大于某个元素但不是最后一个
    {
        auto it = tree.upper_bound("abcdefgh");
        ASSERT_NE(it, tree.end());
        EXPECT_EQ(it->first, "abce");
        EXPECT_EQ(it->second.number, 5);
    }

    // 测试场景11：查找严格大于长前缀但小于下一个元素
    {
        auto it = tree.upper_bound("abcde0");
        ASSERT_NE(it, tree.end());
        EXPECT_EQ(it->first, "abcde1");
        EXPECT_EQ(it->second.number, 10);
    }

    // 测试场景12：查找严格大于真正最后一个元素
    {
        auto it = tree.upper_bound("abce");
        EXPECT_EQ(it, tree.end());
    }

    // 测试场景13：验证迭代器一致性和链式操作
    {
        // 对于存在的键，find和lower_bound应返回相同迭代器
        auto find_it = tree.find("abcde1");
        auto lb_it = tree.lower_bound("abcde1");
        ASSERT_NE(find_it, tree.end());
        ASSERT_NE(lb_it, tree.end());
        EXPECT_EQ(find_it, lb_it);
        
        // upper_bound和lower_bound/find的链式关系
        auto ub_it = tree.upper_bound("abcde1");
        auto next_find_it = find_it;
        ++next_find_it;
        EXPECT_EQ(ub_it, next_find_it); // upper_bound等同于lower_bound后递增
        
        // 从某个位置开始的迭代器链应该相同
        find_it = tree.find("abcdef");
        lb_it = tree.lower_bound("abcdef");
        ub_it = tree.upper_bound("abcde2"); // 应指向"abcdef"
        
        EXPECT_EQ(find_it, lb_it);
        EXPECT_EQ(find_it, ub_it);
        
        // 三个迭代器同时递增到end()
        while(find_it != tree.end()) {
            ASSERT_NE(lb_it, tree.end());
            ASSERT_NE(ub_it, tree.end());
            EXPECT_EQ(find_it->first, lb_it->first);
            EXPECT_EQ(find_it->first, ub_it->first);
            ++find_it;
            ++lb_it;
            ++ub_it;
        }
        
        // 三个迭代器应同时到达end()
        EXPECT_EQ(find_it, tree.end());
        EXPECT_EQ(lb_it, tree.end());
        EXPECT_EQ(ub_it, tree.end());
    }
}

// 未来增加的测试可以放在这里

} // namespace mc::im::tests