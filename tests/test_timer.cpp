/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
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

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

#include <mc/runtime/thread_pool.h>
#include <mc/timer.h>
#include <test_utilities/base.h>

using namespace std::chrono_literals;

namespace {

class timer_test : public mc::test::TestWithRuntime {};

TEST_F(timer_test, stop_prevents_pending_timeout)
{
    auto              timer = mc::make_shared<mc::timer>();
    std::atomic<bool> fired{false};

    timer->timeout.connect([&fired]() {
        fired.store(true, std::memory_order_release);
    });

    timer->start(mc::milliseconds(50));
    timer->stop();

    std::this_thread::sleep_for(100ms);
    EXPECT_FALSE(fired.load(std::memory_order_acquire));
    EXPECT_FALSE(timer->is_active());
}

TEST_F(timer_test, stop_waits_for_running_timeout)
{
    auto timer = mc::make_shared<mc::timer>();

    std::promise<void> entered_promise;
    auto               entered = entered_promise.get_future();
    std::promise<void> release_promise;
    auto               release = release_promise.get_future().share();
    std::atomic<bool>  leaving{false};

    timer->timeout.connect([&]() {
        entered_promise.set_value();
        release.wait();
        leaving.store(true, std::memory_order_release);
    });

    timer->start(mc::milliseconds(1));
    ASSERT_EQ(entered.wait_for(1s), std::future_status::ready);

    auto stopped = std::async(std::launch::async, [&timer]() {
        timer->stop();
    });

    EXPECT_EQ(stopped.wait_for(50ms), std::future_status::timeout);
    EXPECT_FALSE(leaving.load(std::memory_order_acquire));

    release_promise.set_value();

    EXPECT_EQ(stopped.wait_for(1s), std::future_status::ready);
    EXPECT_TRUE(leaving.load(std::memory_order_acquire));
    EXPECT_FALSE(timer->is_active());
}

TEST_F(timer_test, stop_from_timeout_does_not_deadlock)
{
    auto timer = mc::make_shared<mc::timer>();

    std::promise<void> done_promise;
    auto               done = done_promise.get_future();
    std::atomic<bool>  callback_finished{false};

    timer->timeout.connect([&]() {
        timer->stop();
        callback_finished.store(true, std::memory_order_release);
        done_promise.set_value();
    });

    timer->start(mc::milliseconds(1));

    EXPECT_EQ(done.wait_for(1s), std::future_status::ready);
    EXPECT_TRUE(callback_finished.load(std::memory_order_acquire));
    EXPECT_FALSE(timer->is_active());
}

TEST_F(timer_test, stop_on_worker_polls_runtime_while_waiting_for_timeout)
{
    mc::runtime::thread_pool timer_pool(1, "timer_callback");
    mc::runtime::thread_pool stop_pool(1, "timer_stop_wait");
    timer_pool.start();
    stop_pool.start();

    auto timer = mc::make_shared<mc::timer>();
    timer->set_executor(timer_pool.get_executor());

    std::promise<void> callback_entered_promise;
    auto               callback_entered = callback_entered_promise.get_future();
    std::promise<void> release_callback_promise;
    auto               release_callback = release_callback_promise.get_future().share();
    std::promise<void> stop_entered_promise;
    auto               stop_entered = stop_entered_promise.get_future();
    std::promise<void> stop_done_promise;
    auto               stop_done = stop_done_promise.get_future();
    std::atomic<bool>  release_task_ran{false};

    timer->timeout.connect([&]() {
        callback_entered_promise.set_value();
        release_callback.wait();
    });

    timer->start(mc::milliseconds(1));
    bool callback_started = callback_entered.wait_for(1s) == std::future_status::ready;
    EXPECT_TRUE(callback_started);

    if (callback_started) {
        stop_pool.get_executor().post([&]() {
            stop_entered_promise.set_value();
            timer->stop();
            stop_done_promise.set_value();
        });
    }
    bool stop_started = stop_entered.wait_for(1s) == std::future_status::ready;
    EXPECT_TRUE(stop_started);

    if (stop_started) {
        stop_pool.get_executor().post([&]() {
            release_task_ran.store(true, std::memory_order_release);
            release_callback_promise.set_value();
        });
    } else {
        release_callback_promise.set_value();
    }

    if (stop_started) {
        EXPECT_EQ(stop_done.wait_for(1s), std::future_status::ready);
        EXPECT_TRUE(release_task_ran.load(std::memory_order_acquire));
    }
    EXPECT_FALSE(timer->is_active());

    stop_pool.stop();
    stop_pool.join();
    timer_pool.stop();
    timer_pool.join();
}

} // namespace
