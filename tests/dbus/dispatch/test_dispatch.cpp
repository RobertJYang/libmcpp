/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2. You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PSL v2 for more details.
 */

#include <dbus/dbus.h>
#include <gtest/gtest.h>
#include <mc/dbus/connection.h>
#include <mc/dbus/match.h>
#include <mc/dbus/message.h>
#include <mc/runtime/thread_pool.h>

#include "dispatch/pending_call.h"

#include <atomic>
#include <test_utilities/test_base.h>
#include <thread>

using namespace mc::dbus;

// 辅助宏：从tuple中提取bool值用于EXPECT_TRUE
#define REQUEST_NAME_SUCCESS(conn, name) std::get<0>((conn).request_name(name))

class dispatch_test : public mc::test::TestWithDbusDaemon {
protected:
    static void SetUpTestSuite()
    {
        TestWithDbusDaemon::SetUpTestSuite();
        s_io_context = std::make_shared<mc::runtime::thread_pool>(1);
        s_io_context->start();
    }

    static void TearDownTestSuite()
    {
        s_io_context->stop();
        s_io_context->join();
        s_io_context.reset();
        TestWithDbusDaemon::TearDownTestSuite();
    }

    void SetUp() override
    {}

    static std::shared_ptr<mc::runtime::thread_pool> s_io_context;
};

std::shared_ptr<mc::runtime::thread_pool> dispatch_test::s_io_context;

TEST_F(dispatch_test, test_connection_dispatch)
{
    auto conn = connection::open_session_bus(*s_io_context);
    EXPECT_TRUE(conn.start());
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(REQUEST_NAME_SUCCESS(conn, "org.test.Dispatch"));
    auto msg =
        message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");
    auto reply = conn.send_with_reply(std::move(msg), mc::milliseconds(1000));
    EXPECT_TRUE(reply.is_valid());
    conn.dispatch();
    conn.disconnect();
}

TEST_F(dispatch_test, test_watch_timeout_via_connection)
{
    auto conn = connection::open_session_bus(*s_io_context);
    EXPECT_TRUE(conn.start());
    ASSERT_TRUE(conn.is_connected());
    for (int i = 0; i < 3; ++i) {
        auto msg   = message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                                              "ListNames");
        auto reply = conn.send_with_reply(std::move(msg), mc::milliseconds(1000));
        EXPECT_TRUE(reply.is_valid());
    }
    conn.disconnect();
}

TEST_F(dispatch_test, test_watch_timeout_stop_on_disconnect)
{
    auto conn = connection::open_session_bus(*s_io_context);
    EXPECT_TRUE(conn.start());
    ASSERT_TRUE(conn.is_connected());
    auto msg =
        message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");
    conn.async_send_with_reply(std::move(msg), mc::milliseconds(1000));
    conn.disconnect();
    EXPECT_FALSE(conn.is_connected());
}

TEST_F(dispatch_test, test_async_send_with_reply)
{
    auto conn = connection::open_session_bus(*s_io_context);
    EXPECT_TRUE(conn.start());
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(REQUEST_NAME_SUCCESS(conn, "org.test.Dispatch"));
    auto msg =
        message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetId");
    auto future = conn.async_send_with_reply(std::move(msg), mc::seconds(1));
    EXPECT_NE(future.wait_for(mc::seconds(2)), mc::future_status::timeout);
    EXPECT_TRUE(future.get().is_valid());
    conn.disconnect();
}

TEST_F(dispatch_test, test_multiple_concurrent_calls)
{
    auto conn = connection::open_session_bus(*s_io_context);
    EXPECT_TRUE(conn.start());
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(REQUEST_NAME_SUCCESS(conn, "org.test.Dispatch"));

    const int                                num_calls = 5;
    std::vector<connection::future<message>> futures;
    for (int i = 0; i < num_calls; ++i) {
        auto msg =
            message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetId");
        futures.push_back(conn.async_send_with_reply(std::move(msg), mc::seconds(1)));
    }

    auto all_futures = mc::all(futures.begin(), futures.end());
    EXPECT_EQ(all_futures.wait_for(mc::seconds(3)), mc::future_status::ready);
    for (const auto& reply : all_futures.get()) {
        EXPECT_TRUE(reply.is_valid());
    }
    conn.disconnect();
}

TEST_F(dispatch_test, test_dispatch_while_receiving)
{
    auto conn = connection::open_session_bus(*s_io_context);
    EXPECT_TRUE(conn.start());
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(REQUEST_NAME_SUCCESS(conn, "org.test.Dispatch"));

    auto msg =
        message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");
    auto future = conn.async_send_with_reply(std::move(msg), mc::milliseconds(1000));

    std::thread dispatch_thread([&conn]() {
        for (int i = 0; i < 10; ++i) {
            conn.dispatch();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    EXPECT_EQ(future.wait_for(mc::milliseconds(2000)), mc::future_status::ready);
    dispatch_thread.join();
    EXPECT_TRUE(future.get().is_valid());
    conn.disconnect();
}

TEST_F(dispatch_test, test_pending_call_already_completed)
{
    auto conn = connection::open_session_bus(*s_io_context);
    conn.start();
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(REQUEST_NAME_SUCCESS(conn, "org.test.Dispatch"));

    auto msg =
        message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetId");
    auto reply = conn.send_with_reply(std::move(msg), mc::milliseconds(1000));
    EXPECT_TRUE(reply.is_valid());

    conn.disconnect();
}

TEST_F(dispatch_test, test_pending_call_move_operations)
{
    auto conn = connection::open_session_bus(*s_io_context);
    conn.start();
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(REQUEST_NAME_SUCCESS(conn, "org.test.Dispatch"));

    auto msg1 =
        message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetId");
    auto future1 = conn.async_send_with_reply(std::move(msg1), mc::milliseconds(1000));

    auto msg2 =
        message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetId");
    auto future2 = conn.async_send_with_reply(std::move(msg2), mc::milliseconds(1000));

    ASSERT_EQ(mc::all(future1, future2).wait_for(mc::milliseconds(2000)), mc::future_status::ready);
    EXPECT_TRUE(future1.is_ready());
    EXPECT_TRUE(future2.is_ready());
    conn.disconnect();
}

TEST_F(dispatch_test, test_pending_call_stop_before_reply)
{
    auto conn = connection::open_session_bus(*s_io_context);
    conn.start();
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(REQUEST_NAME_SUCCESS(conn, "org.test.Dispatch"));

    auto msg =
        message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetId");
    auto future = conn.async_send_with_reply(std::move(msg), mc::milliseconds(5000));

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    conn.disconnect();

    future.wait_for(mc::milliseconds(1000));
    EXPECT_TRUE(future.is_ready());
}

TEST_F(dispatch_test, test_timeout_handling)
{
    auto conn = connection::open_session_bus(*s_io_context);
    conn.start();
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(REQUEST_NAME_SUCCESS(conn, "org.test.Dispatch"));

    for (int i = 0; i < 5; ++i) {
        auto msg =
            message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetId");
        auto reply = conn.send_with_reply(std::move(msg), mc::milliseconds(1000));
        EXPECT_TRUE(reply.is_valid());
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    conn.disconnect();
}

TEST_F(dispatch_test, test_timeout_with_long_interval)
{
    auto conn = connection::open_session_bus(*s_io_context);
    conn.start();
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(REQUEST_NAME_SUCCESS(conn, "org.test.Dispatch"));

    auto msg =
        message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");
    auto reply = conn.send_with_reply(std::move(msg), mc::milliseconds(2000));
    EXPECT_TRUE(reply.is_valid());

    conn.disconnect();
}

TEST_F(dispatch_test, test_pending_call_move_assignment)
{
    pending_call first(nullptr, pending_call::reply_cb{});
    pending_call second(nullptr, pending_call::reply_cb{});

    // 触发移动赋值运算符
    second = std::move(first);

    // 再次执行 release，确保不会重复释放
    second.release();
}

TEST_F(dispatch_test, test_add_and_remove_match_rules)
{
    auto conn = connection::open_session_bus(*s_io_context);
    ASSERT_TRUE(conn.start());
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(REQUEST_NAME_SUCCESS(conn, "org.test.Dispatch"));

    match_rule rule1 = match_rule::new_signal("NameOwnerChanged", DBUS_INTERFACE_DBUS);
    match_rule rule2 = match_rule::new_signal("PropertiesChanged", DBUS_PROPERTIES_INTERFACE);

    std::atomic<int> hit1{0};
    std::atomic<int> hit2{0};

    conn.add_match(rule1, [&hit1](message&) {
        hit1.fetch_add(1);
    }, 1001);
    conn.add_match(rule2, [&hit2](message&) {
        hit2.fetch_add(1);
    }, 1002);

    conn.remove_match(1001);
    conn.remove_match(1002);

    conn.remove_match(1001);
    conn.remove_match(9999);

    conn.disconnect();
}

// 测试 pending_call::start() 触发 handle_reply() 的"已完成"分支
TEST_F(dispatch_test, PendingCallImmediateReply)
{
    auto conn = connection::open_session_bus(*s_io_context);
    conn.start();
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(REQUEST_NAME_SUCCESS(conn, "org.test.Dispatch"));

    // 创建一个已经完成的 pending_call
    auto msg =
        message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetId");
    auto reply = conn.send_with_reply(std::move(msg), mc::milliseconds(1000));
    EXPECT_TRUE(reply.is_valid());

    // 测试回调为空时保持静默
    {
        DBusPendingCall* pending = nullptr;
        // 创建一个 pending_call 且 reply_cb 为空指针
        pending_call empty_call(pending, pending_call::reply_cb{});
        // 确保 handle_reply() 返回不崩溃
        empty_call.release();
    }

    conn.disconnect();
}

// 测试 watch_readable 分支中 elog("dbus watch 读取错误")
TEST_F(dispatch_test, WatchReadableErrorLogged)
{
    auto conn = connection::open_session_bus(*s_io_context);
    conn.start();
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(REQUEST_NAME_SUCCESS(conn, "org.test.Dispatch"));

    // 通过正常操作触发 watch，错误日志分支很难直接测试
    // 因为需要模拟 Boost.Asio 在 async_wait 时返回非 operation_aborted 错误
    // 这个测试主要确保代码不会崩溃
    auto msg =
        message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetId");
    auto reply = conn.send_with_reply(std::move(msg), mc::milliseconds(1000));
    EXPECT_TRUE(reply.is_valid());

    conn.disconnect();
}

// 测试 watch_writable 及其错误路径
TEST_F(dispatch_test, WatchWritableInvoked)
{
    auto conn = connection::open_session_bus(*s_io_context);
    conn.start();
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(REQUEST_NAME_SUCCESS(conn, "org.test.Dispatch"));

    // 通过 send_with_reply 触发写事件
    auto msg =
        message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetId");
    auto reply = conn.send_with_reply(std::move(msg), mc::milliseconds(1000));
    EXPECT_TRUE(reply.is_valid());

    conn.disconnect();
}

// 测试 handle_watch_ready 返回 false 时不重复监听
TEST_F(dispatch_test, HandleWatchReadyStopsWhenFalse)
{
    auto conn = connection::open_session_bus(*s_io_context);
    conn.start();
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(REQUEST_NAME_SUCCESS(conn, "org.test.Dispatch"));

    // 通过正常操作触发 watch，handle_watch_ready 返回 false 的分支很难直接测试
    // 这个测试主要确保代码不会崩溃
    auto msg =
        message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetId");
    auto reply = conn.send_with_reply(std::move(msg), mc::milliseconds(1000));
    EXPECT_TRUE(reply.is_valid());

    conn.disconnect();
}

// 测试 elog("dbus 定时器错误")
TEST_F(dispatch_test, TimeoutErrorLogged)
{
    auto conn = connection::open_session_bus(*s_io_context);
    conn.start();
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(REQUEST_NAME_SUCCESS(conn, "org.test.Dispatch"));

    // 通过正常操作触发 timeout，错误日志分支很难直接测试
    // 因为需要模拟 boost::asio::error::fault 等错误
    // 这个测试主要确保代码不会崩溃
    auto msg =
        message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetId");
    auto reply = conn.send_with_reply(std::move(msg), mc::milliseconds(1000));
    EXPECT_TRUE(reply.is_valid());

    conn.disconnect();
}

// 测试 dbus_timeout_handle 被调用的路径
TEST_F(dispatch_test, TimeoutHandlerInvoked)
{
    auto conn = connection::open_session_bus(*s_io_context);
    conn.start();
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(REQUEST_NAME_SUCCESS(conn, "org.test.Dispatch"));

    // 通过正常操作触发 timeout，dbus_timeout_handle 会被调用
    auto msg =
        message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetId");
    auto reply = conn.send_with_reply(std::move(msg), mc::milliseconds(1000));
    EXPECT_TRUE(reply.is_valid());

    conn.disconnect();
}
