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

#include <test_utilities/dbus_daemon_manager.h>
#include <test_utilities/test_base.h>

using namespace mc::dbus;

class connection_test : public mc::test::TestBase {
protected:
    static void SetUpTestSuite() {
        s_dbus_manager = std::make_unique<mc::test::dbus_daemon_manager>();
        bool success   = s_dbus_manager->start();
        ASSERT_TRUE(success) << "启动 DBus 守护进程失败";

        s_io_context = std::make_shared<boost::asio::io_context>();
        s_thread     = std::make_unique<std::thread>([io_context = s_io_context]() {
            auto work = boost::asio::make_work_guard(*io_context);
            io_context->run();
        });
    }

    static void TearDownTestSuite() {
        if (s_dbus_manager) {
            s_dbus_manager->stop();
            s_dbus_manager.reset();
        }
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
        return s_dbus_manager->get_address();
    }

    std::filesystem::path get_socket_path() {
        return s_dbus_manager->get_socket_path();
    }

    // 静态成员变量，由整个测试类共享
    static std::unique_ptr<mc::test::dbus_daemon_manager> s_dbus_manager;
    static std::shared_ptr<boost::asio::io_context>       s_io_context;
    static std::unique_ptr<std::thread>                   s_thread;
};

// 初始化静态成员变量
std::unique_ptr<mc::test::dbus_daemon_manager> connection_test::s_dbus_manager;
std::shared_ptr<boost::asio::io_context>       connection_test::s_io_context;
std::unique_ptr<std::thread>                   connection_test::s_thread;

TEST_F(connection_test, test_call_method_error) {
    connection_ptr conn = mc::dbus::connection::open_session_bus(*s_io_context);
    conn->start();
    ASSERT_TRUE(conn->is_connected());
    EXPECT_TRUE(conn->request_name("org.test.Connection"));

    auto msg   = mc::dbus::message::new_method_call("org.test.Connection", "/org/test/Connection",
                                                    "org.test.Connection", "Hello");
    auto reply = conn->send_with_reply(std::move(msg), mc::milliseconds(1000));
    ASSERT_TRUE(reply.is_valid() && reply.is_error());
    EXPECT_EQ(reply.get_error_name(), error_names::unknown_method);
}

TEST_F(connection_test, test_list_names) {
    connection_ptr conn = mc::dbus::connection::open_session_bus(*s_io_context);
    conn->start();
    ASSERT_TRUE(conn->is_connected());
    EXPECT_TRUE(conn->request_name("org.test.Connection"));

    auto msg   = mc::dbus::message::new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                                    "org.freedesktop.DBus", "ListNames");
    auto reply = conn->send_with_reply(std::move(msg), mc::milliseconds(1000));
    ASSERT_TRUE(reply.is_valid() && reply.is_method_return());

    std::vector<std::string> names;
    reply.reader() >> names;
    EXPECT_GE(names.size(), 1);
}
