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

#include <mc/engine/service_proto.h>
#include <mc/engine/std_interface.h>
#include <mc/protocol.h>
#include <mc/quark.h>
#include <mc/string.h>

namespace {

mc::string buffer_payload_view(const mc::proto::proto_request& req)
{
    const auto& buffer = req.buffer();
    const auto  base   = buffer.payload_base();
    const auto  size   = buffer.length() >= base ? buffer.length() - base : 0U;
    return mc::string(reinterpret_cast<const char*>(buffer.data() + base), size);
}

mc::engine::message make_method_call_message()
{
    mc::engine::message msg;
    msg.header.type        = mc::engine::message_type::method_call;
    msg.header.member_name = "add";
    msg.header.serial      = 7;
    msg.body               = mc::engine::make_payload<mc::engine::method_call_payload>("ii", mc::variants{1, 2});
    return msg;
}

mc::engine::message make_method_return_message(std::uint64_t reply_serial)
{
    mc::engine::message msg;
    msg.header.type         = mc::engine::message_type::method_return;
    msg.header.reply_serial = reply_serial;
    msg.body                = mc::engine::make_payload<mc::engine::method_return_payload>(mc::variant(3), "i");
    return msg;
}

class capture_transport : public mc::proto::protocol {
public:
    mc::string last_payload;
    int        push_hits{0};
    int        pop_hits{0};

protected:
    mc::proto::execution_state on_push(mc::proto::proto_request& req) override
    {
        ++push_hits;
        last_payload = buffer_payload_view(req);
        return complete(req);
    }

    mc::proto::execution_state on_pop(mc::proto::proto_request& req) override
    {
        ++pop_hits;
        return pop_next(req);
    }
};

TEST(service_proto, push_forwards_to_child_without_encode)
{
    mc::engine::service_proto engine;
    capture_transport         transport;
    engine.add_child(transport);

    mc::proto::proto_request req;
    auto&                    ctx = req.ensure_context<mc::engine::service_proto::message_context>(&engine);
    auto                     msg = make_method_call_message();
    ctx.msg                      = msg;

    const auto state = engine.push(req);

    // service_proto::on_push 不再统一 encode，仅负责 push_next 给子节点
    EXPECT_EQ(state, mc::proto::execution_state::completed);
    EXPECT_EQ(transport.push_hits, 1);
    EXPECT_TRUE(transport.last_payload.empty());
}

TEST(service_proto, pop_routes_response_back_to_same_transport)
{
    mc::engine::service_proto engine;
    capture_transport         transport;
    engine.add_child(transport);
    engine.set_inbound_handler([](mc::engine::message request) {
        EXPECT_EQ(request.header.type, mc::engine::message_type::method_call);
        EXPECT_EQ(request.header.member_name, "add");
        return make_method_return_message(request.header.serial);
    });

    mc::proto::proto_request req;
    auto                     inbound = make_method_call_message().to_bytes();
    req.buffer().append_payload(inbound.data(), inbound.size());

    const auto state = transport.pop(req);

    EXPECT_EQ(state, mc::proto::execution_state::completed);
    EXPECT_EQ(transport.pop_hits, 1);
    EXPECT_EQ(transport.push_hits, 1);

    auto* ctx = req.find_context<mc::engine::service_proto::message_context>(&engine);
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->msg.header.type, mc::engine::message_type::method_return);
    EXPECT_EQ(ctx->msg.header.reply_serial, 7u);

    const auto outbound = mc::engine::message::from_bytes(transport.last_payload);
    EXPECT_EQ(outbound.header.type, mc::engine::message_type::method_return);
    EXPECT_EQ(outbound.header.reply_serial, 7u);
}

TEST(message_codec, decoded_known_header_strings_use_quark_storage)
{
    // 预先注册测试用 quark
    const auto path_quark = MC_QUARK("/mc/test/object");

    const auto& interface_str = mc::engine::std_ifaces::properties;
    const auto& member_str    = mc::engine::std_ifaces::get;

    mc::engine::message msg;
    msg.header.type           = mc::engine::message_type::method_call;
    msg.header.path           = "/mc/test/object";
    msg.header.interface_name = "org.freedesktop.DBus.Properties";
    msg.header.member_name    = "Get";
    msg.header.destination    = "org.example.DynamicDestination";
    msg.header.serial         = 7;
    msg.body                  = mc::engine::make_payload<mc::engine::method_call_payload>("ss", mc::variants{});

    // 二进制编解码路径：已注册的 quark 字符串会自动转为 quark backend
    auto decoded = mc::engine::message::from_bytes(msg.to_bytes());
    EXPECT_TRUE(decoded.header.path.is_quark());
    EXPECT_TRUE(decoded.header.interface_name.is_quark());
    EXPECT_TRUE(decoded.header.member_name.is_quark());
    EXPECT_FALSE(decoded.header.destination.is_quark());

    // quark-backed mc::string 与框架预定义常量直接比较
    EXPECT_EQ(decoded.header.interface_name, interface_str);
    EXPECT_EQ(decoded.header.member_name, member_str);

    // dict 编解码路径：同样会走 try_from 规范化
    mc::dict variant_dict;
    mc::engine::message::to_variant(msg, variant_dict);
    mc::engine::message variant_decoded;
    mc::engine::message::from_variant(variant_dict, variant_decoded);
    EXPECT_TRUE(variant_decoded.header.interface_name.is_quark());
    EXPECT_TRUE(variant_decoded.header.member_name.is_quark());
    EXPECT_EQ(variant_decoded.header.interface_name, interface_str);
    EXPECT_EQ(variant_decoded.header.member_name, member_str);
    EXPECT_FALSE(variant_decoded.header.destination.is_quark());
}

} // namespace
