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

#include <mc/runtime.h>
#include <mc/runtime/steady_timer.h>
#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/message_queue/mq_queue.h>
#include <mc/shm/message_queue/mq_transport_proto.h>
#include <test_utilities/base.h>

#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "shm/message_queue/mq_notifier.h"

namespace {

using namespace std::chrono_literals;

std::string make_queue_name(const std::string& base)
{
    const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return "mc_mq_transport_proto_" + base + "_" + std::to_string(::getpid()) + "_" + std::to_string(now_ns);
}

mc::shm::mq_queue_options make_options(const std::string& name, std::size_t slot_count = 16)
{
    mc::shm::mq_queue_options opts;
    opts.shared_memory_name = name;
    opts.slot_count         = slot_count;
    return opts;
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

class mq_transport_proto_test : public mc::test::TestWithRuntime {
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

TEST_F(mq_transport_proto_test, push_sends_payload_to_queue)
{
    const auto name = make_queue_name("push");
    remember_queue(name);

    auto                        queue = mc::shm::mq_queue(make_options(name));
    mc::shm::mq_transport_proto transport;
    transport.configure(queue, 7, 101);

    mc::proto::proto_request req;
    req.buffer().append_payload("abc", 3);

    const auto state = transport.push(req);
    ASSERT_EQ(state, mc::proto::execution_state::completed);

    auto message = queue.try_receive_message();
    ASSERT_TRUE(message.has_value());
    EXPECT_EQ(message->writer_id, 7U);
    EXPECT_EQ(message->writer_instance_id, 101U);
    EXPECT_EQ(message->payload, "abc");
}

TEST_F(mq_transport_proto_test, pop_reads_payload_from_queue)
{
    const auto name = make_queue_name("pop");
    remember_queue(name);

    auto queue = mc::shm::mq_queue(make_options(name));
    ASSERT_TRUE(queue.send_message(9, 303, "hello"));

    mc::shm::mq_transport_proto transport;
    transport.configure(queue, 7, 101);

    mc::proto::proto_request req;
    const auto               state = transport.pop(req);

    ASSERT_EQ(state, mc::proto::execution_state::completed);
    EXPECT_EQ(payload_from_buffer(req.buffer()), "hello");
}

TEST_F(mq_transport_proto_test, pop_preserves_fifo_order)
{
    const auto name = make_queue_name("validator");
    remember_queue(name);

    auto queue = mc::shm::mq_queue(make_options(name));
    ASSERT_TRUE(queue.send_message(1, 11, "discard"));
    ASSERT_TRUE(queue.send_message(7, 101, "accept"));

    mc::shm::mq_transport_proto transport;
    transport.configure(queue, 7, 101);

    mc::proto::proto_request req;
    const auto               state = transport.pop(req);

    ASSERT_EQ(state, mc::proto::execution_state::completed);
    EXPECT_EQ(payload_from_buffer(req.buffer()), "discard");
}

TEST_F(mq_transport_proto_test, suspend_then_resume_after_message_arrives)
{
    const auto name = make_queue_name("resume");
    remember_queue(name);

    auto queue = mc::shm::mq_queue(make_options(name));

    mc::shm::mq_transport_proto transport;
    transport.configure(queue, 7, 101);

    mc::proto::proto_request req;
    const auto               first = transport.pop(req);
    ASSERT_EQ(first, mc::proto::execution_state::suspended);

    std::promise<void> done;
    auto               wait_done = done.get_future();

    mc::runtime::steady_timer timer(mc::get_io_executor());
    timer.expires_after(20ms);
    timer.async_wait([&queue, &transport, &req, p = std::move(done)](const std::error_code& ec) mutable {
        ASSERT_FALSE(ec);
        ASSERT_TRUE(queue.send_message(7, 101, "later"));

        const auto second = transport.resume(req);
        ASSERT_EQ(second, mc::proto::execution_state::completed);
        EXPECT_EQ(payload_from_buffer(req.buffer()), "later");
        p.set_value();
    });

    EXPECT_EQ(wait_done.wait_for(5s), std::future_status::ready);
}

TEST_F(mq_transport_proto_test, push_suspends_until_queue_has_space)
{
    const auto name = make_queue_name("push_resume");
    remember_queue(name);

    auto options       = make_options(name, 1);
    auto queue         = mc::shm::mq_queue(options);
    ASSERT_TRUE(queue.send_message(1, 1, "busy"));

    mc::shm::mq_transport_proto transport;
    transport.configure(queue, 7, 101);

    mc::proto::proto_request req;
    req.buffer().append_payload("later", 5);

    const auto first = transport.push(req);
    ASSERT_EQ(first, mc::proto::execution_state::suspended);

    auto released = queue.try_receive_message();
    ASSERT_TRUE(released.has_value());
    EXPECT_EQ(released->payload, "busy");

    auto deadline = std::chrono::steady_clock::now() + 2s;
    std::optional<mc::shm::mq_queue_message> message;
    while (std::chrono::steady_clock::now() < deadline && !message.has_value()) {
        message = queue.try_receive_message();
        if (!message.has_value()) {
            std::this_thread::sleep_for(10ms);
        }
    }
    ASSERT_TRUE(message.has_value());
    EXPECT_EQ(message->payload, "later");
    EXPECT_EQ(message->writer_id, 7U);
    EXPECT_EQ(message->writer_instance_id, 101U);
    EXPECT_EQ(req.state(), mc::proto::execution_state::completed);
}

} // namespace
