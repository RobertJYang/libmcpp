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
#include <future>
#include <memory>
#include <unistd.h>

#include <mc/runtime.h>
#include <test_utilities/base.h>

using namespace std::chrono_literals;

namespace {

class runtime_timer_test : public mc::test::TestWithRuntime {};

TEST_F(runtime_timer_test, steady_timer_async_wait)
{
    std::promise<void> done;
    std::atomic<bool>  fired{false};

    mc::runtime::steady_timer timer(mc::get_io_executor());
    timer.expires_after(20ms);
    timer.async_wait([&](const std::error_code& ec) {
        EXPECT_FALSE(ec);
        fired.store(true, std::memory_order_release);
        done.set_value();
    });

    auto status = done.get_future().wait_for(1s);
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_TRUE(fired.load(std::memory_order_acquire));
}

TEST_F(runtime_timer_test, steady_timer_async_wait_accepts_move_only_handler)
{
    auto done = std::make_unique<std::promise<void>>();
    auto wait = done->get_future();

    std::atomic<bool> fired{false};

    mc::runtime::steady_timer timer(mc::get_io_executor());
    timer.expires_after(20ms);
    timer.async_wait([&fired, done = std::move(done)](const std::error_code& ec) mutable {
        EXPECT_FALSE(ec);
        fired.store(true, std::memory_order_release);
        done->set_value();
    });

    auto status = wait.wait_for(1s);
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_TRUE(fired.load(std::memory_order_acquire));
}

} // namespace
