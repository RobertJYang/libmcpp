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

#include <atomic>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

namespace {

class RuntimeContextTest : public mc::test::TestWithRuntime {
    void SetUp() override {
        mc::singleton<mc::runtime::runtime_context>::reset_for_test();
    }

    void TearDown() override {
        mc::singleton<mc::runtime::runtime_context>::reset_for_test();
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

    runtime.stop();
    runtime.join();
}

// 测试IO执行器的基本功能
TEST_F(RuntimeContextTest, IoExecutorBasicPost) {
    auto& runtime = mc::get_runtime_context();
    runtime.initialize(mc::runtime_config{.io_threads = 1});
    runtime.start();

    std::atomic<bool> task_executed{false};

    // 投递任务到IO执行器
    mc::post([&task_executed]() {
        task_executed.store(true);
    });

    // 等待任务执行
    std::this_thread::sleep_for(100ms);

    EXPECT_TRUE(task_executed.load());

    runtime.stop();
    runtime.join();
}

// 测试系统执行器的基本功能
TEST_F(RuntimeContextTest, SystemExecutorBasicPost) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    std::atomic<bool> task_executed{false};

    // 投递任务到系统执行器
    mc::post([&task_executed]() {
        task_executed.store(true);
    }, mc::work_executor);

    // 等待任务执行
    std::this_thread::sleep_for(100ms);

    EXPECT_TRUE(task_executed.load());

    runtime.stop();
    runtime.join();
}

// 测试defer操作
TEST_F(RuntimeContextTest, DeferOperation) {
    auto& runtime = mc::get_runtime_context();
    runtime.initialize(mc::runtime_config{.io_threads = 1});
    runtime.start();

    std::atomic<bool> defer_executed{false};
    std::atomic<bool> post_executed{false};

    // 投递一个defer任务
    mc::defer([&]() {
        defer_executed.store(true);
    });

    // 投递一个post任务
    mc::post([&]() {
        post_executed.store(true);
    });

    // 等待任务执行
    std::this_thread::sleep_for(100ms);

    // defer和post都应该执行
    EXPECT_TRUE(defer_executed.load());
    EXPECT_TRUE(post_executed.load());

    runtime.stop();
    runtime.join();
}

// 测试dispatch操作
TEST_F(RuntimeContextTest, DispatchOperation) {
    auto& runtime = mc::get_runtime_context();
    runtime.start();

    std::atomic<bool> task_executed{false};

    // 分派任务（dispatch可能会在当前线程立即执行）
    mc::dispatch([&task_executed]() {
        task_executed.store(true);
    });

    // 给一点时间确保任务执行
    std::this_thread::sleep_for(10ms);

    EXPECT_TRUE(task_executed.load());

    runtime.stop();
    runtime.join();
}

// 测试多线程IO执行器
TEST_F(RuntimeContextTest, MultiThreadIoExecutor) {
    auto& runtime = mc::get_runtime_context();
    runtime.initialize(mc::runtime_config{.io_threads = 4}); // 4个IO线程
    runtime.start();

    constexpr int    task_count = 100;
    std::atomic<int> completed_tasks{0};

    // 投递多个任务
    for (int i = 0; i < task_count; ++i) {
        mc::post([&completed_tasks]() {
            std::this_thread::sleep_for(1ms); // 模拟一些工作
            completed_tasks.fetch_add(1);
        });
    }

    // 等待所有任务完成
    auto start_time = std::chrono::steady_clock::now();
    while (completed_tasks.load() < task_count) {
        std::this_thread::sleep_for(10ms);

        // 防止无限等待
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        ASSERT_LT(elapsed, 5s) << "任务执行超时";
    }

    EXPECT_EQ(completed_tasks.load(), task_count);

    runtime.stop();
    runtime.join();
}

// 测试混合使用IO执行器和系统执行器
TEST_F(RuntimeContextTest, MixedExecutorUsage) {
    auto& runtime = mc::get_runtime_context();
    runtime.initialize(mc::runtime_config{.io_threads = 2});
    runtime.start();

    std::atomic<int> io_tasks{0};
    std::atomic<int> system_tasks{0};

    constexpr int task_count = 50;

    // 投递IO任务
    for (int i = 0; i < task_count; ++i) {
        mc::post([&io_tasks]() {
            io_tasks.fetch_add(1);
        }, mc::io_executor);
    }

    // 投递系统任务
    for (int i = 0; i < task_count; ++i) {
        mc::post([&system_tasks]() {
            system_tasks.fetch_add(1);
        }, mc::work_executor);
    }

    // 等待所有任务完成
    auto start_time = std::chrono::steady_clock::now();
    while (io_tasks.load() < task_count || system_tasks.load() < task_count) {
        std::this_thread::sleep_for(10ms);

        // 防止无限等待
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        ASSERT_LT(elapsed, 5s) << "任务执行超时";
    }

    EXPECT_EQ(io_tasks.load(), task_count);
    EXPECT_EQ(system_tasks.load(), task_count);

    runtime.stop();
    runtime.join();
}

// 测试执行器对象的生命周期
TEST_F(RuntimeContextTest, ExecutorLifetime) {
    auto& runtime = mc::get_runtime_context();
    runtime.initialize(mc::runtime_config{.io_threads = 1});
    runtime.start();

    std::atomic<bool> task_executed{false};

    {
        // 在作用域内创建执行器
        auto executor = mc::get_io_executor();

        // 投递任务
        boost::asio::post(executor, [&task_executed]() {
            task_executed.store(true);
        });
    } // 执行器对象超出作用域

    // 等待任务执行
    std::this_thread::sleep_for(100ms);

    // 任务应该仍然能执行
    EXPECT_TRUE(task_executed.load());

    runtime.stop();
    runtime.join();
}