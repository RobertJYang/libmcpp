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
    struct packet {
        mc::string inbound_payload;
    };

    struct context {};
    using request_type = mc::proto::request_view<packet, context>;

    std::promise<mc::string>* single_delivery{nullptr};
    std::promise<void>*       all_done{nullptr};
    std::mutex*               deliveries_mutex{nullptr};
    std::vector<mc::string>*  deliveries{nullptr};
    int                       start_hits{0};
    int                       stop_hits{0};

    void on_start(mc::proto::instance&) override
    {
        ++start_hits;
    }

    void on_stop(mc::proto::instance&) override
    {
        ++stop_hits;
    }

    template <typename Req>
    static mc::proto::command on_push(Req& req)
    {
        return req.push_next();
    }

    mc::proto::command on_pop(request_type& req)
    {
        const auto& buffer  = req.buffer();
        const auto  base    = buffer.payload_base();
        const auto  size    = buffer.length() >= base ? buffer.length() - base : 0U;
        auto&       payload = req.packet().inbound_payload;
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
        return req.complete();
    }
};

using mq_channel_proto = mc::proto::stack<mq_channel_application_proto, mc::shm::mq_proto, mc::shm::mq_transport_proto>;

std::string make_region_name(const std::string& base)
{
    const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return "mc_mq_channel_region_" + base + "_" + std::to_string(::getpid()) + "_" + std::to_string(now_ns);
}

void append_payload(mc::proto::buffer& buffer, const std::string& payload)
{
    buffer.append_payload(payload.data(), payload.size());
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
        mc::shm::detail::shared_memory_backend::remove(m_endpoint.queue_name);
        mc::shm::detail::shared_memory_backend::remove(m_region_name);
        m_runtime.reset();
        mc::test::TestWithRuntime::TearDown();
    }

    std::shared_ptr<mc::shm::shm_runtime> m_runtime;
    mc::shm::endpoint                     m_endpoint;
    std::string                           m_region_name;
};

} // anonymous namespace

TEST_F(mq_channel_test, send_and_receive_single_message)
{
    auto                proto = mq_channel_proto::create();
    mc::shm::mq_channel channel;
    channel.set_protocol(proto);

    auto* app = channel.get_protocol<mq_channel_application_proto>();
    ASSERT_NE(app, nullptr);
    std::promise<mc::string> received_promise;
    auto                     received = received_promise.get_future();
    app->single_delivery              = &received_promise;
    channel.start(m_runtime, m_endpoint, m_endpoint.instance_id);

    auto outbound   = mq_channel_proto::make_request();
    auto completion = outbound.enable_async_completion();
    outbound.prepare_buffer();
    append_payload(outbound.buffer(), "abc");
    ASSERT_EQ(channel.send(outbound), mc::proto::execution_state::completed);

    ASSERT_EQ(completion.wait_for(2s), mc::future_status::ready);
    EXPECT_NO_THROW(completion.get());
    ASSERT_EQ(received.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(received.get(), "abc");
    EXPECT_EQ(app->start_hits, 1);
    channel.stop();
    EXPECT_EQ(app->stop_hits, 1);
}

TEST_F(mq_channel_test, send_large_message_fragmented)
{
    const std::string   payload(400, 'L');
    auto                proto = mq_channel_proto::create();
    mc::shm::mq_channel channel;
    channel.set_protocol(proto);

    auto* app = channel.get_protocol<mq_channel_application_proto>();
    ASSERT_NE(app, nullptr);
    std::promise<mc::string> received_promise;
    auto                     received = received_promise.get_future();
    app->single_delivery              = &received_promise;
    channel.start(m_runtime, m_endpoint, m_endpoint.instance_id);

    auto outbound   = mq_channel_proto::make_request();
    auto completion = outbound.enable_async_completion();
    outbound.prepare_buffer();
    append_payload(outbound.buffer(), payload);
    ASSERT_EQ(channel.send(outbound), mc::proto::execution_state::completed);

    ASSERT_EQ(completion.wait_for(2s), mc::future_status::ready);
    EXPECT_NO_THROW(completion.get());
    ASSERT_EQ(received.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(received.get(), payload);
    channel.stop();
}

TEST_F(mq_channel_test, receive_handler_preserves_fifo_order)
{
    auto                proto = mq_channel_proto::create();
    mc::shm::mq_channel channel;
    channel.set_protocol(proto);

    auto* app = channel.get_protocol<mq_channel_application_proto>();
    ASSERT_NE(app, nullptr);
    std::promise<void>      done_promise;
    auto                    done = done_promise.get_future();
    std::mutex              mutex;
    std::vector<mc::string> deliveries;
    app->all_done         = &done_promise;
    app->deliveries_mutex = &mutex;
    app->deliveries       = &deliveries;
    channel.start(m_runtime, m_endpoint, m_endpoint.instance_id);

    auto first            = mq_channel_proto::make_request();
    auto first_completion = first.enable_async_completion();
    first.prepare_buffer();
    append_payload(first.buffer(), "first");
    ASSERT_EQ(channel.send(first), mc::proto::execution_state::completed);
    ASSERT_EQ(first_completion.wait_for(2s), mc::future_status::ready);
    EXPECT_NO_THROW(first_completion.get());

    auto second            = mq_channel_proto::make_request();
    auto second_completion = second.enable_async_completion();
    second.prepare_buffer();
    append_payload(second.buffer(), "second");
    ASSERT_EQ(channel.send(second), mc::proto::execution_state::completed);
    ASSERT_EQ(second_completion.wait_for(2s), mc::future_status::ready);
    EXPECT_NO_THROW(second_completion.get());

    ASSERT_EQ(done.wait_for(2s), std::future_status::ready);
    ASSERT_EQ(deliveries.size(), 2U);
    EXPECT_EQ(deliveries[0], "first");
    EXPECT_EQ(deliveries[1], "second");
    channel.stop();
}

TEST_F(mq_channel_test, get_protocol_returns_configured_layer)
{
    auto                proto = mq_channel_proto::create();
    mc::shm::mq_channel channel;
    channel.set_protocol(proto);

    ASSERT_NE(channel.get_protocol<mc::shm::mq_proto>(), nullptr);
    ASSERT_NE(channel.get_protocol<mc::shm::mq_transport_proto>(), nullptr);
}
