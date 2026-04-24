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

// app_proto 单元测试：覆盖路由到 dbus_proto 的 push 语义。
// MQ 分支依赖进程级 mq_channel，在骨架阶段通过 nullptr 注入跳过；
// 集成测试在 test_application / test_endpoint_service 中补齐。

#include <gtest/gtest.h>
#include <mc/app/app_proto.h>
#include <mc/dbus/connection.h>
#include <mc/engine/service_proto.h>
#include <mc/engine/message.h>
#include <mc/protocol/request.h>

namespace test_app_proto {

mc::engine::message _make_signal(mc::string interface_name, mc::string member_name)
{
    mc::engine::message msg;
    msg.header.type           = mc::engine::message_type::signal;
    msg.header.interface_name = std::move(interface_name);
    msg.header.member_name    = std::move(member_name);
    msg.header.sender         = "mc.test.app_proto";
    return msg;
}

void _push_message(mc::app::app_proto& proto, const mc::engine::message& msg)
{
    mc::proto::proto_request req;
    auto& ctx = req.ensure_context<mc::engine::service_proto::message_context>(&proto);
    ctx.msg   = msg;
    proto.push(req);
}

TEST(app_proto_test, push_routes_to_dbus_child_by_default)
{
    mc::app::app_proto proto("mc.test.app_proto", mc::dbus::connection{}, /*mq_channel=*/nullptr);

    EXPECT_EQ(proto.dbus().outbound_count(), 0U);

    _push_message(proto, _make_signal("org.test.app_proto", "Tick"));

    EXPECT_EQ(proto.dbus().outbound_count(), 1U);
}

TEST(app_proto_test, push_routes_to_dbus_when_source_is_dbus)
{
    mc::app::app_proto proto("mc.test.dbus_route", mc::dbus::connection{}, /*mq_channel=*/nullptr);

    auto msg = _make_signal("org.test.dbus", "Ping");
    msg.header.context[std::string(mc::app::message_source_key)] = mc::variant(mc::string("dbus"));

    _push_message(proto, msg);

    EXPECT_EQ(proto.dbus().outbound_count(), 1U);
}

TEST(app_proto_test, push_skips_dbus_when_source_is_mq)
{
    mc::app::app_proto proto("mc.test.mq_route", mc::dbus::connection{}, /*mq_channel=*/nullptr);

    auto msg = _make_signal("org.test.mq", "Ping");
    msg.header.context[std::string(mc::app::message_source_key)] = mc::variant(mc::string("mq"));

    _push_message(proto, msg);

    // mq_channel 为 nullptr，dbus 不应被触发
    EXPECT_EQ(proto.dbus().outbound_count(), 0U);
}

TEST(app_proto_test, accessors_return_constructor_values)
{
    mc::app::app_proto proto("mc.test.app_proto.acc", mc::dbus::connection{}, /*mq_channel=*/nullptr);

    EXPECT_EQ(proto.service_name(), mc::string_view("mc.test.app_proto.acc"));
    EXPECT_EQ(proto.dbus().service_name(), mc::string_view("mc.test.app_proto.acc"));
}

} // namespace test_app_proto
