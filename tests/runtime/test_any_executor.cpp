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
#include <future>
#include <thread>

using namespace std::chrono_literals;

namespace {

class AnyExecutorTest : public mc::test::TestWithRuntime {
    // 使用基类的设置和清理逻辑
};

} // namespace

// 测试 any_executor 的基本构造和使用（融合多种构造方式）
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

    // 测试 system_context::executor_type 构造
    boost::asio::system_context system_ctx;
    auto                        system_exec = system_ctx.get_executor();
    mc::any_executor            any_system(system_exec);
    EXPECT_TRUE(any_system.valid());

    // 测试 runtime::executor 构造
    auto                  io_strand = mc::make_io_strand();
    mc::runtime::executor runtime_exec(io_strand);
    mc::any_executor       any_runtime(runtime_exec);
    EXPECT_TRUE(any_runtime.valid());

    // 测试模板构造函数（从 strand 构造，会被包装到 runtime::executor）
    auto             strand = mc::make_io_strand();
    mc::any_executor any_strand(strand);
    EXPECT_TRUE(any_strand.valid());
    EXPECT_TRUE(std::holds_alternative<mc::runtime::executor>(any_strand.get_executor()));
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

// 测试 any_executor 的比较操作（融合所有比较场景）
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

    // 测试 immediate_executor 比较
    mc::any_executor immediate1;
    mc::any_executor immediate2;
    EXPECT_EQ(immediate1, immediate2);

    // 测试 system_executor 比较
    boost::asio::system_context system_ctx1;
    boost::asio::system_context system_ctx2;
    auto                        system_exec1 = system_ctx1.get_executor();
    auto                        system_exec2 = system_ctx2.get_executor();
    mc::any_executor            any_system1(system_exec1);
    mc::any_executor            any_system2(system_exec1);
    mc::any_executor            any_system3(system_exec2);
    EXPECT_EQ(any_system1, any_system2);
    // system_executor 在 Boost.Asio 中都被认为是相等的
    EXPECT_EQ(any_system1.get_executor().index(), any_system3.get_executor().index());

    // 测试 runtime::executor 比较
    auto                  io_strand1 = mc::make_io_strand();
    auto                  io_strand2 = mc::make_io_strand();
    mc::runtime::executor runtime_exec1(io_strand1);
    mc::runtime::executor runtime_exec2(io_strand1);
    mc::runtime::executor runtime_exec3(io_strand2);
    mc::any_executor      any_runtime1(runtime_exec1);
    mc::any_executor      any_runtime2(runtime_exec2);
    mc::any_executor      any_runtime3(runtime_exec3);
    EXPECT_EQ(any_runtime1, any_runtime2);
    EXPECT_NE(any_runtime1, any_runtime3);

    // 测试相同类型但不同值的比较
    boost::asio::io_context ctx1;
    boost::asio::io_context ctx2;
    mc::any_executor         any_ctx1(ctx1.get_executor());
    mc::any_executor         any_ctx2(ctx2.get_executor());
    EXPECT_NE(any_ctx1, any_ctx2);
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

// 测试 any_executor 的 valid() 方法（融合所有场景）
TEST_F(AnyExecutorTest, ValidAllScenarios) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    // 测试 boost::asio 执行器（应该总是有效）
    boost::asio::io_context ctx;
    auto                    exec = ctx.get_executor();
    mc::any_executor        any_boost(exec);
    EXPECT_TRUE(any_boost.valid());

    // 测试 system_executor（应该总是有效）
    boost::asio::system_context system_ctx;
    auto                        system_exec = system_ctx.get_executor();
    mc::any_executor            any_system(system_exec);
    EXPECT_TRUE(any_system.valid());

    // 测试 runtime::executor 有效情况
    auto                  io_strand = mc::make_io_strand();
    mc::runtime::executor valid_exec(io_strand);
    mc::any_executor      any_valid(valid_exec);
    EXPECT_TRUE(any_valid.valid());

    // 测试 runtime::executor 无效情况
    mc::runtime::executor invalid_exec;
    mc::any_executor       any_invalid(invalid_exec);
    EXPECT_FALSE(any_invalid.valid());

    // 测试 immediate_executor（应该总是有效）
    mc::any_executor immediate_exec;
    EXPECT_TRUE(immediate_exec.valid());
}

// 测试 any_executor 的 context() 方法（融合所有执行器类型）
TEST_F(AnyExecutorTest, ContextAllExecutorTypes) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    // 测试 immediate_executor
    mc::any_executor immediate_exec;
    EXPECT_NO_THROW(immediate_exec.context());

    // 测试 system_executor
    boost::asio::system_context system_ctx;
    auto                        system_exec = system_ctx.get_executor();
    mc::any_executor            any_system(system_exec);
    auto&                       system_ctx_ref = any_system.context();
    EXPECT_NE(&system_ctx_ref, nullptr);
    auto& exec_ctx = system_exec.context();
    EXPECT_EQ(&system_ctx_ref, &exec_ctx);

    // 测试 runtime::executor
    auto                  io_strand = mc::make_io_strand();
    mc::runtime::executor runtime_exec(io_strand);
    mc::any_executor      any_runtime(runtime_exec);
    auto&                 runtime_ctx = any_runtime.context();
    EXPECT_NE(&runtime_ctx, nullptr);

    // 测试 io_context::executor_type
    auto             io_exec = mc::get_io_executor();
    mc::any_executor any_io(io_exec);
    auto&            io_ctx = any_io.context();
    EXPECT_NE(&io_ctx, nullptr);
}

// 测试 any_executor 的 on_work_started/on_work_finished（融合所有执行器类型）
TEST_F(AnyExecutorTest, WorkLifecycleAllExecutorTypes) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    // 测试 immediate_executor
    mc::any_executor immediate_exec;
    EXPECT_NO_THROW(immediate_exec.on_work_started());
    EXPECT_NO_THROW(immediate_exec.on_work_finished());

    // 测试 system_executor
    boost::asio::system_context system_ctx;
    auto                        system_exec = system_ctx.get_executor();
    mc::any_executor            any_system(system_exec);
    EXPECT_NO_THROW(any_system.on_work_started());
    EXPECT_NO_THROW(any_system.on_work_finished());

    // 测试 runtime::executor
    auto                  io_strand = mc::make_io_strand();
    mc::runtime::executor runtime_exec(io_strand);
    mc::any_executor      any_runtime(runtime_exec);
    EXPECT_NO_THROW(any_runtime.on_work_started());
    EXPECT_NO_THROW(any_runtime.on_work_finished());

    // 测试 io_context::executor_type
    auto             io_exec = mc::get_io_executor();
    mc::any_executor any_io(io_exec);
    EXPECT_NO_THROW(any_io.on_work_started());
    EXPECT_NO_THROW(any_io.on_work_finished());
}

// 测试 any_executor 的 get_executor() 方法（融合 const 和非 const 版本）
TEST_F(AnyExecutorTest, GetExecutor) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    auto             io_executor = mc::get_io_executor();
    mc::any_executor any_exec(io_executor);

    // 测试非 const 版本的 get_executor
    auto& executor_variant = any_exec.get_executor();
    EXPECT_TRUE(std::holds_alternative<boost::asio::io_context::executor_type>(executor_variant));

    // 测试 const 版本的 get_executor
    const mc::any_executor& const_any_exec = any_exec;
    const auto&             const_executor_variant = const_any_exec.get_executor();
    EXPECT_TRUE(std::holds_alternative<boost::asio::io_context::executor_type>(const_executor_variant));

    // 测试模板构造函数（从 strand 构造，会被包装到 runtime::executor）
    auto             strand = mc::make_io_strand();
    mc::any_executor any_strand(strand);
    EXPECT_TRUE(any_strand.valid());
    const auto& strand_variant = any_strand.get_executor();
    EXPECT_TRUE(std::holds_alternative<mc::runtime::executor>(strand_variant));
}

// 测试 any_executor 的 post/defer/dispatch 覆盖所有执行器类型（融合复杂场景）
TEST_F(AnyExecutorTest, PostDeferDispatchAllExecutorTypes) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    std::atomic<int> immediate_count{0};
    std::atomic<int> io_count{0};
    std::atomic<int> system_count{0};
    std::atomic<int> runtime_count{0};

    // 测试 immediate_executor - 确保所有操作都覆盖
    mc::any_executor immediate_exec;
    immediate_exec.post([&immediate_count]() { immediate_count++; });
    immediate_exec.defer([&immediate_count]() { immediate_count++; });
    immediate_exec.dispatch([&immediate_count]() { immediate_count++; });
    EXPECT_EQ(immediate_count.load(), 3);

    // 测试 io_context::executor_type - 确保所有操作都覆盖
    auto             io_exec = mc::get_io_executor();
    mc::any_executor io_any_exec(io_exec);
    io_any_exec.post([&io_count]() { io_count++; });
    io_any_exec.defer([&io_count]() { io_count++; });
    io_any_exec.dispatch([&io_count]() { io_count++; });
    runtime.get_io_context().run_for(100ms);
    EXPECT_EQ(io_count.load(), 3);

    // 测试 system_context::executor_type - 确保所有操作都覆盖，特别是 defer 分支
    // system_context 是全局上下文，任务会在系统线程池中执行
    boost::asio::system_context system_ctx;
    auto                        system_exec = system_ctx.get_executor();
    mc::any_executor            system_any_exec(system_exec);
    
    // 使用 promise/future 来同步等待任务完成，确保 defer 分支被覆盖
    std::promise<void> post_promise, defer_promise, dispatch_promise;
    auto              post_future = post_promise.get_future();
    auto              defer_future = defer_promise.get_future();
    auto              dispatch_future = dispatch_promise.get_future();
    
    system_any_exec.post([&system_count, &post_promise]() {
        system_count++;
        post_promise.set_value();
    });
    // 确保 defer 分支被覆盖
    system_any_exec.defer([&system_count, &defer_promise]() {
        system_count++;
        defer_promise.set_value();
    });
    system_any_exec.dispatch([&system_count, &dispatch_promise]() {
        system_count++;
        dispatch_promise.set_value();
    });
    
    // 等待所有任务完成（最多等待 1 秒）
    auto post_status = post_future.wait_for(1s);
    auto defer_status = defer_future.wait_for(1s);
    auto dispatch_status = dispatch_future.wait_for(1s);
    
    // 验证任务已执行
    EXPECT_EQ(post_status, std::future_status::ready);
    EXPECT_EQ(defer_status, std::future_status::ready);
    EXPECT_EQ(dispatch_status, std::future_status::ready);
    EXPECT_EQ(system_count.load(), 3);

    // 测试 runtime::executor - 确保所有操作都覆盖
    auto                  strand = mc::make_io_strand();
    mc::runtime::executor runtime_exec(strand);
    mc::any_executor      runtime_any_exec(runtime_exec);
    runtime_any_exec.post([&runtime_count]() { runtime_count++; });
    runtime_any_exec.defer([&runtime_count]() { runtime_count++; });
    runtime_any_exec.dispatch([&runtime_count]() { runtime_count++; });
    runtime.get_io_context().run_for(100ms);
    EXPECT_EQ(runtime_count.load(), 3);
}

// 测试 any_executor 的 post/defer/dispatch 使用自定义分配器（融合所有执行器类型）
TEST_F(AnyExecutorTest, PostDeferDispatchWithCustomAllocator) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    std::atomic<int> immediate_count{0};
    std::atomic<int> io_count{0};
    std::atomic<int> system_count{0};
    std::atomic<int> runtime_count{0};
    std::allocator<void> custom_allocator;

    // 测试 immediate_executor 使用自定义分配器
    mc::any_executor immediate_exec;
    immediate_exec.post([&immediate_count]() { immediate_count++; }, custom_allocator);
    immediate_exec.defer([&immediate_count]() { immediate_count++; }, custom_allocator);
    immediate_exec.dispatch([&immediate_count]() { immediate_count++; }, custom_allocator);
    EXPECT_EQ(immediate_count.load(), 3);

    // 测试 io_context::executor_type 使用自定义分配器
    auto             io_exec = mc::get_io_executor();
    mc::any_executor io_any_exec(io_exec);
    io_any_exec.post([&io_count]() { io_count++; }, custom_allocator);
    io_any_exec.defer([&io_count]() { io_count++; }, custom_allocator);
    io_any_exec.dispatch([&io_count]() { io_count++; }, custom_allocator);
    runtime.get_io_context().run_for(100ms);
    EXPECT_EQ(io_count.load(), 3);

    // 测试 system_context::executor_type 使用自定义分配器
    boost::asio::system_context system_ctx;
    auto                        system_exec = system_ctx.get_executor();
    mc::any_executor            system_any_exec(system_exec);
    std::promise<void>          system_promise;
    auto                        system_future = system_promise.get_future();
    system_any_exec.post([&system_count, &system_promise]() {
        system_count++;
        system_promise.set_value();
    }, custom_allocator);
    system_any_exec.defer([&system_count]() { system_count++; }, custom_allocator);
    system_any_exec.dispatch([&system_count]() { system_count++; }, custom_allocator);
    system_future.wait_for(1s);
    EXPECT_GE(system_count.load(), 1);

    // 测试 runtime::executor 使用自定义分配器
    auto                  strand = mc::make_io_strand();
    mc::runtime::executor runtime_exec(strand);
    mc::any_executor      runtime_any_exec(runtime_exec);
    runtime_any_exec.post([&runtime_count]() { runtime_count++; }, custom_allocator);
    runtime_any_exec.defer([&runtime_count]() { runtime_count++; }, custom_allocator);
    runtime_any_exec.dispatch([&runtime_count]() { runtime_count++; }, custom_allocator);
    runtime.get_io_context().run_for(100ms);
    EXPECT_EQ(runtime_count.load(), 3);
}
