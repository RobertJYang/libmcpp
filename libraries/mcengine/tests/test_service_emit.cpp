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

// 端到端覆盖 service::emit 的 match 聚合 + 本地 route_inbound / 远端经
// service::get_proto() outbound 的路由选择。默认 match_table 下 endpoint 均为
// 0，即全部视为本进程目标 —— 验证命中数量、sender 自动补齐、空目标静默。

#include <gtest/gtest.h>
#include <mc/engine/engine.h>
#include <mc/engine/match.h>
#include <mc/engine/message.h>
#include <mc/engine/object.h>
#include <mc/engine/payload.h>
#include <mc/engine/property.h>
#include <mc/engine/service.h>
#include <mc/engine/service_proto.h>
#include <test_utilities/engine_test_base.h>

#include <atomic>
#include <utility>

namespace test_service_emit {

struct sample_service : public mc::engine::service {
    explicit sample_service(mc::string name) : mc::engine::service(std::move(name))
    {}
};

class props_interface : public mc::engine::interface<props_interface> {
public:
    MC_INTERFACE("org.test.emit.Props")

    mc::engine::property<int32_t> Counter;
};

class props_object : public mc::engine::object<props_object> {
public:
    MC_OBJECT(props_object, "EmitPropsObject", "/mc/test/emit/props", (props_interface))

    props_interface iface;
};

class service_emit_test : public mc::test::TestWithEngine {};

TEST_F(service_emit_test, emit_without_targets_is_silent_noop)
{
    sample_service svc("mc.test.emit.sender");
    svc.init();
    svc.start();

    mc::engine::message msg;
    msg.header.type           = mc::engine::message_type::signal;
    msg.header.interface_name = "org.test.noop";
    msg.header.member_name    = "Empty";
    EXPECT_NO_THROW(svc.emit(msg));

    svc.stop();
}

TEST_F(service_emit_test, emit_local_target_routes_via_route_inbound)
{
    sample_service sender("mc.test.emit.sender_local");
    sender.init();
    sender.start();

    sample_service receiver("mc.test.emit.receiver_local");
    receiver.init();
    receiver.start();

    mc::engine::match_rule rule;
    rule.type           = "signal";
    rule.interface_name = "org.test.emit";
    rule.member_name    = "LocalHit";

    std::atomic<int> hits{0};
    auto             id = receiver.add_match(rule, mc::engine::filter_spec{}, [&](const mc::engine::message& /*m*/) {
        hits.fetch_add(1, std::memory_order_acq_rel);
    });
    ASSERT_NE(id, 0u);

    mc::engine::message msg;
    msg.header.type           = mc::engine::message_type::signal;
    msg.header.interface_name = "org.test.emit";
    msg.header.member_name    = "LocalHit";
    sender.emit(msg);

    EXPECT_EQ(hits.load(std::memory_order_acquire), 1);

    receiver.remove_match(id);
    sender.stop();
    receiver.stop();
}

TEST_F(service_emit_test, emit_auto_fills_sender_when_empty)
{
    sample_service sender("mc.test.emit.sender_fill");
    sender.init();
    sender.start();

    sample_service receiver("mc.test.emit.receiver_fill");
    receiver.init();
    receiver.start();

    mc::engine::match_rule rule;
    rule.type           = "signal";
    rule.interface_name = "org.test.emit.sender";

    std::atomic<int> hits{0};
    mc::string       captured_sender;
    auto             id = receiver.add_match(rule, mc::engine::filter_spec{}, [&](const mc::engine::message& m) {
        hits.fetch_add(1, std::memory_order_acq_rel);
        captured_sender = m.header.sender;
    });
    ASSERT_NE(id, 0u);

    mc::engine::message msg;
    msg.header.type           = mc::engine::message_type::signal;
    msg.header.interface_name = "org.test.emit.sender";
    msg.header.member_name    = "Ping";
    sender.emit(msg);

    EXPECT_EQ(hits.load(std::memory_order_acquire), 1);
    EXPECT_EQ(captured_sender, mc::string("mc.test.emit.sender_fill"));

    receiver.remove_match(id);
    sender.stop();
    receiver.stop();
}

TEST_F(service_emit_test, emit_fanout_to_multiple_receivers)
{
    sample_service sender("mc.test.emit.sender_fanout");
    sender.init();
    sender.start();

    sample_service receiver_a("mc.test.emit.fanout_a");
    sample_service receiver_b("mc.test.emit.fanout_b");
    receiver_a.init();
    receiver_a.start();
    receiver_b.init();
    receiver_b.start();

    mc::engine::match_rule rule;
    rule.type           = "signal";
    rule.interface_name = "org.test.emit.fanout";
    rule.member_name    = "Tick";

    std::atomic<int> hits_a{0};
    std::atomic<int> hits_b{0};
    auto             id_a = receiver_a.add_match(rule, mc::engine::filter_spec{}, [&](const mc::engine::message&) {
        hits_a.fetch_add(1);
    });
    auto             id_b = receiver_b.add_match(rule, mc::engine::filter_spec{}, [&](const mc::engine::message&) {
        hits_b.fetch_add(1);
    });

    mc::engine::message msg;
    msg.header.type           = mc::engine::message_type::signal;
    msg.header.interface_name = "org.test.emit.fanout";
    msg.header.member_name    = "Tick";
    sender.emit(msg);

    EXPECT_EQ(hits_a.load(), 1);
    EXPECT_EQ(hits_b.load(), 1);

    receiver_a.remove_match(id_a);
    receiver_b.remove_match(id_b);
    sender.stop();
    receiver_a.stop();
    receiver_b.stop();
}

} // namespace test_service_emit

MC_REFLECT(test_service_emit::props_interface, ((Counter, "Counter")))
MC_REFLECT(test_service_emit::props_object, ((iface, "Iface")))

namespace test_service_emit {

TEST_F(service_emit_test, invalidated_property_changed_uses_invalidated_properties_body)
{
    sample_service sender("mc.test.emit.invalidated.sender");
    sample_service receiver("mc.test.emit.invalidated.receiver");
    sender.init();
    receiver.init();
    sender.start();
    receiver.start();

    auto obj = props_object::create();
    obj->set_object_name("emit_invalidated_props");
    obj->iface.set_property_invalidated("Counter");
    sender.register_object(obj);

    mc::engine::match_rule rule;
    rule.type           = "signal";
    rule.interface_name = "org.freedesktop.DBus.Properties";
    rule.member_name    = "PropertiesChanged";

    std::atomic<int> hits{0};
    std::atomic<int> body_ok{0};
    auto             id = receiver.add_match(rule, mc::engine::filter_spec{}, [&](const mc::engine::message& msg) {
        if (const auto* payload = msg.try_as<mc::engine::signal_payload>()) {
            if (payload->signature == "sa{sv}as" && payload->args.size() == 3 &&
                payload->args[0] == mc::variant(mc::string("org.test.emit.Props")) && payload->args[1].is_dict() &&
                payload->args[2].is_array()) {
                const auto changed     = payload->args[1].as<mc::dict>();
                const auto invalidated = payload->args[2].as<mc::variants>();
                if (changed.empty() && invalidated.size() == 1U &&
                    invalidated[0] == mc::variant(mc::string("Counter"))) {
                    body_ok = 1;
                }
            }
        }
        ++hits;
    });
    ASSERT_NE(id, 0u);

    obj->iface.Counter = int32_t{77};

    EXPECT_GE(hits.load(), 1);
    EXPECT_EQ(body_ok.load(), 1);

    receiver.remove_match(id);
    sender.stop();
    receiver.stop();
}

TEST_F(service_emit_test, unregistered_object_property_change_is_suppressed)
{
    sample_service sender("mc.test.emit.unregistered.sender");
    sample_service receiver("mc.test.emit.unregistered.receiver");
    sender.init();
    receiver.init();
    sender.start();
    receiver.start();

    mc::engine::match_rule rule;
    rule.type           = "signal";
    rule.interface_name = "org.freedesktop.DBus.Properties";
    rule.member_name    = "PropertiesChanged";

    std::atomic<int> hits{0};
    auto             id = receiver.add_match(rule, mc::engine::filter_spec{}, [&](const mc::engine::message&) {
        hits.fetch_add(1, std::memory_order_acq_rel);
    });
    ASSERT_NE(id, 0u);

    auto obj = props_object::create();
    obj->set_object_name("emit_unregistered_props");
    obj->set_service(&sender);

    obj->iface.Counter = int32_t{42};
    EXPECT_EQ(hits.load(std::memory_order_acquire), 0);

    sender.register_object(obj);
    obj->iface.Counter = int32_t{43};
    EXPECT_GT(hits.load(std::memory_order_acquire), 0);

    receiver.remove_match(id);
    sender.stop();
    receiver.stop();
}

} // namespace test_service_emit
