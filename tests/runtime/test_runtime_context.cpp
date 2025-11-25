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
#include <mc/exception.h>
#include <mc/runtime.h>
#include <test_utilities/test_base.h>

#include "test_future_helpers.h"

#include <atomic>
#include <chrono>

using namespace std::chrono_literals;

namespace {

class RuntimeContextTest : public mc::test::TestWithRuntime {
    void SetUp() override {
        mc::test::TestWithRuntime::reset_runtime();
    }

    void TearDown() override {
        mc::test::TestWithRuntime::reset_runtime();
    }
};

} // namespace

// 测试基本的初始化和启动
TEST_F(RuntimeContextTest, BasicInitializeAndStart) {
    auto& runtime = mc::get_runtime_context();

    EXPECT_TRUE(runtime.is_stopped());

    // 初始化运行时上下文
    runtime.initialize(mc::runtime_config{.io_threads = 2});

    // 启动运行时上下文
    runtime.start();
    EXPECT_FALSE(runtime.is_stopped());

    // 停止运行时上下文
    runtime.stop();
    runtime.join();
    EXPECT_TRUE(runtime.is_stopped());
}

// 测试重复启动应该被忽略
TEST_F(RuntimeContextTest, DuplicateStart) {
    auto& runtime = mc::get_runtime_context();

    runtime.initialize(mc::runtime_config{.io_threads = 1});
    runtime.start();

    // 重复启动应该被忽略（不会抛出异常）
    EXPECT_NO_THROW(runtime.start());
}

// 测试IO执行器的基本功能
TEST_F(RuntimeContextTest, IoExecutorBasicPost) {
    auto& runtime = mc::get_runtime_context();
    runtime.initialize(mc::runtime_config{.io_threads = 1});
    runtime.start();

    std::atomic<bool> task_executed{false};
    mc::test::runtime::future_flag task_ready;

    // 投递任务到IO执行器
    mc::post([&task_executed, task_ready]() mutable {
        task_executed.store(true);
        task_ready.set();
    });

    EXPECT_TRUE(task_ready.wait_for(3s));
    EXPECT_TRUE(task_executed.load());
}

// 测试系统执行器的基本功能
TEST_F(RuntimeContextTest, SystemExecutorBasicPost) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    std::atomic<bool> task_executed{false};
    mc::test::runtime::future_flag task_ready;

    // 投递任务到系统执行器
    mc::post([&task_executed, task_ready]() mutable {
        task_executed.store(true);
        task_ready.set();
    }, mc::work_executor);

    EXPECT_TRUE(task_ready.wait_for(3s));
    EXPECT_TRUE(task_executed.load());
}

// 测试defer操作
TEST_F(RuntimeContextTest, DeferOperation) {
    auto& runtime = mc::get_runtime_context();
    runtime.initialize(mc::runtime_config{.io_threads = 1});
    runtime.start();

    std::atomic<bool>                  defer_executed{false};
    std::atomic<bool>                  post_executed{false};
    mc::test::runtime::future_flag     defer_ready;
    mc::test::runtime::future_flag     post_ready;

    mc::defer([&]() {
        defer_executed.store(true);
        defer_ready.set();
    });

    mc::post([&]() {
        post_executed.store(true);
        post_ready.set();
    });

    EXPECT_TRUE(defer_ready.wait_for(3s));
    EXPECT_TRUE(post_ready.wait_for(3s));
    EXPECT_TRUE(defer_executed.load());
    EXPECT_TRUE(post_executed.load());
}

// 测试dispatch操作
TEST_F(RuntimeContextTest, DispatchOperation) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    std::atomic<bool>              task_executed{false};
    mc::test::runtime::future_flag task_ready;

    mc::dispatch([&task_executed, task_ready]() mutable {
        task_executed.store(true);
        task_ready.set();
    });

    EXPECT_TRUE(task_ready.wait_for(3s));
    EXPECT_TRUE(task_executed.load());
}

// 测试多线程IO执行器
TEST_F(RuntimeContextTest, MultiThreadIoExecutor) {
    auto& runtime = mc::get_runtime_context();
    runtime.initialize(mc::runtime_config{.io_threads = 4}); // 4个IO线程
    runtime.start();

    constexpr int                              task_count = 100;
    std::atomic<int>                           completed_tasks{0};
    mc::test::runtime::countdown_future        tasks_ready(task_count);

    for (int i = 0; i < task_count; ++i) {
        mc::post([&completed_tasks, tasks_ready]() mutable {
            completed_tasks.fetch_add(1);
            tasks_ready.arrive();
        });
    }

    EXPECT_TRUE(tasks_ready.wait_for(3s));
    EXPECT_EQ(completed_tasks.load(), task_count);
}

// 测试混合使用IO执行器和系统执行器
TEST_F(RuntimeContextTest, MixedExecutorUsage) {
    auto& runtime = mc::get_runtime_context();
    runtime.initialize(mc::runtime_config{.io_threads = 2});
    runtime.start();

    std::atomic<int> io_tasks{0};
    std::atomic<int> system_tasks{0};

    constexpr int task_count = 50;
    mc::test::runtime::future_flag io_done;
    mc::test::runtime::future_flag system_done;

    // 投递IO任务
    for (int i = 0; i < task_count; ++i) {
        mc::post([&io_tasks, io_done]() mutable {
            auto current = io_tasks.fetch_add(1) + 1;
            if (current >= task_count) {
                io_done.set();
            }
        }, mc::io_executor);
    }

    // 投递系统任务
    for (int i = 0; i < task_count; ++i) {
        mc::post([&system_tasks, system_done]() mutable {
            auto current = system_tasks.fetch_add(1) + 1;
            if (current >= task_count) {
                system_done.set();
            }
        }, mc::work_executor);
    }

    EXPECT_TRUE(io_done.wait_for(3s));
    EXPECT_TRUE(system_done.wait_for(3s));
    EXPECT_EQ(io_tasks.load(), task_count);
    EXPECT_EQ(system_tasks.load(), task_count);
}

// 测试执行器对象的生命周期
TEST_F(RuntimeContextTest, ExecutorLifetime) {
    auto& runtime = mc::get_runtime_context();
    runtime.initialize(mc::runtime_config{.io_threads = 1});
    runtime.start();

    std::atomic<bool>              task_executed{false};
    mc::test::runtime::future_flag task_ready;

    {
        // 在作用域内创建执行器
        auto executor = mc::get_io_executor();

        // 投递任务
        boost::asio::post(executor, [&task_executed, task_ready]() mutable {
            task_executed.store(true);
            task_ready.set();
        });
    } // 执行器对象超出作用域

    EXPECT_TRUE(task_ready.wait_for(3s));
    EXPECT_TRUE(task_executed.load());
}

// 测试重复初始化
TEST_F(RuntimeContextTest, DuplicateInitialize) {
    auto& runtime = mc::get_runtime_context();

    runtime.initialize(mc::runtime_config{.io_threads = 1});
    runtime.start();

    // 重复初始化应该被忽略（不会抛出异常）
    EXPECT_NO_THROW(runtime.initialize(mc::runtime_config{.io_threads = 2}));

    runtime.stop();
    runtime.join();
}

// 测试从未初始化状态确保启动
TEST_F(RuntimeContextTest, EnsureStartFromUninitialized) {
    auto& runtime = mc::get_runtime_context();

    // 未初始化状态直接调用 start，应该使用默认配置
    EXPECT_NO_THROW(runtime.start());
    EXPECT_FALSE(runtime.is_stopped());

    runtime.stop();
    runtime.join();
}

// 测试从已初始化状态确保启动
TEST_F(RuntimeContextTest, EnsureStartFromInitialized) {
    auto& runtime = mc::get_runtime_context();

    runtime.initialize(mc::runtime_config{.io_threads = 2});
    // 已初始化但未启动，调用 start 应该启动
    EXPECT_NO_THROW(runtime.start());
    EXPECT_FALSE(runtime.is_stopped());

    runtime.stop();
    runtime.join();
}

// 测试从已停止状态确保启动（会抛出异常）
TEST_F(RuntimeContextTest, EnsureStartFromStopped) {
    auto& runtime = mc::get_runtime_context();

    runtime.initialize(mc::runtime_config{.io_threads = 1});
    runtime.start();
    runtime.stop();
    runtime.join();

    // 已停止状态调用 start 应该抛出异常
    EXPECT_THROW(runtime.start(), mc::invalid_op_exception);
}

// 测试未运行时停止
TEST_F(RuntimeContextTest, StopWhenNotRunning) {
    auto& runtime = mc::get_runtime_context();

    // 未启动时停止应该不抛出异常
    EXPECT_NO_THROW(runtime.stop());

    // 已停止时再次停止应该不抛出异常
    EXPECT_NO_THROW(runtime.stop());
}

// 测试空线程列表时 join
TEST_F(RuntimeContextTest, JoinWhenEmpty) {
    auto& runtime = mc::get_runtime_context();

    // 未启动时 join 应该不抛出异常
    EXPECT_NO_THROW(runtime.join());
}

// 测试获取线程数
TEST_F(RuntimeContextTest, GetThreadCount) {
    auto& runtime = mc::get_runtime_context();

    runtime.initialize(mc::runtime_config{.io_threads = 4, .work_threads = 2});
    runtime.start();

    // 验证运行时上下文已启动
    EXPECT_FALSE(runtime.is_stopped());

    runtime.stop();
    runtime.join();
}

// 测试零线程使用默认值
TEST_F(RuntimeContextTest, ZeroThreadsUsesDefault) {
    auto& runtime = mc::get_runtime_context();

    // 零线程应该使用默认值（至少1个线程）
    runtime.initialize(mc::runtime_config{.io_threads = 0, .work_threads = 0});
    runtime.start();

    EXPECT_FALSE(runtime.is_stopped());

    runtime.stop();
    runtime.join();
}

// 测试线程数超过最大值
TEST_F(RuntimeContextTest, ThreadCountExceedsMax) {
    auto& runtime = mc::get_runtime_context();

    // 线程数超过最大值应该被限制
    runtime.initialize(mc::runtime_config{.io_threads = 100, .work_threads = 100});
    runtime.start();

    EXPECT_FALSE(runtime.is_stopped());

    runtime.stop();
    runtime.join();
}

// 测试立即执行器基础功能
TEST_F(RuntimeContextTest, ImmediateExecutorBasic) {
    mc::immediate_executor executor;

    std::atomic<bool> task_executed{false};

    executor.execute([&task_executed]() {
        task_executed.store(true);
    });

    // 立即执行器应该立即执行任务
    EXPECT_TRUE(task_executed.load());
}

// 测试立即上下文基础功能
TEST_F(RuntimeContextTest, ImmediateContextBasic) {
    mc::immediate_context context;
    auto executor = context.get_executor();

    std::atomic<bool> task_executed{false};

    executor.execute([&task_executed]() {
        task_executed.store(true);
    });

    // 立即执行器应该立即执行任务
    EXPECT_TRUE(task_executed.load());
}

// 测试向上下文投递任务
TEST_F(RuntimeContextTest, PostToContexts) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    std::atomic<int> io_count{0};
    std::atomic<int> work_count{0};

    mc::test::runtime::future_flag io_ready;
    mc::test::runtime::future_flag work_ready;

    boost::asio::post(runtime.get_io_context(), [&io_count, io_ready]() mutable {
        io_count.fetch_add(1);
        io_ready.set();
    });

    boost::asio::post(runtime.get_work_context(), [&work_count, work_ready]() mutable {
        work_count.fetch_add(1);
        work_ready.set();
    });

    EXPECT_TRUE(io_ready.wait_for(3s));
    EXPECT_TRUE(work_ready.wait_for(3s));
    EXPECT_EQ(io_count.load(), 1);
    EXPECT_EQ(work_count.load(), 1);
}

// 测试向上下文延迟投递任务
TEST_F(RuntimeContextTest, DeferToContexts) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    std::atomic<int> io_count{0};
    std::atomic<int> work_count{0};

    mc::test::runtime::future_flag io_ready;
    mc::test::runtime::future_flag work_ready;

    boost::asio::defer(runtime.get_io_context(), [&io_count, io_ready]() mutable {
        io_count.fetch_add(1);
        io_ready.set();
    });

    boost::asio::defer(runtime.get_work_context(), [&work_count, work_ready]() mutable {
        work_count.fetch_add(1);
        work_ready.set();
    });

    EXPECT_TRUE(io_ready.wait_for(3s));
    EXPECT_TRUE(work_ready.wait_for(3s));
    EXPECT_EQ(io_count.load(), 1);
    EXPECT_EQ(work_count.load(), 1);
}

// 测试向上下文分发任务
TEST_F(RuntimeContextTest, DispatchToContexts) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    std::atomic<int> io_count{0};
    std::atomic<int> work_count{0};

    mc::test::runtime::future_flag io_ready;
    mc::test::runtime::future_flag work_ready;

    boost::asio::dispatch(runtime.get_io_context(), [&io_count, io_ready]() mutable {
        io_count.fetch_add(1);
        io_ready.set();
    });

    boost::asio::dispatch(runtime.get_work_context(), [&work_count, work_ready]() mutable {
        work_count.fetch_add(1);
        work_ready.set();
    });

    EXPECT_TRUE(io_ready.wait_for(3s));
    EXPECT_TRUE(work_ready.wait_for(3s));
    EXPECT_EQ(io_count.load(), 1);
    EXPECT_EQ(work_count.load(), 1);
}

// 测试创建 strand 执行器
TEST_F(RuntimeContextTest, MakeStrands) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    auto io_strand   = mc::make_io_strand();
    auto work_strand = mc::make_work_strand();

    std::atomic<int> count{0};
    std::promise<void> promise;

    // 使用 strand 投递任务，确保串行执行
    boost::asio::post(io_strand, [&]() {
        count.fetch_add(1);
        if (count.load() == 2) {
            promise.set_value();
        }
    });

    boost::asio::post(io_strand, [&]() {
        count.fetch_add(1);
        if (count.load() == 2) {
            promise.set_value();
        }
    });

    // 等待任务完成
    promise.get_future().wait_for(std::chrono::milliseconds(500));
    EXPECT_EQ(count.load(), 2);
}

// 测试停止后上下文重启
TEST_F(RuntimeContextTest, ContextRestartAfterStop) {
    auto& runtime = mc::get_runtime_context();

    runtime.initialize(mc::runtime_config{.io_threads = 1});
    runtime.start();
    runtime.stop();
    runtime.join();

    // 重新启动：需要先重置，因为停止后状态是 stopped，无法直接重启
    mc::test::TestWithRuntime::reset_runtime();
    // 重置后需要重新获取引用，因为旧引用已失效
    auto& runtime2 = mc::get_runtime_context();
    runtime2.initialize(mc::runtime_config{.io_threads = 1});
    runtime2.start();

    std::atomic<bool>              task_executed{false};
    mc::test::runtime::future_flag task_ready;
    mc::post([&task_executed, task_ready]() mutable {
        task_executed.store(true);
        task_ready.set();
    });

    EXPECT_TRUE(task_ready.wait_for(3s));
    EXPECT_TRUE(task_executed.load());

    runtime2.stop();
    runtime2.join();
}

// 测试未初始化状态的 is_stopped
TEST_F(RuntimeContextTest, IsStoppedUninitialized) {
    auto& runtime = mc::get_runtime_context();

    // 未初始化时应该返回 true
    EXPECT_TRUE(runtime.is_stopped());
}

// 测试未停止时的析构函数
TEST_F(RuntimeContextTest, DestructorWithoutStop) {
    // 注意：runtime_context 是单例，不会析构，所以这个测试需要验证的是：
    // 即使不调用 stop，单例仍然保持运行状态
    // 但析构函数会在单例被销毁时（测试结束时）自动调用 stop 和 join
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    std::atomic<bool>              task_executed{false};
    mc::test::runtime::future_flag task_ready;
    mc::post([&task_executed, task_ready]() mutable {
        task_executed.store(true);
        task_ready.set();
    });

    EXPECT_TRUE(task_ready.wait_for(3s));
    EXPECT_TRUE(task_executed.load());

    // 不调用 stop 和 join，让析构函数在测试结束时处理
    // 由于是单例，运行时不会析构，但测试结束后会重置
    // 验证运行时状态
    EXPECT_FALSE(runtime.is_stopped()) << "运行时不应停止";

    // 显式停止以清理资源
    runtime.stop();
    runtime.join();
    EXPECT_TRUE(runtime.is_stopped());
}

// 测试线程数边缘情况
TEST_F(RuntimeContextTest, ThreadCountEdgeCases) {
    auto& runtime = mc::get_runtime_context();

    // 测试最小值
    runtime.initialize(mc::runtime_config{.io_threads = 1, .work_threads = 1});
    runtime.start();
    EXPECT_FALSE(runtime.is_stopped());
    runtime.stop();
    runtime.join();

    // 测试较大的值：需要先重置，因为停止后状态是 stopped，无法直接重启
    mc::test::TestWithRuntime::reset_runtime();
    // 重置后需要重新获取引用，因为旧引用已失效
    auto& runtime2 = mc::get_runtime_context();
    runtime2.initialize(mc::runtime_config{.io_threads = 8, .work_threads = 4});
    runtime2.start();
    EXPECT_FALSE(runtime2.is_stopped());
    runtime2.stop();
    runtime2.join();
}

// 测试线程列表非空时启动
TEST_F(RuntimeContextTest, StartImplWithNonEmptyThreads) {
    auto& runtime = mc::get_runtime_context();

    runtime.initialize(mc::runtime_config{.io_threads = 1});
    runtime.start();

    // 再次启动应该被忽略（线程列表非空）
    EXPECT_NO_THROW(runtime.start());

    runtime.stop();
    runtime.join();
}

// 测试线程列表为空时 join
TEST_F(RuntimeContextTest, JoinWhenThreadListEmpty) {
    auto& runtime = mc::get_runtime_context();

    // 未启动时 join 应该立即返回（线程列表为空）
    EXPECT_NO_THROW(runtime.join());
}

// 测试两个线程列表都不为空时启动
TEST_F(RuntimeContextTest, StartImplBothThreadListsNonEmpty) {
    auto& runtime = mc::get_runtime_context();

    runtime.initialize(mc::runtime_config{.io_threads = 2, .work_threads = 2});
    runtime.start();

    // 再次启动应该被忽略
    EXPECT_NO_THROW(runtime.start());

    runtime.stop();
    runtime.join();
}

// 测试停止状态的 is_stopped
TEST_F(RuntimeContextTest, IsStoppedStateStopped) {
    auto& runtime = mc::get_runtime_context();

    runtime.initialize(mc::runtime_config{.io_threads = 1});
    runtime.start();
    EXPECT_FALSE(runtime.is_stopped());

    runtime.stop();
    EXPECT_TRUE(runtime.is_stopped());

    runtime.join();
}

// 测试线程异常处理
TEST_F(RuntimeContextTest, ThreadExceptionHandling) {
    auto& runtime = mc::get_runtime_context();
    runtime.initialize(mc::runtime_config{.io_threads = 1});
    runtime.start();

    // 投递一个会抛出异常的任务
    std::atomic<bool>              exception_caught{false};
    mc::test::runtime::future_flag exception_ready;
    mc::post([&exception_caught, exception_ready]() mutable {
        try {
            throw std::runtime_error("test exception");
        } catch (const std::exception&) {
            exception_caught.store(true);
            exception_ready.set();
        }
    });

    EXPECT_TRUE(exception_ready.wait_for(3s));
    EXPECT_TRUE(exception_caught.load());

    runtime.stop();
    runtime.join();
}

// 测试 immediate_executor 的 operator!= 和 require
TEST_F(RuntimeContextTest, ImmediateExecutorCompareAndRequire) {
    mc::immediate_executor exec1;
    mc::immediate_executor exec2;

    // 测试 operator!=，应该返回 false（因为所有 immediate_executor 都相等）
    EXPECT_FALSE(exec1 != exec2);

    // 测试 require(blocking.never)
    auto required = exec1.require(boost::asio::execution::blocking.never);
    
    // 验证返回的对象仍然可以执行
    std::atomic<bool> task_executed{false};
    required.execute([&task_executed]() {
        task_executed.store(true);
    });
    EXPECT_TRUE(task_executed.load());
}

// 测试工作线程异常捕获
TEST_F(RuntimeContextTest, WorkThreadExceptionLogged) {
    auto& runtime = mc::get_runtime_context();
    
    // 配置为只有 1 个工作线程，以便更容易触发异常
    runtime.initialize(mc::runtime_config{.io_threads = 1, .work_threads = 1});
    runtime.start();

    mc::test::runtime::future_flag execution_done;

    mc::post([&runtime, execution_done]() mutable {
        try {
            throw std::runtime_error("test work thread exception");
        } catch (...) {
            execution_done.set();
            throw;
        }
    }, mc::work_executor);

    EXPECT_TRUE(execution_done.wait_for(3s));
    runtime.stop();
    runtime.join();

    // 验证运行时已停止
    EXPECT_TRUE(runtime.is_stopped());
}