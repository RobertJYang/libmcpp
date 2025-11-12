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

    // 默认构造的执行器是 immediate_executor
    EXPECT_TRUE(default_executor.valid());
    EXPECT_TRUE(std::holds_alternative<mc::immediate_executor>(default_executor.get_executor()));
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

// 测试 any_executor 的 valid() 方法 - runtime::executor 无效情况
TEST_F(AnyExecutorTest, ValidWithInvalidRuntimeExecutor) {
    // 创建无效的 runtime::executor
    mc::runtime::executor invalid_exec;

    // 包装到 any_executor 中
    mc::any_executor any_exec(invalid_exec);

    // 无效的 executor 应该返回 false
    EXPECT_FALSE(any_exec.valid());
}

// 测试 any_executor 的 operator== - 相同类型相同值
TEST_F(AnyExecutorTest, EqualitySameTypeSameValue) {
    auto& runtime = mc::get_runtime_context();

    auto io_executor = mc::get_io_executor();

    // 创建两个相同值的 any_executor
    mc::any_executor any1(io_executor);
    mc::any_executor any2(io_executor);

    // 相同类型的执行器应该相等
    EXPECT_EQ(any1, any2);
}

// 测试 any_executor 的 operator!= 函数
TEST_F(AnyExecutorTest, InequalityOperator) {
    auto& runtime = mc::get_runtime_context();

    auto io_executor   = mc::get_io_executor();
    auto work_executor = mc::get_work_executor();

    mc::any_executor any_io(io_executor);
    mc::any_executor any_work(work_executor);
    mc::any_executor invalid;

    // 使用 operator!= 直接测试
    EXPECT_NE(any_io, any_work);
    EXPECT_NE(any_io, invalid);
    EXPECT_NE(any_work, invalid);

    // 相同执行器应该相等
    EXPECT_FALSE(any_io != any_io);
}

// 测试 any_executor 的 on_work_started 和 on_work_finished - immediate_executor 分支
TEST_F(AnyExecutorTest, WorkLifecycleWithImmediateExecutor) {
    // 默认构造的 any_executor 是 immediate_executor
    mc::any_executor immediate_exec;

    // 对 immediate_executor 调用应该不抛出异常
    EXPECT_NO_THROW(immediate_exec.on_work_started());
    EXPECT_NO_THROW(immediate_exec.on_work_finished());
}

// 测试 any_executor 的 context() - immediate_executor 异常分支
TEST_F(AnyExecutorTest, ContextWithImmediateExecutor) {
    // 默认构造的 any_executor 是 immediate_executor
    mc::any_executor immediate_exec;

    EXPECT_NO_THROW(immediate_exec.context());
}

// 测试 any_executor 的 post/defer/dispatch - immediate_executor 异常分支
TEST_F(AnyExecutorTest, OperationsWithImmediateExecutor) {
    // 默认构造的 any_executor 是 immediate_executor
    mc::any_executor immediate_exec;

    EXPECT_NO_THROW(immediate_exec.post([]() {
    }));
    EXPECT_NO_THROW(immediate_exec.defer([]() {
    }));
    EXPECT_NO_THROW(immediate_exec.dispatch([]() {
    }));
}

// 测试 any_executor 从 system_context::executor_type 构造
TEST_F(AnyExecutorTest, ConstructionFromSystemExecutor) {
    boost::asio::system_context system_ctx;
    auto                        system_exec = system_ctx.get_executor();

    mc::any_executor any_exec(system_exec);

    EXPECT_TRUE(any_exec.valid());
}

// 测试 any_executor 从 runtime::executor 构造（有效情况）
TEST_F(AnyExecutorTest, ConstructionFromRuntimeExecutor) {
    auto& runtime = mc::get_runtime_context();

    auto                  io_strand = mc::make_io_strand();
    mc::runtime::executor runtime_exec(io_strand);

    mc::any_executor any_exec(runtime_exec);

    EXPECT_TRUE(any_exec.valid());
}

// 测试 any_executor::operator== 中相同类型但不同值的比较
TEST_F(AnyExecutorTest, EqualitySameTypeDifferentValue) {
    auto& runtime = mc::get_runtime_context();

    // 创建两个不同的 io_context 执行器
    boost::asio::io_context ctx1;
    boost::asio::io_context ctx2;

    mc::any_executor any1(ctx1.get_executor());
    mc::any_executor any2(ctx2.get_executor());

    // 相同类型但不同值，应该不相等
    EXPECT_NE(any1, any2);
}

// 测试 any_executor::operator== 中 immediate_executor 的比较（已测试过，但确保覆盖）
TEST_F(AnyExecutorTest, EqualityImmediateExecutorBoth) {
    mc::any_executor immediate1;
    mc::any_executor immediate2;

    // 两个 immediate_executor 应该相等
    EXPECT_EQ(immediate1, immediate2);
}

// 测试 any_executor::operator== 中相同类型相同执行器的比较
TEST_F(AnyExecutorTest, EqualitySameExecutor) {
    auto& runtime = mc::get_runtime_context();

    auto io_executor = mc::get_io_executor();

    mc::any_executor any1(io_executor);
    mc::any_executor any2(io_executor);

    // 包装相同执行器的 any_executor 应该相等
    EXPECT_EQ(any1, any2);
}

// 测试 any_executor valid() 中 boost::asio 执行器分支
TEST_F(AnyExecutorTest, ValidBoostAsioExecutor) {
    boost::asio::io_context ctx;
    auto                    exec = ctx.get_executor();

    mc::any_executor any_exec(exec);

    // boost::asio 执行器应该总是有效
    EXPECT_TRUE(any_exec.valid());
}

// 测试 any_executor 从任意执行器类型构造（通过模板构造函数）
TEST_F(AnyExecutorTest, ConstructionFromArbitraryExecutor) {
    auto& runtime = mc::get_runtime_context();

    // 通过 strand 构造，应该使用模板构造函数包装到 runtime::executor
    auto io_strand = mc::make_io_strand();

    mc::any_executor any_exec(io_strand);

    EXPECT_TRUE(any_exec.valid());
}

// 测试 any_executor::valid() 中 runtime::executor valid() 返回 false 的分支
TEST_F(AnyExecutorTest, ValidRuntimeExecutorReturnsFalse) {
    // 创建无效的 runtime::executor
    mc::runtime::executor invalid_exec;

    // 包装到 any_executor 中
    mc::any_executor any_exec(invalid_exec);

    // 应该返回 false
    EXPECT_FALSE(any_exec.valid());
}

// 测试 any_executor::valid() 中 runtime::executor valid() 返回 true 的分支
TEST_F(AnyExecutorTest, ValidRuntimeExecutorReturnsTrue) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    auto                  io_strand = mc::make_io_strand();
    mc::runtime::executor valid_exec(io_strand);

    mc::any_executor any_exec(valid_exec);

    // 应该返回 true
    EXPECT_TRUE(any_exec.valid());
}

// 测试 any_executor::operator== 中 index 相同但 exec != other_exec 的分支
TEST_F(AnyExecutorTest, EqualitySameIndexDifferentExec) {
    // 创建两个不同的 boost::asio 执行器
    boost::asio::io_context ctx1;
    boost::asio::io_context ctx2;

    mc::any_executor any1(ctx1.get_executor());
    mc::any_executor any2(ctx2.get_executor());

    // 相同类型（相同 index），但不同执行器，应该不相等
    EXPECT_NE(any1, any2);
}

// 测试 any_executor::operator== 中 index 相同且 exec == other_exec 的分支
TEST_F(AnyExecutorTest, EqualitySameIndexSameExec) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    auto io_executor = mc::get_io_executor();

    mc::any_executor any1(io_executor);
    mc::any_executor any2(io_executor);

    // 相同类型且相同执行器，应该相等
    EXPECT_EQ(any1, any2);
}
