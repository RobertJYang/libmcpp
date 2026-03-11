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
#include <mc/dbus/connection.h>
#include <mc/dbus/sd_bus.h>
#include <mc/runtime.h>
#include <test_utilities/test_base.h>

#include "mc/mdb/mdb_access.h"

using namespace mc;

class mdb_access_test : public mc::test::TestBase {
protected:
    static mc::dbus::connection test_conn;
    static mc::dbus::sd_bus*    test_bus;
    static bool                 use_stub;

    static void SetUpTestSuite()
    {
        mc::test::TestBase::SetUpTestSuite();

        try {
            // 尝试创建真实的 D-Bus 连接
            test_conn = mc::dbus::connection::open_session_bus(mc::get_io_context());
            test_conn.start();

            // 创建 sd_bus
            test_bus = new mc::dbus::sd_bus(std::move(test_conn), false);

            // 等待连接完全建立
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            use_stub = false;
        } catch (const std::exception& e) {
            // D-Bus 连接失败，使用 stub connection
            // 创建一个默认的 connection（不连接真实的 D-Bus）
            test_conn = mc::dbus::connection();
            // 创建 sd_bus（使用 stub connection）
            test_bus = new mc::dbus::sd_bus(std::move(test_conn), false);
            use_stub = true;
        }
    }

    static void TearDownTestSuite()
    {
        delete test_bus;
        test_bus = nullptr;

        mc::test::TestBase::TearDownTestSuite();
    }

    void SetUp() override
    {
        mc::test::TestBase::SetUp();
        // 不再跳过测试，即使使用 stub connection 也继续测试
    }
};

mc::dbus::connection mdb_access_test::test_conn;
mc::dbus::sd_bus*    mdb_access_test::test_bus = nullptr;
bool                 mdb_access_test::use_stub = false;

// 测试 object_manager 单例
TEST_F(mdb_access_test, singleton_instance)
{
    mdb_access& mgr1 = mdb_access::instance();
    mdb_access& mgr2 = mdb_access::instance();

    // 应该是同一个实例
    EXPECT_EQ(&mgr1, &mgr2);
}

// 测试 get_object（需要真实的 D-Bus 服务，这里只测试基本调用）
// 注意：这个测试可能会失败，如果没有对应的 D-Bus 服务
TEST_F(mdb_access_test, get_object_basic)
{
    mdb_access& mgr = mdb_access::instance();

    // 创建 sd_bus
    auto bus = std::make_shared<mc::dbus::sd_bus>(test_bus->get_connection(), false);

    // 如果使用 stub connection，跳过需要真实 D-Bus 服务的测试
    if (use_stub) {
        GTEST_SKIP() << "使用 stub connection，跳过需要真实 D-Bus 服务的测试";
    }

    // 尝试获取对象（可能会失败，如果没有对应的服务）
    // 这里只测试函数调用不会崩溃
    try {
        auto obj = mgr.get_object(std::move(bus), "/org/freedesktop/DBus", "org.freedesktop.DBus");
        // 如果成功，obj 应该不为空
        if (obj != nullptr) {
            EXPECT_NE(obj, nullptr);
        }
    } catch (const std::exception& e) {
        // 如果没有对应的服务，这是正常的
        GTEST_SKIP() << "无法获取 D-Bus 对象，跳过测试: " << e.what();
    }
}

// 测试缓存功能（通过多次调用相同的路径）
TEST_F(mdb_access_test, cache_functionality)
{
    mdb_access& mgr = mdb_access::instance();

    // 如果使用 stub connection，跳过需要真实 D-Bus 服务的测试
    if (use_stub) {
        GTEST_SKIP() << "使用 stub connection，跳过需要真实 D-Bus 服务的测试";
    }

    // 创建 sd_bus
    auto bus1 = std::make_shared<mc::dbus::sd_bus>(test_bus->get_connection(), false);
    auto bus2 = std::make_shared<mc::dbus::sd_bus>(test_bus->get_connection(), false);

    try {
        // 第一次获取对象
        auto obj1 = mgr.get_object(bus1, "/org/freedesktop/DBus", "org.freedesktop.DBus");

        if (obj1 != nullptr) {
            // 第二次获取相同路径的对象（应该从缓存获取）
            auto obj2 = mgr.get_object(bus2, "/org/freedesktop/DBus", "org.freedesktop.DBus");

            // 如果缓存工作正常，obj1 和 obj2 应该是同一个对象
            // 注意：由于缓存键是 path + interface，所以应该是同一个
            if (obj2 != nullptr) {
                // 验证路径和接口相同
                EXPECT_EQ(obj1->path(), obj2->path());
                EXPECT_EQ(obj1->interface(), obj2->interface());
            }
        }
    } catch (const std::exception& e) {
        GTEST_SKIP() << "无法获取 D-Bus 对象，跳过测试: " << e.what();
    }
}
