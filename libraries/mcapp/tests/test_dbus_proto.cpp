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
#include <mc/app/dbus_proto.h>
#include <mc/dbus/connection.h>
#include <mc/dbus/message.h>
#include <mc/engine/message.h>
#include <mc/engine/payload.h>
#include <mc/engine/service_proto.h>
#include <mc/protocol/request.h>
#include <mc/runtime/runtime_context.h>
#include <mc/signal/connection.h>
#include <test_utilities/test_base.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <optional>

namespace test_dbus_proto {
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

mc::engine::message make_method_return(mc::variant value, mc::string_view signature = "s")
{
    mc::engine::message msg;
    msg.header.type = mc::engine::message_type::method_return;
    msg.body        = mc::engine::make_payload<mc::engine::method_return_payload>(std::move(value), signature);
    return msg;
}

} // namespace

class dbus_proto_test : public mc::test::TestWithDbusDaemon {};

TEST_F(dbus_proto_test, default_outbound_count_is_zero)
{
    mc::app::dbus_proto proto("mc.test.dbus_proto.zero", mc::dbus::connection{});

    EXPECT_EQ(proto.outbound_count(), 0U);
    EXPECT_EQ(proto.inbound_count(), 0U);
    EXPECT_EQ(proto.service_name(), mc::string_view("mc.test.dbus_proto.zero"));
}

TEST_F(dbus_proto_test, inbound_method_call_roundtrips_through_handler)
{
    const auto server_name = make_unique_service_name("mc.test.dbus_proto.server");
    const auto path        = mc::string("/mc/test/dbus_proto/server");
    const auto interface   = mc::string("org.test.dbus_proto.Server");
    const auto member      = mc::string("Echo");

    auto               server_conn = open_test_connection(server_name);
    auto               peer_conn   = open_test_connection(make_unique_service_name("mc.test.dbus_proto.peer"));
    mc::app::app_proto proto(server_name, server_conn, /*mq_channel=*/nullptr);

    std::atomic<int>                   handler_invocations{0};
    std::optional<mc::engine::message> captured_call;
    proto.set_inbound_handler([&](mc::engine::message msg) -> mc::engine::message {
        handler_invocations.fetch_add(1, std::memory_order_relaxed);
        captured_call            = msg;
        const auto& call_payload = msg.as<mc::engine::method_call_payload>();
        const auto  arg_str      = call_payload.args.front().as<mc::string>();
        return make_method_return(mc::variant(mc::string("pong:") + arg_str));
    });

    auto call_wire = mc::dbus::message::new_method_call(server_name, path, interface, member);
    {
        auto writer = call_wire.writer();
        writer.write_variant_value(mc::variant(mc::string("ping")));
    }

    auto future = peer_conn.async_send_with_reply(std::move(call_wire), mc::seconds(5));
    auto reply  = std::move(future).get();

    ASSERT_TRUE(reply.is_method_return()) << "expected method_return, got " << static_cast<int>(reply.get_type());

    auto reply_args = reply.read_args();
    ASSERT_EQ(reply_args.size(), 1U);
    EXPECT_EQ(reply_args.front(), mc::variant(mc::string("pong:ping")));

    EXPECT_EQ(handler_invocations.load(), 1);
    ASSERT_TRUE(captured_call.has_value());
    EXPECT_EQ(captured_call->header.type, mc::engine::message_type::method_call);
    EXPECT_EQ(captured_call->header.path, path);
    EXPECT_EQ(captured_call->header.interface_name, interface);
    EXPECT_EQ(captured_call->header.member_name, member);
    EXPECT_EQ(captured_call->header.destination, server_name);
    EXPECT_EQ(captured_call->as<mc::engine::method_call_payload>().args.front(), mc::variant(mc::string("ping")));

    EXPECT_GE(proto.dbus().inbound_count(), 1U);
    EXPECT_GE(proto.dbus().outbound_count(), 1U);
}

TEST_F(dbus_proto_test, inbound_method_call_preserves_multiple_return_values)
{
    const auto server_name = make_unique_service_name("mc.test.dbus_proto.server.multi");
    const auto path        = mc::string("/mc/test/dbus_proto/server/multi");
    const auto interface   = mc::string("org.test.dbus_proto.Server");
    const auto member      = mc::string("EchoMany");

    auto               server_conn = open_test_connection(server_name);
    auto               peer_conn   = open_test_connection(make_unique_service_name("mc.test.dbus_proto.peer"));
    mc::app::app_proto proto(server_name, server_conn, /*mq_channel=*/nullptr);

    proto.set_inbound_handler([&](mc::engine::message msg) -> mc::engine::message {
        const auto& call_payload = msg.as<mc::engine::method_call_payload>();
        const auto  arg_str      = call_payload.args.front().as<mc::string>();
        return make_method_return(mc::variant(mc::variants{mc::variant(mc::string("pong")), mc::variant(arg_str)}),
                                  "ss");
    });

    auto call_wire = mc::dbus::message::new_method_call(server_name, path, interface, member);
    {
        auto writer = call_wire.writer();
        writer.write_variant_value(mc::variant(mc::string("ping")));
    }

    auto future = peer_conn.async_send_with_reply(std::move(call_wire), mc::seconds(5));
    auto reply  = std::move(future).get();

    ASSERT_TRUE(reply.is_method_return());
    auto reply_args = reply.read_args();
    ASSERT_EQ(reply_args.size(), 2U);
    EXPECT_EQ(reply_args[0], mc::variant(mc::string("pong")));
    EXPECT_EQ(reply_args[1], mc::variant(mc::string("ping")));
}

TEST_F(dbus_proto_test, outbound_method_call_delivers_reply_upstream)
{
    const auto         client_name = make_unique_service_name("mc.test.dbus_proto.client");
    auto               client_conn = open_test_connection(client_name);
    mc::app::app_proto proto(client_name, client_conn, /*mq_channel=*/nullptr);

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

    auto call_msg =
        make_noarg_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");
    mc::proto::proto_request req;
    auto&                    ctx = req.ensure_context<mc::engine::service_proto::message_context>(&proto);
    ctx.msg                      = call_msg;
    ASSERT_EQ(proto.push(req), mc::proto::execution_state::completed);
    EXPECT_GE(proto.dbus().outbound_count(), 1U);

    std::unique_lock lock(mutex);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&]() {
        return inbound_reply.has_value();
    }));
    ASSERT_TRUE(inbound_reply.has_value());
    EXPECT_EQ(inbound_reply->header.type, mc::engine::message_type::method_return);
    ASSERT_NE(inbound_reply->try_as<mc::engine::method_return_payload>(), nullptr);
    ASSERT_TRUE(inbound_reply->as<mc::engine::method_return_payload>().value.is_array());

    bool found_self = false;
    for (const auto& entry : inbound_reply->as<mc::engine::method_return_payload>().value.get_array()) {
        if (entry.is_string() && entry.get_string() == client_name) {
            found_self = true;
            break;
        }
    }
    EXPECT_TRUE(found_self) << "ListNames 应返回当前 client bus name: " << client_name;
}

TEST_F(dbus_proto_test, outbound_method_call_writes_args_with_payload_signature)
{
    const auto          client_name = make_unique_service_name("mc.test.dbus_proto.client.sig");
    const auto          server_name = make_unique_service_name("mc.test.dbus_proto.server.sig");
    auto                client_conn = open_test_connection(client_name);
    auto                server_conn = open_test_connection(server_name);
    mc::app::dbus_proto proto(client_name, client_conn);

    std::mutex                  mutex;
    std::condition_variable     cv;
    std::optional<mc::string>   captured_signature;
    std::optional<mc::variants> captured_args;

    mc::connection slot = server_conn.filter_message().connect([&](mc::dbus::message& wire_msg) -> DBusHandlerResult {
        if (!wire_msg.is_method_call() || wire_msg.get_destination() != std::string_view(server_name)) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }

        {
            std::lock_guard lock(mutex);
            captured_signature = mc::string(wire_msg.get_signature());
            captured_args      = wire_msg.read_args();
        }
        cv.notify_all();

        auto reply = mc::dbus::message::new_method_return(wire_msg);
        server_conn.send(std::move(reply));
        server_conn.flush();
        return DBUS_HANDLER_RESULT_HANDLED;
    });
    ASSERT_TRUE(slot.connected());

    mc::dict            props{{"Name", mc::variant(mc::string("sensor"))}, {"State", mc::variant(mc::string("ok"))}};
    mc::engine::message call_msg;
    call_msg.header.type           = mc::engine::message_type::method_call;
    call_msg.header.destination    = server_name;
    call_msg.header.path           = "/mc/test/dbus_proto/sig";
    call_msg.header.interface_name = "org.test.dbus_proto.Signature";
    call_msg.header.member_name    = "SetProperties";
    call_msg.body =
        mc::engine::make_payload<mc::engine::method_call_payload>("a{sv}", mc::variants{mc::variant(props)});

    auto reply = proto.async_send_with_reply(std::move(call_msg), mc::seconds(5)).get();
    ASSERT_EQ(reply.header.type, mc::engine::message_type::method_return);

    std::unique_lock lock(mutex);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&]() {
        return captured_signature.has_value();
    }));
    EXPECT_EQ(*captured_signature, "a{sv}");
    ASSERT_TRUE(captured_args.has_value());
    ASSERT_EQ(captured_args->size(), 1U);
    EXPECT_EQ(captured_args->front(), mc::variant(props));
}

TEST_F(dbus_proto_test, outbound_method_call_preserves_multiple_reply_values_upstream)
{
    const auto client_name = make_unique_service_name("mc.test.dbus_proto.client.multi");
    const auto server_name = make_unique_service_name("mc.test.dbus_proto.server.multi");
    auto       client_conn = open_test_connection(client_name);
    auto       server_conn = open_test_connection(server_name);
    mc::app::app_proto proto(client_name, client_conn, /*mq_channel=*/nullptr);

    mc::connection slot = server_conn.filter_message().connect([&](mc::dbus::message& wire_msg) -> DBusHandlerResult {
        if (!wire_msg.is_method_call() || wire_msg.get_destination() != std::string_view(server_name)) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }

        auto reply  = mc::dbus::message::new_method_return(wire_msg);
        auto writer = reply.writer();
        writer.write_variant(mc::dbus::signature_iterator("s"), mc::variant(mc::string("first")), 0);
        writer.write_variant(mc::dbus::signature_iterator("s"), mc::variant(mc::string("second")), 0);
        server_conn.send(std::move(reply));
        server_conn.flush();
        return DBUS_HANDLER_RESULT_HANDLED;
    });
    ASSERT_TRUE(slot.connected());

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

    auto                     call_msg = make_noarg_method_call(server_name, "/mc/test/dbus_proto/multi",
                                                               "org.test.dbus_proto.Client", "AskMany");
    mc::proto::proto_request req;
    auto&                    ctx = req.ensure_context<mc::engine::service_proto::message_context>(&proto);
    ctx.msg                      = call_msg;
    ASSERT_EQ(proto.push(req), mc::proto::execution_state::completed);

    std::unique_lock lock(mutex);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&]() {
        return inbound_reply.has_value();
    }));
    ASSERT_TRUE(inbound_reply.has_value());
    const auto& payload = inbound_reply->as<mc::engine::method_return_payload>();
    EXPECT_EQ(payload.signature, "ss");
    ASSERT_TRUE(payload.value.is_array());
    const auto& values = payload.value.get_array();
    ASSERT_EQ(values.size(), 2U);
    EXPECT_EQ(values[0], mc::variant(mc::string("first")));
    EXPECT_EQ(values[1], mc::variant(mc::string("second")));
}

TEST_F(dbus_proto_test, destructor_waits_for_running_inbound_handler)
{
    const auto server_name = make_unique_service_name("mc.test.dbus_proto.server.destroy");
    const auto peer_name   = make_unique_service_name("mc.test.dbus_proto.peer.destroy");
    const auto path        = mc::string("/mc/test/dbus_proto/destroy");
    const auto interface   = mc::string("org.test.dbus_proto.Destroy");
    const auto member      = mc::string("Block");

    auto server_conn = open_test_connection(server_name);
    auto peer_conn   = open_test_connection(peer_name);
    auto proto       = std::make_unique<mc::app::app_proto>(server_name, server_conn, /*mq_channel=*/nullptr);

    std::promise<void>       handler_entered;
    auto                     entered = handler_entered.get_future();
    std::promise<void>       release_handler;
    auto                     release = release_handler.get_future();
    std::atomic<bool>        destroy_returned{false};
    std::atomic<bool>        reply_ready{false};

    proto->set_inbound_handler([&](mc::engine::message msg) -> mc::engine::message {
        MC_UNUSED(msg);
        handler_entered.set_value();
        release.wait();
        return make_method_return(mc::variant(mc::string("done")));
    });

    auto call_wire = mc::dbus::message::new_method_call(server_name, path, interface, member);
    auto reply_future = std::async(std::launch::async, [&]() {
        auto pending = peer_conn.async_send_with_reply(std::move(call_wire), mc::seconds(5));
        auto reply   = std::move(pending).get();
        reply_ready.store(reply.is_method_return(), std::memory_order_release);
    });

    ASSERT_EQ(entered.wait_for(std::chrono::seconds(2)), std::future_status::ready);

    auto destroy_future = std::async(std::launch::async, [&]() {
        proto.reset();
        destroy_returned.store(true, std::memory_order_release);
    });

    EXPECT_EQ(destroy_future.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);
    EXPECT_FALSE(destroy_returned.load(std::memory_order_acquire));

    release_handler.set_value();
    ASSERT_EQ(destroy_future.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    ASSERT_EQ(reply_future.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    EXPECT_TRUE(reply_ready.load(std::memory_order_acquire));
}

} // namespace test_dbus_proto
