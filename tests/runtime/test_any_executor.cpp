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
#include <mc/future.h>
#include <mc/runtime.h>
#include <test_utilities/test_base.h>

#include <atomic>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

namespace {

class AnyExecutorTest : public mc::test::TestWithRuntime {
    // 使用基类的设置和清理逻辑
};

} // namespace

// 测试 any_executor 的基本构造和使用
TEST_F(AnyExecutorTest, BasicConstruction) {
    auto& runtime = mc::get_runtime_context();

    // 从不同类型的执行器构造 any_executor
    auto io_executor   = mc::get_io_executor();
    auto work_executor = mc::get_work_executor();

    mc::any_executor any_io(io_executor);
    mc::any_executor any_work(work_executor);

    // 验证执行器有效性
    EXPECT_TRUE(any_io.valid());
    EXPECT_TRUE(any_work.valid());
}

// 测试 any_executor 的默认构造
TEST_F(AnyExecutorTest, DefaultConstruction) {
    mc::any_executor default_executor;

    // 默认构造的执行器应该是无效的
    EXPECT_FALSE(default_executor.valid());
}

// 测试 any_executor 的拷贝语义
TEST_F(AnyExecutorTest, CopySemantics) {
    auto& runtime = mc::get_runtime_context();

    auto             io_executor = mc::get_io_executor();
    mc::any_executor original(io_executor);

    // 拷贝构造
    mc::any_executor copied(original);
    EXPECT_TRUE(copied.valid());
    EXPECT_EQ(original, copied);

    // 拷贝赋值
    mc::any_executor assigned;
    assigned = original;
    EXPECT_TRUE(assigned.valid());
    EXPECT_EQ(original, assigned);
}

// 测试 any_executor 的移动语义
TEST_F(AnyExecutorTest, MoveSemantics) {
    auto& runtime = mc::get_runtime_context();

    auto             io_executor = mc::get_io_executor();
    mc::any_executor original(io_executor);
    mc::any_executor backup = original;

    // 移动构造
    mc::any_executor moved(std::move(original));
    EXPECT_TRUE(moved.valid());
    EXPECT_EQ(moved, backup);

    // 移动赋值
    mc::any_executor move_assigned;
    move_assigned = std::move(moved);
    EXPECT_TRUE(move_assigned.valid());
    EXPECT_EQ(move_assigned, backup);
}

// 测试 any_executor 的 post 操作
TEST_F(AnyExecutorTest, PostOperation) {
    auto& runtime = mc::get_runtime_context();

    std::atomic<int> task_count{0};

    // 测试不同类型的执行器包装
    auto io_executor   = mc::get_io_executor();
    auto work_executor = mc::get_work_executor();

    mc::any_executor any_io(io_executor);
    mc::any_executor any_work(work_executor);

    std::promise<void> promise;

    // 使用 any_executor 投递任务
    any_io.post([&task_count, &promise]() {
        task_count.fetch_add(1);
        if (task_count.load() == 2) {
            promise.set_value();
        }
    });

    any_work.post([&task_count, &promise]() {
        task_count.fetch_add(1);
        if (task_count.load() == 2) {
            promise.set_value();
        }
    });

    // 等待任务完成
    promise.get_future().wait_for(std::chrono::milliseconds(500));

    EXPECT_EQ(task_count.load(), 2);
}

// 测试 any_executor 的 defer 操作
TEST_F(AnyExecutorTest, DeferOperation) {
    auto& runtime = mc::get_runtime_context();

    bool task_executed{false};

    auto             io_executor = mc::get_io_executor();
    mc::any_executor any_io(io_executor);

    any_io.defer([&task_executed]() {
        task_executed = true;
    });

    // 等待任务执行
    std::this_thread::sleep_for(100ms);
    EXPECT_TRUE(task_executed);
}

// 测试 any_executor 的 dispatch 操作
TEST_F(AnyExecutorTest, DispatchOperation) {
    auto& runtime = mc::get_runtime_context();

    bool task_executed{false};

    mc::any_executor any_io(mc::get_io_executor());

    any_io.dispatch([&task_executed]() {
        task_executed = true;
    });

    // 给一点时间确保任务执行
    std::this_thread::sleep_for(10ms);
    EXPECT_TRUE(task_executed);
}

// 测试 any_executor 的比较操作
TEST_F(AnyExecutorTest, ComparisonOperations) {
    auto& runtime = mc::get_runtime_context();

    auto io_executor   = mc::get_io_executor();
    auto work_executor = mc::get_work_executor();

    mc::any_executor any_io1(io_executor);
    mc::any_executor any_io2(io_executor);
    mc::any_executor any_work(work_executor);

    // 相同类型的执行器应该相等
    EXPECT_EQ(any_io1, any_io2);
    EXPECT_FALSE(any_io1 != any_io2);

    // 不同类型的执行器应该不等
    EXPECT_NE(any_io1, any_work);
    EXPECT_FALSE(any_io1 == any_work);

    // 无效执行器的比较
    mc::any_executor invalid1;
    mc::any_executor invalid2;
    EXPECT_EQ(invalid1, invalid2);
    EXPECT_NE(invalid1, any_io1);
}

// 测试 any_executor 包装 strand
TEST_F(AnyExecutorTest, StrandWrapping) {
    auto& runtime = mc::get_runtime_context();

    // 创建 strand
    auto io_strand   = mc::make_io_strand();
    auto work_strand = mc::make_work_strand();

    // 包装到 any_executor 中
    mc::any_executor any_io_strand(io_strand);
    mc::any_executor any_work_strand(work_strand);

    EXPECT_TRUE(any_io_strand.valid());
    EXPECT_TRUE(any_work_strand.valid());

    // 测试 strand 的串行执行特性
    std::atomic<int>   counter{0};
    std::vector<int>   execution_order;
    std::promise<void> promise;

    const int task_count = 10;
    for (int i = 0; i < task_count; ++i) {
        any_io_strand.post([&counter, &execution_order, &promise]() {
            int current = counter.fetch_add(1);
            std::this_thread::sleep_for(1ms);

            execution_order.push_back(current);
            if (current == task_count) {
                promise.set_value();
            }
        });
    }

    // 等待所有任务完成
    promise.get_future().wait_for(std::chrono::milliseconds(500));

    for (int i = 0; i < task_count; ++i) {
        EXPECT_EQ(execution_order[i], i) << "Strand 执行顺序不正确";
    }
}
