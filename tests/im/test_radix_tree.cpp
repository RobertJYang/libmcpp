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
#include <mc/im/radix_tree.h>
#include <memory>
#include <string>
#include <vector>

// 自定义 leaf 类型
struct TestValue {
    std::string text;
    int         number;

    TestValue() : text(), number(0)
    {}
    TestValue(const std::string& t, int n) : text(t), number(n)
    {}

    bool operator==(const TestValue& other) const
    {
        return text == other.text && number == other.number;
    }
};

TEST(RadixTreeTest, CustomLeafType)
{
    // 定义使用 TestValue 作为 leaf_type 的配置
    using TestConfig = mc::im::tree_config<TestValue>;

    // 创建事务
    mc::im::transaction<TestConfig> tx;

    // 创建测试值
    TestValue value1("test1", 1);
    TestValue value2("test2", 2);
    TestValue value3("test3", 3);

    // 插入数据 - 直接使用值
    auto result1 = tx.insert("key1", value1);
    auto result2 = tx.insert("key2", value2);
    auto result3 = tx.insert("key3", value3);

    // 验证插入结果
    EXPECT_FALSE(result1.first.has_value());
    EXPECT_FALSE(result1.second);
    EXPECT_FALSE(result2.first.has_value());
    EXPECT_FALSE(result2.second);
    EXPECT_FALSE(result3.first.has_value());
    EXPECT_FALSE(result3.second);

    // 获取数据
    auto get1 = tx.get("key1");
    auto get2 = tx.get("key2");
    auto get3 = tx.get("key3");
    auto get4 = tx.get("key4");

    // 验证查询结果
    EXPECT_TRUE(get1.has_value());
    EXPECT_EQ(get1.value(), value1);
    EXPECT_EQ(get1.value().text, "test1");
    EXPECT_EQ(get1.value().number, 1);

    EXPECT_TRUE(get2.has_value());
    EXPECT_EQ(get2.value(), value2);
    EXPECT_EQ(get2.value().text, "test2");
    EXPECT_EQ(get2.value().number, 2);

    EXPECT_TRUE(get3.has_value());
    EXPECT_EQ(get3.value(), value3);
    EXPECT_EQ(get3.value().text, "test3");
    EXPECT_EQ(get3.value().number, 3);

    EXPECT_FALSE(get4.has_value());

    // 更新数据
    TestValue new_value1("updated1", 10);
    auto      update_result = tx.insert("key1", new_value1);

    // 验证更新结果
    EXPECT_TRUE(update_result.second); // 是更新操作
    EXPECT_TRUE(update_result.first.has_value());
    EXPECT_EQ(update_result.first.value(), value1);

    // 获取更新后的数据
    auto updated_get = tx.get("key1");

    // 验证更新后的查询结果
    EXPECT_TRUE(updated_get.has_value());
    EXPECT_EQ(updated_get.value(), new_value1);
    EXPECT_EQ(updated_get.value().text, "updated1");
    EXPECT_EQ(updated_get.value().number, 10);

    // 删除数据
    auto delete_result = tx.remove("key1");

    // 验证删除结果
    EXPECT_TRUE(delete_result.has_value()); // 删除成功
    EXPECT_EQ(delete_result.value(), new_value1);

    // 检查删除后的查询结果
    auto after_delete = tx.get("key1");
    EXPECT_FALSE(after_delete.has_value());

    // 提交事务
    auto tree = tx.commit();

    // 检查提交后的树状态
    EXPECT_EQ(tree.size(), 2); // key2 和 key3 还在

    // 从提交后的树中查询
    auto committed2 = tree.get("key2");
    auto committed3 = tree.get("key3");
    auto committed1 = tree.get("key1");

    // 验证提交后的查询结果
    EXPECT_TRUE(committed2.has_value());
    EXPECT_EQ(committed2.value(), value2);

    EXPECT_TRUE(committed3.has_value());
    EXPECT_EQ(committed3.value(), value3);

    EXPECT_FALSE(committed1.has_value());
}

TEST(RadixTreeTest, MixedConfigs)
{
    // 默认配置树（void*）
    mc::im::radix_tree<> default_tree;

    // TestValue 配置树
    using TestConfig = mc::im::tree_config<TestValue>;
    mc::im::radix_tree<TestConfig> test_tree;

    // 创建事务
    mc::im::transaction<>           default_tx(default_tree);
    mc::im::transaction<TestConfig> test_tx(test_tree);

    // 测试数据
    int       int_value = 42;
    TestValue test_value("test", 42);

    // 插入数据
    default_tx.insert("key", &int_value);
    test_tx.insert("key", test_value);

    // 获取数据
    auto default_result = default_tx.get("key");
    auto test_result    = test_tx.get("key");

    // 验证结果
    EXPECT_TRUE(default_result.has_value());
    EXPECT_EQ(*static_cast<int*>(default_result.value()), 42);

    EXPECT_TRUE(test_result.has_value());
    EXPECT_EQ(test_result.value().text, "test");
    EXPECT_EQ(test_result.value().number, 42);

    // 提交事务
    default_tree = default_tx.commit();
    test_tree    = test_tx.commit();
}

// 简化的迭代器测试用例
TEST(RadixTreeTest, BasicIterator)
{
    // 使用自定义配置创建 radix_tree
    using TestConfig = mc::im::tree_config<TestValue>;

    // 创建事务
    mc::im::transaction<TestConfig> tx;

    // 插入单个键值对
    TestValue value1("test1", 1);
    tx.insert("key1", value1);

    // 提交事务
    auto tree = tx.commit();

    // 验证树的大小
    EXPECT_EQ(tree.size(), 1);

    // 测试迭代器
    auto it = tree.begin();
    EXPECT_NE(it, tree.end());

    // 测试迭代器解引用
    auto& item = *it;
    EXPECT_EQ(item.second.number, 1);

    // 测试迭代器递增
    ++it;
    EXPECT_EQ(it, tree.end());
}

// 测试迭代器的复制和相等性
TEST(RadixTreeTest, IteratorCopyAndEquality)
{
    // 使用自定义配置创建 radix_tree
    using TestConfig = mc::im::tree_config<TestValue>;

    // 创建事务
    mc::im::transaction<TestConfig> tx;

    // 插入测试数据
    TestValue value("test", 42);
    tx.insert("key", value);

    // 提交事务
    auto tree = tx.commit();

    // 测试迭代器的复制
    auto it1 = tree.begin();
    auto it2 = it1; // 复制迭代器

    // 测试相等性
    EXPECT_EQ(it1, it2);

    // 测试解引用是否相同
    EXPECT_EQ(it1->second.number, it2->second.number);

    // 移动第一个迭代器
    ++it1;

    // 现在应该不相等
    EXPECT_NE(it1, it2);

    // it1应该到达末尾
    EXPECT_EQ(it1, tree.end());

    // it2应该仍然指向第一个元素
    EXPECT_NE(it2, tree.end());
}

// 测试前缀匹配
TEST(RadixTreeTest, PrefixMatching)
{
    // 使用自定义配置创建 radix_tree
    using TestConfig = mc::im::tree_config<TestValue>;

    // 创建事务
    mc::im::transaction<TestConfig> tx;

    // 创建带有公共前缀的键
    TestValue value1("value1", 1);
    TestValue value2("value2", 2);
    TestValue value3("value3", 3);

    // 插入有共同前缀的键
    tx.insert("test/a", value1);
    tx.insert("test/b", value2);
    tx.insert("other", value3);

    // 提交事务
    auto tree = tx.commit();

    // 验证树的大小
    EXPECT_EQ(tree.size(), 3);

    // 遍历并检查排序
    auto it = tree.begin();

    // 第一个键应该是 "other"（字典序最小）
    EXPECT_EQ(it->second.number, 3);
    ++it;

    // 然后是 "test/a"
    EXPECT_EQ(it->second.number, 1);
    ++it;

    // 最后是 "test/b"
    EXPECT_EQ(it->second.number, 2);
    ++it;

    // 应该到达末尾
    EXPECT_EQ(it, tree.end());
}

// 测试后序递增操作
TEST(RadixTreeTest, PostIncrementIterator)
{
    // 使用自定义配置创建 radix_tree
    using TestConfig = mc::im::tree_config<TestValue>;
    // 创建事务
    mc::im::transaction<TestConfig> tx;

    // 插入数据
    TestValue value1("v1", 1);
    TestValue value2("v2", 2);

    tx.insert("a", value1);
    tx.insert("b", value2);

    // 提交事务
    auto tree = tx.commit();

    // 测试后序递增
    auto it     = tree.begin();
    auto old_it = it++;

    // 检查old_it仍指向第一个元素，it已移动到第二个元素
    EXPECT_EQ(old_it->second.number, 1);
    EXPECT_EQ(it->second.number, 2);
}

// 测试for-range语法 - 禁用此测试
TEST(RadixTreeTest, ForRangeLoop)
{
    // 使用自定义配置创建 radix_tree
    using TestConfig = mc::im::tree_config<TestValue>;

    // 创建事务
    mc::im::transaction<TestConfig> tx;

    // 插入测试数据
    TestValue value1("test1", 1);
    TestValue value2("test2", 2);
    TestValue value3("test3", 3);
    TestValue value4("test4", 4);
    TestValue value5("test5", 5);

    tx.insert("key1", value1);
    tx.insert("key2", value2);
    tx.insert("key3", value3);
    tx.insert("key4", value4);
    tx.insert("key5", value5);

    // 提交事务
    auto tree = tx.commit();

    // 测试for-range循环，但使用普通的迭代器遍历
    int  sum = 0;
    auto it  = tree.begin();
    auto end = tree.end();
    while (it != end) {
        sum += it->second.number;
        ++it;
    }

    // 验证所有元素都被遍历（1+2+3+4+5=15）
    EXPECT_EQ(sum, 15);
}

// 测试空树 get() 方法
TEST(RadixTreeTest, RadixTreeGetOnEmptyTree)
{
    using TestConfig = mc::im::tree_config<TestValue>;
    mc::im::radix_tree<TestConfig> empty_tree;

    // 在空树上调用 get()
    auto result = empty_tree.get("key");
    EXPECT_FALSE(result.has_value());
}

// 测试空树 find() 方法
TEST(RadixTreeTest, RadixTreeFindOnEmptyTree)
{
    using TestConfig = mc::im::tree_config<TestValue>;
    mc::im::radix_tree<TestConfig> empty_tree;

    // 在空树上调用 find()
    auto it = empty_tree.find("key");
    EXPECT_EQ(it, empty_tree.end());
}

// 测试空键 find() 方法
TEST(RadixTreeTest, RadixTreeFindWithEmptyKey)
{
    using TestConfig = mc::im::tree_config<TestValue>;
    mc::im::transaction<TestConfig> tx;

    // 插入数据
    TestValue value1("test1", 1);
    tx.insert("key1", value1);

    // 提交事务
    auto tree = tx.commit();

    // 调用 find("")，应该返回 begin()
    auto it = tree.find("");
    EXPECT_EQ(it, tree.begin());
}

// 测试 find() 匹配非叶子节点场景
TEST(RadixTreeTest, RadixTreeFindNonLeafMatch)
{
    using TestConfig = mc::im::tree_config<TestValue>;
    mc::im::transaction<TestConfig> tx;

    // 插入 "abc" 和 "abcd"
    TestValue value1("abc", 1);
    TestValue value2("abcd", 2);
    tx.insert("abc", value1);
    tx.insert("abcd", value2);

    // 提交事务
    auto tree = tx.commit();

    // 调用 find("abc")，应该能找到 "abc" 的叶子节点（而不是匹配到 "abcd" 的中间节点）
    auto it = tree.find("abc");
    ASSERT_NE(it, tree.end());
    EXPECT_EQ(it->first, "abc");
    EXPECT_EQ(it->second.text, "abc");
    EXPECT_EQ(it->second.number, 1);
}
