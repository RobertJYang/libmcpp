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

#include <gtest/gtest.h>

#include <mc/protocol.h>
#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/message_queue/mq_proto.h>
#include <mc/shm/message_queue/mq_queue.h>
#include <mc/shm/message_queue/mq_transport_proto.h>
#include <test_utilities/base.h>

#include <chrono>
#include <cstring>
#include <functional>
#include <future>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "shm/message_queue/mq_notifier.h"

namespace {

struct mq_application : mc::proto::protocol {
    struct message_context : public mc::proto::proto_context {
        mc::string inbound_payload;
    };

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
        return complete(req);
    }
};

std::string make_queue_name(const std::string& base)
{
    const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return "mc_mq_proto_" + base + "_" + std::to_string(::getpid()) + "_" + std::to_string(now_ns);
}

auto make_options(const std::string& name, std::size_t slot_count = 16)
{
    mc::shm::mq_queue_options opts;
    opts.shared_memory_name = name;
    opts.slot_count         = slot_count;
    return opts;
}

mc::string make_fragment(std::uint32_t msg_id, std::uint32_t instance_id, std::uint16_t endpoint_id, std::uint16_t part,
                         const std::string& payload)
{
    mc::string frag(13 + payload.size(), '\0');
    std::memcpy(&frag[0], &msg_id, 4);
    std::uint32_t src = (instance_id << 16U) | endpoint_id;
    std::memcpy(&frag[4], &src, 4);
    std::memcpy(&frag[8], &part, 2);
    std::memcpy(&frag[13], payload.data(), payload.size());
    return frag;
}

std::string fragment_body(mc::string_view fragment)
{
    return std::string(fragment.substr(mc::shm::mq_proto::fragment_header_size));
}

mc::string payload_from_buffer(const mc::proto::buffer& buffer)
{
    const auto base = buffer.payload_base();
    const auto size = buffer.length() >= base ? buffer.length() - base : 0U;
    if (size == 0) {
        return {};
    }
    return mc::string(reinterpret_cast<const char*>(buffer.data() + base), size);
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

struct protocol_chain {
    explicit protocol_chain(mc::shm::mq_queue& queue, std::uint32_t writer_id = 7, std::uint32_t instance_id = 1)
    {
        app.add_child(mq);
        mq.add_child(transport);
        mq.configure(instance_id, static_cast<std::uint16_t>(writer_id));
        transport.configure(queue, writer_id, instance_id);
    }

    void send(mc::string_view payload)
    {
        mc::proto::proto_request req;
        req.buffer().append_payload(payload.data(), payload.size());
        ASSERT_EQ(app.push(req), mc::proto::execution_state::completed);
    }

    mc::string receive()
    {
        mc::proto::proto_request req;
        auto                     state = transport.pop(req);
        for (int i = 0; state == mc::proto::execution_state::suspended && i < 64; ++i) {
            state = transport.resume(req);
        }

        EXPECT_EQ(state, mc::proto::execution_state::completed);
        auto* ctx = req.find_context<mq_application::message_context>(&app);
        EXPECT_NE(ctx, nullptr);
        return ctx == nullptr ? mc::string{} : ctx->inbound_payload;
    }

    void shutdown()
    {
        transport.shutdown();
    }

    mq_application              app;
    mc::shm::mq_proto           mq;
    mc::shm::mq_transport_proto transport;
};

struct direct_protocol_chain {
    explicit direct_protocol_chain(mc::shm::mq_queue& queue, std::uint32_t writer_id = 7, std::uint32_t instance_id = 1)
    {
        mq.add_child(transport);
        mq.configure(instance_id, static_cast<std::uint16_t>(writer_id));
        transport.configure(queue, writer_id, instance_id);
    }

    mc::proto::execution_state push(mc::proto::proto_request& req)
    {
        return mq.push(req);
    }

    mc::proto::execution_state pop(mc::proto::proto_request& req)
    {
        return mq.pop(req);
    }

    void shutdown()
    {
        transport.shutdown();
    }

    mc::shm::mq_proto           mq;
    mc::shm::mq_transport_proto transport;
};

class mq_proto_test : public mc::test::TestWithRuntime {
protected:
    void remember_queue(const std::string& name)
    {
        m_queue_names.push_back(name);
    }

    void TearDown() override
    {
        for (const auto& name : m_queue_names) {
            mc::shm::detail::mq_notifier::remove(mc::shm::detail::mq_notifier::make_default_name(name));
            mc::shm::detail::mq_notifier::remove(mc::shm::detail::mq_notifier::make_space_name(name));
            mc::shm::detail::shared_memory_backend::remove(name);
        }
        m_queue_names.clear();
        mc::test::TestWithRuntime::TearDown();
    }

    std::vector<std::string> m_queue_names;
};

} // namespace

TEST_F(mq_proto_test, single_fragment_send_and_receive)
{
    const auto name = make_queue_name("single");
    remember_queue(name);
    auto queue = mc::shm::mq_queue(make_options(name));

    protocol_chain chain(queue);
    chain.send("abc");
    EXPECT_EQ(chain.receive(), "abc");
}

TEST_F(mq_proto_test, multi_fragment_send_and_receive)
{
    const auto        name = make_queue_name("multi");
    const std::string payload(400, 'L');
    remember_queue(name);
    auto queue = mc::shm::mq_queue(make_options(name, 16));

    protocol_chain chain(queue);
    chain.send(payload);
    EXPECT_EQ(chain.receive(), payload);
}

TEST_F(mq_proto_test, old_instance_fragment_is_filtered)
{
    const auto name = make_queue_name("instance");
    remember_queue(name);
    auto queue = mc::shm::mq_queue(make_options(name, 16));

    ASSERT_TRUE(queue.send_message(7, 99, make_fragment(1, 99, 7, 1, "old")));
    ASSERT_TRUE(queue.send_message(7, 1, make_fragment(2, 1, 7, 1, "new")));

    protocol_chain chain(queue, 7, 1);
    EXPECT_EQ(chain.receive(), "new");
}

TEST_F(mq_proto_test, push_resumes_fragment_by_fragment_when_queue_is_full)
{
    const auto        name    = make_queue_name("push_suspend_resume");
    const std::string payload = "abcdefghijkl";
    remember_queue(name);

    auto queue = mc::shm::mq_queue(make_options(name, 1));

    mq_application              app;
    mc::shm::mq_proto           mq;
    mc::shm::mq_transport_proto transport;
    app.add_child(mq);
    mq.add_child(transport);
    mq.configure(1, 7, 4);
    transport.configure(queue, 7, 1);

    mc::proto::proto_request req;
    req.buffer().append_payload(payload.data(), payload.size());

    const auto first = app.push(req);
    ASSERT_EQ(first, mc::proto::execution_state::suspended);

    auto first_message = queue.try_receive_message();
    ASSERT_TRUE(first_message.has_value());
    EXPECT_EQ(fragment_body(first_message->payload), "abcd");

    auto                                     deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    std::optional<mc::shm::mq_queue_message> second_message;
    while (std::chrono::steady_clock::now() < deadline && !second_message.has_value()) {
        second_message = queue.try_receive_message();
        if (!second_message.has_value()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    ASSERT_TRUE(second_message.has_value());
    EXPECT_EQ(fragment_body(second_message->payload), "efgh");

    std::optional<mc::shm::mq_queue_message> third_message;
    while (std::chrono::steady_clock::now() < deadline && !third_message.has_value()) {
        third_message = queue.try_receive_message();
        if (!third_message.has_value()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    ASSERT_TRUE(third_message.has_value());
    EXPECT_EQ(fragment_body(third_message->payload), "ijkl");
    EXPECT_EQ(req.state(), mc::proto::execution_state::completed);
}

TEST_F(mq_proto_test, direct_push_auto_resumes_without_manual_resume)
{
    const auto        name = make_queue_name("direct_auto_resume");
    const std::string payload(400, 'Z');
    remember_queue(name);

    auto queue = mc::shm::mq_queue(make_options(name, 1));

    protocol_chain receiver(queue);

    mq_application              sender_app;
    mc::shm::mq_proto           sender_mq;
    mc::shm::mq_transport_proto sender_transport;
    sender_app.add_child(sender_mq);
    sender_mq.add_child(sender_transport);
    sender_mq.configure(1, 7);
    sender_transport.configure(queue, 7, 1);

    auto receive_future = std::async(std::launch::async, [&receiver]() {
        mc::proto::proto_request req;
        auto                     state = receiver.mq.pop(req);
        if (state == mc::proto::execution_state::suspended) {
            wait_until([&req]() {
                return req.state() == mc::proto::execution_state::completed;
            });
            state = req.state();
        }

        EXPECT_EQ(state, mc::proto::execution_state::completed);
        auto* ctx = req.find_context<mq_application::message_context>(&receiver.app);
        EXPECT_NE(ctx, nullptr);
        return ctx == nullptr ? mc::string{} : ctx->inbound_payload;
    });

    mc::proto::proto_request req;
    req.buffer().append_payload(payload.data(), payload.size());

    const auto state = sender_app.push(req);
    ASSERT_NE(state, mc::proto::execution_state::failed);

    ASSERT_EQ(receive_future.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    EXPECT_EQ(receive_future.get(), payload);
    ASSERT_TRUE(wait_until([&req]() {
        return req.state() == mc::proto::execution_state::completed;
    }));
    EXPECT_EQ(req.state(), mc::proto::execution_state::completed);
    sender_transport.shutdown();
    receiver.shutdown();
}

TEST_F(mq_proto_test, top_level_pop_auto_resumes_without_channel)
{
    const auto        name = make_queue_name("top_level_pop_resume");
    const std::string payload(400, 'P');
    remember_queue(name);

    auto queue = mc::shm::mq_queue(make_options(name, 4));

    direct_protocol_chain receiver(queue);
    direct_protocol_chain sender(queue);

    mc::proto::proto_request inbound;
    ASSERT_EQ(receiver.pop(inbound), mc::proto::execution_state::suspended);

    auto send_future = std::async(std::launch::async, [&sender, &payload]() {
        mc::proto::proto_request outbound;
        outbound.buffer().append_payload(payload.data(), payload.size());
        const auto state = sender.push(outbound);
        if (state == mc::proto::execution_state::suspended) {
            wait_until([&outbound]() {
                return outbound.state() == mc::proto::execution_state::completed;
            });
        }
        return outbound.state();
    });

    ASSERT_EQ(send_future.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    EXPECT_EQ(send_future.get(), mc::proto::execution_state::completed);
    ASSERT_TRUE(wait_until([&inbound]() {
        return inbound.state() == mc::proto::execution_state::completed;
    }));
    EXPECT_EQ(payload_from_buffer(inbound.buffer()), payload);
    sender.shutdown();
    receiver.shutdown();
}

TEST_F(mq_proto_test, interleaved_fragments_reassemble_across_pop_requests)
{
    const auto name = make_queue_name("interleaved_across_requests");
    remember_queue(name);

    auto queue = mc::shm::mq_queue(make_options(name, 8));
    ASSERT_TRUE(queue.send_message(7, 1, make_fragment(1, 1, 7, 2, "alpha-")));
    ASSERT_TRUE(queue.send_message(7, 1, make_fragment(2, 1, 7, 2, "beta-")));
    ASSERT_TRUE(queue.send_message(7, 1, make_fragment(1, 1, 7, 1, "done")));
    ASSERT_TRUE(queue.send_message(7, 1, make_fragment(2, 1, 7, 1, "done")));

    direct_protocol_chain receiver(queue);

    mc::proto::proto_request first;
    ASSERT_EQ(receiver.pop(first), mc::proto::execution_state::completed);
    EXPECT_EQ(payload_from_buffer(first.buffer()), "alpha-done");

    mc::proto::proto_request second;
    ASSERT_EQ(receiver.pop(second), mc::proto::execution_state::completed);
    EXPECT_EQ(payload_from_buffer(second.buffer()), "beta-done");
}

TEST_F(mq_proto_test, stale_source_fragment_does_not_pollute_current_reassembly)
{
    const auto name = make_queue_name("stale_source_interleaved");
    remember_queue(name);

    auto queue = mc::shm::mq_queue(make_options(name, 8));
    ASSERT_TRUE(queue.send_message(7, 99, make_fragment(1, 99, 7, 2, "stale-")));
    ASSERT_TRUE(queue.send_message(7, 1, make_fragment(1, 1, 7, 2, "fresh-")));
    ASSERT_TRUE(queue.send_message(7, 1, make_fragment(1, 1, 7, 1, "done")));

    direct_protocol_chain receiver(queue, 7, 1);

    mc::proto::proto_request req;
    ASSERT_EQ(receiver.pop(req), mc::proto::execution_state::completed);
    EXPECT_EQ(payload_from_buffer(req.buffer()), "fresh-done");
}

TEST_F(mq_proto_test, dead_source_fragments_are_discarded)
{
    const auto name = make_queue_name("dead_source_discard");
    remember_queue(name);

    auto queue = mc::shm::mq_queue(make_options(name, 8));
    ASSERT_TRUE(queue.send_message(7, 1, make_fragment(1, 1, 7, 1, "stale")));

    direct_protocol_chain receiver(queue, 7, 1);
    bool                  source_alive = false;
    receiver.mq.set_source_liveness_probe([&source_alive](std::uint16_t endpoint_id, std::uint64_t instance_id) {
        if (endpoint_id == 7 && instance_id == 1) {
            return source_alive;
        }
        return true;
    });

    mc::proto::proto_request dropped;
    EXPECT_EQ(receiver.pop(dropped), mc::proto::execution_state::suspended);

    source_alive = true;
    ASSERT_TRUE(queue.send_message(7, 1, make_fragment(2, 1, 7, 1, "fresh")));

    ASSERT_TRUE(wait_until([&dropped]() {
        return dropped.state() == mc::proto::execution_state::completed;
    }));
    EXPECT_EQ(payload_from_buffer(dropped.buffer()), "fresh");
    receiver.shutdown();
}

TEST_F(mq_proto_test, partial_assembly_is_dropped_after_source_dies)
{
    const auto name = make_queue_name("dead_source_partial_cleanup");
    remember_queue(name);

    auto queue = mc::shm::mq_queue(make_options(name, 8));

    direct_protocol_chain receiver(queue, 7, 1);
    bool                  source_alive = true;
    receiver.mq.set_source_liveness_probe([&source_alive](std::uint16_t endpoint_id, std::uint64_t instance_id) {
        if (endpoint_id == 7 && instance_id == 1) {
            return source_alive;
        }
        return true;
    });

    ASSERT_TRUE(queue.send_message(7, 1, make_fragment(1, 1, 7, 2, "old-")));

    mc::proto::proto_request first;
    EXPECT_EQ(receiver.pop(first), mc::proto::execution_state::suspended);

    source_alive = false;
    ASSERT_TRUE(queue.send_message(7, 1, make_fragment(1, 1, 7, 1, "tail")));

    mc::proto::proto_request stale_tail;
    EXPECT_EQ(receiver.pop(stale_tail), mc::proto::execution_state::suspended);
    receiver.shutdown();
}
