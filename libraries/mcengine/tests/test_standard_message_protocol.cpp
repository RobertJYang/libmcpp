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
#include <mc/engine.h>
#include <mc/engine/dispatcher.h>
#include <mc/engine/engine_proto.h>
#include <mc/engine/message_codec.h>
#include <mc/io/io_buffer.h>
#include <mc/io/io_stream.h>
#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/message_queue/mq_proto.h>
#include <mc/shm/message_queue/mq_transport_proto.h>
#include <mc/shm/mq.h>
#include <test_utilities/engine_test_base.h>

#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mc/reflect.h>
#include <string>
#include <unistd.h>
#include <vector>

namespace standard_message_protocol_test {

using namespace std::chrono_literals;

constexpr uint32_t test_message_magic             = 0x4d43454dU;
constexpr uint16_t test_message_version           = 2;
constexpr uint32_t test_payload_magic             = 0x4d43504cU;
constexpr uint16_t test_payload_version           = 1;
constexpr uint8_t  test_body_encoding_dict        = 0;
constexpr uint8_t  test_body_encoding_field_tlv   = 1;
constexpr uint8_t  test_header_encoding_dict      = 0;
constexpr uint8_t  test_header_encoding_field_tlv = 1;

void write_length_prefixed_bytes(mc::io::io_stream& stream, mc::string_view bytes)
{
    stream.write_value<uint32_t>(static_cast<uint32_t>(bytes.size()));
    if (!bytes.empty()) {
        stream.write(bytes);
    }
}

mc::string_view read_length_prefixed_bytes(mc::io::io_stream& stream)
{
    auto length = stream.read_value<uint32_t>();
    return length == 0 ? mc::string_view{} : stream.read(length);
}

mc::string encode_int32_variant_bytes(int32_t value)
{
    mc::io::io_stream stream(16);
    stream.write_value<uint8_t>(5);
    stream.write_value<int32_t>(value);
    return mc::string(stream.get_data());
}

struct decoded_message_layout {
    uint8_t         header_encoding{0};
    mc::string_view header_bytes;
    mc::string_view payload_bytes;
};

decoded_message_layout split_message_layout(mc::string_view message_bytes)
{
    mc::io::io_stream message_stream(mc::io::io_buffer::wrap(message_bytes.data(), message_bytes.size()), false);
    EXPECT_EQ(message_stream.read_value<uint32_t>(), test_message_magic);
    EXPECT_EQ(message_stream.read_value<uint16_t>(), test_message_version);

    decoded_message_layout layout;
    layout.header_encoding = message_stream.read_value<uint8_t>();
    layout.header_bytes    = read_length_prefixed_bytes(message_stream);
    layout.payload_bytes   = read_length_prefixed_bytes(message_stream);
    return layout;
}

mc::string assemble_message_bytes(uint8_t header_encoding, mc::string_view header_bytes, mc::string_view payload_bytes)
{
    mc::io::io_stream stream(512);
    stream.write_value<uint32_t>(test_message_magic);
    stream.write_value<uint16_t>(test_message_version);
    stream.write_value<uint8_t>(header_encoding);
    write_length_prefixed_bytes(stream, header_bytes);
    write_length_prefixed_bytes(stream, payload_bytes);
    return mc::string(stream.get_data());
}

mc::string append_unknown_int32_field(mc::string_view message_bytes, uint32_t field_id, int32_t value)
{
    auto layout = split_message_layout(message_bytes);

    mc::io::io_stream payload_stream(mc::io::io_buffer::wrap(layout.payload_bytes.data(), layout.payload_bytes.size()),
                                     false);
    EXPECT_EQ(payload_stream.read_value<uint32_t>(), test_payload_magic);
    EXPECT_EQ(payload_stream.read_value<uint16_t>(), test_payload_version);
    auto payload_id    = payload_stream.read_value<mc::engine::payload_id_type>();
    auto message_kind  = payload_stream.read_value<uint32_t>();
    auto body_encoding = payload_stream.read_value<uint8_t>();
    EXPECT_EQ(body_encoding, test_body_encoding_field_tlv);
    auto body_bytes = read_length_prefixed_bytes(payload_stream);

    mc::io::io_stream body_stream(mc::io::io_buffer::wrap(body_bytes.data(), body_bytes.size()), false);
    auto              field_count = body_stream.read_value<uint32_t>();
    auto              field_tail  = body_stream.read(body_stream.readable_bytes());

    mc::io::io_stream new_body_stream(128);
    new_body_stream.write_value<uint32_t>(field_count + 1);
    if (!field_tail.empty()) {
        new_body_stream.write(field_tail);
    }
    new_body_stream.write_value<uint32_t>(field_id);
    write_length_prefixed_bytes(new_body_stream, encode_int32_variant_bytes(value));

    mc::io::io_stream new_payload_stream(256);
    new_payload_stream.write_value<uint32_t>(test_payload_magic);
    new_payload_stream.write_value<uint16_t>(test_payload_version);
    new_payload_stream.write_value<mc::engine::payload_id_type>(payload_id);
    new_payload_stream.write_value<uint32_t>(message_kind);
    new_payload_stream.write_value<uint8_t>(body_encoding);
    write_length_prefixed_bytes(new_payload_stream, new_body_stream.get_data());

    return assemble_message_bytes(layout.header_encoding, layout.header_bytes, new_payload_stream.get_data());
}

mc::string append_unknown_header_field(mc::string_view message_bytes, uint32_t field_id, int32_t value)
{
    auto layout = split_message_layout(message_bytes);
    EXPECT_EQ(layout.header_encoding, test_header_encoding_field_tlv);

    mc::io::io_stream header_stream(mc::io::io_buffer::wrap(layout.header_bytes.data(), layout.header_bytes.size()),
                                    false);
    auto              field_count = header_stream.read_value<uint32_t>();
    auto              field_tail  = header_stream.read(header_stream.readable_bytes());

    mc::io::io_stream new_header_stream(192);
    new_header_stream.write_value<uint32_t>(field_count + 1);
    if (!field_tail.empty()) {
        new_header_stream.write(field_tail);
    }
    new_header_stream.write_value<uint32_t>(field_id);
    write_length_prefixed_bytes(new_header_stream, encode_int32_variant_bytes(value));

    return assemble_message_bytes(layout.header_encoding, new_header_stream.get_data(), layout.payload_bytes);
}

class standard_test_interface : public mc::engine::interface<standard_test_interface> {
public:
    MC_INTERFACE("org.test.standard")

    int32_t add(int32_t lhs, int32_t rhs)
    {
        return lhs + rhs;
    }

    int32_t fail()
    {
        MC_REPLY_ERROR_AND_THROW(mc::engine::errors::not_supported);
    }
};

class standard_test_object : public mc::engine::object<standard_test_object> {
public:
    MC_OBJECT(standard_test_object, "StandardTestObject", "/org/test/standard", (standard_test_interface))

    standard_test_interface m_iface;
};

class standard_test_service : public mc::engine::service {
public:
    explicit standard_test_service(mc::string_view name) : mc::engine::service(name)
    {}
};

class custom_extension_payload
    : public mc::engine::payload<custom_extension_payload, 0x1001, mc::engine::message_type::signal> {
public:
    MC_REFLECTABLE("standard.message.custom_extension_payload");

    custom_extension_payload() = default;

    custom_extension_payload(mc::string_view topic_value, mc::variants args_value = {})
        : topic(topic_value), args(std::move(args_value))
    {}

    void to_variant(mc::dict& dict) const override
    {
        mc::reflect::to_variant(*this, dict);
    }

    mc::string   topic;
    mc::variants args;
};

mc::string sanitize_name(mc::string_view value)
{
    mc::string result;
    result.reserve(value.size());
    for (const unsigned char ch : value) {
        result.push_back(std::isalnum(ch) ? static_cast<char>(ch) : '_');
    }
    return result.empty() ? mc::string("default") : result;
}

mc::string make_notifier_name(mc::string_view prefix, mc::string_view queue_name)
{
    return "/tmp/" + mc::string(prefix) + sanitize_name(queue_name) + ".fifo";
}

struct protocol_chain {
    protocol_chain(mc::shm::mq_queue& queue, std::uint32_t writer_id, std::uint64_t writer_instance_id)
    {
        engine.add_child(mq);
        mq.add_child(transport);
        mq.configure(static_cast<std::uint32_t>(writer_instance_id), static_cast<std::uint16_t>(writer_id));
        transport.configure(queue, writer_id, writer_instance_id);
    }

    void send(const mc::engine::message& msg)
    {
        mc::proto::proto_request req;
        auto&                    ctx = req.ensure_context<mc::engine::engine_proto::message_context>(&engine);
        ctx.msg                      = msg;

        const auto state = engine.push(req);
        ASSERT_EQ(state, mc::proto::execution_state::completed);
    }

    mc::engine::message receive()
    {
        mc::proto::proto_request req;
        auto                     state = transport.pop(req);
        while (state == mc::proto::execution_state::suspended) {
            state = transport.resume(req);
        }

        EXPECT_EQ(state, mc::proto::execution_state::completed);
        auto* ctx = req.find_context<mc::engine::engine_proto::message_context>(&engine);
        EXPECT_NE(ctx, nullptr);
        return ctx == nullptr ? mc::engine::message{} : ctx->msg;
    }

    mc::engine::engine_proto    engine;
    mc::shm::mq_proto           mq;
    mc::shm::mq_transport_proto transport;
};

protocol_chain make_protocols(mc::shm::mq_queue& queue, std::uint32_t writer_id, std::uint32_t instance_id)
{
    return protocol_chain(queue, writer_id, instance_id);
}

class standard_message_protocol_fixture : public mc::test::TestWithEngine {
protected:
    void SetUp() override
    {
        TestWithEngine::SetUp();

        ASSERT_TRUE(m_service.init());
        ASSERT_TRUE(m_service.start());

        auto obj = standard_test_object::create();
        obj->set_object_name("standard_test_object");
        obj->set_position("0101");
        m_service.register_object(obj);
    }

    void TearDown() override
    {
        m_service.stop();
        for (const auto& name : m_queue_names) {
            std::remove(make_notifier_name("mc_shm_queue_", name).c_str());
            std::remove(make_notifier_name("mc_shm_queue_space_", name).c_str());
            mc::shm::detail::shared_memory_backend::remove(name);
        }
        m_queue_names.clear();
        TestWithEngine::TearDown();
    }

    mc::string make_queue_name(const char* base)
    {
        const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
        mc::string name   = "mc_engine_message_test_";
        name += base;
        name += "_";
        name += std::to_string(::getpid());
        name += "_";
        name += std::to_string(now_ns);
        m_queue_names.push_back(name);
        return name;
    }

    mc::shm::mq_queue make_queue(mc::string_view name)
    {
        mc::shm::mq_queue_options options;
        options.shared_memory_name = mc::string(name);
        options.slot_count         = 16;
        options.max_payload_size   = 4096;
        return mc::shm::mq_queue(options);
    }

    void send_message(const mc::engine::message& msg, protocol_chain& protocols)
    {
        protocols.send(msg);
    }

    mc::engine::message receive_message(protocol_chain& protocols)
    {
        return protocols.receive();
    }

    mc::engine::message make_method_call(mc::string_view member_name, mc::variants args = {}) const
    {
        mc::engine::message msg;
        msg.header.type           = mc::engine::message_type::method_call;
        msg.header.destination    = m_service.name();
        msg.header.sender         = "org.test.client";
        msg.header.path           = "/org/test/standard";
        msg.header.interface_name = "org.test.standard";
        msg.header.member_name    = mc::string(member_name);
        msg.header.serial         = 11;
        msg.body                  = mc::engine::make_payload<mc::engine::method_call_payload>("ii", std::move(args));
        return msg;
    }

    standard_test_service   m_service{"org.test.standard.service"};
    std::vector<mc::string> m_queue_names;
};

} // namespace standard_message_protocol_test

MC_REFLECT(standard_message_protocol_test::standard_test_interface, ((add, "Add"))((fail, "Fail")))
MC_REFLECT(standard_message_protocol_test::standard_test_object, ((m_iface, "iface")))
MC_REFLECT(standard_message_protocol_test::custom_extension_payload, (topic)(args))

using namespace standard_message_protocol_test;

TEST_F(standard_message_protocol_fixture, message_supports_reflect_and_bytes_roundtrip)
{
    auto outbound                       = make_method_call("Add", {mc::variant(7), mc::variant(5)});
    outbound.header.context["trace_id"] = "abc";

    mc::variant variant;
    mc::reflect::to_variant(outbound, variant);

    mc::engine::message from_variant;
    mc::reflect::from_variant(variant, from_variant);

    auto from_bytes = mc::engine::message::from_bytes(outbound.to_bytes());

    ASSERT_NE(from_variant.try_as<mc::engine::method_call_payload>(), nullptr);
    ASSERT_NE(from_bytes.try_as<mc::engine::method_call_payload>(), nullptr);
    EXPECT_EQ(from_variant.header.member_name, "Add");
    EXPECT_EQ(from_bytes.header.member_name, "Add");
    EXPECT_EQ(from_variant.header.context["trace_id"], "abc");
    EXPECT_EQ(from_bytes.header.context["trace_id"], "abc");
    EXPECT_EQ(from_variant.as<mc::engine::method_call_payload>().args[0], 7);
    EXPECT_EQ(from_bytes.as<mc::engine::method_call_payload>().args[1], 5);
}

TEST_F(standard_message_protocol_fixture, method_call_to_method_return_over_protocol_stack)
{
    auto request_queue  = make_queue(make_queue_name("request"));
    auto response_queue = make_queue(make_queue_name("response"));

    auto client_request_protocols  = make_protocols(request_queue, 11, 1);
    auto server_request_protocols  = make_protocols(request_queue, 11, 1);
    auto server_response_protocols = make_protocols(response_queue, 22, 2);
    auto client_response_protocols = make_protocols(response_queue, 22, 2);

    send_message(make_method_call("Add", {mc::variant(7), mc::variant(5)}), client_request_protocols);

    auto request  = receive_message(server_request_protocols);
    auto response = mc::engine::dispatch(m_service, request);
    send_message(response, server_response_protocols);

    auto client_response = receive_message(client_response_protocols);
    ASSERT_EQ(client_response.header.type, mc::engine::message_type::method_return);
    EXPECT_EQ(client_response.as<mc::engine::method_return_payload>().value, 12);
}

TEST_F(standard_message_protocol_fixture, method_call_to_error_over_protocol_stack)
{
    auto request_queue  = make_queue(make_queue_name("request_error"));
    auto response_queue = make_queue(make_queue_name("response_error"));

    auto client_request_protocols  = make_protocols(request_queue, 31, 1);
    auto server_request_protocols  = make_protocols(request_queue, 31, 1);
    auto server_response_protocols = make_protocols(response_queue, 42, 2);
    auto client_response_protocols = make_protocols(response_queue, 42, 2);

    send_message(make_method_call("Fail"), client_request_protocols);

    auto request  = receive_message(server_request_protocols);
    auto response = mc::engine::dispatch(m_service, request);
    send_message(response, server_response_protocols);

    auto client_response = receive_message(client_response_protocols);
    ASSERT_EQ(client_response.header.type, mc::engine::message_type::error);

    auto* payload = client_response.try_as<mc::engine::error_payload>();
    ASSERT_NE(payload, nullptr);
    EXPECT_EQ(payload->name, "mc.engine.not_supported");
}

TEST_F(standard_message_protocol_fixture, signal_event_is_delivered_over_protocol_stack)
{
    auto event_queue     = make_queue(make_queue_name("event"));
    auto server_protocol = make_protocols(event_queue, 51, 2);
    auto client_protocol = make_protocols(event_queue, 51, 2);

    mc::engine::message event;
    event.header.type           = mc::engine::message_type::signal;
    event.header.destination    = "org.test.client";
    event.header.sender         = m_service.name();
    event.header.path           = "/org/test/standard";
    event.header.interface_name = "org.test.standard";
    event.header.member_name    = "Changed";
    event.body = mc::engine::make_payload<mc::engine::signal_payload>("s", mc::variants{mc::variant("payload")});

    send_message(event, server_protocol);

    auto received = receive_message(client_protocol);
    ASSERT_EQ(received.header.type, mc::engine::message_type::signal);
    ASSERT_NE(received.try_as<mc::engine::signal_payload>(), nullptr);
    EXPECT_EQ(received.header.member_name, "Changed");
    EXPECT_EQ(received.as<mc::engine::signal_payload>().args[0], "payload");
}

TEST_F(standard_message_protocol_fixture, decode_hook_can_rewrite_field_value)
{
    mc::engine::message outbound;
    outbound.header.type           = mc::engine::message_type::method_call;
    outbound.header.destination    = m_service.name();
    outbound.header.sender         = "org.test.client";
    outbound.header.path           = "/org/test/standard";
    outbound.header.interface_name = "org.test.standard";
    outbound.header.member_name    = "Add";
    outbound.body                  = mc::engine::make_payload<mc::engine::method_call_payload>(
        "av", mc::variants{mc::variant(mc::variants{mc::variant(mc::variants{})})});

    mc::engine::message_decode_options options;
    options.on_field_decoded = [](const mc::engine::decode_field_context& context, mc::variant& value) {
        if (context.field_path != "body.args") {
            return;
        }

        auto args = value.as<mc::variants>();
        args[0]   = mc::dict();
        value     = args;
    };

    auto  decoded = mc::engine::message::from_bytes(outbound.to_bytes(), options);
    auto* payload = decoded.try_as<mc::engine::method_call_payload>();
    ASSERT_NE(payload, nullptr);
    ASSERT_EQ(payload->args.size(), 1u);
    EXPECT_TRUE(payload->args[0].is_dict());
    EXPECT_TRUE(payload->args[0].as<mc::dict>().empty());
}

TEST_F(standard_message_protocol_fixture, unknown_payload_id_falls_back_to_opaque_payload)
{
    mc::engine::message outbound;
    outbound.header.type           = mc::engine::message_type::signal;
    outbound.header.destination    = "org.test.client";
    outbound.header.sender         = m_service.name();
    outbound.header.path           = "/org/test/custom";
    outbound.header.interface_name = "org.test.custom";
    outbound.header.member_name    = "Custom";
    outbound.body                  = mc::engine::make_payload<custom_extension_payload>("custom.topic",
                                                                                        mc::variants{mc::variant(7), mc::variant("x")});

    auto decoded = mc::engine::message::from_bytes(outbound.to_bytes());
    ASSERT_NE(decoded.body, nullptr);
    EXPECT_EQ(decoded.body->payload_id(), custom_extension_payload::static_payload_id());
    EXPECT_EQ(decoded.body->message_type_id(), mc::engine::message_type::signal);

    auto* opaque = dynamic_cast<const mc::engine::opaque_payload*>(decoded.body.get());
    ASSERT_NE(opaque, nullptr);
    EXPECT_EQ(opaque->wire_bytes, outbound.body->to_bytes());
}

TEST_F(standard_message_protocol_fixture, custom_payload_decoder_can_restore_typed_payload)
{
    mc::engine::message outbound;
    outbound.header.type           = mc::engine::message_type::signal;
    outbound.header.destination    = "org.test.client";
    outbound.header.sender         = m_service.name();
    outbound.header.path           = "/org/test/custom";
    outbound.header.interface_name = "org.test.custom";
    outbound.header.member_name    = "Custom";
    outbound.body                  = mc::engine::make_payload<custom_extension_payload>("custom.topic",
                                                                                        mc::variants{mc::variant(7), mc::variant("x")});

    mc::engine::message_decode_options options;
    options.decode_payload = [](mc::engine::payload_id_type payload_id, mc::engine::message_type message_kind,
                                const mc::dict& body, const mc::engine::message_decode_options& decode_options)
        -> mc::shared_ptr<const mc::engine::abstract_payload> {
        if (payload_id != custom_extension_payload::static_payload_id() ||
            message_kind != mc::engine::message_type::signal) {
            return nullptr;
        }

        auto topic = body.at("topic").as<mc::string>();
        auto args  = body.at("args").as<mc::variants>();
        if (decode_options.on_field_decoded) {
            mc::variant                      topic_value(topic);
            mc::engine::decode_field_context context{
                "body.topic",
                0,
                custom_extension_payload::static_payload_id(),
                mc::engine::message_type::signal,
                topic_value.get_type(),
            };
            decode_options.on_field_decoded(context, topic_value);
            topic = topic_value.as<mc::string>();
        }
        return mc::make_shared<custom_extension_payload>(topic, std::move(args));
    };

    auto  decoded = mc::engine::message::from_bytes(outbound.to_bytes(), options);
    auto* payload = decoded.try_as<custom_extension_payload>();
    ASSERT_NE(payload, nullptr);
    EXPECT_EQ(payload->topic, "custom.topic");
    ASSERT_EQ(payload->args.size(), 2u);
    EXPECT_EQ(payload->args[0], 7);
    EXPECT_EQ(payload->args[1], "x");
}

TEST_F(standard_message_protocol_fixture, standard_payload_decoder_skips_unknown_field_id)
{
    auto outbound = make_method_call("Add", {mc::variant(7), mc::variant(5)});
    auto mutated  = append_unknown_int32_field(outbound.to_bytes(), 99, 1234);

    auto  decoded = mc::engine::message::from_bytes(mutated);
    auto* payload = decoded.try_as<mc::engine::method_call_payload>();
    ASSERT_NE(payload, nullptr);
    EXPECT_EQ(payload->signature, "ii");
    ASSERT_EQ(payload->args.size(), 2u);
    EXPECT_EQ(payload->args[0], 7);
    EXPECT_EQ(payload->args[1], 5);
}

TEST_F(standard_message_protocol_fixture, decode_skips_unknown_header_field)
{
    auto outbound                       = make_method_call("Add", {mc::variant(7), mc::variant(5)});
    outbound.header.context["trace_id"] = "trace-42";
    auto mutated                        = append_unknown_header_field(outbound.to_bytes(), 4242, 0xC0DE);

    auto decoded = mc::engine::message::from_bytes(mutated);
    EXPECT_EQ(decoded.header.type, mc::engine::message_type::method_call);
    EXPECT_EQ(decoded.header.member_name, "Add");
    EXPECT_EQ(decoded.header.serial, 11U);
    ASSERT_NE(decoded.try_as<mc::engine::method_call_payload>(), nullptr);
    EXPECT_EQ(decoded.as<mc::engine::method_call_payload>().args[0], 7);
    EXPECT_EQ(decoded.header.context.at("trace_id"), "trace-42");
}

TEST_F(standard_message_protocol_fixture, decode_hook_exposes_stable_field_id_for_header)
{
    auto outbound = make_method_call("Add", {mc::variant(7), mc::variant(5)});
    auto mutated  = append_unknown_header_field(outbound.to_bytes(), 4242, 0xC0DE);

    std::vector<mc::engine::header_field_id_type> header_field_ids;
    mc::engine::message_decode_options            options;
    options.on_field_decoded = [&header_field_ids](const mc::engine::decode_field_context& context, mc::variant&) {
        if (context.field_path.rfind("header.", 0) == 0) {
            header_field_ids.push_back(context.field_id);
        }
    };

    auto decoded = mc::engine::message::from_bytes(mutated, options);
    EXPECT_EQ(decoded.header.member_name, "Add");
    EXPECT_NE(std::find(header_field_ids.begin(), header_field_ids.end(), mc::engine::header_field_ids::member_name),
              header_field_ids.end());
    EXPECT_NE(std::find(header_field_ids.begin(), header_field_ids.end(), mc::engine::header_field_ids::serial),
              header_field_ids.end());
    EXPECT_NE(std::find(header_field_ids.begin(), header_field_ids.end(), 4242U), header_field_ids.end());
}

TEST_F(standard_message_protocol_fixture, decode_hook_exposes_stable_field_id_for_standard_payload)
{
    auto outbound = make_method_call("Add", {mc::variant(7), mc::variant(5)});
    auto mutated  = append_unknown_int32_field(outbound.to_bytes(), 99, 1234);

    std::vector<mc::engine::payload_field_id_type> field_ids;
    mc::engine::message_decode_options             options;
    options.on_field_decoded = [&field_ids](const mc::engine::decode_field_context& context, mc::variant&) {
        if (context.field_path.rfind("body.", 0) == 0) {
            field_ids.push_back(context.field_id);
        }
    };

    auto decoded = mc::engine::message::from_bytes(mutated, options);
    ASSERT_NE(decoded.try_as<mc::engine::method_call_payload>(), nullptr);
    EXPECT_NE(std::find(field_ids.begin(), field_ids.end(), mc::engine::payload_field_ids::method_call::signature),
              field_ids.end());
    EXPECT_NE(std::find(field_ids.begin(), field_ids.end(), mc::engine::payload_field_ids::method_call::args),
              field_ids.end());
    EXPECT_NE(std::find(field_ids.begin(), field_ids.end(), 99U), field_ids.end());
}
