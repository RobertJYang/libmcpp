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
#include <mc/dbus/connection.h>
#include <mc/dbus/error.h>
#include <mc/engine.h>
#include <mc/engine/context.h>
#include <mc/engine/proxy.h>
#include <mc/reflect.h>
#include <mc/runtime/runtime_context.h>
#include <test_utilities/test_base.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <thread>

// ============================================================================
// mcapp proxy 测试
//
// 测试场景矩阵：
//   - 同进程 proxy 读写（server service 和 client 在同一进程）
//   - 跨进程 proxy 读写（server 在父进程，client 在 fork 子进程）
//   - cacheable 属性（int32_t, string）和 nocache 属性（vector, dict）
//   - typed proxy（proxy_interface / proxy_object）和 dynamic API（object_proxy）
//   - DBus 标准接口 Properties.Get / GetAll 直接访问（模拟 busctl 场景）
// ============================================================================

namespace mcapp_proxy_test {

// ============================================================================
// 服务端类型定义
// ============================================================================

class calc_interface : public mc::engine::interface<calc_interface> {
public:
    MC_INTERFACE("org.test.Calc")

    // cacheable 属性
    mc::engine::property<int32_t>    Counter;
    mc::engine::property<mc::string> Label;
    // nocache 属性（vector/dict 自动标记 MC_REFLECT_FLAG_NOCACHE）
    mc::engine::property<std::vector<int32_t>> Items;
    mc::engine::property<mc::dict>             Meta;

    virtual int32_t Add(int32_t a, int32_t b)
    {
        return a + b;
    }

    virtual int32_t GetVersion()
    {
        return 42;
    }

    virtual mc::string EchoCtxTag(mc::engine::context ctx, mc::string tail)
    {
        auto       tag_var = ctx.get_arg("WireTag");
        mc::string buf;
        if (tag_var.is_string()) {
            buf = tag_var.as_string();
        } else if (!tag_var.is_null()) {
            buf = tag_var.to_string();
        }
        buf += "|";
        buf += tail;
        return buf;
    }

    virtual mc::string EchoImplicitWire(mc::string tail)
    {
        auto* ptr = mc::engine::context::get_current_context_ptr();
        if (ptr == nullptr) {
            return mc::string("|") + tail;
        }
        auto       tag_var = ptr->get_arg("WireTag");
        mc::string buf;
        if (tag_var.is_string()) {
            buf = tag_var.as_string();
        } else if (!tag_var.is_null()) {
            buf = tag_var.to_string();
        }
        buf += "|";
        buf += tail;
        return buf;
    }
};

class calc_object : public mc::engine::object<calc_object> {
public:
    MC_OBJECT(calc_object, "CalcObject", "/mc/test/proxy/calc", (calc_interface))

    calc_interface iface;
};

class props_interface : public mc::engine::interface<props_interface> {
public:
    MC_INTERFACE("org.test.Props")

    mc::engine::property<int32_t> Counter;
    mc::engine::property<int32_t> InvalidatedCounter;
};

class props_object : public mc::engine::object<props_object> {
public:
    MC_OBJECT(props_object, "PropsObject", "/mc/test/props/publisher", (props_interface))

    props_interface iface;
};

// server service：注册 calc_object，提供 Properties.Get/Set 和方法调用
class calc_service : public mc::app::service {
public:
    explicit calc_service(std::string name) : mc::app::service(std::move(name))
    {}

    mc::shared_ptr<calc_object> obj() const noexcept
    {
        return m_obj;
    }

protected:
    bool on_start() override
    {
        m_obj                = mc::make_shared<calc_object>();
        m_obj->iface.Counter = int32_t{0};
        m_obj->iface.Label   = mc::string("initial");
        m_obj->iface.Items   = std::vector<int32_t>{1, 2, 3};
        m_obj->iface.Meta    = mc::dict{{"version", mc::variant(int32_t{1})}};
        register_object(m_obj);
        return true;
    }

    bool on_stop() override
    {
        if (m_obj) {
            unregister_object(m_obj);
            m_obj.reset();
        }
        return true;
    }

private:
    mc::shared_ptr<calc_object> m_obj;
};

// client service：最小化实现，只提供 DBus 通道
class client_service : public mc::app::service {
public:
    explicit client_service(mc::string name) : mc::app::service(std::move(name))
    {}

protected:
    bool on_start() override
    {
        return true;
    }
    bool on_stop() override
    {
        return true;
    }
};

class props_service : public mc::app::service {
public:
    explicit props_service(std::string name) : mc::app::service(std::move(name))
    {}

    mc::shared_ptr<props_object> obj() const noexcept
    {
        return m_obj;
    }

protected:
    bool on_start() override
    {
        m_obj = mc::make_shared<props_object>();
        m_obj->set_object_name("props_publisher");
        m_obj->iface.Counter            = int32_t{0};
        m_obj->iface.InvalidatedCounter = int32_t{0};
        m_obj->iface.set_property_invalidated("InvalidatedCounter");
        register_object(m_obj);
        return true;
    }

    bool on_stop() override
    {
        if (m_obj) {
            unregister_object(m_obj);
            m_obj.reset();
        }
        return true;
    }

private:
    mc::shared_ptr<props_object> m_obj;
};

// ============================================================================
// 客户端 proxy 类型
// ============================================================================

class calc_iface_proxy : public mc::engine::interface_proxy<calc_iface_proxy> {
public:
    MC_INTERFACE_PROXY("org.test.Calc");

    MC_PROXY_PROP(int32_t, Counter);
    MC_PROXY_PROP(mc::string, Label);
    MC_PROXY_PROP(std::vector<int32_t>, Items);
    MC_PROXY_PROP(mc::dict, Meta);

    MC_PROXY_METHOD(int32_t, Add, ((int32_t, a))((int32_t, b)));
    MC_PROXY_METHOD(int32_t, GetVersion);
};

class calc_obj_proxy {
public:
    calc_iface_proxy iface;

    void bind_proxy(const mc::engine::object_proxy_seed& seed)
    {
        iface.bind_proxy(seed);
    }
};

} // namespace mcapp_proxy_test

MC_REFLECT(mcapp_proxy_test::calc_interface,
           ((Counter, "Counter"))((Label, "Label"))((Items, "Items"))((Meta, "Meta"))((Add, "Add"))(
               (GetVersion, "GetVersion"))((EchoCtxTag, "EchoCtxTag"))((EchoImplicitWire, "EchoImplicitWire")))
MC_REFLECT(mcapp_proxy_test::calc_object, ((iface, "Iface")))
MC_REFLECT(mcapp_proxy_test::props_interface, ((Counter, "Counter"))((InvalidatedCounter, "InvalidatedCounter")))
MC_REFLECT(mcapp_proxy_test::props_object, ((iface, "Iface")))

// ============================================================================
// 测试 fixture
// ============================================================================

class proxy_test : public mc::test::TestWithDbusDaemon {
protected:
    static constexpr mc::string_view k_server_name{"mc.test.proxy.server"};

    void SetUp() override
    {
        TestWithDbusDaemon::SetUp();

        mc::app::base_app::reset_for_test();

        m_app = std::make_unique<mc::app::application>();
        m_app->registry().register_service<mcapp_proxy_test::calc_service>("calc_service");

        mc::app::service_definition def;
        def.name    = mc::string(k_server_name);
        def.type    = "calc_service";
        def.enabled = true;

        mc::app::service_plan plan;
        plan.application.name         = "proxy-test";
        plan.application.io_threads   = 1;
        plan.application.work_threads = 1;
        plan.services.push_back(std::move(def));

        ASSERT_TRUE(m_app->initialize_with_plan(std::move(plan)));
        ASSERT_TRUE(m_app->start());

        m_server = mc::static_pointer_cast<mcapp_proxy_test::calc_service>(m_app->get_service(k_server_name));
        ASSERT_NE(m_server, nullptr);
    }

    void TearDown() override
    {
        if (m_app) {
            m_app->stop();
        }
        m_app.reset();
        m_server.reset();

        mc::app::base_app::reset_for_test();
        // engine::reset_for_test 与 SHM region 释放都由基类 TearDown 统一管理。
        TestWithDbusDaemon::TearDown();
    }

    // 在子进程中创建独立的 client service + DBus 通道
    struct child_client {
        std::unique_ptr<mcapp_proxy_test::client_service> svc;
        std::unique_ptr<mc::app::app_proto>               proto;

        bool init()
        {
            svc = std::make_unique<mcapp_proxy_test::client_service>("mc.test.proxy.client");
            svc->init();
            if (!svc->start()) {
                return false;
            }

            auto conn = mc::dbus::connection::open_session_bus(mc::runtime::get_io_context());
            if (!conn.start() || !conn.is_connected()) {
                return false;
            }

            proto = std::make_unique<mc::app::app_proto>("mc.test.proxy.client", conn, nullptr);
            svc->set_proto(proto.get());
            return true;
        }
    };

    // fork 后子进程：runtime context（线程/io）+ engine singleton（service_registry/
    // match_table/threads）都需要重置以摆脱继承自父进程的状态；engine::reset_after_fork
    // 不动 SHM region，所以子进程的全局表会 attach 到父进程同一 SHM map（recover
    // 后即可看到父进程注册的所有对象，cacheable 属性走 SHM 直读加速）。
    int run_in_child(const std::function<int()>& body)
    {
        mc::runtime::get_runtime_context().reset_after_fork();
        mc::engine::engine::reset_after_fork();
        return body();
    }

    std::unique_ptr<mc::app::application>          m_app;
    mc::shared_ptr<mcapp_proxy_test::calc_service> m_server;
};

// ============================================================================
// 同进程：typed proxy 读写 cacheable + nocache 属性，方法调用
// ============================================================================

TEST_F(proxy_test, same_process_typed_proxy_read_write)
{
    m_server->obj()->iface.Counter = int32_t{100};
    m_server->obj()->iface.Label   = mc::string("hello");
    m_server->obj()->iface.Items   = std::vector<int32_t>{10, 20, 30};

    auto proxy = m_server->get_proxy<mcapp_proxy_test::calc_obj_proxy>("/mc/test/proxy/calc");
    ASSERT_NE(proxy, nullptr);
    ASSERT_TRUE(proxy->iface.is_bound());

    // cacheable 属性
    EXPECT_EQ(proxy->iface.Counter.get_value(), 100);
    EXPECT_EQ(proxy->iface.Label.get_value(), mc::string("hello"));

    // nocache 属性（vector/dict 走消息）
    auto items = proxy->iface.Items.get_value();
    EXPECT_EQ(items.size(), 3U);
    EXPECT_EQ(items[0], 10);
    EXPECT_EQ(items[2], 30);

    // 方法调用
    EXPECT_EQ(proxy->iface.Add(3, 4), 7);
    EXPECT_EQ(proxy->iface.GetVersion(), 42);

    // 写入属性
    proxy->iface.Counter.set_value(int32_t{200});
    proxy->iface.Label.set_value(mc::string("updated"));

    EXPECT_EQ(static_cast<int32_t>(m_server->obj()->iface.Counter), 200);
    EXPECT_EQ(static_cast<mc::string>(m_server->obj()->iface.Label), "updated");
}

// ============================================================================
// 同进程：dynamic API（object_proxy）读写属性
// ============================================================================

TEST_F(proxy_test, same_process_dynamic_api)
{
    m_server->obj()->iface.Counter = int32_t{50};
    m_server->obj()->iface.Meta =
        mc::dict{{"key", mc::variant(mc::string("value"))}, {"num", mc::variant(int32_t{99})}};

    mc::engine::object_ref ref;
    ref.service = "mc.test.proxy.server";
    ref.path    = "/mc/test/proxy/calc";

    mc::engine::object_proxy proxy(m_server.get(), ref);

    // cacheable 属性
    EXPECT_EQ(proxy.get_property("org.test.Calc", "Counter"), mc::variant(int32_t{50}));

    // nocache 属性（dict）
    auto meta = proxy.get_property("org.test.Calc", "Meta");
    ASSERT_TRUE(meta.is_dict());
    auto d = meta.as<mc::dict>();
    EXPECT_EQ(d["key"], mc::string("value"));
    EXPECT_EQ(d["num"], int32_t{99});

    // GetAll
    auto all = proxy.get_all_properties("org.test.Calc");
    EXPECT_FALSE(all.empty());
    EXPECT_EQ(all["Counter"], int32_t{50});

    // invoke
    EXPECT_EQ(proxy.invoke("org.test.Calc", "Add", mc::variants{int32_t{10}, int32_t{20}}, "ii"),
              mc::variant(int32_t{30}));

    // set
    proxy.set_property("org.test.Calc", "Counter", mc::variant(int32_t{999}));
    EXPECT_EQ(static_cast<int32_t>(m_server->obj()->iface.Counter), 999);
}

// ============================================================================
// 同进程：通过 service->call 模拟 busctl 的 Properties.Get/GetAll 访问
// service::call 内部通过 engine::dispatch 做本地分发
// ============================================================================

TEST_F(proxy_test, same_process_properties_get_and_getall_via_dispatch)
{
    m_server->obj()->iface.Counter = int32_t{77};
    m_server->obj()->iface.Label   = mc::string("dbus-test");
    m_server->obj()->iface.Items   = std::vector<int32_t>{5, 6, 7, 8};

    // Properties.Get — cacheable
    auto value = m_server->call("/mc/test/proxy/calc", "mc.test.proxy.server", "org.freedesktop.DBus.Properties", "Get",
                                mc::variants{mc::string("org.test.Calc"), mc::string("Counter")}, "ss");
    // Properties.Get 返回 variant，实际编码为 variant 包裹的值
    EXPECT_FALSE(value.is_null());

    // Properties.Get — nocache (Items)
    auto items_val = m_server->call("/mc/test/proxy/calc", "mc.test.proxy.server", "org.freedesktop.DBus.Properties",
                                    "Get", mc::variants{mc::string("org.test.Calc"), mc::string("Items")}, "ss");
    EXPECT_FALSE(items_val.is_null());

    // Properties.GetAll
    auto all = m_server->call("/mc/test/proxy/calc", "mc.test.proxy.server", "org.freedesktop.DBus.Properties",
                              "GetAll", mc::variants{mc::string("org.test.Calc")}, "s");
    EXPECT_TRUE(all.is_dict());
    auto all_dict = all.as<mc::dict>();
    EXPECT_EQ(all_dict["Counter"], int32_t{77});
    EXPECT_EQ(all_dict["Label"], mc::string("dbus-test"));
}

// ============================================================================
// 跨进程：子进程通过 typed proxy 读写属性和调用方法
// ============================================================================

TEST_F(proxy_test, cross_process_typed_proxy_read_write)
{
    m_server->obj()->iface.Counter = int32_t{100};
    m_server->obj()->iface.Label   = mc::string("cross-process");
    m_server->obj()->iface.Items   = std::vector<int32_t>{10, 20, 30};

    fork_child([&]() -> int {
        return run_in_child([&]() {
            child_client client;
            if (!client.init()) {
                return 10;
            }

            // 跨进程对象不在全局表中，使用 object_proxy_seed 手动绑定
            mc::engine::object_proxy_seed seed;
            seed.ref.service    = "mc.test.proxy.server";
            seed.ref.path       = "/mc/test/proxy/calc";
            seed.svc            = client.svc.get();
            seed.policy.route   = mc::engine::proxy_route::no_cache;
            seed.policy.timeout = mc::milliseconds(3000);

            auto proxy = std::make_unique<mcapp_proxy_test::calc_obj_proxy>();
            proxy->bind_proxy(seed);

            // cacheable 属性
            auto counter = proxy->iface.Counter.get_value();
            if (counter != 100) {
                return 20;
            }

            auto label = proxy->iface.Label.get_value();
            if (label != mc::string("cross-process")) {
                return 21;
            }

            // nocache 属性（vector）
            auto items = proxy->iface.Items.get_value();
            if (items.size() != 3 || items[0] != 10 || items[2] != 30) {
                return 22;
            }

            // 方法调用
            if (proxy->iface.Add(3, 4) != 7) {
                return 30;
            }

            if (proxy->iface.GetVersion() != 42) {
                return 31;
            }

            // 写入属性
            proxy->iface.Counter.set_value(int32_t{200});
            proxy->iface.Label.set_value(mc::string("from-child"));

            // 回读验证
            if (proxy->iface.Counter.get_value() != 200) {
                return 40;
            }
            if (proxy->iface.Label.get_value() != mc::string("from-child")) {
                return 41;
            }

            return 0;
        });
    });

    // 验证子进程写入生效
    EXPECT_EQ(static_cast<int32_t>(m_server->obj()->iface.Counter), 200);
    EXPECT_EQ(static_cast<mc::string>(m_server->obj()->iface.Label), "from-child");
}

#if MCENGINE_USE_SHM
TEST_F(proxy_test, cross_process_app_property_change_emits_properties_changed_over_mq)
{
    std::atomic<int> hits{0};
    std::atomic<int> pair_count{0};
    std::atomic<int> body_ok{0};

    mc::engine::match::match_rule rule;
    rule.type           = "signal";
    rule.interface_name = "org.freedesktop.DBus.Properties";
    rule.member_name    = "PropertiesChanged";

    auto id = m_server->add_match(rule, mc::engine::match::filter_spec{}, [&](const mc::engine::message& msg) {
        pair_count = static_cast<int>(mc::engine::match::get_target_match_ids(msg.header).size());
        if (const auto* payload = msg.try_as<mc::engine::signal_payload>()) {
            if (payload->signature == "sa{sv}as" && payload->args.size() == 3 &&
                payload->args[0] == mc::variant(mc::string("org.test.Props")) && payload->args[1].is_dict() &&
                payload->args[2].is_array()) {
                auto changed = payload->args[1].as<mc::dict>();
                if (changed.find("Counter") != changed.end() && changed["Counter"] == int32_t{321}) {
                    body_ok = 1;
                }
            }
        }
        ++hits;
    });
    ASSERT_NE(id, 0U);

    fork_child([&]() -> int {
        return run_in_child([&]() {
            try {
                mc::app::base_app::reset_for_test();

                auto child_app = std::make_unique<mc::app::application>();
                child_app->registry().register_service<mcapp_proxy_test::props_service>("props_service");

                mc::app::service_definition def;
                def.name    = "mc.test.props.publisher";
                def.type    = "props_service";
                def.enabled = true;

                mc::app::service_plan plan;
                plan.application.name         = "props-child-app";
                plan.application.io_threads   = 1;
                plan.application.work_threads = 1;
                plan.services.push_back(std::move(def));

                if (!child_app->initialize_with_plan(std::move(plan))) {
                    return 10;
                }
                if (!child_app->start()) {
                    return 11;
                }

                auto publisher = mc::static_pointer_cast<mcapp_proxy_test::props_service>(
                    child_app->get_service("mc.test.props.publisher"));
                if (publisher == nullptr || publisher->obj() == nullptr) {
                    return 12;
                }

                publisher->obj()->iface.Counter = int32_t{321};
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                child_app->stop();
                mc::app::base_app::reset_for_test();
                return 0;
            } catch (const std::exception& ex) {
                std::fprintf(stderr, "child exception: %s\n", ex.what());
                return 99;
            } catch (...) {
                std::fprintf(stderr, "child exception: unknown\n");
                return 98;
            }
        });
    });

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline && hits.load() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_GE(hits.load(), 1);
    EXPECT_EQ(pair_count.load(), 1);
    EXPECT_EQ(body_ok.load(), 1);

    m_server->remove_match(id);
}

TEST_F(proxy_test, cross_process_app_invalidated_property_emits_invalidated_properties_over_mq)
{
    std::atomic<int> hits{0};
    std::atomic<int> pair_count{0};
    std::atomic<int> body_ok{0};

    mc::engine::match::match_rule rule;
    rule.type           = "signal";
    rule.interface_name = "org.freedesktop.DBus.Properties";
    rule.member_name    = "PropertiesChanged";

    auto id = m_server->add_match(rule, mc::engine::match::filter_spec{}, [&](const mc::engine::message& msg) {
        pair_count = static_cast<int>(mc::engine::match::get_target_match_ids(msg.header).size());
        if (const auto* payload = msg.try_as<mc::engine::signal_payload>()) {
            if (payload->signature == "sa{sv}as" && payload->args.size() == 3 &&
                payload->args[0] == mc::variant(mc::string("org.test.Props")) && payload->args[1].is_dict() &&
                payload->args[2].is_array()) {
                auto changed     = payload->args[1].as<mc::dict>();
                auto invalidated = payload->args[2].as<mc::variants>();
                if (changed.empty() && invalidated.size() == 1U &&
                    invalidated[0] == mc::variant(mc::string("InvalidatedCounter"))) {
                    body_ok = 1;
                }
            }
        }
        ++hits;
    });
    ASSERT_NE(id, 0U);

    fork_child([&]() -> int {
        return run_in_child([&]() {
            try {
                mc::app::base_app::reset_for_test();

                auto child_app = std::make_unique<mc::app::application>();
                child_app->registry().register_service<mcapp_proxy_test::props_service>("props_service");

                mc::app::service_definition def;
                def.name    = "mc.test.props.invalidated.publisher";
                def.type    = "props_service";
                def.enabled = true;

                mc::app::service_plan plan;
                plan.application.name         = "props-invalidated-child-app";
                plan.application.io_threads   = 1;
                plan.application.work_threads = 1;
                plan.services.push_back(std::move(def));

                if (!child_app->initialize_with_plan(std::move(plan))) {
                    return 10;
                }
                if (!child_app->start()) {
                    return 11;
                }

                auto publisher = mc::static_pointer_cast<mcapp_proxy_test::props_service>(
                    child_app->get_service("mc.test.props.invalidated.publisher"));
                if (publisher == nullptr || publisher->obj() == nullptr) {
                    return 12;
                }

                publisher->obj()->iface.InvalidatedCounter = int32_t{654};
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                child_app->stop();
                mc::app::base_app::reset_for_test();
                return 0;
            } catch (const std::exception& ex) {
                std::fprintf(stderr, "child exception: %s\n", ex.what());
                return 99;
            } catch (...) {
                std::fprintf(stderr, "child exception: unknown\n");
                return 98;
            }
        });
    });

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline && hits.load() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_GE(hits.load(), 1);
    EXPECT_EQ(pair_count.load(), 1);
    EXPECT_EQ(body_ok.load(), 1);

    m_server->remove_match(id);
}
#endif // MCENGINE_USE_SHM

// ============================================================================
// 跨进程：子进程通过 dynamic API 读写 + DBus Get/GetAll
// ============================================================================

TEST_F(proxy_test, cross_process_dynamic_api_and_dbus_get)
{
    m_server->obj()->iface.Counter = int32_t{55};
    m_server->obj()->iface.Label   = mc::string("dynamic-cross");
    m_server->obj()->iface.Items   = std::vector<int32_t>{100, 200};

    fork_child([&]() -> int {
        return run_in_child([&]() {
            child_client client;
            if (!client.init()) {
                return 10;
            }

            mc::engine::object_ref ref;
            ref.service = "mc.test.proxy.server";
            ref.path    = "/mc/test/proxy/calc";

            mc::engine::proxy_policy policy;
            policy.route   = mc::engine::proxy_route::no_cache;
            policy.timeout = mc::milliseconds(3000);

            mc::engine::object_proxy proxy(client.svc.get(), ref, policy);

            // get_property — cacheable
            auto counter = proxy.get_property("org.test.Calc", "Counter");
            if (counter != mc::variant(int32_t{55})) {
                return 20;
            }

            // get_property — nocache
            auto items = proxy.get_property("org.test.Calc", "Items");
            if (items.is_null()) {
                return 21;
            }

            // get_all_properties
            auto all = proxy.get_all_properties("org.test.Calc");
            if (all.empty()) {
                return 22;
            }
            if (all.find("Counter") == all.end()) {
                return 23;
            }
            if (all["Counter"] != int32_t{55}) {
                return 24;
            }

            // invoke
            auto result = proxy.invoke("org.test.Calc", "Add", mc::variants{int32_t{10}, int32_t{5}}, "ii");
            if (result != mc::variant(int32_t{15})) {
                return 30;
            }

            // set_property
            proxy.set_property("org.test.Calc", "Counter", mc::variant(int32_t{999}));

            // 通过 DBus Properties.Get 回读确认（模拟 busctl）
            mc::engine::message get_msg;
            get_msg.header.type           = mc::engine::message_type::method_call;
            get_msg.header.destination    = mc::string("mc.test.proxy.server");
            get_msg.header.path           = mc::string("/mc/test/proxy/calc");
            get_msg.header.interface_name = mc::string("org.freedesktop.DBus.Properties");
            get_msg.header.member_name    = mc::string("Get");
            get_msg.body                  = mc::engine::make_payload<mc::engine::method_call_payload>(
                "ss", mc::variants{mc::string("org.test.Calc"), mc::string("Counter")});

            auto reply = client.proto->send_with_reply(std::move(get_msg), mc::milliseconds(5000));
            if (reply.header.type != mc::engine::message_type::method_return) {
                return 40;
            }
            auto* payload = reply.try_as<mc::engine::method_return_payload>();
            if (payload == nullptr || payload->value != mc::variant(int32_t{999})) {
                return 41;
            }

            return 0;
        });
    });

    // 验证子进程写入生效
    EXPECT_EQ(static_cast<int32_t>(m_server->obj()->iface.Counter), 999);
}

// ============================================================================
// 跨进程 get_proxy<T>(path, service_hint) 自动解析远端对象
// 子进程 fork 后，通过 client.svc->get_proxy<T>(path, service_hint) 一步获取
// 跨进程代理：cacheable 属性走消息读，nocache 属性走 DBus Properties.Get，
// 方法调用走 DBus 消息。
// service::get_proxy 在 make_proxy_seed 失败但有 service_hint 时，
// 会构造远程 seed（ref.path + ref.service + svc=this + route=no_cache）。
// ============================================================================
TEST_F(proxy_test, cross_process_get_proxy_with_service_hint)
{
    m_server->obj()->iface.Counter = int32_t{42};
    m_server->obj()->iface.Label   = mc::string("shm-test");
    m_server->obj()->iface.Items   = std::vector<int32_t>{5, 6, 7};

    fork_child([&]() -> int {
        return run_in_child([&]() {
            child_client client;
            if (!client.init()) {
                return 10;
            }

            // get_proxy(path, service_hint) 解析跨进程对象
            auto proxy =
                client.svc->get_proxy<mcapp_proxy_test::calc_obj_proxy>("/mc/test/proxy/calc", "mc.test.proxy.server");
            if (proxy == nullptr) {
                return 20;
            }

            // cacheable 属性读（走 DBus Properties.Get 消息）
            auto counter = proxy->iface.Counter.get_value();
            if (counter != 42) {
                return 21;
            }

            // nocache 属性读（走 DBus Properties.Get）
            auto items = proxy->iface.Items.get_value();
            if (items.size() != 3 || items[0] != 5 || items[2] != 7) {
                return 22;
            }

            // 方法调用（走 DBus 消息）
            if (proxy->iface.Add(10, 20) != 30) {
                return 23;
            }

            return 0;
        });
    });
}

// ============================================================================
// SHM 直读：跨进程 cacheable 属性经默认 SHM resolver 直接从共享内存读取，
// 不走 DBus 消息路径。仅在 MCENGINE_USE_SHM=1 时有效。
// ============================================================================
#if MCENGINE_USE_SHM
TEST_F(proxy_test, cross_process_shm_direct_read_cacheable_via_fast_path)
{
    m_server->obj()->iface.Counter = int32_t{42};
    m_server->obj()->iface.Label   = mc::string("shm-direct");

    fork_child([&]() -> int {
        return run_in_child([&]() {
            child_client client;
            if (!client.init()) {
                return 10;
            }

            mc::engine::object_ref ref;
            ref.service = "mc.test.proxy.server";
            ref.path    = "/mc/test/proxy/calc";

            mc::engine::proxy_policy policy;
            policy.route = mc::engine::proxy_route::auto_;

            // 显式注入默认 SHM resolver；svc 仍传 client.svc.get() 兼容 nocache fallback，
            // 但 fast_available_only 模式下不会触达消息路径。
            mc::engine::object_proxy proxy(client.svc.get(), ref, policy,
                                           mc::engine::make_default_proxy_shm_resolver());

            // fast_available_only 严格只读 SHM；若 SHM 未跨进程共享或 resolve 失败，
            // 返回空 dict。
            auto props = proxy.get_all_properties("org.test.Calc", mc::engine::proxy_get_all_mode::fast_available_only);
            if (props.empty()) {
                return 20;
            }
            if (props.find("Counter") == props.end() || props["Counter"] != int32_t{42}) {
                return 21;
            }
            if (props.find("Label") == props.end() || props["Label"] != mc::string("shm-direct")) {
                return 22;
            }

            // typed proxy + auto_：service::get_proxy fallback 应注入 SHM resolver，
            // Counter / Label 命中 SHM 缓存直接返回，不发消息。
            auto typed =
                client.svc->get_proxy<mcapp_proxy_test::calc_obj_proxy>("/mc/test/proxy/calc", "mc.test.proxy.server");
            if (typed == nullptr) {
                return 30;
            }
            if (typed->iface.Counter.get_value() != 42) {
                return 31;
            }
            if (typed->iface.Label.get_value() != mc::string("shm-direct")) {
                return 32;
            }

            return 0;
        });
    });
}
#endif // MCENGINE_USE_SHM

// ============================================================================
// 跨进程 path-only get_proxy<T>(path) 无 service_hint 时返回 nullptr
// 没有服务名无法确定消息目标，这是预期行为。
// ============================================================================
TEST_F(proxy_test, cross_process_get_proxy_by_path_only_returns_null)
{
    m_server->obj()->iface.Counter = int32_t{300};

    fork_child([&]() -> int {
        return run_in_child([&]() {
            child_client client;
            if (!client.init()) {
                return 10;
            }

            // 无 service_hint 的 get_proxy：跨进程时返回 nullptr
            // 这是正确行为 —— 不知道目标服务名就无法路由消息
            auto proxy = client.svc->get_proxy<mcapp_proxy_test::calc_obj_proxy>("/mc/test/proxy/calc");
            if (proxy != nullptr) {
                return 99;
            }

            // 带 service_hint 的版本能工作（见 cross_process_get_proxy_with_service_hint）
            return 0;
        });
    });
}

// ============================================================================
// 跨进程 nocache 属性（vector/dict）和方法调用通过 DBus 消息走通
// 使用 object_proxy_seed 手动构造，验证 Items/Meta/Add/GetVersion 完整链路。
// ============================================================================
TEST_F(proxy_test, cross_process_nocache_property_and_method_via_dbus)
{
    m_server->obj()->iface.Counter = int32_t{0};
    m_server->obj()->iface.Items   = std::vector<int32_t>{7, 8, 9};

    fork_child([&]() -> int {
        return run_in_child([&]() {
            child_client client;
            if (!client.init()) {
                return 10;
            }

            // 使用 object_proxy_seed 手动构造，走 no_cache 全消息模式
            mc::engine::object_proxy_seed seed;
            seed.ref.service    = "mc.test.proxy.server";
            seed.ref.path       = "/mc/test/proxy/calc";
            seed.svc            = client.svc.get();
            seed.policy.route   = mc::engine::proxy_route::no_cache;
            seed.policy.timeout = mc::milliseconds(3000);

            auto proxy = std::make_unique<mcapp_proxy_test::calc_obj_proxy>();
            proxy->bind_proxy(seed);

            // nocache 属性 Items（vector<int32_t>）走 Properties.Get 消息
            auto items = proxy->iface.Items.get_value();
            if (items.size() != 3 || items[0] != 7 || items[2] != 9) {
                return 20;
            }

            // nocache 属性 Meta（dict）走 Properties.Get 消息
            proxy->iface.Meta.set_value(mc::dict{{"x", mc::variant(int32_t{1})}});
            auto meta = proxy->iface.Meta.get_value();
            if (meta.find("x") == meta.end() || meta["x"] != int32_t{1}) {
                return 21;
            }

            // 方法调用 Add — 通过 DBus 消息
            auto add_result = proxy->iface.Add(100, 200);
            if (add_result != 300) {
                return 30;
            }

            // 方法调用 GetVersion — 无参方法
            auto version = proxy->iface.GetVersion();
            if (version != 42) {
                return 31;
            }

            return 0;
        });
    });
}

// ============================================================================
// 原生 DBus 互通 — 模拟外围服务直接用 DBus 协议调 libmcpp 服务端
// 通过裸 DBus 消息调用 Properties.Get/Set/GetAll + 自定义方法 Add，
// 验证签名 "ss"/"ssv"/"s"/"ii" 的编解码正确性。
// ============================================================================
TEST_F(proxy_test, native_dbus_interop_properties_and_methods)
{
    m_server->obj()->iface.Counter = int32_t{123};
    m_server->obj()->iface.Label   = mc::string("native-dbus");
    m_server->obj()->iface.Items   = std::vector<int32_t>{10, 20, 30};
    m_server->obj()->iface.Meta    = mc::dict{{"status", mc::variant(mc::string("ok"))}};

    fork_child([&]() -> int {
        return run_in_child([&]() {
            // 创建 client service（提供 send_with_reply 的 DBus 通道）
            child_client client;
            if (!client.init()) {
                return 10;
            }

            mc::engine::proxy_policy policy;
            policy.timeout = mc::milliseconds(5000);

            // --- Properties.Get (cacheable: Counter) ---
            {
                mc::engine::message msg;
                msg.header.type           = mc::engine::message_type::method_call;
                msg.header.destination    = mc::string("mc.test.proxy.server");
                msg.header.path           = mc::string("/mc/test/proxy/calc");
                msg.header.interface_name = mc::string("org.freedesktop.DBus.Properties");
                msg.header.member_name    = mc::string("Get");
                msg.body                  = mc::engine::make_payload<mc::engine::method_call_payload>(
                    "ss", mc::variants{mc::string("org.test.Calc"), mc::string("Counter")});

                auto reply = client.proto->send_with_reply(std::move(msg), policy.timeout);
                if (reply.header.type != mc::engine::message_type::method_return) {
                    return 20;
                }
                auto* payload = reply.try_as<mc::engine::method_return_payload>();
                if (payload == nullptr) {
                    return 21;
                }
                if (payload->value != mc::variant(int32_t{123})) {
                    return 22;
                }
            }

            // --- Properties.Get (nocache: Items) ---
            {
                mc::engine::message msg;
                msg.header.type           = mc::engine::message_type::method_call;
                msg.header.destination    = mc::string("mc.test.proxy.server");
                msg.header.path           = mc::string("/mc/test/proxy/calc");
                msg.header.interface_name = mc::string("org.freedesktop.DBus.Properties");
                msg.header.member_name    = mc::string("Get");
                msg.body                  = mc::engine::make_payload<mc::engine::method_call_payload>(
                    "ss", mc::variants{mc::string("org.test.Calc"), mc::string("Items")});

                auto reply = client.proto->send_with_reply(std::move(msg), policy.timeout);
                if (reply.header.type != mc::engine::message_type::method_return) {
                    return 23;
                }
                auto* payload = reply.try_as<mc::engine::method_return_payload>();
                if (payload == nullptr || payload->value.is_null()) {
                    return 24;
                }
                auto items = payload->value.as<std::vector<int32_t>>();
                if (items.size() != 3 || items[0] != 10 || items[2] != 30) {
                    return 25;
                }
            }

            // --- Properties.GetAll ---
            {
                mc::engine::message msg;
                msg.header.type           = mc::engine::message_type::method_call;
                msg.header.destination    = mc::string("mc.test.proxy.server");
                msg.header.path           = mc::string("/mc/test/proxy/calc");
                msg.header.interface_name = mc::string("org.freedesktop.DBus.Properties");
                msg.header.member_name    = mc::string("GetAll");
                msg.body                  = mc::engine::make_payload<mc::engine::method_call_payload>(
                    "s", mc::variants{mc::string("org.test.Calc")});

                auto reply = client.proto->send_with_reply(std::move(msg), policy.timeout);
                if (reply.header.type != mc::engine::message_type::method_return) {
                    return 26;
                }
                auto* payload = reply.try_as<mc::engine::method_return_payload>();
                if (payload == nullptr || !payload->value.is_dict()) {
                    return 27;
                }
                auto all = payload->value.as<mc::dict>();
                if (all.find("Counter") == all.end() || all["Counter"] != int32_t{123}) {
                    return 28;
                }
                if (all.find("Label") == all.end() || all["Label"] != mc::string("native-dbus")) {
                    return 29;
                }
            }

            // --- Properties.Set ---
            {
                mc::engine::message msg;
                msg.header.type           = mc::engine::message_type::method_call;
                msg.header.destination    = mc::string("mc.test.proxy.server");
                msg.header.path           = mc::string("/mc/test/proxy/calc");
                msg.header.interface_name = mc::string("org.freedesktop.DBus.Properties");
                msg.header.member_name    = mc::string("Set");
                msg.body                  = mc::engine::make_payload<mc::engine::method_call_payload>(
                    "ssv", mc::variants{mc::string("org.test.Calc"), mc::string("Counter"), mc::variant(int32_t{456})});

                auto reply = client.proto->send_with_reply(std::move(msg), policy.timeout);
                if (reply.header.type != mc::engine::message_type::method_return) {
                    return 30;
                }
            }

            // --- D5: 自定义方法 Add(int32_t, int32_t) -> int32_t ---
            {
                mc::engine::message msg;
                msg.header.type           = mc::engine::message_type::method_call;
                msg.header.destination    = mc::string("mc.test.proxy.server");
                msg.header.path           = mc::string("/mc/test/proxy/calc");
                msg.header.interface_name = mc::string("org.test.Calc");
                msg.header.member_name    = mc::string("Add");
                msg.body                  = mc::engine::make_payload<mc::engine::method_call_payload>(
                    "ii", mc::variants{int32_t{100}, int32_t{200}});

                auto reply = client.proto->send_with_reply(std::move(msg), policy.timeout);
                if (reply.header.type != mc::engine::message_type::method_return) {
                    return 31;
                }
                auto* payload = reply.try_as<mc::engine::method_return_payload>();
                if (payload == nullptr || payload->value != mc::variant(int32_t{300})) {
                    return 32;
                }
            }

            return 0;
        });
    });

    // 验证 Set 生效
    EXPECT_EQ(static_cast<int32_t>(m_server->obj()->iface.Counter), 456);
}
TEST_F(proxy_test, native_dbus_wire_context_echo_methods_and_invalid_signature)
{
    fork_child([&]() -> int {
        return run_in_child([&]() {
            child_client client;
            if (!client.init()) {
                return 10;
            }

            mc::engine::proxy_policy policy;
            policy.timeout               = mc::milliseconds(5000);
            mc::dict         wire_dict   = mc::dict{{"WireTag", mc::variant(mc::string("e2e-wire"))}};
            const mc::string cs_sig      = mc::string(mc::engine::context_signature_string());
            const mc::string wire_plus_s = cs_sig + "s";

            // 显式 context 首参 + tail
            {
                mc::engine::message msg;
                msg.header.type           = mc::engine::message_type::method_call;
                msg.header.destination    = mc::string("mc.test.proxy.server");
                msg.header.path           = mc::string("/mc/test/proxy/calc");
                msg.header.interface_name = mc::string("org.test.Calc");
                msg.header.member_name    = mc::string("EchoCtxTag");
                msg.body                  = mc::engine::make_payload<mc::engine::method_call_payload>(
                    wire_plus_s, mc::variants{wire_dict, mc::string("tail-alpha")});

                auto reply = client.proto->send_with_reply(std::move(msg), policy.timeout);
                if (reply.header.type != mc::engine::message_type::method_return) {
                    return 40;
                }
                auto* payload = reply.try_as<mc::engine::method_return_payload>();
                if (payload == nullptr || payload->value != mc::variant(mc::string("e2e-wire|tail-alpha"))) {
                    return 41;
                }
            }

            // 隐式 context：dispatcher 合并 wire dict 后进 TLS
            {
                mc::engine::message msg;
                msg.header.type           = mc::engine::message_type::method_call;
                msg.header.destination    = mc::string("mc.test.proxy.server");
                msg.header.path           = mc::string("/mc/test/proxy/calc");
                msg.header.interface_name = mc::string("org.test.Calc");
                msg.header.member_name    = mc::string("EchoImplicitWire");
                msg.body                  = mc::engine::make_payload<mc::engine::method_call_payload>(
                    wire_plus_s, mc::variants{wire_dict, mc::string("tail-beta")});

                auto reply = client.proto->send_with_reply(std::move(msg), policy.timeout);
                if (reply.header.type != mc::engine::message_type::method_return) {
                    return 42;
                }
                auto* payload = reply.try_as<mc::engine::method_return_payload>();
                if (payload == nullptr || payload->value != mc::variant(mc::string("e2e-wire|tail-beta"))) {
                    return 43;
                }
            }

            // EchoCtxTag 错误签名："ss" 且首参非 dict → invalid_args
            {
                mc::engine::message msg;
                msg.header.type           = mc::engine::message_type::method_call;
                msg.header.destination    = "mc.test.proxy.server";
                msg.header.path           = "/mc/test/proxy/calc";
                msg.header.interface_name = "org.test.Calc";
                msg.header.member_name    = "EchoCtxTag";
                msg.body = mc::engine::make_payload<mc::engine::method_call_payload>("ss", mc::variants{"bad", "tail"});

                auto reply = client.proto->send_with_reply(std::move(msg), policy.timeout);
                if (reply.header.type != mc::engine::message_type::error) {
                    return 44;
                }
                if (reply.header.error_name != mc::string(mc::dbus::error_names::invalid_args)) {
                    return 45;
                }
            }

            return 0;
        });
    });
}
