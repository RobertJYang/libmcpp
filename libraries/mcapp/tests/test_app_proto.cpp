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
#include <mc/dbus/connection.h>
#include <mc/engine/message.h>
#include <mc/engine/payload.h>
#include <mc/engine/service_proto.h>
#include <mc/protocol/request.h>
#include <mc/runtime/runtime_context.h>
#include <test_utilities/test_base.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>

namespace test_app_proto {
namespace {

mc::string make_unique_service_name(mc::string_view prefix)
{
    static std::atomic<std::uint64_t> seq{0};
    const auto ticks = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto n     = seq.fetch_add(1, std::memory_order_relaxed);
    return mc::string(prefix) + ".s" + std::to_string(ticks) + "x" + std::to_string(n);
}

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

mc::engine::message make_method_call(mc::string destination, mc::string path, mc::string interface_name,
                                     mc::string member_name, mc::string arg)
{
    mc::engine::message msg;
    msg.header.type           = mc::engine::message_type::method_call;
    msg.header.destination    = std::move(destination);
    msg.header.path           = std::move(path);
    msg.header.interface_name = std::move(interface_name);
    msg.header.member_name    = std::move(member_name);
    msg.body =
        mc::engine::make_payload<mc::engine::method_call_payload>("s", mc::variants{mc::variant(std::move(arg))});
    return msg;
}

mc::engine::message make_noarg_method_call(mc::string_view destination, mc::string_view path,
                                           mc::string_view interface_name, mc::string_view member_name)
{
    mc::engine::message msg;
    msg.header.type           = mc::engine::message_type::method_call;
    msg.header.destination    = mc::string(destination);
    msg.header.path           = mc::string(path);
    msg.header.interface_name = mc::string(interface_name);
    msg.header.member_name    = mc::string(member_name);
    msg.body                  = mc::engine::make_payload<mc::engine::method_call_payload>("", mc::variants{});
    return msg;
}

mc::proto::execution_state push_message(mc::app::app_proto& proto, const mc::engine::message& msg)
{
    mc::proto::proto_request req;
    auto&                    ctx = req.ensure_context<mc::engine::service_proto::message_context>(&proto);
    ctx.msg                      = msg;
    return proto.push(req);
}

bool wait_for_reply(std::mutex& mutex, std::condition_variable& cv, std::optional<mc::engine::message>& inbound_reply,
                    std::chrono::milliseconds timeout)
{
    std::unique_lock lock(mutex);
    return cv.wait_for(lock, timeout, [&]() {
        return inbound_reply.has_value();
    });
}

} // namespace

class app_proto_test : public mc::test::TestWithDbusDaemon {};

// 无 source 时推给 dbus 子协议（本测试 mq_channel 为空，不经 MQ）。
TEST_F(app_proto_test, push_routes_to_dbus_child_by_default)
{
    const auto sender_name = make_unique_service_name("mc.test.app_proto.sender");

    auto               sender_conn = open_test_connection(sender_name);
    mc::app::app_proto proto(sender_name, sender_conn, /*mq_channel=*/nullptr);
    std::mutex                         mutex;
    std::condition_variable            cv;
    std::optional<mc::engine::message> inbound_reply;
    proto.set_inbound_handler([&](mc::engine::message msg) -> mc::engine::message {
        {
            std::lock_guard lock(mutex);
            inbound_reply = std::move(msg);
        }
        cv.notify_all();
        return {};
    });

    auto msg = make_noarg_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                                      "ListNames");
    EXPECT_EQ(push_message(proto, msg), mc::proto::execution_state::completed);
    ASSERT_TRUE(wait_for_reply(mutex, cv, inbound_reply, std::chrono::seconds(2)));
    ASSERT_TRUE(inbound_reply.has_value());
    EXPECT_EQ(proto.dbus().outbound_count(), 1U);
}

// source=dbus 时走 dbus 子协议。
TEST_F(app_proto_test, push_routes_to_dbus_when_source_is_dbus)
{
    const auto sender_name = make_unique_service_name("mc.test.dbus_route.sender");

    auto               sender_conn = open_test_connection(sender_name);
    mc::app::app_proto proto(sender_name, sender_conn, /*mq_channel=*/nullptr);
    std::mutex                         mutex;
    std::condition_variable            cv;
    std::optional<mc::engine::message> inbound_reply;
    proto.set_inbound_handler([&](mc::engine::message msg) -> mc::engine::message {
        {
            std::lock_guard lock(mutex);
            inbound_reply = std::move(msg);
        }
        cv.notify_all();
        return {};
    });

    auto msg = make_noarg_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                                      "ListNames");
    msg.header.context[std::string(mc::app::message_source_key)] = mc::variant(mc::string("dbus"));
    EXPECT_EQ(push_message(proto, msg), mc::proto::execution_state::completed);
    ASSERT_TRUE(wait_for_reply(mutex, cv, inbound_reply, std::chrono::seconds(2)));
    ASSERT_TRUE(inbound_reply.has_value());
    EXPECT_EQ(proto.dbus().outbound_count(), 1U);
}

// source=mq 时绕开 dbus 外发；无 mq_channel 时消息被丢弃。
TEST_F(app_proto_test, push_skips_dbus_when_source_is_mq)
{
    const auto sender_name = make_unique_service_name("mc.test.mq_route.sender");
    const auto path        = mc::string("/mc/test/app_proto/mq");
    const auto interface   = mc::string("org.test.mq");
    const auto member      = mc::string("Ping");

    auto               sender_conn = open_test_connection(sender_name);
    mc::app::app_proto proto(sender_name, sender_conn, /*mq_channel=*/nullptr);

    auto msg = make_method_call(make_unique_service_name("mc.test.mq_route.target"), path, interface, member,
                                mc::string("mq-route"));
    msg.header.context[std::string(mc::app::message_source_key)] = mc::variant(mc::string("mq"));
    EXPECT_EQ(push_message(proto, msg), mc::proto::execution_state::completed);
    EXPECT_EQ(proto.dbus().outbound_count(), 0U);
}

TEST(app_proto_accessors_test, accessors_return_constructor_values)
{
    mc::app::app_proto proto("mc.test.app_proto.acc", mc::dbus::connection{}, /*mq_channel=*/nullptr);

    EXPECT_EQ(proto.service_name(), mc::string_view("mc.test.app_proto.acc"));
    EXPECT_EQ(proto.dbus().service_name(), mc::string_view("mc.test.app_proto.acc"));
}

} // namespace test_app_proto
