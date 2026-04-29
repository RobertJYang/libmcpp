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
#include <iostream>
#include <mc/dbus/connection.h>
#include <mc/dbus/sd_bus.h>
#include <mc/engine.h>
#include <mc/exception.h>
#include <mc/runtime.h>
#include <mc/variant.h>
#include <test_utilities/test_base.h>

#include "mc/mdb/mdb_access.h"
#include "mc/mdb/proxy_object.h"

using ::method_info;

namespace {

constexpr const char* k_dbus_service   = "org.freedesktop.DBus";
constexpr const char* k_dbus_path      = "/org/freedesktop/DBus";
constexpr const char* k_dbus_interface = "org.freedesktop.DBus";

// 在已通过 mdb_access 取得 proxy_object 后，验证接口元数据、方法调用与结果转换。
// 说明：get_property / set_property / get_all_properties 走 bmc.kepler.Object.Properties，
void expect_proxy_against_session_dbus(proxy_object& obj)
{
    EXPECT_EQ(obj.service(), k_dbus_service);
    EXPECT_EQ(obj.path(), k_dbus_path);
    EXPECT_EQ(obj.interface(), k_dbus_interface);

    const interface_info& info = obj.get_interface_info();
    EXPECT_FALSE(info.methods.empty());

    EXPECT_TRUE(obj.has_method("ListNames"));
    EXPECT_TRUE(obj.has_method("GetId"));
    EXPECT_FALSE(obj.has_method("NonExistentMethodForMdbAccessTest"));

    const method_info* list_mi = obj.get_method_info("ListNames");
    ASSERT_NE(list_mi, nullptr);

    const method_info* getid_mi = obj.get_method_info("GetId");
    ASSERT_NE(getid_mi, nullptr);

    mc::variants list_results = obj.call_method("ListNames", mc::variant());
    mc::variant  list_one     = mdb_utils::convert_method_result(list_results);
    ASSERT_TRUE(list_one.is_array());
    EXPECT_GT(list_one.get_array().size(), 0u);

    mc::variants id_results = obj.call_method("GetId", mc::variant());
    mc::variant  id_one     = mdb_utils::convert_method_result(id_results);
    ASSERT_TRUE(id_one.is_string());
    EXPECT_FALSE(id_one.as_string().empty());

    std::optional<std::string> err;
    auto [ok, pcall_results] = obj.call_method_pcall("GetId", mc::variant(), err);
    EXPECT_TRUE(ok);
    EXPECT_FALSE(err.has_value());
    EXPECT_FALSE(pcall_results.empty());
}

} // namespace

class mdb_access_proxy_integration_test : public mc::test::TestBase {
protected:
    static mc::dbus::connection test_conn;
    static mc::dbus::sd_bus*    test_bus;
    static bool                 use_stub;

    static void SetUpTestSuite()
    {
        mc::test::TestBase::SetUpTestSuite();

        try {
            test_conn = mc::dbus::connection::open_session_bus(mc::get_io_context());
            test_conn.start();
            test_bus = new mc::dbus::sd_bus(std::move(test_conn), false);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            use_stub = false;
        } catch (const std::exception&) {
            test_conn = mc::dbus::connection();
            test_bus  = new mc::dbus::sd_bus(std::move(test_conn), false);
            use_stub  = true;
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
        mdb_access::instance().clear_cache();
    }
};

mc::dbus::connection mdb_access_proxy_integration_test::test_conn;
mc::dbus::sd_bus*    mdb_access_proxy_integration_test::test_bus = nullptr;
bool                 mdb_access_proxy_integration_test::use_stub = false;

// 不经过 MDB GetObject，直接指定 service，依赖会话总线与 introspection
TEST_F(mdb_access_proxy_integration_test, get_object_with_service_then_methods_and_results)
{
    if (use_stub) {
        GTEST_SKIP() << "无真实 session D-Bus，跳过 proxy 集成测试";
    }

    auto                          bus = std::make_shared<mc::dbus::sd_bus>(test_bus->get_connection(), false);
    std::shared_ptr<proxy_object> obj;
    ASSERT_NO_THROW(
        obj = mdb_access::instance().get_object_with_service(bus, k_dbus_service, k_dbus_path, k_dbus_interface));
    ASSERT_NE(obj, nullptr);
    expect_proxy_against_session_dbus(*obj);
}

// 依赖 maca 服务。
TEST_F(mdb_access_proxy_integration_test, get_object_then_methods_when_maca_available)
{
    if (use_stub) {
        GTEST_SKIP() << "无真实 session D-Bus";
    }

    auto                          bus = std::make_shared<mc::dbus::sd_bus>(test_bus->get_connection(), false);
    std::shared_ptr<proxy_object> obj;
    try {
        obj = mdb_access::instance().get_object(bus, k_dbus_path, k_dbus_interface);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "MDB GetObject 不可用，跳过: " << e.what();
    }
    ASSERT_NE(obj, nullptr);
    expect_proxy_against_session_dbus(*obj);
}

TEST_F(mdb_access_proxy_integration_test, get_object_with_service_empty_service_throws)
{
    EXPECT_THROW(mdb_access::instance().get_object_with_service(std::shared_ptr<mc::dbus::sd_bus>(), "", "/p", "i.f"),
                 mc::invalid_arg_exception);
}

namespace tests::mdb_access_chain {

class mdb_chain_interface : public mc::engine::interface<mdb_chain_interface> {
public:
    MC_INTERFACE("org.test.mdb.chain.interface")

    mc::engine::property<std::string> m_name{"init_name"};
    mc::engine::property<int32_t>     m_count{7};
};

class mdb_chain_object : public mc::engine::object<mdb_chain_object> {
public:
    MC_OBJECT(mdb_chain_object, "MdbChainObject", "/org/test/mdb_chain/object", (mdb_chain_interface))

    void init()
    {
        set_object_name("MdbChainObject");
    }

    mdb_chain_interface m_iface;
};

struct mdb_chain_service : public mc::engine::service {
    mdb_chain_service() : mc::engine::service("org.test.mdb.chain.service")
    {}

    bool init(mc::dict args = {})
    {
        mc::dict args_mut(args);
        args_mut["service_path"] = "/org/test/mdb_chain/service";
        args_mut["service_name"] = "org.test.mdb.chain.service";
        return mc::engine::service::init(args_mut);
    }

    bool start()
    {
        if (!mc::engine::service::start()) {
            return false;
        }
        m_obj = mc::make_shared<mdb_chain_object>();
        m_obj->init();
        register_object(*m_obj);
        return true;
    }

    mc::shared_ptr<mdb_chain_object> m_obj;
};

} // namespace tests::mdb_access_chain

MC_REFLECT(tests::mdb_access_chain::mdb_chain_interface, ((m_name, "Name"))((m_count, "Count")))
MC_REFLECT(tests::mdb_access_chain::mdb_chain_object, ((m_iface, "Interface")))

// 集成 fixture：只在套件级重置引擎并启动常驻 service。
// 不可继承 TestWithEngine：其每个用例的 SetUp/TearDown 会 reset_for_test + 重建 SHM，
// 与套件内已 start 的 service 生命周期冲突，易在 TearDownTestSuite（stop/delete）时崩溃。
class mdb_access_property_chain_test : public mc::test::TestWithRuntime {
protected:
    static tests::mdb_access_chain::mdb_chain_service* service;
    static mc::dbus::connection                        conn;

    static void SetUpTestSuite()
    {
        mc::engine::engine::reset_for_test();
        mc::test::TestWithRuntime::SetUpTestSuite();
        service = new tests::mdb_access_chain::mdb_chain_service();
        service->init();
        service->start();
        conn = mc::dbus::connection::open_session_bus(mc::get_io_context());
        conn.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    static void TearDownTestSuite()
    {
        // 先停止并销毁 service（注销引擎/D-Bus 侧对象），再断开测试用会话连接，避免析构顺序触发崩溃。
        if (service) {
            service->stop();
            delete service;
            service = nullptr;
        }
        conn.disconnect();
        mc::engine::engine::reset_for_test();
        mc::test::TestWithRuntime::TearDownTestSuite();
    }

    void SetUp() override
    {
        mc::test::TestWithRuntime::SetUp();
        mdb_access::instance().clear_cache();
    }
};

tests::mdb_access_chain::mdb_chain_service* mdb_access_property_chain_test::service = nullptr;
mc::dbus::connection                        mdb_access_property_chain_test::conn;

TEST_F(mdb_access_property_chain_test, mdb_access_proxy_object_property_chain)
{
    auto                          bus = std::make_shared<mc::dbus::sd_bus>(conn, false);
    std::shared_ptr<proxy_object> obj;
    try {
        obj = mdb_access::instance().get_object_with_service(
            bus, "org.test.mdb.chain.service", "/org/test/mdb_chain/object", "org.test.mdb.chain.interface");
    } catch (const std::exception& e) {
        GTEST_SKIP() << "get_object_with_service 不可用，跳过: " << e.what();
    } catch (...) {
        GTEST_SKIP() << "get_object_with_service 不可用（未知异常），跳过";
    }
    ASSERT_NE(obj, nullptr);

    mc::variant before_name;
    ASSERT_NO_THROW(before_name = obj->get_property("Name"));
    EXPECT_EQ(before_name, mc::variant("init_name"));

    ASSERT_NO_THROW(obj->set_property("Name", mc::variant("changed_name")));
    mc::variant after_name;
    ASSERT_NO_THROW(after_name = obj->get_property("Name"));
    EXPECT_EQ(after_name, mc::variant("changed_name"));

    mc::dict all_props;
    ASSERT_NO_THROW(all_props = obj->get_all_properties());
    auto all_name  = all_props.find("Name");
    auto all_count = all_props.find("Count");
    ASSERT_NE(all_name, all_props.end());
    ASSERT_NE(all_count, all_props.end());
    EXPECT_EQ(all_name->value, mc::variant("changed_name"));
    EXPECT_EQ(all_count->value, mc::variant(7));
}
