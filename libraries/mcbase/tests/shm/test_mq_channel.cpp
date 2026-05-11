/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <mutex>
#include <string>
#include <unistd.h>
#include <vector>

#include <mc/future.h>
#include <mc/protocol.h>
#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/mq.h>
#include <mc/shm/shm_runtime.h>
#include <test_utilities/base.h>

#include "shm/message_queue/mq_notifier.h"

using namespace std::chrono_literals;

namespace {

struct mq_channel_application_proto : mc::proto::protocol {
    struct message_context : public mc::proto::proto_context {
        mc::string inbound_payload;
    };

    std::promise<mc::string>* single_delivery{nullptr};
    std::promise<void>*       all_done{nullptr};
    std::mutex*               deliveries_mutex{nullptr};
    std::vector<mc::string>*  deliveries{nullptr};

    mc::proto::execution_state on_push(mc::proto::proto_request& req) override
    {
        return push_next(req);
    }

    mc::proto::execution_state on_pop(mc::proto::proto_request& req) override
    {
        const auto& buffer  = req.buffer();
        const auto  base    = buffer.payload_base();
        const auto  size    = buffer.length() >= base ? buffer.length() - base : 0U;
        auto&       payload = req.ensure_context<message_context>(this).inbound_payload;
        payload.clear();
        payload.append(reinterpret_cast<const char*>(buffer.data() + base), size);

        if (single_delivery != nullptr) {
            single_delivery->set_value(payload);
            single_delivery = nullptr;
        }

        if (deliveries != nullptr && deliveries_mutex != nullptr) {
            std::lock_guard<std::mutex> lock(*deliveries_mutex);
            deliveries->push_back(payload);
            if (all_done != nullptr && deliveries->size() == 2U) {
                all_done->set_value();
                all_done = nullptr;
            }
        }
        return complete(req);
    }
};

std::string make_region_name(const std::string& base)
{
    const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return "mc_mq_channel_region_" + base + "_" + std::to_string(::getpid()) + "_" + std::to_string(now_ns);
}

void append_payload(mc::proto::buffer& buffer, const std::string& payload)
{
    buffer.append_payload(payload.data(), payload.size());
}

bool wait_until(const std::function<bool()>& predicate, std::chrono::milliseconds timeout = std::chrono::seconds(2))
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return predicate();
}

class mq_channel_test : public mc::test::TestWithRuntime {
protected:
    void SetUp() override
    {
        mc::test::TestWithRuntime::SetUp();
        mc::shm::runtime_options options;
        m_region_name           = make_region_name("runtime");
        options.region_name     = m_region_name;
        options.manager_process = true;
        m_runtime               = std::make_shared<mc::shm::shm_runtime>(options);
        ASSERT_TRUE(m_runtime->is_valid());
        auto endpoint = m_runtime->register_endpoint("service.channel", 8);
        ASSERT_TRUE(endpoint.has_value());
        m_endpoint = *endpoint;
    }

    void TearDown() override
    {
        mc::shm::detail::mq_notifier::remove(m_endpoint.notifier_name);
        mc::shm::detail::mq_notifier::remove(mc::shm::detail::mq_notifier::make_space_name(m_endpoint.queue_name));
        for (const auto& endpoint : m_extra_endpoints) {
            mc::shm::detail::mq_notifier::remove(endpoint.notifier_name);
            mc::shm::detail::mq_notifier::remove(mc::shm::detail::mq_notifier::make_space_name(endpoint.queue_name));
            mc::shm::detail::shared_memory_backend::remove(endpoint.queue_name);
        }
        mc::shm::detail::shared_memory_backend::remove(m_endpoint.queue_name);
        mc::shm::detail::shared_memory_backend::remove(m_region_name);
        m_runtime.reset();
        mc::test::TestWithRuntime::TearDown();
    }

    mc::shm::endpoint register_endpoint(std::string_view name, std::size_t slot_count)
    {
        auto endpoint = m_runtime->register_endpoint(name, slot_count);
        EXPECT_TRUE(endpoint.has_value());
        if (!endpoint.has_value()) {
            return {};
        }
        m_extra_endpoints.push_back(*endpoint);
        return *endpoint;
    }

    std::shared_ptr<mc::shm::shm_runtime> m_runtime;
    mc::shm::endpoint                     m_endpoint;
    std::vector<mc::shm::endpoint>        m_extra_endpoints;
    std::string                           m_region_name;
};

} // anonymous namespace

TEST_F(mq_channel_test, send_and_receive_single_message)
{
    mq_channel_application_proto root;
    mc::shm::mq_proto            mq;
    mc::shm::mq_transport_proto  transport;
    root.add_child(mq);
    mq.add_child(transport);
    mc::shm::mq_channel channel;
    channel.set_protocol(&root);

    auto* app_layer = channel.get_protocol<mq_channel_application_proto>();
    ASSERT_NE(app_layer, nullptr);
    std::promise<mc::string> received_promise;
    auto                     received = received_promise.get_future();
    app_layer->single_delivery        = &received_promise;
    channel.start(m_runtime, m_endpoint, m_endpoint.instance_id);

    mc::proto::proto_request outbound;
    append_payload(outbound.buffer(), "abc");
    ASSERT_EQ(channel.send(outbound), mc::proto::execution_state::completed);

    ASSERT_EQ(received.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(received.get(), "abc");
    channel.stop();
}

TEST_F(mq_channel_test, send_large_message_fragmented)
{
    const std::string            payload(400, 'L');
    mq_channel_application_proto root;
    mc::shm::mq_proto            mq;
    mc::shm::mq_transport_proto  transport;
    root.add_child(mq);
    mq.add_child(transport);
    mc::shm::mq_channel channel;
    channel.set_protocol(&root);

    auto* app_layer = channel.get_protocol<mq_channel_application_proto>();
    ASSERT_NE(app_layer, nullptr);
    std::promise<mc::string> received_promise;
    auto                     received = received_promise.get_future();
    app_layer->single_delivery        = &received_promise;
    channel.start(m_runtime, m_endpoint, m_endpoint.instance_id);

    mc::proto::proto_request outbound;
    append_payload(outbound.buffer(), payload);
    ASSERT_EQ(channel.send(outbound), mc::proto::execution_state::completed);

    ASSERT_EQ(received.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(received.get(), payload);
    channel.stop();
}

TEST_F(mq_channel_test, receive_handler_preserves_fifo_order)
{
    mq_channel_application_proto root;
    mc::shm::mq_proto            mq;
    mc::shm::mq_transport_proto  transport;
    root.add_child(mq);
    mq.add_child(transport);
    mc::shm::mq_channel channel;
    channel.set_protocol(&root);

    auto* app_layer = channel.get_protocol<mq_channel_application_proto>();
    ASSERT_NE(app_layer, nullptr);
    std::promise<void>      done_promise;
    auto                    done = done_promise.get_future();
    std::mutex              mutex;
    std::vector<mc::string> deliveries;
    app_layer->all_done         = &done_promise;
    app_layer->deliveries_mutex = &mutex;
    app_layer->deliveries       = &deliveries;
    channel.start(m_runtime, m_endpoint, m_endpoint.instance_id);

    mc::proto::proto_request first;
    append_payload(first.buffer(), "first");
    ASSERT_EQ(channel.send(first), mc::proto::execution_state::completed);

    mc::proto::proto_request second;
    append_payload(second.buffer(), "second");
    ASSERT_EQ(channel.send(second), mc::proto::execution_state::completed);

    ASSERT_EQ(done.wait_for(2s), std::future_status::ready);
    ASSERT_EQ(deliveries.size(), 2U);
    EXPECT_EQ(deliveries[0], "first");
    EXPECT_EQ(deliveries[1], "second");
    channel.stop();
}

TEST_F(mq_channel_test, get_protocol_returns_configured_layer)
{
    mq_channel_application_proto root;
    mc::shm::mq_proto            mq;
    mc::shm::mq_transport_proto  transport;
    root.add_child(mq);
    mq.add_child(transport);
    mc::shm::mq_channel channel;
    channel.set_protocol(&root);

    ASSERT_NE(channel.get_protocol<mc::shm::mq_proto>(), nullptr);
    ASSERT_NE(channel.get_protocol<mc::shm::mq_transport_proto>(), nullptr);
}

TEST_F(mq_channel_test, send_large_message_auto_resumes_when_space_is_released)
{
    const std::string            payload(400, 'R');
    const auto                   small_endpoint = register_endpoint("service.channel.small", 1);
    mq_channel_application_proto root;
    mc::shm::mq_proto            mq;
    mc::shm::mq_transport_proto  transport;
    root.add_child(mq);
    mq.add_child(transport);
    mc::shm::mq_channel channel;
    channel.set_protocol(&root);

    auto* app_layer = channel.get_protocol<mq_channel_application_proto>();
    ASSERT_NE(app_layer, nullptr);
    std::promise<mc::string> received_promise;
    auto                     received = received_promise.get_future();
    app_layer->single_delivery        = &received_promise;
    channel.start(m_runtime, small_endpoint, small_endpoint.instance_id);

    mc::proto::proto_request outbound;
    append_payload(outbound.buffer(), payload);

    const auto state = channel.send(outbound);
    ASSERT_NE(state, mc::proto::execution_state::failed);
    ASSERT_EQ(received.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(received.get(), payload);
    ASSERT_TRUE(wait_until([&outbound]() {
        return outbound.state() == mc::proto::execution_state::completed;
    }));
    EXPECT_EQ(outbound.state(), mc::proto::execution_state::completed);

    channel.stop();
}

TEST_F(mq_channel_test, send_owned_keeps_request_alive_until_auto_resume)
{
    const std::string            payload(400, 'O');
    const auto                   small_endpoint = register_endpoint("service.channel.owned", 1);
    mq_channel_application_proto root;
    mc::shm::mq_proto            mq;
    mc::shm::mq_transport_proto  transport;
    root.add_child(mq);
    mq.add_child(transport);
    mc::shm::mq_channel channel;
    channel.set_protocol(&root);

    auto* app_layer = channel.get_protocol<mq_channel_application_proto>();
    ASSERT_NE(app_layer, nullptr);
    std::promise<mc::string> received_promise;
    auto                     received = received_promise.get_future();
    app_layer->single_delivery        = &received_promise;
    channel.start(m_runtime, small_endpoint, small_endpoint.instance_id);

    auto outbound = std::make_unique<mc::proto::proto_request>();
    append_payload(outbound->buffer(), payload);
    channel.send_owned(std::move(outbound));

    ASSERT_EQ(received.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(received.get(), payload);

    channel.stop();
}
