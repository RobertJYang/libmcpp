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

#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <unordered_set>

#include <mc/runtime/thread_pool.h>
#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/mq.h>
#include <shm/message_queue/mq_notifier.h>
#include <shm/message_queue/mq_watcher.h>
#include <test_utilities/base.h>

namespace {

std::string make_unique_name(std::string_view prefix)
{
    const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::string(prefix) + "_" + std::to_string(getpid()) + "_" + std::to_string(now_ns);
}

class mq_queue_test : public mc::test::TestWithRuntime {
protected:
    void SetUp() override
    {
        TestWithRuntime::SetUp();
        m_queue_name    = make_unique_name("mc_shm_queue");
        m_notifier_name = mc::shm::detail::mq_notifier::make_default_name(m_queue_name);
    }

    void TearDown() override
    {
        mc::shm::detail::mq_notifier::remove(m_notifier_name);
        mc::shm::detail::mq_notifier::remove(mc::shm::detail::mq_notifier::make_space_name(m_queue_name));
        mc::shm::detail::shared_memory_backend::remove(m_queue_name);
        TestWithRuntime::TearDown();
    }

    mc::shm::mq_queue_options make_options() const
    {
        mc::shm::mq_queue_options options;
        options.shared_memory_name = m_queue_name;
        options.slot_count         = 64;
        return options;
    }

    std::string m_queue_name;
    std::string m_notifier_name;
};

TEST_F(mq_queue_test, rejects_oversized_single_message_but_keeps_normal_delivery)
{
    mc::shm::mq_queue queue(make_options());
    ASSERT_TRUE(queue.is_valid());

    EXPECT_FALSE(queue.send_message(9, 1, std::string(600, 'x')));

    ASSERT_TRUE(queue.send_message(1, 7, "ready"));
    auto received = queue.try_receive_message([](std::uint32_t writer_id, std::uint64_t instance_id) {
        return writer_id == 1 && instance_id == 7;
    });
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->payload, "ready");
}

TEST_F(mq_queue_test, watcher_integrates_with_runtime_executor)
{
    mc::shm::mq_queue queue(make_options());
    ASSERT_TRUE(queue.is_valid());

    std::promise<mc::shm::mq_queue_message> promise;
    auto                                    future = promise.get_future();

    auto watcher =
        std::make_shared<mc::shm::detail::mq_watcher>(mc::get_io_executor(), &queue, [&queue, &promise]() mutable {
        auto message = queue.try_receive_message([](std::uint32_t writer_id, std::uint64_t instance_id) {
            return writer_id == 3 && instance_id == 9;
        });
        if (message.has_value()) {
            promise.set_value(*message);
        }
    });
    watcher->start();

    ASSERT_TRUE(queue.send_message(3, 9, "watched"));
    ASSERT_EQ(future.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    EXPECT_EQ(future.get().payload, "watched");

    watcher->stop();
}

TEST_F(mq_queue_test, watcher_accepts_move_only_handler)
{
    mc::shm::mq_queue queue(make_options());
    ASSERT_TRUE(queue.is_valid());

    auto done = std::make_unique<std::promise<mc::shm::mq_queue_message>>();
    auto wait = done->get_future();

    auto watcher = std::make_shared<mc::shm::detail::mq_watcher>(mc::get_io_executor(), &queue,
                                                                 [&queue, done = std::move(done)]() mutable {
        auto message = queue.try_receive_message([](std::uint32_t writer_id, std::uint64_t instance_id) {
            return writer_id == 5 && instance_id == 11;
        });
        if (message.has_value()) {
            done->set_value(*message);
        }
    });
    watcher->start();

    ASSERT_TRUE(queue.send_message(5, 11, "move-only"));
    ASSERT_EQ(wait.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    EXPECT_EQ(wait.get().payload, "move-only");

    watcher->stop();
}

TEST_F(mq_queue_test, try_receive_message_skips_old_instance_message)
{
    mc::shm::mq_queue queue(make_options());
    ASSERT_TRUE(queue.is_valid());

    ASSERT_TRUE(queue.send_message(7, 1, "old"));
    ASSERT_TRUE(queue.send_message(7, 2, "new"));

    auto received = queue.try_receive_message([](std::uint32_t writer_id, std::uint64_t instance_id) {
        return writer_id == 7 && instance_id == 2;
    });

    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->writer_id, 7U);
    EXPECT_EQ(received->writer_instance_id, 2U);
    EXPECT_EQ(received->payload, "new");
}

TEST_F(mq_queue_test, watcher_drains_backlog_after_start)
{
    auto options       = make_options();
    options.slot_count = 8;

    mc::shm::mq_queue queue(options);
    ASSERT_TRUE(queue.is_valid());

    ASSERT_TRUE(queue.send_message(1, 1, "m1"));
    ASSERT_TRUE(queue.send_message(1, 1, "m2"));
    ASSERT_TRUE(queue.send_message(1, 1, "m3"));
    ASSERT_TRUE(queue.send_message(1, 1, "m4"));

    mc::runtime::thread_pool pool(2, "mq_backlog_drain");
    pool.start();

    std::promise<void>       done;
    auto                     wait = done.get_future();
    std::mutex               mutex;
    std::vector<mc::string>  deliveries;
    std::atomic<std::size_t> delivered{0};
    std::atomic<bool>        finished{false};

    auto watcher = std::make_shared<mc::shm::detail::mq_watcher>(pool.get_executor(), &queue, [&]() mutable {
        while (true) {
            auto message = queue.try_receive_message([](std::uint32_t, std::uint64_t) {
                return true;
            });
            if (!message.has_value()) {
                return;
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                deliveries.push_back(message->payload);
            }
            if (delivered.fetch_add(1) + 1 == 4 && !finished.exchange(true)) {
                done.set_value();
            }
        }
    });
    watcher->start();

    ASSERT_EQ(wait.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    watcher->stop();
    pool.stop();
    pool.join();

    ASSERT_EQ(deliveries.size(), 4U);
    EXPECT_EQ(deliveries[0], "m1");
    EXPECT_EQ(deliveries[1], "m2");
    EXPECT_EQ(deliveries[2], "m3");
    EXPECT_EQ(deliveries[3], "m4");
}

TEST_F(mq_queue_test, multi_writer_single_reader_receives_all_messages)
{
    auto options       = make_options();
    options.slot_count = 8;

    mc::shm::mq_queue queue(options);
    ASSERT_TRUE(queue.is_valid());

    mc::runtime::thread_pool pool(4, "mq_multi_writer");
    pool.start();

    constexpr std::size_t writer_count        = 4;
    constexpr std::size_t messages_per_writer = 24;
    constexpr std::size_t total_message_count = writer_count * messages_per_writer;

    std::promise<void>              done;
    auto                            wait = done.get_future();
    std::mutex                      mutex;
    std::unordered_set<std::string> deliveries;
    std::atomic<std::size_t>        delivered{0};
    std::atomic<bool>               all_sent{true};
    std::atomic<bool>               finished{false};

    auto watcher = std::make_shared<mc::shm::detail::mq_watcher>(pool.get_executor(), &queue, [&]() mutable {
        while (true) {
            auto message = queue.try_receive_message([](std::uint32_t, std::uint64_t) {
                return true;
            });
            if (!message.has_value()) {
                return;
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                deliveries.emplace(message->payload.c_str());
            }
            if (delivered.fetch_add(1) + 1 == total_message_count && !finished.exchange(true)) {
                done.set_value();
            }
        }
    });
    watcher->start();

    std::vector<std::thread> writers;
    writers.reserve(writer_count);
    for (std::size_t writer = 0; writer < writer_count; ++writer) {
        writers.emplace_back([&, writer]() {
            for (std::size_t index = 0; index < messages_per_writer; ++index) {
                const auto payload = std::string("w") + std::to_string(writer) + ":" + std::to_string(index);
                if (!queue.send_message_wait(static_cast<std::uint32_t>(writer + 1), 1, payload,
                                             std::chrono::milliseconds(2000))) {
                    all_sent.store(false);
                    return;
                }
            }
        });
    }
    for (auto& writer : writers) {
        writer.join();
    }

    ASSERT_TRUE(all_sent.load());
    ASSERT_EQ(wait.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    watcher->stop();
    pool.stop();
    pool.join();

    ASSERT_EQ(deliveries.size(), total_message_count);
    for (std::size_t writer = 0; writer < writer_count; ++writer) {
        for (std::size_t index = 0; index < messages_per_writer; ++index) {
            const auto payload = std::string("w") + std::to_string(writer) + ":" + std::to_string(index);
            EXPECT_NE(deliveries.find(payload), deliveries.end());
        }
    }
}

TEST_F(mq_queue_test, send_message_wait_succeeds_after_reader_frees_space)
{
    auto options       = make_options();
    options.slot_count = 1;

    mc::shm::mq_queue queue(options);
    ASSERT_TRUE(queue.is_valid());

    ASSERT_TRUE(queue.send_message(1, 1, "first"));

    auto writer = std::async(std::launch::async, [&queue]() {
        return queue.send_message_wait(1, 1, "second", std::chrono::milliseconds(1000));
    });

    EXPECT_EQ(writer.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);

    auto first = queue.try_receive_message([](std::uint32_t, std::uint64_t) {
        return true;
    });
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->payload, "first");

    ASSERT_EQ(writer.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    EXPECT_TRUE(writer.get());

    auto second = queue.try_receive_message([](std::uint32_t, std::uint64_t) {
        return true;
    });
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->payload, "second");
}

} // namespace
