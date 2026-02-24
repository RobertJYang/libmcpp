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

#include "mc/introspection/introspection_types.h"
#include "mc/mdb/proxy_object.h"

using namespace mc;
// 使用全局命名空间的 method_info 类型，避免与反射库中的 method 冲突
using ::method_info;

class proxy_object_test : public mc::test::TestBase {
protected:
    static mc::dbus::connection test_conn;
    static mc::dbus::sd_bus*    test_bus;
    static bool                 use_stub;

    static void SetUpTestSuite() {
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

    static void TearDownTestSuite() {
        delete test_bus;
        test_bus = nullptr;

        mc::test::TestBase::TearDownTestSuite();
    }

    void SetUp() override {
        mc::test::TestBase::SetUp();
        // 不再跳过测试，即使使用 stub connection 也继续测试
    }
};

mc::dbus::connection proxy_object_test::test_conn;
mc::dbus::sd_bus*    proxy_object_test::test_bus = nullptr;
bool                 proxy_object_test::use_stub  = false;

// 测试 proxy_object 构造函数（使用 raw pointer）
TEST_F(proxy_object_test, constructor_with_raw_pointer) {
    // 创建简单的 interface_info
    interface_info iface_info;

    // 创建 proxy_object
    proxy_object obj(test_bus, "org.test.Service", "/org/test/Path", "org.test.Interface", iface_info);

    EXPECT_EQ(obj.service(), "org.test.Service");
    EXPECT_EQ(obj.path(), "/org/test/Path");
    EXPECT_EQ(obj.interface(), "org.test.Interface");
}

// 测试 proxy_object 构造函数（使用 unique_ptr）
TEST_F(proxy_object_test, constructor_with_unique_ptr) {
    // 创建新的 sd_bus
    auto bus = std::make_unique<mc::dbus::sd_bus>(test_bus->get_connection(), false);

    // 创建简单的 interface_info
    interface_info iface_info;

    // 创建 proxy_object
    proxy_object obj(std::move(bus), "org.test.Service", "/org/test/Path", "org.test.Interface", iface_info);

    EXPECT_EQ(obj.service(), "org.test.Service");
    EXPECT_EQ(obj.path(), "/org/test/Path");
    EXPECT_EQ(obj.interface(), "org.test.Interface");
}

// 测试 has_property 和 has_method（空接口）
TEST_F(proxy_object_test, has_property_and_method_empty) {
    interface_info iface_info;

    proxy_object obj(test_bus, "org.test.Service", "/org/test/Path", "org.test.Interface", iface_info);

    EXPECT_FALSE(obj.has_property("NonExistentProperty"));
    EXPECT_FALSE(obj.has_method("NonExistentMethod"));
}

// 测试 has_property 和 has_method（有属性和方法）
TEST_F(proxy_object_test, has_property_and_method_with_data) {
    interface_info iface_info;

    // 添加属性
    property_info prop_info;
    prop_info.type                        = "s";
    prop_info.access                      = "readwrite";
    iface_info.properties["TestProperty"] = prop_info;

    // 添加方法
    ::method_info method;
    method.args.push_back({"arg1", "s", "in", std::nullopt});
    iface_info.methods["TestMethod"] = method;

    proxy_object obj(test_bus, "org.test.Service", "/org/test/Path", "org.test.Interface", iface_info);

    EXPECT_TRUE(obj.has_property("TestProperty"));
    EXPECT_FALSE(obj.has_property("NonExistentProperty"));

    EXPECT_TRUE(obj.has_method("TestMethod"));
    EXPECT_FALSE(obj.has_method("NonExistentMethod"));
}

// 测试 get_property_info 和 get_method_info
TEST_F(proxy_object_test, get_property_and_method_info) {
    interface_info iface_info;

    // 添加属性
    property_info prop_info;
    prop_info.type                        = "s";
    prop_info.access                      = "readwrite";
    iface_info.properties["TestProperty"] = prop_info;

    // 添加方法
    ::method_info method;
    method.args.push_back({"arg1", "s", "in", std::nullopt});
    iface_info.methods["TestMethod"] = method;

    proxy_object obj(test_bus, "org.test.Service", "/org/test/Path", "org.test.Interface", iface_info);

    const property_info* prop = obj.get_property_info("TestProperty");
    ASSERT_NE(prop, nullptr);
    EXPECT_EQ(prop->type, "s");
    EXPECT_EQ(prop->access, "readwrite");

    const ::method_info* method_ptr = obj.get_method_info("TestMethod");
    ASSERT_NE(method_ptr, nullptr);
    EXPECT_EQ(method_ptr->args.size(), 1u);
    EXPECT_EQ(method_ptr->args[0].name, "arg1");
    EXPECT_EQ(method_ptr->args[0].type, "s");

    // 测试不存在的属性和方法
    EXPECT_EQ(obj.get_property_info("NonExistentProperty"), nullptr);
    EXPECT_EQ(obj.get_method_info("NonExistentMethod"), nullptr);
}

// 测试 get_interface_info
TEST_F(proxy_object_test, get_interface_info) {
    interface_info iface_info;

    proxy_object obj(test_bus, "org.test.Service", "/org/test/Path", "org.test.Interface", iface_info);

    const interface_info& info = obj.get_interface_info();
    // interface_info 没有 name 成员，名称通过 proxy_object 的 interface() 方法获取
    EXPECT_EQ(obj.interface(), "org.test.Interface");
    EXPECT_EQ(info.properties.size(), 0u);
    EXPECT_EQ(info.methods.size(), 0u);
}

// 测试 bus() 方法
TEST_F(proxy_object_test, bus_method) {
    interface_info iface_info;

    proxy_object obj(test_bus, "org.test.Service", "/org/test/Path", "org.test.Interface", iface_info);

    // bus() 应该返回 connection 的引用
    auto& conn = obj.bus();
    // 如果使用 stub connection，is_connected() 可能返回 false，这是正常的
    if (!use_stub) {
        EXPECT_TRUE(conn.is_connected());
    } else {
        // 使用 stub 时，只验证方法可以调用，不验证连接状态
        EXPECT_FALSE(conn.is_connected());
    }
}
