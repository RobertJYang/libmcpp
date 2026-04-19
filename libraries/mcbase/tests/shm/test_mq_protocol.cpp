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
#include <cstring>
#include <future>
#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include <mc/future.h>
#include <mc/protocol.h>
#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/mq.h>
#include <mc/time.h>
#include <test_utilities/base.h>

#include "shm/message_queue/mq_notifier.h"

using namespace std::chrono_literals;

namespace {

struct mq_application_proto : mc::proto::protocol {
    struct packet {
        mc::string inbound_payload;
    };

    struct context {};

    template <typename Req>
    static mc::proto::command on_push(Req& req)
    {
        return req.push_next();
    }

    template <typename Req>
    static mc::proto::command on_pop(Req& req)
    {
        const auto& buffer  = req.buffer();
        const auto  base    = buffer.payload_base();
        const auto  size    = buffer.length() >= base ? buffer.length() - base : 0U;
        auto&       payload = req.template packet<mq_application_proto>().inbound_payload;
        payload.clear();
        payload.append(reinterpret_cast<const char*>(buffer.data() + base), size);
        return req.complete();
    }
};

using mq_test_proto = mc::proto::stack<mq_application_proto, mc::shm::mq_proto, mc::shm::mq_transport_proto>;

using mq_protocol_tuple = typename mc::proto::runtime<typename mq_test_proto::spec_type>::protocol_tuple_type;

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

mq_protocol_tuple make_protocols(mc::shm::mq_queue& queue, std::uint32_t writer_id = 7, std::uint32_t instance_id = 1)
{
    mq_protocol_tuple protocols;
    auto&             transport = std::get<2>(protocols);
    auto&             mq        = std::get<1>(protocols);
    transport.configure(queue, writer_id, instance_id, [](std::uint32_t, std::uint64_t) {
        return true;
    });
    mq.configure(instance_id, static_cast<std::uint16_t>(writer_id));
    return protocols;
}

void append_payload(mc::proto::buffer& buffer, const std::string& payload)
{
    buffer.append_payload(payload.data(), payload.size());
}

mc::proto::execution_state drain_pop(mq_test_proto::request& req, mq_protocol_tuple& protocols)
{
    auto state = mc::proto::runtime<typename mq_test_proto::spec_type>::pop(req, protocols);
    while (state == mc::proto::execution_state::suspended) {
        state = mc::proto::runtime<typename mq_test_proto::spec_type>::pop(req, protocols);
    }
    return state;
}

class mq_protocol_test : public mc::test::TestWithRuntime {
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

} // anonymous namespace

TEST_F(mq_protocol_test, single_fragment_send_and_receive)
{
    const auto name = make_queue_name("single");
    remember_queue(name);
    auto opts      = make_options(name);
    auto queue     = mc::shm::mq_queue(opts);
    auto protocols = make_protocols(queue);

    auto outbound_request = mq_test_proto::make_request();
    auto completion       = outbound_request.enable_async_completion();
    outbound_request.prepare_buffer();
    append_payload(outbound_request.buffer(), "abc");
    ASSERT_EQ(mc::proto::runtime<typename mq_test_proto::spec_type>::push(outbound_request, protocols),
              mc::proto::execution_state::completed);
    ASSERT_EQ(completion.wait_for(2s), mc::future_status::ready);
    EXPECT_NO_THROW(completion.get());

    auto inbound_request = mq_test_proto::make_request();
    ASSERT_EQ(drain_pop(inbound_request, protocols), mc::proto::execution_state::completed);
    EXPECT_EQ(inbound_request.packet<mq_application_proto>().inbound_payload, "abc");
}

TEST_F(mq_protocol_test, multi_fragment_send_and_receive)
{
    const std::string payload(400, 'L');
    const auto        name = make_queue_name("multi");
    remember_queue(name);
    auto opts      = make_options(name, 16);
    auto queue     = mc::shm::mq_queue(opts);
    auto protocols = make_protocols(queue);

    auto outbound_request = mq_test_proto::make_request();
    auto completion       = outbound_request.enable_async_completion();
    outbound_request.prepare_buffer();
    append_payload(outbound_request.buffer(), payload);
    ASSERT_EQ(mc::proto::runtime<typename mq_test_proto::spec_type>::push(outbound_request, protocols),
              mc::proto::execution_state::completed);
    ASSERT_EQ(completion.wait_for(2s), mc::future_status::ready);
    EXPECT_NO_THROW(completion.get());

    auto inbound_request = mq_test_proto::make_request();
    ASSERT_EQ(drain_pop(inbound_request, protocols), mc::proto::execution_state::completed);
    EXPECT_EQ(inbound_request.packet<mq_application_proto>().inbound_payload, payload);
}

TEST_F(mq_protocol_test, deadline_timeout_marks_future_failed)
{
    const auto name = make_queue_name("timeout");
    remember_queue(name);
    auto opts      = make_options(name, 1);
    auto queue     = mc::shm::mq_queue(opts);
    auto protocols = make_protocols(queue);

    ASSERT_TRUE(queue.send_message(99, 1, "occupied"));

    auto req        = mq_test_proto::make_request();
    auto completion = req.enable_async_completion();
    req.prepare_buffer();
    append_payload(req.buffer(), "wait-timeout");
    req.set_timeout(mc::milliseconds(100));
    ASSERT_EQ(mc::proto::runtime<typename mq_test_proto::spec_type>::push(req, protocols),
              mc::proto::execution_state::completed);
    ASSERT_EQ(completion.wait_for(2s), mc::future_status::ready);
    EXPECT_ANY_THROW(completion.get());
}

TEST_F(mq_protocol_test, full_queue_wait_eventually_succeeds)
{
    const auto name = make_queue_name("wait-success");
    remember_queue(name);
    auto opts      = make_options(name, 1);
    auto queue     = mc::shm::mq_queue(opts);
    auto protocols = make_protocols(queue);

    ASSERT_TRUE(queue.send_message(99, 1, "occupied"));

    auto reader = std::async(std::launch::async, [&queue]() {
        std::this_thread::sleep_for(50ms);
        return queue.try_receive_message([](std::uint32_t, std::uint64_t) {
            return true;
        });
    });

    auto req        = mq_test_proto::make_request();
    auto completion = req.enable_async_completion();
    req.prepare_buffer();
    append_payload(req.buffer(), "wait-success");
    req.set_timeout(mc::seconds(1));
    ASSERT_EQ(mc::proto::runtime<typename mq_test_proto::spec_type>::push(req, protocols),
              mc::proto::execution_state::completed);
    ASSERT_EQ(completion.wait_for(2s), mc::future_status::ready);
    EXPECT_NO_THROW(completion.get());
    ASSERT_EQ(reader.wait_for(1s), std::future_status::ready);
    ASSERT_TRUE(reader.get().has_value());
}

TEST_F(mq_protocol_test, cancel_pending_future_marks_request_failed)
{
    const auto name = make_queue_name("cancel");
    remember_queue(name);
    auto opts      = make_options(name, 1);
    auto queue     = mc::shm::mq_queue(opts);
    auto protocols = make_protocols(queue);

    ASSERT_TRUE(queue.send_message(99, 1, "occupied"));

    auto req        = mq_test_proto::make_request();
    auto completion = req.enable_async_completion();
    req.prepare_buffer();
    append_payload(req.buffer(), "cancel-me");
    ASSERT_EQ(mc::proto::runtime<typename mq_test_proto::spec_type>::push(req, protocols),
              mc::proto::execution_state::completed);

    completion.cancel();

    ASSERT_EQ(completion.wait_for(2s), mc::future_status::ready);
    EXPECT_TRUE(req.is_cancelled());
    EXPECT_ANY_THROW(completion.get());
}

TEST_F(mq_protocol_test, stop_rejects_pending_future)
{
    const auto name = make_queue_name("stop");
    remember_queue(name);
    auto opts      = make_options(name, 1);
    auto queue     = mc::shm::mq_queue(opts);
    auto protocols = make_protocols(queue);

    ASSERT_TRUE(queue.send_message(99, 1, "occupied"));

    auto req        = mq_test_proto::make_request();
    auto completion = req.enable_async_completion();
    req.prepare_buffer();
    append_payload(req.buffer(), "pending");
    ASSERT_EQ(mc::proto::runtime<typename mq_test_proto::spec_type>::push(req, protocols),
              mc::proto::execution_state::completed);

    std::get<2>(protocols).stop();
    ASSERT_EQ(completion.wait_for(2s), mc::future_status::ready);
    EXPECT_ANY_THROW(completion.get());
}

TEST_F(mq_protocol_test, old_instance_fragment_is_filtered)
{
    const auto name = make_queue_name("instance");
    remember_queue(name);
    auto opts      = make_options(name, 16);
    auto queue     = mc::shm::mq_queue(opts);
    auto protocols = make_protocols(queue, 7, 1);

    ASSERT_TRUE(queue.send_message(7, 99, make_fragment(1, 99, 7, 1, "old")));
    ASSERT_TRUE(queue.send_message(7, 1, make_fragment(2, 1, 7, 1, "new")));

    auto inbound_request = mq_test_proto::make_request();
    ASSERT_EQ(drain_pop(inbound_request, protocols), mc::proto::execution_state::completed);
    EXPECT_EQ(inbound_request.packet<mq_application_proto>().inbound_payload, "new");
}
