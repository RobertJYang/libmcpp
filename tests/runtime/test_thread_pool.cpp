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

#include "test_future_helpers.h"
#include <gtest/gtest.h>
#include <mc/runtime.h>
#include <mc/runtime/thread_pool.h>
#include <test_utilities/test_base.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <set>
#include <thread>

using namespace std::chrono_literals;

namespace {

class ThreadPoolTest : public mc::test::TestWithRuntime {
    void SetUp() override {
        mc::test::TestWithRuntime::reset_runtime();
    }

    void TearDown() override {
        mc::test::TestWithRuntime::reset_runtime();
    }
};

} // namespace

TEST_F(ThreadPoolTest, BasicPost) {
    auto& runtime = mc::get_runtime_context();
    runtime.initialize(mc::runtime_config{.io_threads = 2});
    runtime.start();

    auto                           executor = runtime.get_executor();
    std::atomic<bool>              executed{false};
    mc::test::runtime::future_flag flag;

    executor.post([&executed, flag]() mutable {
        executed.store(true);
        flag.set();
    }, std::allocator<void>());

    EXPECT_TRUE(flag.wait_for(1s));
    EXPECT_TRUE(executed.load());
}

TEST_F(ThreadPoolTest, LoadBalancing) {
    constexpr int            thread_count = 4;
    mc::runtime::thread_pool ctx(thread_count, "load_balancing_ctx");
    ctx.start();

    // 等待 Worker 启动
    std::this_thread::sleep_for(50ms);  // 从 100ms 减少到 50ms

    auto                                executor = ctx.get_executor();
    std::set<std::thread::id>           thread_ids;
    std::mutex                          mtx;
    mc::test::runtime::countdown_future tasks_done(100);

    for (int i = 0; i < 100; ++i) {
        executor.post(
            [&]() {
            {
                std::lock_guard lock(mtx);
                thread_ids.insert(std::this_thread::get_id());
            }
            // 简单睡眠模拟负载，促使调度器使用更多线程
            std::this_thread::sleep_for(5ms);  // 从 10ms 减少到 5ms
            tasks_done.arrive();
        }, std::allocator<void>());
    }

    EXPECT_TRUE(tasks_done.wait_for(3s));  // 从 5s 减少到 3s
    EXPECT_GE(thread_ids.size(), thread_count);
}

TEST_F(ThreadPoolTest, StrandSerialization) {
    auto& runtime = mc::get_runtime_context();
    runtime.initialize(mc::runtime_config{.io_threads = 4});
    runtime.start();

    auto                                strand     = runtime.create_strand();
    int                                 value      = 0;
    int                                 task_count = 100;
    mc::test::runtime::countdown_future countdown(task_count);

    for (int i = 0; i < task_count; ++i) {
        strand.post([&]() {
            // 每个任务简单增加 value，strand 会确保按顺序执行没有并发问题
            value++;
            countdown.arrive();
        }, std::allocator<void>());
    }

    EXPECT_TRUE(countdown.wait_for(5s));
    EXPECT_EQ(value, task_count);
}

// 验证 mc::runtime::condition_variable 阻塞等待并不会阻塞调度线程
TEST_F(ThreadPoolTest, WaitBlocked) {
    auto& runtime = mc::get_runtime_context();
    runtime.initialize(mc::runtime_config{.io_threads = 2});
    runtime.start();

    auto executor = runtime.get_executor();

    std::mutex                      mtx;
    mc::runtime::condition_variable cv;

    bool                      is_notified = false;
    mc::runtime::steady_timer timer(executor);
    timer.expires_after(10ms);
    timer.async_wait([&cv, &mtx, &is_notified](const auto& ec) mutable {
        mc::runtime::thread_pool::shard_t* shard = mc::runtime::thread_pool::get_current_shard();
        EXPECT_TRUE(shard != nullptr);
        EXPECT_EQ(shard->recursion_depth, 0); // 当前嵌套深度 0

        // 在当前 shard 上投递一个新的任务
        mc::runtime::steady_timer timer_inner(*shard->ctx);
        timer_inner.expires_after(10ms);
        timer_inner.async_wait([&cv, shard, &is_notified](auto&&) mutable {
            is_notified = true;
            cv.notify_all();
            EXPECT_EQ(shard->recursion_depth, 1); // 当前嵌套深度 1，从 cv.wait 进入
        });

        // 阻塞当前 shard 上的线程
        // cv.wait 底层会继续调度当前 shard 的其他任务，这样 timer_inner 能被正常触发
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock);

        // 退出后，嵌套深度恢复为 0
        EXPECT_EQ(shard->recursion_depth, 0);
    });

    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock);
    EXPECT_TRUE(is_notified);
}
