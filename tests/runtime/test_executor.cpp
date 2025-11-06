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

// 测试 nullptr 拷贝构造
TEST_F(ExecutorTest, CopyConstructionWithNullptr) {
    mc::executor invalid_executor;

    // 从无效执行器拷贝构造
    mc::executor copied(invalid_executor);
    EXPECT_FALSE(copied.valid());
    EXPECT_EQ(invalid_executor, copied);
}

// 测试自赋值拷贝
TEST_F(ExecutorTest, CopyAssignmentSelfAssignment) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    mc::executor exec(mc::make_io_strand());
    auto backup = exec;

    // 自赋值应该安全
    exec = exec;

    EXPECT_TRUE(exec.valid());
    EXPECT_EQ(exec, backup);
}

// 测试 nullptr 拷贝赋值
TEST_F(ExecutorTest, CopyAssignmentWithNullptr) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    mc::executor valid_exec(mc::make_io_strand());
    mc::executor invalid_exec;

    // 从无效执行器赋值
    valid_exec = invalid_exec;
    EXPECT_FALSE(valid_exec.valid());
}

// 测试自赋值移动
TEST_F(ExecutorTest, MoveAssignmentSelfAssignment) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    mc::executor exec(mc::make_io_strand());
    auto backup = exec;

    // 自移动赋值应该安全
    exec = std::move(exec);

    EXPECT_TRUE(exec.valid());
    EXPECT_EQ(exec, backup);
}

// 测试相同 impl 指针的相等性
TEST_F(ExecutorTest, EqualitySameImplPointer) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    auto strand = mc::make_io_strand();
    mc::executor exec1(strand);
    mc::executor exec2(strand);

    // 相同 strand 应该相等
    EXPECT_EQ(exec1, exec2);
}

// 测试与 nullptr 的相等性
TEST_F(ExecutorTest, EqualityWithNullptr) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    mc::executor valid_exec(mc::make_io_strand());
    mc::executor invalid_exec1;
    mc::executor invalid_exec2;

    // 无效执行器之间应该相等
    EXPECT_EQ(invalid_exec1, invalid_exec2);

    // 有效和无效执行器应该不等
    EXPECT_NE(valid_exec, invalid_exec1);
    EXPECT_NE(invalid_exec1, valid_exec);
}

// 测试不等操作符
TEST_F(ExecutorTest, InequalityOperator) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    auto strand1  = mc::make_io_strand();
    auto strand2  = mc::make_io_strand();
    mc::executor exec1(strand1);
    mc::executor exec2(strand2);
    mc::executor invalid;

    // 使用 operator!= 直接测试
    EXPECT_NE(exec1, exec2);
    EXPECT_NE(exec1, invalid);
    EXPECT_NE(invalid, exec1);

    // 相同执行器应该相等
    EXPECT_FALSE(exec1 != exec1);
}

// 测试 nullptr 的工作生命周期
TEST_F(ExecutorTest, WorkLifecycleWithNullptr) {
    mc::executor invalid_exec;

    // 对无效执行器调用工作生命周期方法应该不抛出异常
    EXPECT_NO_THROW(invalid_exec.on_work_started());
    EXPECT_NO_THROW(invalid_exec.on_work_finished());
}

// 测试 nullptr 的 context 抛出异常
TEST_F(ExecutorTest, ContextWithNullptrThrows) {
    mc::executor invalid_exec;

    // 对无效执行器调用 context 应该抛出异常
    EXPECT_THROW(invalid_exec.context(), mc::invalid_op_exception);
}

// 测试多引用的析构函数
TEST_F(ExecutorTest, DestructorWithMultipleReferences) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    // 创建多个引用
    {
        mc::executor original(mc::make_io_strand());
        std::vector<mc::executor> copies;
        for (int i = 0; i < 10; ++i) {
            copies.push_back(original);
        }
        // 所有副本超出作用域，引用计数应该正确管理
    }

    // 原始执行器也应该仍然有效（如果还在作用域内）
}

// 测试单引用的析构函数
TEST_F(ExecutorTest, DestructorWithSingleReference) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    // 创建单个执行器
    {
        mc::executor exec(mc::make_io_strand());
        EXPECT_TRUE(exec.valid());
        // 执行器超出作用域，应该正确清理
    }
}

// 测试两个 nullptr 的拷贝赋值
TEST_F(ExecutorTest, CopyAssignmentBothNullptr) {
    mc::executor invalid1;
    mc::executor invalid2;

    // 两个无效执行器之间的赋值应该安全
    invalid1 = invalid2;
    EXPECT_FALSE(invalid1.valid());
    EXPECT_EQ(invalid1, invalid2);
}

// 测试 release 返回 false 的拷贝赋值
TEST_F(ExecutorTest, CopyAssignmentReleaseReturnsFalse) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    auto strand = mc::make_io_strand();
    mc::executor exec1(strand);
    mc::executor exec2(strand);

    // exec1 和 exec2 共享同一个 impl，release 应该返回 false
    // （因为还有 exec2 持有引用）
    exec1 = exec2;
    EXPECT_TRUE(exec1.valid());
    EXPECT_TRUE(exec2.valid());
    EXPECT_EQ(exec1, exec2);
}

// 测试 equal 返回 false 的相等性
TEST_F(ExecutorTest, EqualityEqualReturnsFalse) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    auto strand1  = mc::make_io_strand();
    auto strand2  = mc::make_io_strand();
    mc::executor exec1(strand1);
    mc::executor exec2(strand2);

    // 不同 strand 应该不相等
    EXPECT_NE(exec1, exec2);
}

// 测试 context 抛出异常
TEST_F(ExecutorTest, ContextThrowsException) {
    mc::executor invalid_exec;

    // 对无效执行器调用 context 应该抛出异常
    EXPECT_THROW(invalid_exec.context(), mc::invalid_op_exception);
}

// 测试包装不同执行器类型
TEST_F(ExecutorTest, WrapDifferentExecutorTypes) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    // 包装 IO strand
    auto io_strand = mc::make_io_strand();
    mc::executor exec_io(io_strand);
    EXPECT_TRUE(exec_io.valid());

    // 包装 work strand
    auto work_strand = mc::make_work_strand();
    mc::executor exec_work(work_strand);
    EXPECT_TRUE(exec_work.valid());

    // 包装普通执行器
    auto io_executor = mc::get_io_executor();
    mc::executor exec_io_plain(io_executor);
    EXPECT_TRUE(exec_io_plain.valid());
}

// 测试一个 nullptr 的相等性
TEST_F(ExecutorTest, EqualityOneNullptr) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    mc::executor valid_exec(mc::make_io_strand());
    mc::executor invalid_exec;

    // 一个有效一个无效应该不相等
    EXPECT_NE(valid_exec, invalid_exec);
    EXPECT_NE(invalid_exec, valid_exec);
}

// 测试两个非 nullptr 但不同的相等性
TEST_F(ExecutorTest, EqualityBothNonNullptrDifferent) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    auto strand1  = mc::make_io_strand();
    auto strand2  = mc::make_io_strand();
    mc::executor exec1(strand1);
    mc::executor exec2(strand2);

    // 两个不同的有效执行器应该不相等
    EXPECT_NE(exec1, exec2);
}

// 测试自赋值拷贝
TEST_F(ExecutorTest, CopyAssignmentSelf) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    mc::executor exec(mc::make_io_strand());
    auto backup = exec;

    // 自赋值
    exec = exec;

    EXPECT_TRUE(exec.valid());
    EXPECT_EQ(exec, backup);
}

// 测试 this 为 nullptr 的拷贝赋值
TEST_F(ExecutorTest, CopyAssignmentThisNullptr) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    mc::executor invalid_exec;
    mc::executor valid_exec(mc::make_io_strand());

    // 从有效执行器赋值给无效执行器
    invalid_exec = valid_exec;
    EXPECT_TRUE(invalid_exec.valid());
    EXPECT_EQ(invalid_exec, valid_exec);
}

// 测试 release 返回 true 的拷贝赋值
TEST_F(ExecutorTest, CopyAssignmentReleaseReturnsTrue) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    auto strand1 = mc::make_io_strand();
    auto strand2 = mc::make_io_strand();

    mc::executor exec1(strand1);
    {
        mc::executor exec2(strand2);
        // exec1 持有唯一的 strand1 引用，exec2 持有唯一的 strand2 引用
        exec1 = exec2;
        // exec2 超出作用域，但 exec1 现在持有 strand2 的引用
    }

    EXPECT_TRUE(exec1.valid());
}

// 测试 other 为 nullptr 的拷贝赋值
TEST_F(ExecutorTest, CopyAssignmentOtherNullptr) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    mc::executor valid_exec(mc::make_io_strand());
    mc::executor invalid_exec;

    // 从无效执行器赋值给有效执行器
    valid_exec = invalid_exec;
    EXPECT_FALSE(valid_exec.valid());
}

// 测试 other 非 nullptr 的拷贝赋值
TEST_F(ExecutorTest, CopyAssignmentOtherNonNullptr) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    auto strand1 = mc::make_io_strand();
    auto strand2 = mc::make_io_strand();

    mc::executor exec1(strand1);
    mc::executor exec2(strand2);

    // 从有效执行器赋值给另一个有效执行器
    exec1 = exec2;
    EXPECT_TRUE(exec1.valid());
    EXPECT_EQ(exec1, exec2);
}

// 测试自赋值移动
TEST_F(ExecutorTest, MoveAssignmentSelf) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    mc::executor exec(mc::make_io_strand());
    auto backup = exec;

    // 自移动赋值应该安全
    exec = std::move(exec);

    EXPECT_TRUE(exec.valid());
    EXPECT_EQ(exec, backup);
}

// 测试 other 为 nullptr 的移动赋值
TEST_F(ExecutorTest, MoveAssignmentOtherNullptr) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    mc::executor valid_exec(mc::make_io_strand());
    mc::executor invalid_exec;

    // 从无效执行器移动赋值给有效执行器
    valid_exec = std::move(invalid_exec);
    EXPECT_FALSE(valid_exec.valid());
}