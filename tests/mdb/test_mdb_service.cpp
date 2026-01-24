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
#include <mc/mdb_service.h>
#include <mc/runtime.h>
#include <test_utilities/test_base.h>

using namespace mc;

class mdb_service_test : public mc::test::TestBase {
protected:
    static mc::dbus::connection test_conn;
    static mc::dbus::sd_bus*    test_bus;

    static void SetUpTestSuite() {
        mc::test::TestBase::SetUpTestSuite();

        // 创建 D-Bus 连接
        test_conn = mc::dbus::connection::open_session_bus(mc::get_io_context());
        test_conn.start();

        // 创建 sd_bus
        test_bus = new mc::dbus::sd_bus(std::move(test_conn), false);

        // 等待连接完全建立
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    static void TearDownTestSuite() {
        // 清理缓存
        mc::mdb::service::clear_cache();

        // 清理 sd_bus
        delete test_bus;
        test_bus = nullptr;

        mc::test::TestBase::TearDownTestSuite();
    }

    void SetUp() override {
        mc::test::TestBase::SetUp();
        // 每个测试前清理缓存
        mc::mdb::service::clear_cache();
    }
};

mc::dbus::connection mdb_service_test::test_conn;
mc::dbus::sd_bus*    mdb_service_test::test_bus = nullptr;

// 测试设置和获取最大缓存数量
TEST_F(mdb_service_test, set_and_get_max_cache_size) {
    // 默认应该是 400
    EXPECT_EQ(mc::mdb::service::get_max_cache_size(), 400);

    // 设置新的最大值
    mc::mdb::service::set_max_cache_size(100);
    EXPECT_EQ(mc::mdb::service::get_max_cache_size(), 100);

    // 恢复默认值
    mc::mdb::service::set_max_cache_size(400);
    EXPECT_EQ(mc::mdb::service::get_max_cache_size(), 400);
}

