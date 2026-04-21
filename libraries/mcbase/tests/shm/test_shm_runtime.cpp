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
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/ipc_mutex.h>
#include <mc/shm/ipc_shared_mutex.h>
#include <mc/shm/message_queue/mq_queue.h>
#include <mc/shm/shm_runtime.h>
#include <shm/message_queue/mq_notifier.h>

namespace {

std::string make_unique_name(std::string_view prefix)
{
    const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::string(prefix) + "_" + std::to_string(getpid()) + "_" + std::to_string(now_ns);
}

class shm_runtime_test : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_region_name = make_unique_name("mc_shm_runtime");
    }

    void TearDown() override
    {
        mc::shm::detail::mq_notifier::remove(
            mc::shm::detail::mq_notifier::make_default_name("mc_shm_queue_service.alpha"));
        mc::shm::detail::mq_notifier::remove(
            mc::shm::detail::mq_notifier::make_space_name("mc_shm_queue_service.alpha"));
        mc::shm::detail::shared_memory_backend::remove("mc_shm_queue_service.alpha");
        mc::shm::detail::shared_memory_backend::remove(m_region_name);
    }

    std::string m_region_name;
};

TEST_F(shm_runtime_test, endpoint_restart_reuses_registry_and_updates_instance_id)
{
    mc::shm::runtime_options options;
    options.region_name       = m_region_name;
    options.heartbeat_timeout = std::chrono::milliseconds(50);

    mc::shm::shm_runtime runtime(options);
    ASSERT_TRUE(runtime.is_valid());

    auto first = runtime.register_endpoint("service.alpha", 32);
    ASSERT_TRUE(first.has_value());
    EXPECT_TRUE(runtime.mark_endpoint_running(*first));

    auto second = runtime.register_endpoint("service.alpha", 32);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->endpoint_id, first->endpoint_id);
    EXPECT_GT(second->instance_id, first->instance_id);

    auto snapshot = runtime.get_endpoint(first->endpoint_id);
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->endpoint_name, "service.alpha");
    EXPECT_EQ(snapshot->instance_id, second->instance_id);
}

TEST_F(shm_runtime_test, runtime_opens_queue_and_recovers_stale_endpoint)
{
    mc::shm::runtime_options options;
    options.region_name       = m_region_name;
    options.heartbeat_timeout = std::chrono::milliseconds(1);

    mc::shm::shm_runtime runtime(options);
    ASSERT_TRUE(runtime.is_valid());

    auto endpoint = runtime.register_endpoint("service.alpha", 32);
    ASSERT_TRUE(endpoint.has_value());

    auto queue = runtime.open_queue(*endpoint);
    ASSERT_TRUE(queue.is_valid());
    ASSERT_TRUE(queue.send_message(endpoint->endpoint_id, endpoint->instance_id, "hello"));

    auto received = queue.try_receive_message();
    ASSERT_TRUE(received.has_value());
    EXPECT_TRUE(runtime.writer_instance_is_current(received->writer_id, received->writer_instance_id));
    EXPECT_EQ(received->payload, "hello");

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_EQ(runtime.recover_stale_endpoints(), 1U);

    auto snapshot = runtime.get_endpoint(endpoint->endpoint_id);
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->state, mc::shm::endpoint_state::aborting);
}

TEST_F(shm_runtime_test, named_mutex_is_cached_and_serializes_threads)
{
    mc::shm::runtime_options options;
    options.region_name = m_region_name;

    mc::shm::shm_runtime runtime(options);
    ASSERT_TRUE(runtime.is_valid());

    auto* m1 = runtime.get_or_create_mutex("resource_a");
    auto* m2 = runtime.get_or_create_mutex("resource_a");
    ASSERT_NE(m1, nullptr);
    EXPECT_EQ(m1, m2);

    auto* other = runtime.get_or_create_mutex("resource_b");
    ASSERT_NE(other, nullptr);
    EXPECT_NE(other, m1);

    // 通过命名锁串行化多线程。
    std::atomic<int>         counter{0};
    std::atomic<int>         race{0};
    std::atomic<int>         observed_race{0};
    std::vector<std::thread> workers;
    workers.reserve(4);
    for (int i = 0; i < 4; ++i) {
        workers.emplace_back([&]() {
            for (int j = 0; j < 200; ++j) {
                m1->lock();
                if (race.fetch_add(1, std::memory_order_acq_rel) != 0) {
                    observed_race.fetch_add(1, std::memory_order_relaxed);
                }
                ++counter;
                race.fetch_sub(1, std::memory_order_acq_rel);
                m1->unlock();
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    EXPECT_EQ(counter.load(), 4 * 200);
    EXPECT_EQ(observed_race.load(), 0);
}

TEST_F(shm_runtime_test, named_shared_mutex_allows_multiple_readers)
{
    mc::shm::runtime_options options;
    options.region_name = m_region_name;

    mc::shm::shm_runtime runtime(options);
    ASSERT_TRUE(runtime.is_valid());

    auto* sm = runtime.get_or_create_shared_mutex("shared_rsrc");
    ASSERT_NE(sm, nullptr);

    // 同名类型冲突：mutex / shared_mutex 不能混用。
    EXPECT_EQ(runtime.get_or_create_mutex("shared_rsrc"), nullptr);
    // 同名同类型返回同一实例。
    EXPECT_EQ(runtime.get_or_create_shared_mutex("shared_rsrc"), sm);

    std::atomic<int>         in_read{0};
    std::atomic<int>         peak_readers{0};
    std::atomic<bool>        start{false};
    std::vector<std::thread> readers;
    readers.reserve(6);
    for (int i = 0; i < 6; ++i) {
        readers.emplace_back([&]() {
            while (!start.load()) {
                std::this_thread::yield();
            }
            sm->lock_shared();
            const auto current = in_read.fetch_add(1, std::memory_order_acq_rel) + 1;
            auto       max_obs = peak_readers.load(std::memory_order_relaxed);
            while (current > max_obs &&
                   !peak_readers.compare_exchange_weak(max_obs, current, std::memory_order_relaxed)) {
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            in_read.fetch_sub(1, std::memory_order_acq_rel);
            sm->unlock_shared();
        });
    }
    start.store(true);
    for (auto& r : readers) {
        r.join();
    }
    EXPECT_GE(peak_readers.load(), 2);
}

TEST_F(shm_runtime_test, named_lock_rejects_invalid_and_full_registry)
{
    mc::shm::runtime_options options;
    options.region_name = m_region_name;

    mc::shm::shm_runtime runtime(options);
    ASSERT_TRUE(runtime.is_valid());

    EXPECT_EQ(runtime.get_or_create_mutex(""), nullptr);
    const std::string too_long(mc::shm::shm_runtime::max_named_lock_name + 4, 'x');
    EXPECT_EQ(runtime.get_or_create_mutex(too_long), nullptr);

    for (std::size_t i = 0; i < mc::shm::shm_runtime::max_named_locks; ++i) {
        EXPECT_NE(runtime.get_or_create_mutex("lock_" + std::to_string(i)), nullptr);
    }
    EXPECT_EQ(runtime.get_or_create_mutex("overflow"), nullptr);
}

} // namespace
