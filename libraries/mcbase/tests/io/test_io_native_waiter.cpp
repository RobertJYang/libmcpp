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

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <mc/io.h>
#include <test_utilities/base.h>

using namespace std::chrono_literals;

namespace {

class io_native_waiter_test : public mc::test::TestWithRuntime {};

#ifndef _WIN32
TEST_F(io_native_waiter_test, native_waiter_waits_for_readable)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(pipe(fds), 0);

    std::promise<void> done;
    std::atomic<bool>  fired{false};

    {
        mc::io::native_waiter waiter(mc::get_io_executor(), mc::io::native_waiter::from_descriptor(fds[0]));
        waiter.async_wait(mc::io::native_waiter::wait_type::read, [&](const std::error_code& ec) {
            EXPECT_FALSE(ec);
            fired.store(true, std::memory_order_release);
            done.set_value();
        });

        ASSERT_EQ(write(fds[1], "x", 1), 1);

        auto status = done.get_future().wait_for(1s);
        EXPECT_EQ(status, std::future_status::ready);
        EXPECT_TRUE(fired.load(std::memory_order_acquire));

        waiter.close();
    }

    close(fds[1]);
}

TEST_F(io_native_waiter_test, native_waiter_accepts_move_only_handler)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(pipe(fds), 0);

    auto done = std::make_unique<std::promise<void>>();
    auto wait = done->get_future();

    std::atomic<bool> fired{false};

    {
        mc::io::native_waiter waiter(mc::get_io_executor(), mc::io::native_waiter::from_descriptor(fds[0]));
        waiter.async_wait(mc::io::native_waiter::wait_type::read,
                          [&fired, done = std::move(done)](const std::error_code& ec) mutable {
            EXPECT_FALSE(ec);
            fired.store(true, std::memory_order_release);
            done->set_value();
        });

        ASSERT_EQ(write(fds[1], "x", 1), 1);

        auto status = wait.wait_for(1s);
        EXPECT_EQ(status, std::future_status::ready);
        EXPECT_TRUE(fired.load(std::memory_order_acquire));

        waiter.close();
    }

    close(fds[1]);
}

TEST_F(io_native_waiter_test, native_waiter_close_cancels_pending_wait)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(pipe(fds), 0);

    std::promise<std::error_code> done;
    auto                          wait = done.get_future();

    {
        mc::io::native_waiter waiter(mc::get_io_executor(), mc::io::native_waiter::from_descriptor(fds[0]));
        waiter.async_wait(mc::io::native_waiter::wait_type::read, [&done](const std::error_code& ec) mutable {
            done.set_value(ec);
        });

        waiter.close();

        auto status = wait.wait_for(1s);
        EXPECT_EQ(status, std::future_status::ready);
        EXPECT_EQ(wait.get(), std::make_error_code(std::errc::operation_canceled));
    }

    close(fds[1]);
}
#else
TEST_F(io_native_waiter_test, native_waiter_waits_for_signal)
{
    HANDLE handle = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    ASSERT_NE(handle, nullptr);

    std::promise<void> done;
    std::atomic<bool>  fired{false};

    {
        mc::io::native_waiter waiter(mc::get_io_executor(), mc::io::native_waiter::from_waitable_handle(handle));
        waiter.async_wait(mc::io::native_waiter::wait_type::signal, [&](const std::error_code& ec) {
            EXPECT_FALSE(ec);
            fired.store(true, std::memory_order_release);
            done.set_value();
        });

        ASSERT_NE(SetEvent(handle), 0);

        auto status = done.get_future().wait_for(1s);
        EXPECT_EQ(status, std::future_status::ready);
        EXPECT_TRUE(fired.load(std::memory_order_acquire));

        waiter.close();
    }
}

TEST_F(io_native_waiter_test, native_waiter_close_cancels_pending_wait)
{
    HANDLE handle = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    ASSERT_NE(handle, nullptr);

    std::promise<std::error_code> done;
    auto                          wait = done.get_future();

    {
        mc::io::native_waiter waiter(mc::get_io_executor(), mc::io::native_waiter::from_waitable_handle(handle));
        waiter.async_wait(mc::io::native_waiter::wait_type::signal, [&done](const std::error_code& ec) mutable {
            done.set_value(ec);
        });

        waiter.close();

        auto status = wait.wait_for(1s);
        EXPECT_EQ(status, std::future_status::ready);
        EXPECT_EQ(wait.get(), std::make_error_code(std::errc::operation_canceled));
    }
}
#endif

} // namespace
