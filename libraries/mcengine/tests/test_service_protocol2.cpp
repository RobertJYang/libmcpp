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

class service_proto_test_interface : public mc::engine::interface<service_proto_test_interface> {
public:
    MC_INTERFACE("org.test.service.proto")

    int32_t add(int32_t lhs, int32_t rhs)
    {
        return lhs + rhs;
    }

    int32_t fail()
    {
        MC_REPLY_ERROR_AND_THROW(mc::engine::errors::not_supported);
    }
};

class service_proto_test_object : public mc::engine::object<service_proto_test_object> {
public:
    MC_OBJECT(service_proto_test_object, "ServiceProtoTestObject", "/org/test/service/proto",
              (service_proto_test_interface))

    service_proto_test_interface m_iface;
};

class service_proto_test_service : public mc::engine::service {
public:
    explicit service_proto_test_service(mc::string_view name) : mc::engine::service(name)
    {}
};

std::string make_queue_name(const char* base)
{
    const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::string("mc_service_proto_test_") + base + "_" + std::to_string(::getpid()) + "_" +
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

struct service_proto_chain {
    service_proto_chain(mc::shm::mq_queue& queue, std::uint32_t writer_id, std::uint64_t writer_instance_id)
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
        ASSERT_EQ(engine.push(req), mc::proto::execution_state::completed);
    }

    void drive_once()
    {
        mc::proto::proto_request req;
        auto                     state = transport.pop(req);
        int                      guard = 0;
        while (state == mc::proto::execution_state::suspended && guard++ < 64) {
            state = transport.resume(req);
        }
        ASSERT_EQ(state, mc::proto::execution_state::completed);
    }

    mc::engine::message receive()
    {
        mc::proto::proto_request req;
        auto                     state = transport.pop(req);
        int                      guard = 0;
        while (state == mc::proto::execution_state::suspended && guard++ < 64) {
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

class service_proto_test : public mc::test::TestWithEngine {
protected:
    void SetUp() override
    {
        TestWithEngine::SetUp();
        ASSERT_TRUE(m_service.init());
        ASSERT_TRUE(m_service.start());

        auto obj = service_proto_test_object::create();
        obj->set_object_name("service_proto_test_object");
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

    mc::engine::message make_method_call() const
    {
        mc::engine::message msg;
        msg.header.type           = mc::engine::message_type::method_call;
        msg.header.destination    = m_service.name();
        msg.header.sender         = "org.test.client";
        msg.header.path           = "/org/test/service/proto";
        msg.header.interface_name = "org.test.service.proto";
        msg.header.member_name    = "Add";
        msg.header.serial         = 11;
        msg.body                  = mc::engine::make_payload<mc::engine::method_call_payload>(
            "ii", mc::variants{mc::variant(7), mc::variant(5)});
        return msg;
    }

    service_proto_test_service m_service{"org.test.service.proto.service"};
    std::vector<mc::string>    m_queue_names;
};

TEST_F(service_proto_test, set_proto_dispatches_request_over_protocol_chain)
{
    auto queue = make_queue(remember_queue_name("dispatch"));

    service_proto_chain server(queue, 11, 1);
    service_proto_chain client(queue, 11, 1);

    m_service.set_proto(&server.engine);
    client.send(make_method_call());
    server.drive_once();

    auto response = client.receive();
    ASSERT_EQ(response.header.type, mc::engine::message_type::method_return);
    EXPECT_EQ(response.as<mc::engine::method_return_payload>().value, 12);
}

TEST_F(service_proto_test, get_proto_returns_configured_protocol_root)
{
    auto                queue = make_queue(remember_queue_name("get_proto"));
    service_proto_chain server(queue, 11, 1);

    m_service.set_proto(&server.engine);

    EXPECT_EQ(m_service.get_proto(), &server.engine);
}

TEST_F(service_proto_test, set_proto_dispatches_error_response_over_protocol_chain)
{
    auto queue = make_queue(remember_queue_name("dispatch_error"));

    service_proto_chain server(queue, 11, 1);
    service_proto_chain client(queue, 11, 1);

    m_service.set_proto(&server.engine);

    auto request                  = make_method_call();
    request.header.member_name    = "Fail";
    request.body                  = mc::engine::make_payload<mc::engine::method_call_payload>("", mc::variants{});

    client.send(request);
    server.drive_once();

    auto response = client.receive();
    ASSERT_EQ(response.header.type, mc::engine::message_type::error);
    auto* payload = response.try_as<mc::engine::error_payload>();
    ASSERT_NE(payload, nullptr);
    EXPECT_EQ(payload->name, "mc.engine.not_supported");
}

} // namespace

MC_REFLECT(service_proto_test_interface, ((add, "Add"))((fail, "Fail")))
MC_REFLECT(service_proto_test_object, ((m_iface, "iface")))
