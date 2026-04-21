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

#include <mc/engine/dispatcher.h>
#include <mc/engine/engine_proto.h>
#include <mc/engine/interface.h>
#include <mc/engine/object.h>
#include <mc/engine/service.h>
#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/message_queue/mq_proto.h>
#include <mc/shm/message_queue/mq_queue.h>
#include <mc/shm/message_queue/mq_transport_proto.h>
#include <test_utilities/engine_test_base.h>

#include <cctype>
#include <chrono>
#include <cstdio>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

using namespace std::chrono_literals;

class proto_test_interface : public mc::engine::interface<proto_test_interface> {
public:
    MC_INTERFACE("org.test.proto")

    int32_t add(int32_t lhs, int32_t rhs)
    {
        return lhs + rhs;
    }

    int32_t fail()
    {
        MC_REPLY_ERROR_AND_THROW(mc::engine::errors::not_supported);
    }
};

class proto_test_object : public mc::engine::object<proto_test_object> {
public:
    MC_OBJECT(proto_test_object, "ProtoTestObject", "/org/test/proto", (proto_test_interface))

    proto_test_interface m_iface;
};

class proto_test_service : public mc::engine::service {
public:
    explicit proto_test_service(mc::string_view name) : mc::engine::service(name)
    {}
};

std::string make_queue_name(const char* base)
{
    const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::string("mc_engine_proto_test_") + base + "_" + std::to_string(::getpid()) + "_" +
           std::to_string(now_ns);
}

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

mc::shm::mq_queue make_queue(mc::string_view name)
{
    mc::shm::mq_queue_options options;
    options.shared_memory_name = mc::string(name);
    options.slot_count         = 16;
    options.max_payload_size   = 256;
    return mc::shm::mq_queue(options);
}

struct proto_chain {
    proto_chain(mc::shm::mq_queue& queue, std::uint32_t writer_id, std::uint64_t writer_instance_id)
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

class engine_proto_protocol_stack_test : public mc::test::TestWithEngine {
protected:
    void SetUp() override
    {
        TestWithEngine::SetUp();
        ASSERT_TRUE(m_service.init());
        ASSERT_TRUE(m_service.start());

        auto obj = proto_test_object::create();
        obj->set_object_name("proto_test_object");
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

    mc::string remember_queue_name(const char* base)
    {
        auto name = mc::string(make_queue_name(base));
        m_queue_names.push_back(name);
        return name;
    }

    mc::engine::message make_method_call(mc::string_view member_name, mc::variants args = {}) const
    {
        mc::engine::message msg;
        msg.header.type           = mc::engine::message_type::method_call;
        msg.header.destination    = m_service.name();
        msg.header.sender         = "org.test.client";
        msg.header.path           = "/org/test/proto";
        msg.header.interface_name = "org.test.proto";
        msg.header.member_name    = mc::string(member_name);
        msg.header.serial         = 11;
        msg.body                  = mc::engine::make_payload<mc::engine::method_call_payload>("ii", std::move(args));
        return msg;
    }

    proto_test_service      m_service{"org.test.proto.service"};
    std::vector<mc::string> m_queue_names;
};

TEST_F(engine_proto_protocol_stack_test, method_call_to_method_return_over_protocol_stack)
{
    auto request_queue  = make_queue(remember_queue_name("request"));
    auto response_queue = make_queue(remember_queue_name("response"));

    proto_chain client_request(request_queue, 11, 1);
    proto_chain server_request(request_queue, 11, 1);
    proto_chain server_response(response_queue, 22, 2);
    proto_chain client_response(response_queue, 22, 2);

    client_request.send(make_method_call("Add", {mc::variant(7), mc::variant(5)}));

    auto request  = server_request.receive();
    auto response = mc::engine::dispatch(m_service, request);
    server_response.send(response);

    auto client_result = client_response.receive();
    ASSERT_EQ(client_result.header.type, mc::engine::message_type::method_return);
    EXPECT_EQ(client_result.as<mc::engine::method_return_payload>().value, 12);
}

TEST_F(engine_proto_protocol_stack_test, method_call_to_error_over_protocol_stack)
{
    auto request_queue  = make_queue(remember_queue_name("request_error"));
    auto response_queue = make_queue(remember_queue_name("response_error"));

    proto_chain client_request(request_queue, 31, 1);
    proto_chain server_request(request_queue, 31, 1);
    proto_chain server_response(response_queue, 42, 2);
    proto_chain client_response(response_queue, 42, 2);

    client_request.send(make_method_call("Fail"));

    auto request  = server_request.receive();
    auto response = mc::engine::dispatch(m_service, request);
    server_response.send(response);

    auto client_result = client_response.receive();
    ASSERT_EQ(client_result.header.type, mc::engine::message_type::error);
    auto* payload = client_result.try_as<mc::engine::error_payload>();
    ASSERT_NE(payload, nullptr);
    EXPECT_EQ(payload->name, "mc.engine.not_supported");
}

TEST_F(engine_proto_protocol_stack_test, signal_event_is_delivered_over_protocol_stack)
{
    auto event_queue = make_queue(remember_queue_name("event"));

    proto_chain sender(event_queue, 51, 2);
    proto_chain receiver(event_queue, 51, 2);

    mc::engine::message event;
    event.header.type           = mc::engine::message_type::signal;
    event.header.destination    = "org.test.client";
    event.header.sender         = m_service.name();
    event.header.path           = "/org/test/proto";
    event.header.interface_name = "org.test.proto";
    event.header.member_name    = "Changed";
    event.body = mc::engine::make_payload<mc::engine::signal_payload>("s", mc::variants{mc::variant("payload")});

    sender.send(event);

    auto received = receiver.receive();
    ASSERT_EQ(received.header.type, mc::engine::message_type::signal);
    ASSERT_NE(received.try_as<mc::engine::signal_payload>(), nullptr);
    EXPECT_EQ(received.header.member_name, "Changed");
    EXPECT_EQ(received.as<mc::engine::signal_payload>().args[0], "payload");
}

TEST_F(engine_proto_protocol_stack_test, large_signal_is_delivered_over_protocol_fragment_path)
{
    auto event_queue = make_queue(remember_queue_name("large_event"));

    proto_chain sender(event_queue, 61, 3);
    proto_chain receiver(event_queue, 61, 3);

    const mc::string large_payload(600, 'L');

    mc::engine::message event;
    event.header.type           = mc::engine::message_type::signal;
    event.header.destination    = "org.test.client";
    event.header.sender         = m_service.name();
    event.header.path           = "/org/test/proto";
    event.header.interface_name = "org.test.proto";
    event.header.member_name    = "LargeChanged";
    event.body = mc::engine::make_payload<mc::engine::signal_payload>("s", mc::variants{mc::variant(large_payload)});

    sender.send(event);

    auto received = receiver.receive();
    ASSERT_EQ(received.header.type, mc::engine::message_type::signal);
    ASSERT_NE(received.try_as<mc::engine::signal_payload>(), nullptr);
    EXPECT_EQ(received.header.member_name, "LargeChanged");
    EXPECT_EQ(received.as<mc::engine::signal_payload>().args[0], large_payload);
}

} // namespace

MC_REFLECT(proto_test_interface, ((add, "Add"))((fail, "Fail")))
MC_REFLECT(proto_test_object, ((m_iface, "iface")))
