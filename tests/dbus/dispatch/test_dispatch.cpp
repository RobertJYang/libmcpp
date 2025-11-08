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
#include <mc/dbus/message.h>
#include <mc/dbus/match.h>

#include "dbus/dispatch/pending_call.h"

#include <test_utilities/test_base.h>
#include <thread>
#include <atomic>

using namespace mc::dbus;

class dispatch_test : public mc::test::TestWithDbusDaemon {
protected:
    static void SetUpTestSuite() {
        TestWithDbusDaemon::SetUpTestSuite();
        s_io_context = std::make_shared<boost::asio::io_context>();
        s_thread     = std::make_unique<std::thread>([io_context = s_io_context]() {
            auto work = boost::asio::make_work_guard(*io_context);
            io_context->run();
        });
    }

    static void TearDownTestSuite() {
        s_io_context->stop();
        s_thread->join();
        s_thread.reset();
        s_io_context.reset();
        TestWithDbusDaemon::TearDownTestSuite();
    }

    void SetUp() override {
        s_io_context->restart();
    }

    static std::shared_ptr<boost::asio::io_context> s_io_context;
    static std::unique_ptr<std::thread>             s_thread;
};

std::shared_ptr<boost::asio::io_context> dispatch_test::s_io_context;
std::unique_ptr<std::thread>             dispatch_test::s_thread;

TEST_F(dispatch_test, test_connection_dispatch) {
    auto conn = connection::open_session_bus(*s_io_context);
    EXPECT_TRUE(conn.start());
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(conn.request_name("org.test.Dispatch"));
    auto msg = message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                       "org.freedesktop.DBus", "ListNames");
    auto reply = conn.send_with_reply(std::move(msg), mc::milliseconds(1000));
    EXPECT_TRUE(reply.is_valid());
    conn.dispatch();
    conn.disconnect();
}

TEST_F(dispatch_test, test_watch_timeout_via_connection) {
    auto conn = connection::open_session_bus(*s_io_context);
    EXPECT_TRUE(conn.start());
    ASSERT_TRUE(conn.is_connected());
    for (int i = 0; i < 3; ++i) {
        auto msg = message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                           "org.freedesktop.DBus", "ListNames");
        auto reply = conn.send_with_reply(std::move(msg), mc::milliseconds(1000));
        EXPECT_TRUE(reply.is_valid());
    }
    conn.disconnect();
}

TEST_F(dispatch_test, test_watch_timeout_stop_on_disconnect) {
    auto conn = connection::open_session_bus(*s_io_context);
    EXPECT_TRUE(conn.start());
    ASSERT_TRUE(conn.is_connected());
    auto msg = message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                       "org.freedesktop.DBus", "ListNames");
    conn.async_send_with_reply(std::move(msg), mc::milliseconds(1000));
    conn.disconnect();
    EXPECT_FALSE(conn.is_connected());
}

TEST_F(dispatch_test, test_async_send_with_reply) {
    auto conn = connection::open_session_bus(*s_io_context);
    EXPECT_TRUE(conn.start());
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(conn.request_name("org.test.Dispatch"));
    auto msg = message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                       "org.freedesktop.DBus", "GetId");
    auto future = conn.async_send_with_reply(std::move(msg), mc::milliseconds(1000));
    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;
    message reply_msg;
    future.then([&cv, &done, &reply_msg](const message& reply) {
        reply_msg = reply;
        done = true;
        cv.notify_one();
    });
    std::unique_lock lock(mutex);
    cv.wait_for(lock, std::chrono::milliseconds(2000));
    EXPECT_TRUE(done);
    EXPECT_TRUE(reply_msg.is_valid());
    conn.disconnect();
}

TEST_F(dispatch_test, test_multiple_concurrent_calls) {
    auto conn = connection::open_session_bus(*s_io_context);
    EXPECT_TRUE(conn.start());
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(conn.request_name("org.test.Dispatch"));
    
    const int num_calls = 5;
    std::vector<connection::future<message>> futures;
    for (int i = 0; i < num_calls; ++i) {
        auto msg = message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                           "org.freedesktop.DBus", "GetId");
        futures.push_back(conn.async_send_with_reply(std::move(msg), mc::milliseconds(1000)));
    }
    
    std::mutex mutex;
    std::condition_variable cv;
    int completed = 0;
    std::vector<message> replies(num_calls);
    
    for (size_t i = 0; i < futures.size(); ++i) {
        futures[i].then([&mutex, &cv, &completed, &replies, i](const message& reply) {
            std::lock_guard<std::mutex> lock(mutex);
            replies[i] = reply;
            completed++;
            cv.notify_one();
        });
    }
    
    std::unique_lock lock(mutex);
    cv.wait_for(lock, std::chrono::milliseconds(3000), [&completed, num_calls]() {
        return completed == num_calls;
    });
    
    EXPECT_EQ(completed, num_calls);
    for (const auto& reply : replies) {
        EXPECT_TRUE(reply.is_valid());
    }
    conn.disconnect();
}

TEST_F(dispatch_test, test_dispatch_while_receiving) {
    auto conn = connection::open_session_bus(*s_io_context);
    EXPECT_TRUE(conn.start());
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(conn.request_name("org.test.Dispatch"));
    
    auto msg = message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                       "org.freedesktop.DBus", "ListNames");
    auto future = conn.async_send_with_reply(std::move(msg), mc::milliseconds(1000));
    
    std::thread dispatch_thread([&conn]() {
        for (int i = 0; i < 10; ++i) {
            conn.dispatch();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;
    message reply_msg;
    future.then([&cv, &done, &reply_msg](const message& reply) {
        reply_msg = reply;
        done = true;
        cv.notify_one();
    });
    
    std::unique_lock lock(mutex);
    cv.wait_for(lock, std::chrono::milliseconds(2000));
    
    dispatch_thread.join();
    EXPECT_TRUE(done);
    EXPECT_TRUE(reply_msg.is_valid());
    conn.disconnect();
}

TEST_F(dispatch_test, test_pending_call_already_completed) {
    auto conn = connection::open_session_bus(*s_io_context);
    conn.start();
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(conn.request_name("org.test.Dispatch"));

    auto msg = message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                       "org.freedesktop.DBus", "GetId");
    auto reply = conn.send_with_reply(std::move(msg), mc::milliseconds(1000));
    EXPECT_TRUE(reply.is_valid());

    conn.disconnect();
}

TEST_F(dispatch_test, test_pending_call_move_operations) {
    auto conn = connection::open_session_bus(*s_io_context);
    conn.start();
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(conn.request_name("org.test.Dispatch"));

    auto msg1 = message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                         "org.freedesktop.DBus", "GetId");
    auto future1 = conn.async_send_with_reply(std::move(msg1), mc::milliseconds(1000));

    auto msg2 = message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                        "org.freedesktop.DBus", "GetId");
    auto future2 = conn.async_send_with_reply(std::move(msg2), mc::milliseconds(1000));

    std::mutex mutex;
    std::condition_variable cv;
    int completed = 0;

    future1.then([&mutex, &cv, &completed](const message&) {
        std::lock_guard<std::mutex> lock(mutex);
        completed++;
        cv.notify_one();
    });

    future2.then([&mutex, &cv, &completed](const message&) {
        std::lock_guard<std::mutex> lock(mutex);
        completed++;
        cv.notify_one();
    });

    std::unique_lock lock(mutex);
    cv.wait_for(lock, std::chrono::milliseconds(2000), [&completed]() {
        return completed == 2;
    });

    EXPECT_EQ(completed, 2);
    conn.disconnect();
}

TEST_F(dispatch_test, test_pending_call_stop_before_reply) {
    auto conn = connection::open_session_bus(*s_io_context);
    conn.start();
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(conn.request_name("org.test.Dispatch"));

    auto msg = message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                       "org.freedesktop.DBus", "GetId");
    auto future = conn.async_send_with_reply(std::move(msg), mc::milliseconds(5000));

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    conn.disconnect();

    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;
    message reply_msg;

    future.then([&mutex, &cv, &done, &reply_msg](const message& reply) {
        std::lock_guard<std::mutex> lock(mutex);
        reply_msg = reply;
        done = true;
        cv.notify_one();
    });

    std::unique_lock lock(mutex);
    cv.wait_for(lock, std::chrono::milliseconds(1000), [&done]() {
        return done;
    });
}

TEST_F(dispatch_test, test_timeout_handling) {
    auto conn = connection::open_session_bus(*s_io_context);
    conn.start();
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(conn.request_name("org.test.Dispatch"));

    for (int i = 0; i < 5; ++i) {
        auto msg = message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                           "org.freedesktop.DBus", "GetId");
        auto reply = conn.send_with_reply(std::move(msg), mc::milliseconds(1000));
        EXPECT_TRUE(reply.is_valid());
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    conn.disconnect();
}

TEST_F(dispatch_test, test_timeout_with_long_interval) {
    auto conn = connection::open_session_bus(*s_io_context);
    conn.start();
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(conn.request_name("org.test.Dispatch"));

    auto msg = message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                       "org.freedesktop.DBus", "ListNames");
    auto reply = conn.send_with_reply(std::move(msg), mc::milliseconds(2000));
    EXPECT_TRUE(reply.is_valid());

    conn.disconnect();
}

TEST_F(dispatch_test, test_pending_call_move_assignment) {
    pending_call first(nullptr, pending_call::reply_cb{});
    pending_call second(nullptr, pending_call::reply_cb{});

    // 触发移动赋值运算符
    second = std::move(first);

    // 再次执行 release，确保不会重复释放
    second.release();
}

TEST_F(dispatch_test, test_add_and_remove_match_rules) {
    auto conn = connection::open_session_bus(*s_io_context);
    ASSERT_TRUE(conn.start());
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(conn.request_name("org.test.Dispatch"));

    match_rule rule1 = match_rule::new_signal("NameOwnerChanged", DBUS_INTERFACE_DBUS);
    match_rule rule2 = match_rule::new_signal("PropertiesChanged", DBUS_PROPERTIES_INTERFACE);

    std::atomic<int> hit1{0};
    std::atomic<int> hit2{0};

    conn.add_match(rule1, [&hit1](message&) { hit1.fetch_add(1); }, 1001);
    conn.add_match(rule2, [&hit2](message&) { hit2.fetch_add(1); }, 1002);

    conn.remove_match(1001);
    conn.remove_match(1002);

    conn.remove_match(1001);
    conn.remove_match(9999);

    conn.disconnect();
}

