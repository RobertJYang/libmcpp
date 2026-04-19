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
#include <mc/app/dbus_application.h>
#include <mc/dbus/connection.h>
#include <mc/dbus/sd_bus.h>
#include <mc/dbus/service_protocol.h>
#include <mc/engine.h>
#include <mc/runtime/runtime_context.h>
#include <mc/runtime/thread_pool.h>

#include <test_utilities/test_base.h>

#include <atomic>
#include <fstream>
#include <memory>
#include <thread>

namespace test_mcapp {

class dbus_test_interface : public mc::engine::interface<dbus_test_interface> {
public:
    MC_INTERFACE("org.test.app.dbus.Interface")

    void set_value(int32_t value)
    {
        m_value = value;
    }

    int32_t get_value() const
    {
        return m_value;
    }

    mc::engine::property<int32_t> m_value{0};
};

class dbus_test_object : public mc::engine::object<dbus_test_object> {
public:
    MC_OBJECT(dbus_test_object, "DbusTestObject", "/org/test/mcapp_protocol_export/TestObject1", (dbus_test_interface))

    void init()
    {
        set_object_name("DbusTestObject");
    }

    dbus_test_interface m_iface;
};

class dbus_export_service : public mc::app::service {
public:
    explicit dbus_export_service(mc::string name) : mc::app::service(std::move(name))
    {}

    bool on_configure() override
    {
        return true;
    }

    bool on_start() override
    {
        m_obj = mc::make_shared<dbus_test_object>();
        m_obj->init();
        m_obj->set_object_path("/org/test/mcapp_protocol_export/TestObject1");
        register_object(*m_obj);
        return true;
    }

    bool on_stop() override
    {
        return true;
    }

    mc::shared_ptr<dbus_test_object> m_obj;
};

class direct_export_service : public mc::engine::service {
public:
    direct_export_service() : mc::engine::service("org.test.direct_protocol_export")
    {}

    bool init(mc::dict args = {})
    {
        mc::dict args_mut(args);
        args_mut["service_path"] = "/org/test/direct_protocol_export";
        args_mut["service_name"] = "org.test.direct_protocol_export";
        return mc::engine::service::init(args_mut);
    }

    bool start()
    {
        if (!mc::engine::service::start()) {
            return false;
        }

        m_obj = mc::make_shared<dbus_test_object>();
        m_obj->init();
        m_obj->set_object_path("/org/test/direct_protocol_export/TestObject1");
        register_object(*m_obj);
        return true;
    }

    mc::shared_ptr<dbus_test_object> m_obj;
};

} // namespace test_mcapp

MC_REFLECT(test_mcapp::dbus_test_interface, ((m_value, "Value"))((set_value, "SetValue"))((get_value, "GetValue")))
MC_REFLECT(test_mcapp::dbus_test_object, ((m_iface, "Interface")))

namespace {

class dispatch_thread_guard {
public:
    dispatch_thread_guard(std::atomic_bool& running, std::thread& thread) : m_running(running), m_thread(thread)
    {}

    ~dispatch_thread_guard()
    {
        m_running.store(false, std::memory_order_release);
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

private:
    std::atomic_bool& m_running;
    std::thread&      m_thread;
};

class application_dbus_test : public mc::test::TestWithDbusDaemon {
protected:
    void SetUp() override
    {
        mc::test::TestBase::SetUp();
        // 每个 case 之间清空 engine + SHM region：本套件两个用例都会注册同名 service /
        // object，POSIX shm_open 跨进程持久化，残留 object_id / 命名互斥量会导致下个
        // case 注册时 "索引键冲突" 或全局析构阶段 mutex lock failed。
        mc::engine::engine::reset_for_test();
        mc::app::base_app::reset_for_test();

        mc::test::TestWithRuntime::reset_runtime();
        auto& runtime = mc::test::TestWithRuntime::get_runtime();
        runtime.initialize({6, 2});
        runtime.start();
        m_dbus_io_context = std::make_shared<mc::runtime::thread_pool>(2, "dbus-test");
        m_dbus_io_context->start();
    }

    void TearDown() override
    {
        mc::app::base_app::reset_for_test();
        if (m_dbus_io_context != nullptr) {
            m_dbus_io_context->stop();
            m_dbus_io_context->join();
            m_dbus_io_context.reset();
        }
        auto& runtime = mc::runtime::get_runtime_context();
        if (!runtime.is_stopped()) {
            runtime.stop();
            runtime.join();
        }
        mc::engine::engine::reset_for_test();
        mc::test::TestWithRuntime::reset_runtime();
        mc::test::TestBase::TearDown();
    }

    static void dispatch_bus_loop(mc::dbus::sd_bus& bus, std::atomic_bool& running)
    {
        while (running.load(std::memory_order_acquire)) {
            auto& conn     = bus.get_connection();
            auto* raw_conn = conn.get_connection();
            if (raw_conn != nullptr) {
                dbus_connection_read_write(raw_conn, 10);
                conn.dispatch();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    mc::dbus::sd_bus create_blocking_bus()
    {
        auto conn = mc::dbus::connection::open_session_bus(*m_dbus_io_context);
        auto bus  = mc::dbus::sd_bus(std::move(conn), true);
        EXPECT_TRUE(bus.get_connection().start());
        return bus;
    }

    bool request_bus_name(mc::dbus::sd_bus& bus, mc::string_view service_name)
    {
        mc::dbus::error error;
        dbus_error_init(&error);
        auto* raw_conn = bus.get_connection().get_connection();
        auto  reply    = dbus_bus_request_name(raw_conn, std::string(service_name).c_str(), 0, &error);
        if (error.is_set()) {
            ADD_FAILURE() << error.message;
            dbus_error_free(&error);
            return false;
        }
        return reply == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER || reply == DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER;
    }

    std::shared_ptr<mc::runtime::thread_pool> m_dbus_io_context;
};

TEST_F(application_dbus_test, configure_dbus_protocol_exports_application_managed_engine_service)
{
    auto temp_dir    = create_test_directory();
    auto config_path = temp_dir.path() / "mcapp_dbus_test_config.json";
    {
        std::ofstream file(config_path.string());
        file << R"([
            {
                "api_version": "v1",
                "kind": "Application",
                "meta": {"name": "dbus-app"}
            },
            {
                "api_version": "v1",
                "kind": "Service",
                "meta": {"name": "alpha"},
                "type": "protocol_export_service",
                "enabled": true
            }
        ])";
    }

    auto exported_bus = create_blocking_bus();
    ASSERT_TRUE(request_bus_name(exported_bus, "org.test.mcapp_protocol_export"));

    std::atomic_bool      dispatch_running{true};
    std::thread           dispatch_thread([&]() {
        dispatch_bus_loop(exported_bus, dispatch_running);
    });
    dispatch_thread_guard dispatch_guard(dispatch_running, dispatch_thread);

    mc::app::base_app::reset_for_test();
    mc::app::application app;
    app.register_service<test_mcapp::dbus_export_service>("protocol_export_service");
    mc::app::configure_dbus_protocol(app, exported_bus);

    std::string          config_arg = config_path.string();
    char                 arg0[]     = "mcapp_dbus_test";
    char                 arg1[]     = "--config";
    char*                argv[]     = {arg0, arg1, config_arg.data(), nullptr};
    mc::app::app_options options{3, argv};

    ASSERT_TRUE(app.initialize(options));
    ASSERT_TRUE(app.start());

    auto client_bus = create_blocking_bus();
    ASSERT_TRUE(request_bus_name(client_bus, "org.test.mcapp_protocol_client"));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto result = client_bus.timeout_call(
        mc::seconds(1), {"org.test.mcapp_protocol_export", "/org/test/mcapp_protocol_export/TestObject1",
                         "org.test.app.dbus.Interface", "SetValue", "i", mc::variants{42}});
    ASSERT_TRUE(result.empty());

    result = client_bus.timeout_call(mc::seconds(1),
                                     {"org.test.mcapp_protocol_export", "/org/test/mcapp_protocol_export/TestObject1",
                                      "org.test.app.dbus.Interface", "GetValue", "", mc::variants{}});
    ASSERT_EQ(result.size(), 1u);
    ASSERT_TRUE(result[0].is_int32());
    EXPECT_EQ(result[0].as_int32(), 42);

    ASSERT_TRUE(app.stop());
    mc::app::base_app::reset_for_test();
}

TEST_F(application_dbus_test, direct_engine_service_with_same_bus_path_still_works)
{
    auto exported_bus = create_blocking_bus();
    ASSERT_TRUE(request_bus_name(exported_bus, "org.test.direct_protocol_export"));
    auto client_bus = create_blocking_bus();

    std::atomic_bool      dispatch_running{true};
    std::thread           dispatch_thread([&]() {
        dispatch_bus_loop(exported_bus, dispatch_running);
    });
    dispatch_thread_guard dispatch_guard(dispatch_running, dispatch_thread);

    test_mcapp::direct_export_service service;
    service.init();
    service.start();
    service.set_protocol(mc::make_shared<mc::dbus::dbus_service_protocol>(exported_bus));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto result = client_bus.timeout_call(
        mc::seconds(1), {"org.test.direct_protocol_export", "/org/test/direct_protocol_export/TestObject1",
                         "org.test.app.dbus.Interface", "SetValue", "i", mc::variants{7}});
    ASSERT_TRUE(result.empty());

    result = client_bus.timeout_call(mc::seconds(1),
                                     {"org.test.direct_protocol_export", "/org/test/direct_protocol_export/TestObject1",
                                      "org.test.app.dbus.Interface", "GetValue", "", mc::variants{}});
    ASSERT_EQ(result.size(), 1u);
    ASSERT_TRUE(result[0].is_int32());
    EXPECT_EQ(result[0].as_int32(), 7);

    service.stop();
}

} // namespace
