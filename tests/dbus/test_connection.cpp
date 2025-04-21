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
    }

    static void TearDownTestSuite() {
        s_io_context.reset();
        if (s_dbus_manager) {
            s_dbus_manager->stop();
            s_dbus_manager.reset();
        }
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
};

// 初始化静态成员变量
std::unique_ptr<mc::test::dbus_daemon_manager> connection_test::s_dbus_manager;
std::shared_ptr<boost::asio::io_context>       connection_test::s_io_context;

TEST_F(connection_test, ConnectionState) {
    auto io_context = get_io_context();
    auto conn       = connection::create(*io_context);
    EXPECT_EQ(conn->get_state(), connection_state::disconnected);

    boost::system::error_code error_code;
    conn->connect("unix:path=" + get_socket_path().string(), [&error_code](const auto& ec) {
        error_code = ec;
    });
    EXPECT_EQ(conn->get_state(), connection_state::connecting);
    io_context->run();
    EXPECT_TRUE(!error_code);
    EXPECT_EQ(conn->get_state(), connection_state::connected);
}

// 添加更多测试用例，它们共享同一个 DBus 守护进程
TEST_F(connection_test, MultipleConnections) {
    auto io_context = get_io_context();
    auto conn1      = connection::create(*io_context);
    auto conn2      = connection::create(*io_context);

    boost::system::error_code error_code1;
    boost::system::error_code error_code2;

    conn1->connect("unix:path=" + get_socket_path().string(), [&error_code1](const auto& ec) {
        error_code1 = ec;
    });

    conn2->connect("unix:path=" + get_socket_path().string(), [&error_code2](const auto& ec) {
        error_code2 = ec;
    });

    io_context->run();

    EXPECT_TRUE(!error_code1);
    EXPECT_TRUE(!error_code2);
    EXPECT_EQ(conn1->get_state(), connection_state::connected);
    EXPECT_EQ(conn2->get_state(), connection_state::connected);
}
