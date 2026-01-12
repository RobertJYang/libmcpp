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
#include <mc/runtime/runtime_context.h>
#include <mc/runtime/runtime_executor.h>
#include <test_utilities/test_base.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

using namespace mc::runtime;

namespace {

class RuntimeExecutorTest : public mc::test::TestWithRuntime {
protected:
    void SetUp() override {
        mc::test::TestWithRuntime::reset_runtime();
        runtime_config config;
        config.io_threads   = 2;
        config.work_threads = 2;
        mc::get_runtime_context().initialize(config);
        mc::get_runtime_context().start();
    }

    void TearDown() override {
        mc::get_runtime_context().stop();
        mc::get_runtime_context().join();
        mc::test::TestWithRuntime::reset_runtime();
    }
};

} // namespace

// 测试 runtime_executor 的基本构造
TEST_F(RuntimeExecutorTest, basic_construction) {
    // 默认构造
    runtime_executor exec1;
    EXPECT_TRUE(true);

    runtime_executor exec2(mc::get_runtime_context());
    EXPECT_TRUE(true);

    EXPECT_EQ(exec1, exec2);
}

// 测试 runtime_executor 的拷贝和移动
TEST_F(RuntimeExecutorTest, copy_and_move) {
    runtime_executor original;

    // 拷贝构造
    runtime_executor copied(original);
    EXPECT_EQ(original, copied);

    // 拷贝赋值
    runtime_executor assigned;
    assigned = original;
    EXPECT_EQ(original, assigned);

    // 移动构造
    runtime_executor moved(std::move(copied));
    EXPECT_EQ(original, moved);

    // 移动赋值
    runtime_executor move_assigned;
    move_assigned = std::move(assigned);
    EXPECT_EQ(original, move_assigned);
}

TEST_F(RuntimeExecutorTest, post_task) {
    runtime_executor  exec;
    std::promise<int> promise;
    std::future<int>  future = promise.get_future();
    std::atomic<bool> executed{false};

    exec.post([&]() {
        executed = true;
        promise.set_value(42);
    });

    auto result = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(result, std::future_status::ready);
    EXPECT_TRUE(executed.load());
    EXPECT_EQ(future.get(), 42);
}

TEST_F(RuntimeExecutorTest, dispatch_task) {
    runtime_executor  exec;
    std::promise<int> promise;
    std::future<int>  future = promise.get_future();

    exec.dispatch([&]() {
        promise.set_value(100);
    });

    auto result = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(result, std::future_status::ready);
    EXPECT_EQ(future.get(), 100);
}

TEST_F(RuntimeExecutorTest, defer_task) {
    runtime_executor  exec;
    std::promise<int> promise;
    std::future<int>  future = promise.get_future();

    exec.defer([&]() {
        promise.set_value(200);
    });

    auto result = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(result, std::future_status::ready);
    EXPECT_EQ(future.get(), 200);
}
