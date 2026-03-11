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
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace mc::im::tests {

// 追踪内存分配和释放的计数
static int allocation_count   = 0;
static int deallocation_count = 0;

/**
 * 自定义内存分配器，用于追踪内存分配和释放
 * 可以跟踪每个类型的内存分配情况
 */
template <typename T>
class tracking_allocator {
public:
    // 标准分配器类型定义
    using value_type      = T;
    using pointer         = T*;
    using const_pointer   = const T*;
    using reference       = T&;
    using const_reference = const T&;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;

    // 默认构造函数
    tracking_allocator() noexcept
    {
    }

    // 拷贝构造函数
    template <typename U>
    tracking_allocator(const tracking_allocator<U>&) noexcept
    {
    }

    // 析构函数
    ~tracking_allocator() = default;

    // 重绑定到其他类型的分配器
    template <typename U>
    struct rebind {
        using other = tracking_allocator<U>;
    };

    // 分配内存
    pointer allocate(size_type n)
    {
        allocation_count++;
        return static_cast<pointer>(::operator new(n * sizeof(T)));
    }

    // 释放内存
    void deallocate(pointer p, size_type n) noexcept
    {
        deallocation_count++;
        ::operator delete(p);
    }

    // 构造对象
    template <typename U, typename... Args>
    void construct(U* p, Args&&... args)
    {
        ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
    }

    // 析构对象
    template <typename U>
    void destroy(U* p)
    {
        p->~U();
    }

    // 添加比较运算符以支持标准库容器
    bool operator==(const tracking_allocator&) const noexcept
    {
        return true;
    }

    bool operator!=(const tracking_allocator&) const noexcept
    {
        return false;
    }

    // 重置计数器
    static void reset_counters()
    {
        allocation_count   = 0;
        deallocation_count = 0;
    }

    // 获取分配计数
    static int get_allocation_count()
    {
        return allocation_count;
    }

    // 获取释放计数
    static int get_deallocation_count()
    {
        return deallocation_count;
    }
};

// 自定义测试值类型
struct test_value {
    std::string text;
    int         number;

    test_value()
        : text(), number(0)
    {
    }
    test_value(const std::string& t, int n)
        : text(t), number(n)
    {
    }

    bool operator==(const test_value& other) const
    {
        return text == other.text && number == other.number;
    }
};

// 使用跟踪分配器定义树配置
template <typename T>
using tracked_tree_config = tree_config<T, tracking_allocator<char>>;

// 测试用例：基本的跟踪分配器测试
TEST(CustomAllocatorTest, BasicTrackingAllocator)
{
    // 重置计数器
    tracking_allocator<int>::reset_counters();

    {
        // 创建使用自定义分配器的基数树
        using test_config = tracked_tree_config<test_value>;

        // 创建一个空树
        radix_tree<test_config> tree;

        // 验证初始分配
        EXPECT_GT(tracking_allocator<int>::get_allocation_count(), 0);
        EXPECT_EQ(tracking_allocator<int>::get_deallocation_count(), 0);

        // 创建事务
        transaction<test_config> tx(tree);

        // 添加一些数据
        test_value value1("测试1", 1);
        test_value value2("测试2", 2);

        tx.insert("key1", value1);
        tx.insert("key2", value2);

        // 提交事务，创建新树
        tree = tx.commit();

        // 验证树大小
        EXPECT_EQ(tree.size(), 2);

        // 查询数据
        auto result1 = tree.get("key1");
        auto result2 = tree.get("key2");

        ASSERT_TRUE(result1.has_value());
        ASSERT_TRUE(result2.has_value());

        EXPECT_EQ(result1.value(), value1);
        EXPECT_EQ(result2.value(), value2);
    }

    // 验证所有内存都已释放
    EXPECT_EQ(tracking_allocator<int>::get_allocation_count(),
              tracking_allocator<int>::get_deallocation_count());
}

// 测试用例：事务操作和分配跟踪
TEST(CustomAllocatorTest, TransactionAllocation)
{
    // 重置计数器
    tracking_allocator<int>::reset_counters();

    using test_config = tracked_tree_config<test_value>;

    // 创建一个事务
    transaction<test_config> tx;

    // 记录初始分配计数
    int initial_alloc_count = tracking_allocator<int>::get_allocation_count();

    // 执行一系列插入操作
    for (int i = 0; i < 100; i++) {
        tx.insert("key" + std::to_string(i), test_value("值" + std::to_string(i), i));
    }

    // 验证插入操作导致的内存分配
    EXPECT_GT(tracking_allocator<int>::get_allocation_count(), initial_alloc_count);

    // 提交事务
    auto tree = tx.commit();

    // 验证树大小
    EXPECT_EQ(tree.size(), 100);

    // 创建一个新事务
    transaction<test_config> tx2(tree);

    // 记录释放内存计数
    int pre_dealloc_count = tracking_allocator<int>::get_deallocation_count();

    // 执行删除操作
    for (int i = 0; i < 50; i++) {
        tx2.remove("key" + std::to_string(i));
    }

    // 验证删除操作导致的内存释放和对象销毁
    EXPECT_GT(tracking_allocator<int>::get_deallocation_count(), pre_dealloc_count);

    // 提交删除操作
    tree = tx2.commit();

    // 验证树大小
    EXPECT_EQ(tree.size(), 50);
}

// 测试用例：保存点与内存分配
TEST(CustomAllocatorTest, SavePointAllocation)
{
    // 重置计数器
    tracking_allocator<int>::reset_counters();

    using test_config = tracked_tree_config<test_value>;

    // 创建事务
    transaction<test_config> tx;

    // 插入一些数据
    tx.insert("key1", test_value("值1", 1));
    tx.insert("key2", test_value("值2", 2));

    // 记录初始分配计数
    int initial_alloc_count = tracking_allocator<int>::get_allocation_count();

    // 创建保存点
    auto save_point = tx.save_point();

    // 继续插入数据
    tx.insert("key3", test_value("值3", 3));
    tx.insert("key4", test_value("值4", 4));

    // 验证插入导致的额外分配
    EXPECT_GT(tracking_allocator<int>::get_allocation_count(), initial_alloc_count);

    // 记录回滚前的分配计数
    int pre_rollback_alloc_count = tracking_allocator<int>::get_allocation_count();

    // 回滚到保存点
    tx.rollback(save_point);

    // 验证树的大小
    auto tree = tx.commit();
    EXPECT_EQ(tree.size(), 2);

    // 验证回滚后内存分配情况
    EXPECT_GE(tracking_allocator<int>::get_deallocation_count(),
              pre_rollback_alloc_count - initial_alloc_count);
}

// 测试用例：大量数据与内存分配性能
TEST(CustomAllocatorTest, LargeDataAllocation)
{
    // 重置计数器
    tracking_allocator<int>::reset_counters();

    using test_config = tracked_tree_config<test_value>;

    // 创建事务
    transaction<test_config> tx;

    // 插入大量数据
    const int DATA_SIZE = 1000;

    for (int i = 0; i < DATA_SIZE; i++) {
        tx.insert("key" + std::to_string(i), test_value("值" + std::to_string(i), i));
    }

    // 提交事务
    auto tree = tx.commit();

    // 验证树大小
    EXPECT_EQ(tree.size(), DATA_SIZE);

    // 创建新事务
    transaction<test_config> tx2(tree);

    // 随机查询一些键
    for (int i = 0; i < 100; i++) {
        int  key_index = rand() % DATA_SIZE;
        auto result    = tx2.get("key" + std::to_string(key_index));
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value().number, key_index);
    }

    // 记录删除前的内存释放计数
    int pre_dealloc_count = tracking_allocator<int>::get_deallocation_count();

    // 随机删除一半的键
    for (int i = 0; i < DATA_SIZE / 2; i++) {
        tx2.remove("key" + std::to_string(i));
    }

    // 验证删除操作导致的内存释放和对象销毁
    EXPECT_GT(tracking_allocator<int>::get_deallocation_count(), pre_dealloc_count);

    // 提交事务
    tree = tx2.commit();

    // 验证树大小
    EXPECT_EQ(tree.size(), DATA_SIZE / 2);
}

} // namespace mc::im::tests