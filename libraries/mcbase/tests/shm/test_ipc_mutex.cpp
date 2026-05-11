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
#include <thread>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/ipc_mutex.h>
#include <mc/shm/ipc_shared_mutex.h>
#include <mc/shm/region.h>

namespace {

std::string make_unique_name(std::string_view prefix)
{
    const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::string(prefix) + "_" + std::to_string(getpid()) + "_" + std::to_string(now_ns);
}

class shm_ipc_mutex_fixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_segment_name = make_unique_name("mc_shm_ipc_mutex_fixture");

        mc::shm::shm_region_options options;
        options.segment_name = m_segment_name;
        options.total_size   = 256 * 1024;

        m_region = std::make_unique<mc::shm::shm_region>(options);
        ASSERT_TRUE(m_region->is_valid());
        m_allocator = m_region->user_arena();
        ASSERT_TRUE(m_allocator.is_valid());
    }

    void TearDown() override
    {
        m_region.reset();
        mc::shm::detail::shared_memory_backend::remove(m_segment_name);
    }

    std::string                              m_segment_name;
    std::unique_ptr<mc::shm::shm_region>     m_region;
    mc::shm::shm_allocator                   m_allocator{};
};

#if defined(__unix__) || defined(__APPLE__)
TEST(shm_ipc_mutex_test, mutex_recovers_after_holder_crash)
{
    const auto segment_name = make_unique_name("mc_shm_ipc_mutex");
    auto cleanup = [&]() { mc::shm::detail::shared_memory_backend::remove(segment_name); };

    mc::shm::shm_region_options options;
    options.segment_name = segment_name;
    options.total_size = 256 * 1024;

    mc::shm::shm_region region(options);
    ASSERT_TRUE(region.is_valid());
    auto allocator = region.user_arena();
    auto* mutex = allocator.construct<mc::shm::ipc_mutex>();
    ASSERT_NE(mutex, nullptr);

    const auto child = fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        mutex->lock();
        _exit(0);
    }

    int status = 0;
    ASSERT_EQ(waitpid(child, &status, 0), child);
    EXPECT_TRUE(WIFEXITED(status));

    const auto state = mutex->try_lock_for_ex(std::chrono::milliseconds(200));
    EXPECT_EQ(state, mc::shm::lock_acquire_state::recovered);
    mutex->unlock();

    cleanup();
}

TEST(shm_ipc_mutex_test, shared_mutex_cleans_dead_reader)
{
    const auto segment_name = make_unique_name("mc_shm_ipc_shared_mutex");
    auto cleanup = [&]() { mc::shm::detail::shared_memory_backend::remove(segment_name); };

    mc::shm::shm_region_options options;
    options.segment_name = segment_name;
    options.total_size = 256 * 1024;

    mc::shm::shm_region region(options);
    ASSERT_TRUE(region.is_valid());
    auto allocator = region.user_arena();
    auto* mutex = allocator.construct<mc::shm::ipc_shared_mutex>();
    ASSERT_NE(mutex, nullptr);

    const auto child = fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        mutex->lock_shared();
        _exit(0);
    }

    int status = 0;
    ASSERT_EQ(waitpid(child, &status, 0), child);
    EXPECT_TRUE(WIFEXITED(status));

    EXPECT_NE(mutex->try_lock_for_ex(std::chrono::milliseconds(200)), mc::shm::lock_acquire_state::failed);
    mutex->unlock();

    cleanup();
}
#else
TEST(shm_ipc_mutex_test, mutex_recovers_after_holder_crash)
{
    GTEST_SKIP() << "requires unix fork";
}

TEST(shm_ipc_mutex_test, shared_mutex_cleans_dead_reader)
{
    GTEST_SKIP() << "requires unix fork";
}
#endif

// --- ipc_mutex 线程级语义与单元行为 ---

TEST_F(shm_ipc_mutex_fixture, try_lock_succeeds_when_free_and_fails_when_held)
{
    auto* mutex = m_allocator.construct<mc::shm::ipc_mutex>();
    ASSERT_NE(mutex, nullptr);

    EXPECT_TRUE(mutex->try_lock());
    EXPECT_TRUE(mutex->is_locked_by_current_thread());
    EXPECT_FALSE(mutex->try_lock());
    mutex->unlock();
    EXPECT_FALSE(mutex->is_locked_by_current_process());
    EXPECT_TRUE(mutex->try_lock());
    mutex->unlock();
}

TEST_F(shm_ipc_mutex_fixture, threads_in_same_process_serialize)
{
    auto* mutex = m_allocator.construct<mc::shm::ipc_mutex>();
    ASSERT_NE(mutex, nullptr);

    constexpr int            thread_count    = 8;
    constexpr int            increments_each = 500;
    std::atomic<int>         counter{0};
    std::atomic<int>         race_detector{0};
    std::atomic<int>         observed_race{0};
    std::vector<std::thread> workers;
    workers.reserve(thread_count);
    for (int t = 0; t < thread_count; ++t) {
        workers.emplace_back([&]() {
            for (int i = 0; i < increments_each; ++i) {
                mutex->lock();
                if (race_detector.fetch_add(1, std::memory_order_acq_rel) != 0) {
                    observed_race.fetch_add(1, std::memory_order_relaxed);
                }
                ++counter;
                race_detector.fetch_sub(1, std::memory_order_acq_rel);
                mutex->unlock();
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }

    EXPECT_EQ(counter.load(), thread_count * increments_each);
    EXPECT_EQ(observed_race.load(), 0);
}

TEST_F(shm_ipc_mutex_fixture, unlock_from_non_owner_thread_is_noop)
{
    auto* mutex = m_allocator.construct<mc::shm::ipc_mutex>();
    ASSERT_NE(mutex, nullptr);

    EXPECT_TRUE(mutex->try_lock());

    std::atomic<bool> other_saw_locked{false};
    std::thread       other([&]() {
        // 另一线程不应能解锁本线程持有的互斥体。
        mutex->unlock();
        other_saw_locked.store(mutex->is_locked_by_current_process());
    });
    other.join();

    EXPECT_TRUE(other_saw_locked.load());
    EXPECT_TRUE(mutex->is_locked_by_current_thread());
    mutex->unlock();
}

TEST_F(shm_ipc_mutex_fixture, try_lock_for_times_out_when_held_by_another_thread)
{
    auto* mutex = m_allocator.construct<mc::shm::ipc_mutex>();
    ASSERT_NE(mutex, nullptr);

    mutex->lock();
    std::atomic<bool> done{false};
    std::thread       worker([&]() {
        const auto start = std::chrono::steady_clock::now();
        const auto state = mutex->try_lock_for_ex(std::chrono::milliseconds(80));
        const auto elapsed = std::chrono::steady_clock::now() - start;
        EXPECT_EQ(state, mc::shm::lock_acquire_state::failed);
        EXPECT_GE(elapsed, std::chrono::milliseconds(70));
        done.store(true);
    });
    worker.join();
    EXPECT_TRUE(done.load());
    mutex->unlock();
}

TEST_F(shm_ipc_mutex_fixture, refresh_lease_only_works_for_owner_thread)
{
    auto* mutex = m_allocator.construct<mc::shm::ipc_mutex>();
    ASSERT_NE(mutex, nullptr);

    mutex->lock();
    EXPECT_TRUE(mutex->refresh_lease());

    std::atomic<bool> other_result{true};
    std::thread       other([&]() { other_result.store(mutex->refresh_lease()); });
    other.join();
    EXPECT_FALSE(other_result.load());
    mutex->unlock();
}

// --- ipc_shared_mutex 线程级语义 ---

TEST_F(shm_ipc_mutex_fixture, shared_mutex_multiple_threads_can_hold_shared)
{
    auto* mutex = m_allocator.construct<mc::shm::ipc_shared_mutex>();
    ASSERT_NE(mutex, nullptr);

    constexpr int            reader_count = 8;
    std::atomic<int>         in_critical{0};
    std::atomic<int>         max_concurrent{0};
    std::atomic<bool>        start_flag{false};
    std::vector<std::thread> readers;
    readers.reserve(reader_count);
    for (int i = 0; i < reader_count; ++i) {
        readers.emplace_back([&]() {
            while (!start_flag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            mutex->lock_shared();
            const auto current = in_critical.fetch_add(1, std::memory_order_acq_rel) + 1;
            int max_obs = max_concurrent.load(std::memory_order_relaxed);
            while (current > max_obs &&
                   !max_concurrent.compare_exchange_weak(max_obs, current, std::memory_order_relaxed)) {
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
            in_critical.fetch_sub(1, std::memory_order_acq_rel);
            mutex->unlock_shared();
        });
    }
    start_flag.store(true, std::memory_order_release);
    for (auto& t : readers) {
        t.join();
    }

    EXPECT_GE(max_concurrent.load(), 2);
}

TEST_F(shm_ipc_mutex_fixture, shared_mutex_writer_blocks_readers)
{
    auto* mutex = m_allocator.construct<mc::shm::ipc_shared_mutex>();
    ASSERT_NE(mutex, nullptr);

    mutex->lock();
    std::atomic<bool> reader_acquired{false};
    std::thread       reader([&]() {
        const auto state = mutex->try_lock_shared_ex();
        if (state != mc::shm::lock_acquire_state::failed) {
            reader_acquired.store(true);
            mutex->unlock_shared();
        }
    });
    reader.join();
    EXPECT_FALSE(reader_acquired.load());
    mutex->unlock();

    // 写解锁后 reader 应能拿到共享锁。
    std::thread reader2([&]() {
        EXPECT_NE(mutex->try_lock_shared_ex(), mc::shm::lock_acquire_state::failed);
        mutex->unlock_shared();
    });
    reader2.join();
}

TEST_F(shm_ipc_mutex_fixture, shared_mutex_reader_blocks_writer)
{
    auto* mutex = m_allocator.construct<mc::shm::ipc_shared_mutex>();
    ASSERT_NE(mutex, nullptr);

    mutex->lock_shared();
    std::atomic<bool> writer_acquired{false};
    std::thread       writer([&]() {
        const auto state = mutex->try_lock_for_ex(std::chrono::milliseconds(80));
        if (state != mc::shm::lock_acquire_state::failed) {
            writer_acquired.store(true);
            mutex->unlock();
        }
    });
    writer.join();
    EXPECT_FALSE(writer_acquired.load());
    mutex->unlock_shared();

    // 所有 reader 退出后，writer 应能取得锁。
    std::thread writer2([&]() {
        EXPECT_NE(mutex->try_lock_for_ex(std::chrono::milliseconds(80)), mc::shm::lock_acquire_state::failed);
        mutex->unlock();
    });
    writer2.join();
}

TEST_F(shm_ipc_mutex_fixture, shared_mutex_unlock_shared_from_non_owner_thread_is_noop)
{
    auto* mutex = m_allocator.construct<mc::shm::ipc_shared_mutex>();
    ASSERT_NE(mutex, nullptr);

    mutex->lock_shared();

    // 其他线程误调 unlock_shared，不能把本线程占用的 slot 清掉。
    std::thread other([&]() { mutex->unlock_shared(); });
    other.join();

    // writer 仍应因本线程持 shared 而无法获取。
    std::atomic<bool> writer_acquired{false};
    std::thread       writer([&]() {
        if (mutex->try_lock_for_ex(std::chrono::milliseconds(40)) != mc::shm::lock_acquire_state::failed) {
            writer_acquired.store(true);
            mutex->unlock();
        }
    });
    writer.join();
    EXPECT_FALSE(writer_acquired.load());

    mutex->unlock_shared();
}

TEST_F(shm_ipc_mutex_fixture, shared_mutex_reader_slot_limit_is_enforced)
{
    auto* mutex = m_allocator.construct<mc::shm::ipc_shared_mutex>();
    ASSERT_NE(mutex, nullptr);

    // 连续从不同线程获取，直到 32 个 slot 被占满，下一次应失败。
    std::atomic<bool>        ready_to_release{false};
    std::atomic<std::size_t> acquired_count{0};
    std::vector<std::thread> workers;
    workers.reserve(mc::shm::ipc_shared_mutex::max_readers);
    for (std::size_t i = 0; i < mc::shm::ipc_shared_mutex::max_readers; ++i) {
        workers.emplace_back([&]() {
            mutex->lock_shared();
            acquired_count.fetch_add(1, std::memory_order_acq_rel);
            while (!ready_to_release.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            mutex->unlock_shared();
        });
    }
    while (acquired_count.load(std::memory_order_acquire) < mc::shm::ipc_shared_mutex::max_readers) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // 第 33 位 reader 应失败。
    EXPECT_FALSE(mutex->try_lock_shared());

    ready_to_release.store(true, std::memory_order_release);
    for (auto& w : workers) {
        w.join();
    }

    EXPECT_TRUE(mutex->try_lock_shared());
    mutex->unlock_shared();
}

} // namespace
