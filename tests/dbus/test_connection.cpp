/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
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

#include <dbus/dbus.h>
#include <mc/dbus/connection.h>
#include <mc/dbus/message.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <unistd.h>

#include <test_utilities/test_base.h>

using namespace mc::dbus;

class connection_test : public mc::test::TestWithDbusDaemon {
protected:
    connection_test() {
    }

    static void SetUpTestSuite() {
        mc::log::default_logger().set_level(mc::log::level::debug);

        TestWithDbusDaemon::SetUpTestSuite();

        s_io_context = std::make_shared<boost::asio::io_context>();
        s_thread     = std::make_unique<std::thread>([io_context = s_io_context]() {
            auto work = boost::asio::make_work_guard(*io_context);
            io_context->run();
        });
    }

    static void TearDownTestSuite() {
        TestWithDbusDaemon::TearDownTestSuite();

        s_io_context->stop();
        s_thread->join();
        s_thread.reset();
        s_io_context.reset();
    }

    void SetUp() override {
        s_io_context->restart();
    }

    void TearDown() override {
    }

    std::shared_ptr<boost::asio::io_context> get_io_context() {
        return s_io_context;
    }

    std::string get_dbus_address() {
        return get_dbus_daemon().get_address();
    }

    std::filesystem::path get_socket_path() {
        return get_dbus_daemon().get_socket_path();
    }

    static std::shared_ptr<boost::asio::io_context> s_io_context;
    static std::unique_ptr<std::thread>             s_thread;
};

std::shared_ptr<boost::asio::io_context> connection_test::s_io_context;
std::unique_ptr<std::thread>             connection_test::s_thread;

TEST_F(connection_test, test_call_method_error) {
    auto conn = mc::dbus::connection::open_session_bus(*s_io_context);
    conn.start();
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(conn.request_name("org.test.Connection"));

    auto msg   = mc::dbus::message::new_method_call("org.test.Connection", "/org/test/Connection",
                                                    "org.test.Connection", "Hello");
    auto reply = conn.send_with_reply(std::move(msg), mc::milliseconds(1000));
    ASSERT_TRUE(reply.is_valid() && reply.is_error());
}

TEST_F(connection_test, test_list_names) {
    auto conn = mc::dbus::connection::open_session_bus(*s_io_context);
    conn.start();
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(conn.request_name("org.test.Connection"));

    auto msg   = mc::dbus::message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                                    "org.freedesktop.DBus", "ListNames");
    auto reply = conn.send_with_reply(std::move(msg), mc::milliseconds(1000));
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());

    std::set<std::string> names;
    reply >> names;
    EXPECT_GE(names.count("org.test.Connection"), 1);
    conn.disconnect();
}

TEST_F(connection_test, test_async_list_names) {
    auto conn = mc::dbus::connection::open_session_bus(*s_io_context);
    conn.start();
    ASSERT_TRUE(conn.is_connected());
    EXPECT_TRUE(conn.request_name("org.test.Connection"));

    auto msg    = mc::dbus::message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                                     "org.freedesktop.DBus", "ListNames");
    auto future = conn.async_send_with_reply(std::move(msg), mc::milliseconds(1000));

    std::mutex              mutex;
    std::condition_variable cv;

    mc::dbus::message reply_msg;
    future.then([&cv, &reply_msg](const mc::dbus::message& reply) {
        reply_msg = reply;
        cv.notify_one();
    }).catch_error([&cv, &reply_msg](const std::exception& e) {
        reply_msg = mc::dbus::message::new_error_message(mc::dbus::error_names::io_error, e.what());
        cv.notify_one();
        return 0;
    });

    std::unique_lock lock(mutex);
    cv.wait(lock, [&reply_msg]() {
        return reply_msg.is_valid();
    });

    ASSERT_TRUE(reply_msg.is_method_return());
    auto names = reply_msg.as<std::set<std::string>>();
    EXPECT_GE(names.count("org.test.Connection"), 1);
}

// 测试连接断开
TEST_F(connection_test, test_disconnect) {
    mc::dbus::connection conn;
    {
        auto tmp = mc::dbus::connection::open_session_bus(*s_io_context);
        tmp.start();
        ASSERT_TRUE(tmp.is_connected());
        EXPECT_TRUE(tmp.request_name("org.test.Connection"));

        auto msg = mc::dbus::message::new_method_call(
            "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");
        auto future = tmp.async_send_with_reply(std::move(msg), mc::milliseconds(1000));
        conn        = tmp;
        tmp.disconnect();
    }
    ASSERT_TRUE(!conn.is_connected());
    conn.disconnect();
}
