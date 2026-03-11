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

#include <gtest/gtest.h>
#include <mc/runtime/thread_list.h>
#include <mc/sync/mutex_box.h>

#include "test_future_helpers.h"

#include <atomic>
#include <chrono>

using namespace std::chrono_literals;

namespace {

class ThreadListTest : public ::testing::Test {
protected:
    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

} // namespace

// 测试 thread_node 的基本构造和析构
TEST_F(ThreadListTest, ThreadNodeBasic)
{
    std::atomic<bool> task_executed{false};

    {
        mc::runtime::thread_node node([&task_executed]() {
            task_executed.store(true);
        });

        // 等待线程完成
        node.join();

        EXPECT_TRUE(task_executed.load());
        EXPECT_FALSE(node.joinable());
    }
}

// 测试 thread_node 的移动构造
TEST_F(ThreadListTest, ThreadNodeMove)
{
    std::atomic<bool> task_executed{false};

    mc::runtime::thread_node node1([&task_executed]() {
        task_executed.store(true);
    });

    auto thread_id = node1.get_id();

    // 移动构造
    mc::runtime::thread_node node2(std::move(node1));

    // 移动后应该保持相同的线程ID
    EXPECT_EQ(node2.get_id(), thread_id);

    node2.join();
    EXPECT_TRUE(task_executed.load());
}

// 测试 thread_node 的 joinable
TEST_F(ThreadListTest, ThreadNodeJoinable)
{
    auto stop_signal = std::make_shared<std::promise<void>>();
    auto stop_future = stop_signal->get_future().share();

    mc::runtime::thread_node node([stop_future]() mutable {
        stop_future.wait();
    });

    EXPECT_TRUE(node.joinable());

    stop_signal->set_value();
    node.join();
    EXPECT_FALSE(node.joinable());
}

// 测试 thread_node 的 get_id
TEST_F(ThreadListTest, ThreadNodeGetId)
{
    std::atomic<bool> task_done{false};

    mc::runtime::thread_node node([&task_done]() {
        task_done.store(true);
    });

    auto thread_id = node.get_id();
    EXPECT_NE(thread_id, std::thread::id());

    node.join();
    EXPECT_TRUE(task_done.load());
}

// 测试 thread_node 的 join 在不可join时
TEST_F(ThreadListTest, ThreadNodeJoinWhenNotJoinable)
{
    std::atomic<bool> task_done{false};

    mc::runtime::thread_node node([&task_done]() {
        task_done.store(true);
    });

    node.join();
    EXPECT_FALSE(node.joinable());

    // 再次join应该不抛出异常
    EXPECT_NO_THROW(node.join());
}

// 测试 thread_list 的基本构造和析构
TEST_F(ThreadListTest, ThreadListBasic)
{
    std::atomic<int> task_count{0};

    {
        mc::runtime::thread_list list;

        EXPECT_TRUE(list.empty());
        EXPECT_EQ(list.get_thread_count(), 0);

        // 添加一个线程
        list.add_thread([&task_count]() {
            task_count.fetch_add(1);
        });

        EXPECT_FALSE(list.empty());
        EXPECT_EQ(list.get_thread_count(), 1);
    } // list 析构时会等待所有线程完成

    EXPECT_EQ(task_count.load(), 1);
}

// 测试 thread_list 的 start_threads (无索引版本)
TEST_F(ThreadListTest, StartThreadsWithoutIndex)
{
    std::atomic<int> task_count{0};

    mc::runtime::thread_list list;

    // 启动3个线程
    list.start_threads(3, [&task_count]() {
        task_count.fetch_add(1);
    });

    EXPECT_EQ(list.get_thread_count(), 3);
    EXPECT_FALSE(list.empty());

    // 等待所有线程完成
    list.join_all();

    EXPECT_EQ(task_count.load(), 3);
}

// 测试 thread_list 的 start_threads (带索引版本)
TEST_F(ThreadListTest, StartThreadsWithIndex)
{
    mc::sync::mutex_box<std::vector<std::size_t>> thread_indices;

    mc::runtime::thread_list list;

    // 启动5个线程，每个线程记录自己的索引
    list.start_threads(5, [&thread_indices](std::size_t index) {
        thread_indices.with_wlock([index](std::vector<std::size_t>& indices) {
            indices.push_back(index);
        });
    });

    EXPECT_EQ(list.get_thread_count(), 5);

    // 等待所有线程完成
    list.join_all();

    // 验证所有索引都被执行
    auto indices = thread_indices.copy();
    EXPECT_EQ(indices.size(), 5);
    for (std::size_t i = 0; i < 5; ++i) {
        EXPECT_NE(std::find(indices.begin(), indices.end(), i), indices.end());
    }
}

// 测试 thread_list 的 add_thread
TEST_F(ThreadListTest, AddThread)
{
    std::atomic<int> task_count{0};

    mc::runtime::thread_list list;

    // 添加多个线程
    auto* node1 = list.add_thread([&task_count]() {
        task_count.fetch_add(1);
    });

    auto* node2 = list.add_thread([&task_count]() {
        task_count.fetch_add(1);
    });

    EXPECT_EQ(list.get_thread_count(), 2);
    EXPECT_NE(node1, node2);

    list.join_all();
    EXPECT_EQ(task_count.load(), 2);
}

// 测试 thread_list 的 remove_thread - 成功情况
TEST_F(ThreadListTest, RemoveThreadSuccess)
{
    std::atomic<int> task_count{0};

    mc::runtime::thread_list list;

    // 添加一个线程
    auto* node = list.add_thread([&task_count]() {
        task_count.fetch_add(1);
    });

    EXPECT_EQ(list.get_thread_count(), 1);

    // 移除线程
    bool removed = list.remove_thread(node);
    EXPECT_TRUE(removed);
    EXPECT_EQ(list.get_thread_count(), 0);
    EXPECT_TRUE(list.empty());

    EXPECT_EQ(task_count.load(), 1);
}

// 测试 thread_list 的 remove_thread - nullptr 情况
TEST_F(ThreadListTest, RemoveThreadWithNullptr)
{
    mc::runtime::thread_list list;

    // 尝试移除 nullptr 应该返回 false
    bool removed = list.remove_thread(nullptr);
    EXPECT_FALSE(removed);
}

// 测试 thread_list 的 remove_thread - 节点不在列表中
TEST_F(ThreadListTest, RemoveThreadNotInList)
{
    std::atomic<bool> task_done{false};

    mc::runtime::thread_list list1;
    mc::runtime::thread_list list2;

    // 在 list1 中添加线程
    auto* node = list1.add_thread([&task_done]() {
        task_done.store(true);
    });

    // 尝试从 list2 中移除 list1 的节点应该返回 false
    bool removed = list2.remove_thread(node);
    EXPECT_FALSE(removed);

    list1.join_all();
}

// 测试 thread_list 的 join_all
TEST_F(ThreadListTest, JoinAll)
{
    std::atomic<int> task_count{0};

    mc::runtime::thread_list list;

    // 添加多个线程
    list.start_threads(5, [&task_count]() {
        task_count.fetch_add(1);
    });

    EXPECT_EQ(list.get_thread_count(), 5);

    // 等待所有线程完成
    list.join_all();

    EXPECT_EQ(task_count.load(), 5);
    EXPECT_EQ(list.get_thread_count(), 5); // join_all 不会移除线程
}

// 测试 thread_list 的 clear
TEST_F(ThreadListTest, Clear)
{
    std::atomic<int> task_count{0};

    {
        mc::runtime::thread_list list;

        // 添加多个线程
        list.start_threads(3, [&task_count]() {
            task_count.fetch_add(1);
        });

        EXPECT_EQ(list.get_thread_count(), 3);

        // 清理所有线程（会等待完成）
        list.clear();

        EXPECT_TRUE(list.empty());
        EXPECT_EQ(list.get_thread_count(), 0);
    }

    EXPECT_EQ(task_count.load(), 3);
}

// 测试 thread_list 的 get_thread_count
TEST_F(ThreadListTest, GetThreadCount)
{
    mc::runtime::thread_list list;

    EXPECT_EQ(list.get_thread_count(), 0);

    list.start_threads(3, []() {
    });

    EXPECT_EQ(list.get_thread_count(), 3);

    list.join_all();
    EXPECT_EQ(list.get_thread_count(), 3); // join_all 不会改变计数
}

// 测试 thread_list 的 empty
TEST_F(ThreadListTest, Empty)
{
    mc::runtime::thread_list list;

    EXPECT_TRUE(list.empty());

    list.add_thread([]() {
    });

    EXPECT_FALSE(list.empty());

    list.join_all();
    EXPECT_FALSE(list.empty()); // join_all 不会清空列表
}

// 测试 thread_list 的 visit_threads (非const版本)
TEST_F(ThreadListTest, VisitThreads)
{
    std::atomic<int> visit_count{0};

    mc::runtime::thread_list list;

    list.start_threads(3, []() {
    });

    // 访问所有线程
    list.visit_threads([&visit_count](mc::runtime::thread_node& node) {
        visit_count.fetch_add(1);
        EXPECT_NE(node.get_id(), std::thread::id());
    });

    EXPECT_EQ(visit_count.load(), 3);

    list.join_all();
}

// 测试 thread_list 的 visit_threads (const版本)
TEST_F(ThreadListTest, VisitThreadsConst)
{
    std::atomic<int> visit_count{0};

    mc::runtime::thread_list list;

    list.start_threads(3, []() {
    });

    // 访问所有线程（const版本）
    const auto& const_list = list;
    const_list.visit_threads([&visit_count](const mc::runtime::thread_node& node) {
        visit_count.fetch_add(1);
        EXPECT_NE(node.get_id(), std::thread::id());
    });

    EXPECT_EQ(visit_count.load(), 3);

    list.join_all();
}

// 测试 thread_list 的 join_all 在空列表时
TEST_F(ThreadListTest, JoinAllWhenEmpty)
{
    mc::runtime::thread_list list;

    // 在空列表上调用 join_all 应该不抛出异常
    EXPECT_NO_THROW(list.join_all());
}

// 测试 thread_list 的 clear 在空列表时
TEST_F(ThreadListTest, ClearWhenEmpty)
{
    mc::runtime::thread_list list;

    // 在空列表上调用 clear 应该不抛出异常
    EXPECT_NO_THROW(list.clear());
    EXPECT_TRUE(list.empty());
}

// 测试 thread_list 的复杂场景 - 添加和移除混合
TEST_F(ThreadListTest, MixedAddRemove)
{
    std::atomic<int> task_count{0};

    mc::runtime::thread_list list;

    // 添加3个线程
    auto* node1 = list.add_thread([&task_count]() {
        task_count.fetch_add(1);
    });

    auto* node2 = list.add_thread([&task_count]() {
        task_count.fetch_add(1);
    });

    auto* node3 = list.add_thread([&task_count]() {
        task_count.fetch_add(1);
    });

    EXPECT_EQ(list.get_thread_count(), 3);

    // 移除中间的线程
    // 注意：remove_thread 会等待线程完成后再移除
    bool removed = list.remove_thread(node2);
    EXPECT_TRUE(removed);
    EXPECT_EQ(list.get_thread_count(), 2);

    // 因为 remove_thread 会等待 node2 完成，所以 node2 的任务已经执行
    // task_count 此时可能是 1（只有 node2 完成）或更多
    int count_after_remove = task_count.load();
    EXPECT_GE(count_after_remove, 1);

    // 等待剩余的线程完成
    list.join_all();

    // 所有线程都会执行（因为 remove_thread 等待被移除的线程完成）
    EXPECT_EQ(task_count.load(), 3);
}

// 测试 thread_list 的析构函数会等待所有线程
TEST_F(ThreadListTest, DestructorWaitsForThreads)
{
    std::atomic<int>  task_count{0};
    std::atomic<bool> all_tasks_done{false};

    {
        mc::runtime::thread_list list;

        // 添加多个线程
        list.start_threads(5, [&task_count, &all_tasks_done]() {
            int count = task_count.fetch_add(1) + 1;
            if (count == 5) {
                all_tasks_done.store(true);
            }
        });

        // 不手动 join，依赖析构函数
    } // list 在这里析构

    // 析构函数应该等待所有线程完成
    EXPECT_EQ(task_count.load(), 5);
    EXPECT_TRUE(all_tasks_done.load());
}

// 测试 thread_node 析构时线程不可 join 的分支
TEST_F(ThreadListTest, ThreadNodeDestructorNotJoinable)
{
    std::atomic<bool> task_done{false};

    {
        mc::runtime::thread_node node([&task_done]() {
            task_done.store(true);
        });

        // 等待线程完成
        node.join();

        // 此时线程已完成，析构时应该不调用 join
        EXPECT_FALSE(node.joinable());
    }

    EXPECT_TRUE(task_done.load());
}

// 测试 thread_list start_threads 中 thread_count 为 0 的分支
TEST_F(ThreadListTest, StartThreadsZeroCount)
{
    mc::runtime::thread_list list;

    // 启动 0 个线程应该不抛出异常
    EXPECT_NO_THROW(list.start_threads(0, []() {
    }));
    EXPECT_EQ(list.get_thread_count(), 0);
    EXPECT_TRUE(list.empty());
}

// 测试 thread_list start_threads 带索引版本中 thread_count 为 0 的分支
TEST_F(ThreadListTest, StartThreadsZeroCountWithIndex)
{
    mc::runtime::thread_list list;

    // 启动 0 个线程应该不抛出异常
    EXPECT_NO_THROW(list.start_threads(0, [](std::size_t) {
    }));
    EXPECT_EQ(list.get_thread_count(), 0);
    EXPECT_TRUE(list.empty());
}

// 测试 thread_list clear 后再次操作
TEST_F(ThreadListTest, ClearThenReuse)
{
    std::atomic<int> task_count{0};

    mc::runtime::thread_list list;

    // 添加线程
    list.start_threads(2, [&task_count]() {
        task_count.fetch_add(1);
    });

    EXPECT_EQ(list.get_thread_count(), 2);

    // 清理
    list.clear();
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(task_count.load(), 2);

    // 清理后可以再次使用
    list.start_threads(1, [&task_count]() {
        task_count.fetch_add(1);
    });

    EXPECT_EQ(list.get_thread_count(), 1);

    list.join_all();
    EXPECT_EQ(task_count.load(), 3);
}

// 测试 thread_list visit_threads 在空列表时的行为
TEST_F(ThreadListTest, VisitThreadsEmpty)
{
    mc::runtime::thread_list list;

    int visit_count = 0;

    // 访问空列表应该不抛出异常，且不调用 visitor
    list.visit_threads([&visit_count](mc::runtime::thread_node&) {
        visit_count++;
    });

    EXPECT_EQ(visit_count, 0);
}

// 测试 thread_list visit_threads const 版本在空列表时的行为
TEST_F(ThreadListTest, VisitThreadsConstEmpty)
{
    const mc::runtime::thread_list list;

    int visit_count = 0;

    // 访问空列表应该不抛出异常，且不调用 visitor
    list.visit_threads([&visit_count](const mc::runtime::thread_node&) {
        visit_count++;
    });

    EXPECT_EQ(visit_count, 0);
}

// 测试 thread_node 析构时 joinable 为 true 的分支
TEST_F(ThreadListTest, ThreadNodeDestructorJoinable)
{
    std::atomic<bool>              task_done{false};
    mc::test::runtime::future_flag task_ready;

    {
        mc::runtime::thread_node node([&task_done, task_ready]() mutable {
            task_done.store(true);
            task_ready.set();
        });

        EXPECT_TRUE(node.joinable());
    }

    EXPECT_TRUE(task_ready.wait_for(3s));
    EXPECT_TRUE(task_done.load());
}

// 测试 thread_node join 时 joinable 为 true 的分支
TEST_F(ThreadListTest, ThreadNodeJoinWhenJoinable)
{
    std::atomic<bool> task_done{false};

    mc::runtime::thread_node node([&task_done]() {
        task_done.store(true);
    });

    // 线程还在运行时应该可以 join
    EXPECT_TRUE(node.joinable());
    node.join();
    EXPECT_FALSE(node.joinable());
    EXPECT_TRUE(task_done.load());
}

// 测试 thread_list remove_thread 中循环查找 found 为 true 的分支
TEST_F(ThreadListTest, RemoveThreadFoundInLoop)
{
    std::atomic<int> task_count{0};

    mc::runtime::thread_list list;

    // 添加多个线程
    auto* node1 = list.add_thread([&task_count]() {
        task_count.fetch_add(1);
    });

    auto* node2 = list.add_thread([&task_count]() {
        task_count.fetch_add(1);
    });

    auto* node3 = list.add_thread([&task_count]() {
        task_count.fetch_add(1);
    });

    EXPECT_EQ(list.get_thread_count(), 3);

    // 移除中间的线程
    bool removed = list.remove_thread(node2);
    EXPECT_TRUE(removed);
    EXPECT_EQ(list.get_thread_count(), 2);

    list.join_all();
    EXPECT_EQ(task_count.load(), 3);
}

// 测试 thread_list remove_thread 中循环查找 found 为 false 的分支（直到循环结束）
TEST_F(ThreadListTest, RemoveThreadNotFoundAfterLoop)
{
    mc::runtime::thread_list list;

    // 创建一个不在列表中的节点指针（野指针，但这里只是测试逻辑）
    // 实际上我们应该创建一个真实的节点但不在列表中
    mc::runtime::thread_list list2;
    auto*                    node = list2.add_thread([]() {
    });

    // 尝试从 list 中移除 list2 的节点
    bool removed = list.remove_thread(node);
    EXPECT_FALSE(removed);

    list2.join_all();
}
