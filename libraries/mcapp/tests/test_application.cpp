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
#include <mc/app/app_proto.h>
#include <mc/app/application.h>
#include <mc/app/service.h>
#include <mc/array.h>
#include <mc/dbus/connection.h>
#include <mc/dbus/message.h>
#include <mc/engine.h>
#include <mc/engine/payload.h>
#include <mc/engine/service.h>
#include <mc/engine/service_proto.h>
#include <mc/filesystem.h>
#include <mc/json.h>
#include <mc/reflect.h>
#include <mc/runtime/runtime_context.h>
#include <mc/shm/default_runtime.h>
#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/signal/connection.h>
#include <test_utilities/base.h>
#include <test_utilities/test_base.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <tuple>
#include <vector>

namespace test_mcapp {

struct sample_service_config {
    MC_REFLECTABLE("mc.test.SampleServiceConfig")

    std::string greeting{"default-greeting"};
};

class sample_service : public mc::app::service {
public:
    using config_type = sample_service_config;

    explicit sample_service(std::string name) : mc::app::service(std::move(name))
    {}

    bool on_configure() override
    {
        mc::from_variant(properties(), m_config);
        return true;
    }

    bool on_start() override
    {
        ++m_start_count;
        return true;
    }

    bool on_stop() override
    {
        ++m_stop_count;
        return true;
    }

    sample_service_config m_config;
    int                   m_start_count{0};
    int                   m_stop_count{0};
};

class echo_interface : public mc::engine::interface<echo_interface> {
public:
    MC_INTERFACE("org.test.application.Echo")

    mc::string m_greeting{"hello"};
    int32_t    m_counter{0};

    mc::string say(const mc::string& who)
    {
        ++m_counter;
        return m_greeting + ":" + who;
    }

    mc::signal<void(mc::string)> greeted;
};

class echo_object : public mc::engine::object<echo_object> {
public:
    MC_OBJECT(echo_object, "EchoObject", "/mc/test/application/echo", (echo_interface))

    echo_interface m_iface;
};

class echo_service : public mc::app::service {
public:
    explicit echo_service(std::string name) : mc::app::service(std::move(name))
    {}

    mc::shared_ptr<echo_object> echo_obj() const noexcept
    {
        return m_echo;
    }

protected:
    bool on_start() override
    {
        m_echo = mc::make_shared<echo_object>();
        register_object(m_echo);
        return true;
    }

    bool on_stop() override
    {
        if (m_echo) {
            unregister_object(m_echo);
            m_echo.reset();
        }
        return true;
    }

private:
    mc::shared_ptr<echo_object> m_echo;
};

} // namespace test_mcapp

MC_REFLECT(test_mcapp::sample_service_config, (greeting))
MC_REFLECT(test_mcapp::echo_interface, ((m_greeting, "Greeting"))((m_counter, "Counter"))((say, "Say"))(greeted))
MC_REFLECT(test_mcapp::echo_object, ((m_iface, "Iface")))

class application_test : public mc::test::TestWithDbusDaemon {
protected:
    static constexpr mc::string_view k_default_shm_region{"mc.default"};

    void SetUp() override
    {
        mc::shm::shutdown_default_runtime();
        mc::shm::detail::shared_memory_backend::remove(k_default_shm_region);
        mc::engine::engine::reset_for_test();
        mc::app::base_app::reset_for_test();
        m_app = std::make_unique<mc::app::application>();
        m_app->registry().register_service<test_mcapp::sample_service>("sample_service");
        m_app->registry().register_service<test_mcapp::echo_service>("echo_service");
    }

    void TearDown() override
    {
        if (m_app) {
            m_app->stop();
        }
        m_app.reset();
        mc::app::base_app::reset_for_test();
        mc::engine::engine::reset_for_test();
        mc::shm::shutdown_default_runtime();
        mc::shm::detail::shared_memory_backend::remove(k_default_shm_region);
    }

    mc::app::service_definition make_sample_service_definition(mc::string_view service_name = "mc.test.alpha")
    {
        mc::app::service_definition definition;
        definition.name    = mc::string(service_name);
        definition.type    = "sample_service";
        definition.enabled = true;
        return definition;
    }

    bool initialize_with_single_service(mc::app::service_definition definition, mc::string_view app_name = "sample-app",
                                        std::size_t io_threads = 1, std::size_t work_threads = 1)
    {
        mc::app::service_plan plan;
        plan.application.name         = mc::string(app_name);
        plan.application.io_threads   = io_threads;
        plan.application.work_threads = work_threads;
        plan.services.push_back(std::move(definition));
        return m_app->initialize_with_plan(std::move(plan));
    }

    mc::shared_ptr<test_mcapp::echo_service> start_echo_service(mc::string_view service_name = "mc.test.application.echo")
    {
        mc::app::service_definition definition;
        definition.name    = mc::string(service_name);
        definition.type    = "echo_service";
        definition.enabled = true;

        EXPECT_TRUE(initialize_with_single_service(std::move(definition), "mcapp_dbus_test", 1, 1));
        EXPECT_TRUE(m_app->start());

        return mc::static_pointer_cast<test_mcapp::echo_service>(m_app->get_service(service_name));
    }

    static mc::dbus::message make_call(mc::string_view destination, mc::string_view interface, mc::string_view member)
    {
        return mc::dbus::message::new_method_call(destination, mc::string_view("/mc/test/application/echo"), interface,
                                                  member);
    }

    static mc::dbus::message call_sync(mc::dbus::connection& conn, mc::dbus::message call)
    {
        auto future = conn.async_send_with_reply(std::move(call), mc::seconds(5));
        return std::move(future).get();
    }

    std::unique_ptr<mc::app::application> m_app;
};

TEST_F(application_test, initialize_builds_service_plan_from_plan)
{
    auto definition                   = make_sample_service_definition();
    definition.properties["greeting"] = mc::variant(mc::string("hello"));

    ASSERT_TRUE(initialize_with_single_service(std::move(definition), "sample-app", 2, 4));

    const auto& plan = m_app->plan();
    ASSERT_EQ(plan.services.size(), 1U);
    EXPECT_EQ(plan.application.name, "sample-app");
    EXPECT_EQ(plan.application.io_threads, 2U);
    EXPECT_EQ(plan.application.work_threads, 4U);
    EXPECT_EQ(plan.services[0].name, "mc.test.alpha");
    EXPECT_EQ(plan.services[0].type, "sample_service");

    auto service = m_app->get_service("mc.test.alpha");
    ASSERT_NE(service, nullptr);
    auto typed = mc::static_pointer_cast<test_mcapp::sample_service>(service);
    EXPECT_EQ(typed->m_config.greeting, "hello");
}

TEST_F(application_test, start_and_stop_manage_service_lifecycle)
{
    auto definition                   = make_sample_service_definition();
    definition.properties["greeting"] = mc::variant(mc::string("hello"));
    ASSERT_TRUE(initialize_with_single_service(std::move(definition)));
    ASSERT_TRUE(m_app->start());

    auto service = m_app->get_service("mc.test.alpha");
    ASSERT_NE(service, nullptr);
    auto typed = mc::static_pointer_cast<test_mcapp::sample_service>(service);
    EXPECT_EQ(typed->state(), mc::app::service_state::running);
    EXPECT_EQ(typed->m_start_count, 1);

    ASSERT_TRUE(m_app->stop());
    EXPECT_EQ(typed->state(), mc::app::service_state::stopped);
    EXPECT_EQ(typed->m_stop_count, 1);
}

TEST_F(application_test, initialize_applies_descriptor_default_properties_before_service_config)
{
    ASSERT_TRUE(initialize_with_single_service(make_sample_service_definition()));

    auto service = m_app->get_service("mc.test.alpha");
    ASSERT_NE(service, nullptr);
    auto typed = mc::static_pointer_cast<test_mcapp::sample_service>(service);
    EXPECT_EQ(typed->m_config.greeting, "default-greeting");
}

TEST_F(application_test, initialize_keeps_module_loading_in_bootstrap_flow)
{
    auto                  definition = make_sample_service_definition();
    mc::app::service_plan plan;
    plan.application.name = "sample-app";
    plan.application.modules.push_back("mc.test.nonexistent.module");
    plan.services.push_back(std::move(definition));

    EXPECT_FALSE(m_app->initialize_with_plan(std::move(plan)));
}

TEST_F(application_test, initialize_creates_root_service_and_derives_service_path_from_name)
{
    ASSERT_TRUE(initialize_with_single_service(make_sample_service_definition("mc.test.alpha.beta")));

    auto root = m_app->root_service();
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->path(), "/");

    auto service = m_app->get_service("mc.test.alpha.beta");
    ASSERT_NE(service, nullptr);
    EXPECT_EQ(service->name(), "mc.test.alpha.beta");
    EXPECT_EQ(service->path(), "/mc/test/alpha/beta");

    auto children = root->get_children();
    ASSERT_EQ(children.size(), 1U);
    EXPECT_EQ(children[0].get(), service.get());
    ASSERT_NE(service->get_parent(), nullptr);
    EXPECT_EQ(service->get_parent().get(), root.get());
}

TEST_F(application_test, initialize_keeps_explicit_service_path)
{
    auto definition = make_sample_service_definition("mc.test.alpha.beta");
    definition.path = "/services/custom";
    ASSERT_TRUE(initialize_with_single_service(std::move(definition)));

    auto service = m_app->get_service("mc.test.alpha.beta");
    ASSERT_NE(service, nullptr);
    EXPECT_EQ(service->path(), "/services/custom");
}

TEST_F(application_test, initialize_rejects_invalid_explicit_service_path)
{
    auto definition = make_sample_service_definition("mc.test.alpha.beta");
    definition.path = "invalid/path";
    EXPECT_FALSE(initialize_with_single_service(std::move(definition)));
}

namespace {

mc::dbus::connection open_test_connection(mc::string_view service_name)
{
    auto conn = mc::dbus::connection::open_session_bus(mc::runtime::get_io_context());
    EXPECT_TRUE(conn.start());
    EXPECT_TRUE(conn.is_connected());
    if (!service_name.empty()) {
        EXPECT_TRUE(std::get<0>(conn.request_name(service_name)));
    }
    return conn;
}
TEST_F(application_test, peer_ping_hits_standard_interface)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    auto peer_conn = open_test_connection("");
    auto call      = make_call(service_name, mc::string_view("org.freedesktop.DBus.Peer"), mc::string_view("Ping"));
    auto reply     = call_sync(peer_conn, std::move(call));

    ASSERT_TRUE(reply.is_method_return())
        << "Peer.Ping 应返回 method_return, got " << static_cast<int>(reply.get_type());
    EXPECT_TRUE(reply.read_args().empty());
}

TEST_F(application_test, properties_get_all_returns_registered_properties)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    auto peer_conn = open_test_connection("");
    auto call = make_call(service_name, mc::string_view("org.freedesktop.DBus.Properties"), mc::string_view("GetAll"));
    {
        auto writer = call.writer();
        writer.write_variant_value(mc::variant(mc::string("org.test.application.Echo")));
    }
    auto reply = call_sync(peer_conn, std::move(call));

    ASSERT_TRUE(reply.is_method_return());
    auto args = reply.read_args();
    ASSERT_EQ(args.size(), 1U);
    ASSERT_TRUE(args.front().is_dict());
    auto dict = args.front().as<mc::dict>();
    EXPECT_EQ(dict["Greeting"], mc::variant(mc::string("hello")));
    EXPECT_EQ(dict["Counter"], mc::variant(int32_t{0}));
}

TEST_F(application_test, properties_get_and_set_round_trip)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    auto peer_conn = open_test_connection("");

    // Set Greeting = "world" (ssv)
    auto set_call = make_call(service_name, mc::string_view("org.freedesktop.DBus.Properties"), mc::string_view("Set"));
    {
        auto writer = set_call.writer();
        writer.write_variant_value(mc::variant(mc::string("org.test.application.Echo")));
        writer.write_variant_value(mc::variant(mc::string("Greeting")));
        writer.write_variant(mc::variant(mc::string("world")), 0);
    }
    auto set_reply = call_sync(peer_conn, std::move(set_call));
    ASSERT_TRUE(set_reply.is_method_return()) << "Set 失败: type=" << static_cast<int>(set_reply.get_type());

    // Get Greeting (ss -> v)
    auto get_call = make_call(service_name, mc::string_view("org.freedesktop.DBus.Properties"), mc::string_view("Get"));
    {
        auto writer = get_call.writer();
        writer.write_variant_value(mc::variant(mc::string("org.test.application.Echo")));
        writer.write_variant_value(mc::variant(mc::string("Greeting")));
    }
    auto get_reply = call_sync(peer_conn, std::move(get_call));
    ASSERT_TRUE(get_reply.is_method_return());
    auto get_args = get_reply.read_args();
    ASSERT_EQ(get_args.size(), 1U);
    EXPECT_EQ(get_args.front(), mc::variant(mc::string("world")));

    EXPECT_EQ(svc->echo_obj()->m_iface.m_greeting, "world");
}

TEST_F(application_test, introspect_returns_xml_with_registered_and_standard_interfaces)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    auto peer_conn = open_test_connection("");
    auto call =
        make_call(service_name, mc::string_view("org.freedesktop.DBus.Introspectable"), mc::string_view("Introspect"));
    auto reply = call_sync(peer_conn, std::move(call));
    ASSERT_TRUE(reply.is_method_return());

    auto args = reply.read_args();
    ASSERT_EQ(args.size(), 1U);
    ASSERT_TRUE(args.front().is_string());
    auto xml = args.front().as<mc::string>();
    EXPECT_NE(xml.find("<interface name=\"org.test.application.Echo\">"), mc::string::npos) << xml;
    EXPECT_NE(xml.find("<method name=\"Say\">"), mc::string::npos);
    EXPECT_NE(xml.find("<property name=\"Greeting\""), mc::string::npos);
    EXPECT_NE(xml.find("<interface name=\"org.freedesktop.DBus.Properties\">"), mc::string::npos);
    EXPECT_NE(xml.find("<interface name=\"org.freedesktop.DBus.Introspectable\">"), mc::string::npos);
    EXPECT_NE(xml.find("<interface name=\"org.freedesktop.DBus.Peer\">"), mc::string::npos);
    EXPECT_NE(xml.find("<interface name=\"org.freedesktop.DBus.ObjectManager\">"), mc::string::npos);
}

TEST_F(application_test, own_method_dispatched_via_reflection)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    auto peer_conn = open_test_connection("");

    auto call = make_call(service_name, mc::string_view("org.test.application.Echo"), mc::string_view("Say"));
    {
        auto writer = call.writer();
        writer.write_variant_value(mc::variant(mc::string("friend")));
    }
    auto reply = call_sync(peer_conn, std::move(call));
    ASSERT_TRUE(reply.is_method_return()) << "Say 失败: type=" << static_cast<int>(reply.get_type());

    auto args = reply.read_args();
    ASSERT_EQ(args.size(), 1U);
    EXPECT_EQ(args.front(), mc::variant(mc::string("hello:friend")));
    EXPECT_EQ(svc->echo_obj()->m_iface.m_counter, 1);
}

TEST_F(application_test, service_can_call_outbound_dbus_via_connection)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);
    ASSERT_TRUE(svc->connection().is_connected()) << "基类应当已经连上 session bus";

    auto call = mc::dbus::message::new_method_call(
        mc::string_view("org.freedesktop.DBus"), mc::string_view("/org/freedesktop/DBus"),
        mc::string_view("org.freedesktop.DBus"), mc::string_view("ListNames"));

    auto future = svc->connection().async_send_with_reply(std::move(call), mc::seconds(5));
    auto reply  = std::move(future).get();

    ASSERT_TRUE(reply.is_method_return());
    auto args = reply.read_args();
    ASSERT_EQ(args.size(), 1U);
    ASSERT_TRUE(args.front().is_array());
    const auto names      = args.front().get_array();
    bool       found_self = false;
    for (const auto& entry : names) {
        if (entry.is_string() && entry.get_string() == service_name) {
            found_self = true;
            break;
        }
    }
    EXPECT_TRUE(found_self) << "service 的 request_name 应体现在 daemon 的 ListNames 里: " << service_name;
}

// ---- DBus error mapping ----

TEST_F(application_test, error_name_on_unknown_object_is_dbus_mapped)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    auto peer_conn = open_test_connection("");

    // 调用一个不存在的 object path，应触发 unknown_object 错误
    auto call = mc::dbus::message::new_method_call(
        service_name, mc::string_view("/mc/test/no/such/object"),
        mc::string_view("org.freedesktop.DBus.Properties"), mc::string_view("Get"));
    {
        auto writer = call.writer();
        writer.write_variant_value(mc::variant(mc::string("org.test.application.Echo")));
        writer.write_variant_value(mc::variant(mc::string("Greeting")));
    }
    auto reply = call_sync(peer_conn, std::move(call));

    ASSERT_TRUE(reply.is_error()) << "expected error, got type=" << static_cast<int>(reply.get_type());
    auto err_name = reply.get_error_name();
    EXPECT_EQ(err_name, std::string_view("org.freedesktop.DBus.Error.UnknownObject"))
        << "engine 错误名 mc.engine.* 应映射为 org.freedesktop.DBus.Error.*";
}

TEST_F(application_test, error_name_on_unknown_method_is_dbus_mapped)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    auto peer_conn = open_test_connection("");

    // 调用已有 object 但不存在的方法，应触发 unknown_method 错误
    auto call = mc::dbus::message::new_method_call(
        service_name, mc::string_view("/mc/test/application/echo"),
        mc::string_view("org.test.application.Echo"), mc::string_view("NonExistentMethod"));
    auto reply = call_sync(peer_conn, std::move(call));

    ASSERT_TRUE(reply.is_error()) << "expected error, got type=" << static_cast<int>(reply.get_type());
    auto err_name = reply.get_error_name();
    EXPECT_EQ(err_name, std::string_view("org.freedesktop.DBus.Error.UnknownMethod"))
        << "engine 错误名 mc.engine.* 应映射为 org.freedesktop.DBus.Error.*";
}

TEST_F(application_test, error_name_on_unknown_interface_is_dbus_mapped)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    auto peer_conn = open_test_connection("");

    // 调用已有 object 但不存在的 interface，应触发 unknown_interface 错误
    auto call = mc::dbus::message::new_method_call(
        service_name, mc::string_view("/mc/test/application/echo"),
        mc::string_view("org.no.such.Interface"), mc::string_view("Ping"));
    auto reply = call_sync(peer_conn, std::move(call));

    ASSERT_TRUE(reply.is_error()) << "expected error, got type=" << static_cast<int>(reply.get_type());
    auto err_name = reply.get_error_name();
    EXPECT_EQ(err_name, std::string_view("org.freedesktop.DBus.Error.UnknownInterface"))
        << "engine 错误名 mc.engine.* 应映射为 org.freedesktop.DBus.Error.*";
}

TEST_F(application_test, properties_get_all_unknown_interface_returns_error)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    auto peer_conn = open_test_connection("");
    auto call = make_call(service_name, mc::string_view("org.freedesktop.DBus.Properties"), mc::string_view("GetAll"));
    {
        auto writer = call.writer();
        writer.write_variant_value(mc::variant(mc::string("org.no.such.Interface")));
    }
    auto reply = call_sync(peer_conn, std::move(call));

    // 未知 interface 应返回 UnknownInterface 错误，而非静默返回空 dict
    ASSERT_TRUE(reply.is_error()) << "GetAll on unknown interface should return error, got type="
                                  << static_cast<int>(reply.get_type());
    EXPECT_EQ(reply.get_error_name(), std::string_view("org.freedesktop.DBus.Error.UnknownInterface"));
}

TEST_F(application_test, introspect_intermediate_path_lists_child_segments)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    auto peer_conn = open_test_connection("");

    // /mc/test/application 上无注册对象，但其下方挂有 /mc/test/application/echo，
    // 按 DBus 规范应返回最小节点 XML，列出下一段子节点名 echo。
    auto call = mc::dbus::message::new_method_call(service_name, mc::string_view("/mc/test/application"),
                                                   mc::string_view("org.freedesktop.DBus.Introspectable"),
                                                   mc::string_view("Introspect"));
    auto reply = call_sync(peer_conn, std::move(call));

    ASSERT_TRUE(reply.is_method_return()) << "Introspect on intermediate path should return XML, got type="
                                          << static_cast<int>(reply.get_type());
    auto args = reply.read_args();
    ASSERT_EQ(args.size(), 1U);
    ASSERT_TRUE(args.front().is_string());
    auto xml = args.front().as<mc::string>();
    EXPECT_NE(xml.find("<node name=\"echo\"/>"), mc::string::npos)
        << "中间路径 Introspect 应包含 <node name=\"echo\"/>: " << xml;
    EXPECT_EQ(xml.find("<interface"), mc::string::npos) << "中间路径 Introspect 不应列出接口: " << xml;
}

TEST_F(application_test, introspect_unknown_path_without_descendants_returns_unknown_object)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    auto peer_conn = open_test_connection("");

    // 该 path 既未注册对象，下方也无任何已注册子对象，应返回 UnknownObject 错误。
    auto call = mc::dbus::message::new_method_call(
        service_name, mc::string_view("/mc/test/application/echo/nonexistent_child"),
        mc::string_view("org.freedesktop.DBus.Introspectable"), mc::string_view("Introspect"));
    auto reply = call_sync(peer_conn, std::move(call));

    ASSERT_TRUE(reply.is_error()) << "Introspect on path with neither object nor descendants should error";
    EXPECT_EQ(reply.get_error_name(), std::string_view("org.freedesktop.DBus.Error.UnknownObject"));
}

// ---- 业务信号 match_rule 订阅示例 ----

static mc::engine::message make_signal_msg(mc::string_view path, mc::string_view interface_name,
                                           mc::string_view member_name, const mc::variants& args,
                                           mc::string_view signature)
{
    mc::engine::message msg;
    msg.header.type           = mc::engine::message_type::signal;
    msg.header.path           = mc::string(path);
    msg.header.interface_name = mc::string(interface_name);
    msg.header.member_name    = mc::string(member_name);
    msg.body                  = mc::engine::make_payload<mc::engine::signal_payload>(signature, mc::variants(args));
    return msg;
}

TEST_F(application_test, business_signal_match_rule_subscribe_and_receive)
{
    constexpr mc::string_view service_name = "mc.test.application.echo";

    auto svc = start_echo_service(service_name);
    ASSERT_NE(svc, nullptr);

    // 构建 match_rule：匹配 org.test.application.Echo 的 Greeted 信号
    mc::engine::match::match_rule rule;
    rule.type           = "signal";
    rule.interface_name = mc::string(mc::string_view("org.test.application.Echo"));
    rule.member_name    = mc::string(mc::string_view("Greeted"));

    // 通过 add_match 订阅
    std::mutex                         mutex;
    std::condition_variable            cv;
    std::optional<mc::engine::message> captured;
    auto id = svc->add_match(rule, mc::engine::filter_spec{}, [&](const mc::engine::message& msg) {
        std::lock_guard lock(mutex);
        captured = msg;
        cv.notify_all();
    });
    ASSERT_NE(id, 0u);

    // 发送 Greeted 信号
    auto sig = make_signal_msg("/mc/test/application/echo", "org.test.application.Echo", "Greeted",
                               mc::variants{mc::variant(mc::string("world"))}, "s");
    svc->emit(sig);

    // 等待 callback 触发
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(2), [&]() {
            return captured.has_value();
        }));
    }
    ASSERT_TRUE(captured.has_value());
    EXPECT_EQ(captured->header.type, mc::engine::message_type::signal);
    EXPECT_EQ(captured->header.interface_name, "org.test.application.Echo");
    EXPECT_EQ(captured->header.member_name, "Greeted");

    svc->remove_match(id);
}

} // namespace
