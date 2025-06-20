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
#include <mc/runtime.h>
#include <mc/runtime/executor.h>
#include <test_utilities/test_base.h>

#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <future>
#include <thread>

using namespace std::chrono_literals;

namespace {

class ExecutorTest : public mc::test::TestWithRuntime {
    // 使用基类的设置和清理逻辑
};

} // namespace

// 测试 executor 的基本构造和使用
TEST_F(ExecutorTest, BasicConstruction) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    // 从 strand 构造 executor
    auto io_strand   = mc::make_io_strand();
    auto work_strand = mc::make_work_strand();

    mc::executor exec_io(io_strand);
    mc::executor exec_work(work_strand);

    // 验证执行器有效性
    EXPECT_TRUE(exec_io.valid());
    EXPECT_TRUE(exec_work.valid());
}

// 测试 executor 的默认构造
TEST_F(ExecutorTest, DefaultConstruction) {
    mc::executor default_executor;

    // 默认构造的执行器应该是无效的
    EXPECT_FALSE(default_executor.valid());
}

// 测试 executor 的拷贝语义（共享底层实现）
TEST_F(ExecutorTest, CopySemantics) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    mc::executor original(mc::make_io_strand());

    // 拷贝构造
    mc::executor copied(original);
    EXPECT_TRUE(copied.valid());
    EXPECT_EQ(original, copied);

    // 拷贝赋值
    mc::executor assigned;
    assigned = original;
    EXPECT_TRUE(assigned.valid());
    EXPECT_EQ(original, assigned);

    // 验证共享底层实现 - 通过比较验证
    EXPECT_EQ(original, copied);
    EXPECT_EQ(original, assigned);
}

// 测试 executor 的移动语义
TEST_F(ExecutorTest, MoveSemantics) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    mc::executor original(mc::make_io_strand());
    mc::executor backup = original;

    // 移动构造
    mc::executor moved(std::move(original));
    EXPECT_TRUE(moved.valid());
    EXPECT_EQ(moved, backup);

    // 移动赋值
    mc::executor move_assigned;
    move_assigned = std::move(moved);
    EXPECT_TRUE(move_assigned.valid());
    EXPECT_EQ(move_assigned, backup);
}

// 测试 executor 的 post 操作
TEST_F(ExecutorTest, PostOperation) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    std::atomic<int>   task_count{0};
    std::promise<void> promise;
    auto               future = promise.get_future();

    auto io_strand   = mc::make_io_strand();
    auto work_strand = mc::make_work_strand();

    mc::executor exec_io(io_strand);
    mc::executor exec_work(work_strand);

    // 使用 executor 投递任务
    exec_io.post([&task_count, &promise]() {
        if (task_count.fetch_add(1) + 1 == 2) {
            promise.set_value();
        }
    });

    exec_work.post([&task_count, &promise]() {
        if (task_count.fetch_add(1) + 1 == 2) {
            promise.set_value();
        }
    });

    // 等待任务完成
    auto result = future.wait_for(500ms);
    ASSERT_EQ(result, std::future_status::ready) << "任务执行超时";

    EXPECT_EQ(task_count.load(), 2);
}

// 测试 executor 的 defer 操作
TEST_F(ExecutorTest, DeferOperation) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    bool task_executed{false};

    mc::executor exec_io(mc::make_io_strand());

    exec_io.defer([&task_executed]() {
        task_executed = true;
    });

    // 等待任务执行
    std::this_thread::sleep_for(100ms);
    EXPECT_TRUE(task_executed);
}

// 测试 executor 的 dispatch 操作
TEST_F(ExecutorTest, DispatchOperation) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    bool task_executed{false};

    mc::executor exec_io(mc::make_io_strand());

    exec_io.dispatch([&task_executed]() {
        task_executed = true;
    });

    // 给一点时间确保任务执行
    std::this_thread::sleep_for(10ms);
    EXPECT_TRUE(task_executed);
}

// 测试 executor 的比较操作
TEST_F(ExecutorTest, ComparisonOperations) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();
    auto io_strand1  = mc::make_io_strand();
    auto io_strand2  = mc::make_io_strand();
    auto work_strand = mc::make_work_strand();

    mc::executor exec_io1(io_strand1);
    mc::executor exec_io1_copy = exec_io1;
    mc::executor exec_io2(io_strand2);
    mc::executor exec_work(work_strand);

    // 相同底层实现的执行器应该相等
    EXPECT_EQ(exec_io1, exec_io1_copy);
    EXPECT_FALSE(exec_io1 != exec_io1_copy);

    // 不同底层实现的执行器应该不等
    EXPECT_NE(exec_io1, exec_io2);
    EXPECT_NE(exec_io1, exec_work);
    EXPECT_FALSE(exec_io1 == exec_io2);
    EXPECT_FALSE(exec_io1 == exec_work);

    // 无效执行器的比较
    mc::executor invalid1;
    mc::executor invalid2;
    EXPECT_EQ(invalid1, invalid2);
    EXPECT_NE(invalid1, exec_io1);
}

// 测试 executor 包装不同类型的 boost::asio 执行器
TEST_F(ExecutorTest, WrappingDifferentExecutorTypes) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    // 测试包装 strand
    auto io_strand   = mc::make_io_strand();
    auto work_strand = mc::make_work_strand();

    mc::executor exec_io_strand(io_strand);
    mc::executor exec_work_strand(work_strand);

    EXPECT_TRUE(exec_io_strand.valid());
    EXPECT_TRUE(exec_work_strand.valid());

    // 测试包装普通执行器
    auto io_executor   = mc::get_io_executor();
    auto work_executor = mc::get_work_executor();

    mc::executor exec_io(io_executor);
    mc::executor exec_work(work_executor);

    EXPECT_TRUE(exec_io.valid());
    EXPECT_TRUE(exec_work.valid());
}

// 测试 executor 的 strand 串行执行特性
TEST_F(ExecutorTest, StrandSerialExecution) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    auto         io_strand = mc::make_io_strand();
    mc::executor exec_strand(io_strand);

    // 测试串行执行特性
    std::atomic<int>   counter{0};
    std::vector<int>   execution_order;
    std::promise<void> promise;
    auto               future = promise.get_future();

    const int task_count = 20;
    for (int i = 0; i < task_count; ++i) {
        exec_strand.post([&]() {
            int current = counter.fetch_add(1);
            execution_order.push_back(current);
            if (current + 1 == task_count) {
                promise.set_value();
            }
        });
    }

    // 等待所有任务完成
    auto result = future.wait_for(500ms);
    ASSERT_EQ(result, std::future_status::ready) << "任务执行超时";

    // 验证串行执行（执行顺序应该是递增的）
    for (int i = 0; i < task_count; ++i) {
        EXPECT_EQ(execution_order[i], i) << "Strand 执行顺序不正确，位置 " << i;
    }
}

// 测试 executor 的异常处理
TEST_F(ExecutorTest, ExceptionHandling) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    // 测试在无效执行器上调用操作应该抛出异常
    mc::executor invalid_executor;

    EXPECT_THROW(invalid_executor.post([]() {
    }),
                 mc::invalid_op_exception);
    EXPECT_THROW(invalid_executor.defer([]() {
    }),
                 mc::invalid_op_exception);
    EXPECT_THROW(invalid_executor.dispatch([]() {
    }),
                 mc::invalid_op_exception);
}

// 测试 executor 的生命周期管理
TEST_F(ExecutorTest, LifecycleManagement) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    bool               task_executed{false};
    std::promise<void> promise;
    auto               future = promise.get_future();

    // 创建执行器并投递任务
    {
        mc::executor exec(mc::make_io_strand());

        exec.post([&]() {
            task_executed = true;
            promise.set_value();
        });

        // executor 在这里销毁，但底层 strand 应该仍然有效
    }

    // 等待任务完成
    auto result = future.wait_for(500ms);
    ASSERT_EQ(result, std::future_status::ready) << "任务执行超时";

    EXPECT_TRUE(task_executed);
}

// 测试 executor 的共享语义
TEST_F(ExecutorTest, SharedSemantics) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    int                task_count{0};
    std::promise<void> promise;
    auto               future = promise.get_future();

    auto         io_strand = mc::make_io_strand();
    mc::executor original(io_strand);

    // 创建多个副本
    std::vector<mc::executor> executors;
    for (int i = 0; i < 5; ++i) {
        executors.push_back(original);
    }

    // 使用不同的副本投递任务
    std::atomic<int> atomic_count{0};
    for (auto& exec : executors) {
        exec.post([&]() {
            task_count++;
            if (atomic_count.fetch_add(1) + 1 == 5) {
                promise.set_value();
            }
        });
    }

    // 等待所有任务完成
    auto result = future.wait_for(500ms);
    ASSERT_EQ(result, std::future_status::ready) << "任务执行超时";

    EXPECT_EQ(task_count, 5);

    // 验证所有副本都相等
    for (const auto& exec : executors) {
        EXPECT_EQ(original, exec);
    }
}