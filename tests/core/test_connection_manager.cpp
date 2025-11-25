/*
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
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
#include <mc/core/object.h>
#include <mc/signal_slot.h>
#include <test_utilities/test_base.h>

#include <set>

#include "core/include/connection_manager.h"

using namespace mc;
using namespace mc::core;

class connection_manager_test : public mc::test::TestBase {
protected:
    void SetUp() override {
        TestBase::SetUp();
    }

    void TearDown() override {
        manager.clear();
        TestBase::TearDown();
    }

    mc::core::connection_manager manager;
    mc::signal<void(int)> test_signal;
};

// 测试添加连接
TEST_F(connection_manager_test, add_connection) {
    bool called = false;
    auto conn   = test_signal.connect([&called](int value) { called = true; });

    auto id = manager.add_connection(&test_signal, conn);
    EXPECT_NE(id, INVALID_CONNECTION_ID);
}

// 测试添加多个连接
TEST_F(connection_manager_test, add_multiple_connections) {
    bool called1 = false;
    bool called2 = false;

    auto conn1 = test_signal.connect([&called1](int) { called1 = true; });
    auto conn2 = test_signal.connect([&called2](int) { called2 = true; });

    auto id1 = manager.add_connection(&test_signal, conn1);
    auto id2 = manager.add_connection(&test_signal, conn2);

    EXPECT_NE(id1, INVALID_CONNECTION_ID);
    EXPECT_NE(id2, INVALID_CONNECTION_ID);
    EXPECT_NE(id1, id2);
}

// 测试添加连接时指定ID
TEST_F(connection_manager_test, add_connection_with_id) {
    bool called = false;
    auto conn   = test_signal.connect([&called](int) { called = true; });

    connection_id_type custom_id = 100;
    auto               id        = manager.add_connection(&test_signal, conn, custom_id);
    EXPECT_EQ(id, custom_id);
}

// 测试添加连接时使用已存在的ID（应该抛出异常）
TEST_F(connection_manager_test, add_connection_duplicate_id) {
    bool called = false;
    auto conn   = test_signal.connect([&called](int) { called = true; });

    connection_id_type id = 100;
    manager.add_connection(&test_signal, conn, id);

    auto conn2 = test_signal.connect([](int) {});
    EXPECT_THROW(manager.add_connection(&test_signal, conn2, id), mc::invalid_arg_exception);
}

// 测试移除连接
TEST_F(connection_manager_test, remove_connection) {
    bool called = false;
    auto conn   = test_signal.connect([&called](int) { called = true; });

    auto id = manager.add_connection(&test_signal, conn);
    manager.remove_connection(id);

    // 验证连接已被移除（通过尝试再次移除应该不会出错）
    manager.remove_connection(id); // 应该不会抛出异常
}

// 测试移除不存在的连接（合并了 remove_nonexistent_connection 和 remove_nonexistent_connection_id）
TEST_F(connection_manager_test, remove_nonexistent_connection) {
    // 移除不存在的连接不应该抛出异常
    EXPECT_NO_THROW(manager.remove_connection(9999));
    EXPECT_NO_THROW(manager.remove_connection(INVALID_CONNECTION_ID));
}

// 测试移除信号的所有连接
TEST_F(connection_manager_test, remove_connections) {
    bool called1 = false;
    bool called2 = false;

    auto conn1 = test_signal.connect([&called1](int) { called1 = true; });
    auto conn2 = test_signal.connect([&called2](int) { called2 = true; });

    manager.add_connection(&test_signal, conn1);
    manager.add_connection(&test_signal, conn2);

    auto count = manager.remove_connections(&test_signal);
    EXPECT_EQ(count, 2);
}

// 测试移除不存在的信号连接
TEST_F(connection_manager_test, remove_nonexistent_signal_connections) {
    mc::signal<void(int)> other_signal;
    auto            count = manager.remove_connections(&other_signal);
    EXPECT_EQ(count, 0);
}

// 测试清空所有连接
TEST_F(connection_manager_test, clear) {
    bool called1 = false;
    bool called2 = false;

    mc::signal<void(int)> signal1;
    mc::signal<void(int)> signal2;

    auto conn1 = signal1.connect([&called1](int) { called1 = true; });
    auto conn2 = signal2.connect([&called2](int) { called2 = true; });

    manager.add_connection(&signal1, conn1);
    manager.add_connection(&signal2, conn2);

    manager.clear();

    // 验证连接已被清空（通过移除应该不会出错）
    manager.remove_connection(1);
    EXPECT_EQ(manager.remove_connections(&signal1), 0);
}

// 测试自动生成ID的唯一性
TEST_F(connection_manager_test, auto_generated_ids_unique) {
    std::set<connection_id_type> ids;

    for (int i = 0; i < 100; ++i) {
        bool called = false;
        auto conn   = test_signal.connect([&called](int) { called = true; });
        auto id     = manager.add_connection(&test_signal, conn);
        EXPECT_TRUE(ids.insert(id).second) << "ID应该唯一: " << id;
    }
}

// 测试多个信号的不同连接
TEST_F(connection_manager_test, multiple_signals) {
    mc::signal<void(int)> signal1;
    mc::signal<void(int, int)> signal2;

    bool called1 = false;
    bool called2 = false;

    auto conn1 = signal1.connect([&called1](int) { called1 = true; });
    auto conn2 = signal2.connect([&called2](int, int) { called2 = true; });

    auto id1 = manager.add_connection(&signal1, conn1);
    auto id2 = manager.add_connection(&signal2, conn2);

    EXPECT_NE(id1, INVALID_CONNECTION_ID);
    EXPECT_NE(id2, INVALID_CONNECTION_ID);
    EXPECT_NE(id1, id2);

    // 移除signal1的连接，不应该影响signal2
    manager.remove_connections(&signal1);
    EXPECT_EQ(manager.remove_connections(&signal2), 1);
}
