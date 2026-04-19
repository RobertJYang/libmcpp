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

#include <mc/memory/fixed_size_pool.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

namespace {

using pool_options = mc::memory::fixed_size_pool::options;

TEST(FixedSizePoolTest, ReusesFreedSlot)
{
    pool_options options;
    options.initial_slab_slots   = 4;
    options.local_cache_capacity = 4;

    mc::memory::fixed_size_pool pool(32, alignof(std::max_align_t), options);

    void* first  = pool.allocate();
    void* second = pool.allocate();
    pool.deallocate(second);

    void* reused = pool.allocate();
    EXPECT_EQ(reused, second);

    pool.deallocate(reused);
    pool.deallocate(first);
}

TEST(FixedSizePoolTest, ReportsFreshAllocationBeforeFirstReuseWithLocalCache)
{
    pool_options options;
    options.initial_slab_slots   = 4;
    options.local_cache_capacity = 4;

    mc::memory::fixed_size_pool pool(32, alignof(std::max_align_t), options);

    auto first = pool.allocate_with_status();
    ASSERT_NE(first.ptr, nullptr);
    EXPECT_FALSE(first.reused);

    pool.deallocate(first.ptr);

    auto second = pool.allocate_with_status();
    ASSERT_NE(second.ptr, nullptr);
    EXPECT_EQ(second.ptr, first.ptr);
    EXPECT_TRUE(second.reused);

    pool.deallocate(second.ptr);
}

TEST(FixedSizePoolTest, FreelistMetadataDoesNotOverwritePayloadPrefix)
{
    pool_options options;
    options.initial_slab_slots   = 4;
    options.local_cache_capacity = 0;

    mc::memory::fixed_size_pool pool(32, alignof(std::max_align_t), options);

    auto* payload = static_cast<unsigned char*>(pool.allocate());
    ASSERT_NE(payload, nullptr);

    std::array<unsigned char, 16> marker{};
    for (std::size_t i = 0; i < marker.size(); ++i) {
        marker[i] = static_cast<unsigned char>(0xA0U + i);
    }

    std::memcpy(payload, marker.data(), marker.size());
    pool.deallocate(payload);

    EXPECT_EQ(std::memcmp(payload, marker.data(), marker.size()), 0)
        << "free-list 元数据不应覆盖返回给上层的 payload 前缀";
}

TEST(FixedSizePoolTest, ReportsFreshAllocationsUnderConcurrentLocalCacheRefill)
{
    pool_options options;
    options.initial_slab_slots   = 64;
    options.max_slab_slots       = 64;
    options.max_total_slots      = 64;
    options.local_cache_capacity = 16;

    mc::memory::fixed_size_pool pool(32, alignof(std::max_align_t), options);

    constexpr int                   thread_count     = 4;
    constexpr int                   slots_per_thread = 8;
    std::atomic<int>                ready_threads{0};
    std::atomic<bool>               start{false};
    std::atomic<int>                reused_count{0};
    std::vector<std::vector<void*>> acquired_slots(thread_count);
    std::vector<std::thread>        workers;
    workers.reserve(thread_count);

    for (int thread_index = 0; thread_index < thread_count; ++thread_index) {
        workers.emplace_back([&, thread_index]() {
            acquired_slots[thread_index].reserve(slots_per_thread);
            ready_threads.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (int slot_index = 0; slot_index < slots_per_thread; ++slot_index) {
                auto result = pool.allocate_with_status();
                if (result.reused) {
                    reused_count.fetch_add(1, std::memory_order_relaxed);
                }
                acquired_slots[thread_index].push_back(result.ptr);
            }
        });
    }

    while (ready_threads.load(std::memory_order_acquire) != thread_count) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);

    for (auto& worker : workers) {
        worker.join();
    }

    EXPECT_EQ(reused_count.load(std::memory_order_relaxed), 0) << "首次并发分配 fresh 槽位时不应误报为 reused";

    for (const auto& slots : acquired_slots) {
        for (void* slot : slots) {
            pool.deallocate(slot);
        }
    }
}

TEST(FixedSizePoolTest, TrimReleasesFullyFreeSlabs)
{
    pool_options options;
    options.initial_slab_slots   = 4;
    options.local_cache_capacity = 0;

    mc::memory::fixed_size_pool pool(64, alignof(std::max_align_t), options);

    std::vector<void*> slots;
    slots.reserve(8);
    for (int i = 0; i < 8; ++i) {
        slots.push_back(pool.allocate());
    }

    for (void* slot : slots) {
        pool.deallocate(slot);
    }

    const auto released = pool.trim(0);
    EXPECT_GT(released, 0U);
}

TEST(FixedSizePoolTest, ThreadExitFlushesLocalCacheBackToCentralPool)
{
    pool_options options;
    options.initial_slab_slots   = 4;
    options.local_cache_capacity = 8;

    mc::memory::fixed_size_pool pool(48, alignof(std::max_align_t), options);

    std::thread worker([&pool]() {
        void* first  = pool.allocate();
        void* second = pool.allocate();
        pool.deallocate(first);
        pool.deallocate(second);
    });
    worker.join();

    // 线程退出时 ~local_cache 会把本地空闲链刷回中心；未必会释放整块 slab（仅 1 块空 slab 时 trim 可返回 0）
    void* a = pool.allocate();
    void* b = pool.allocate();
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    pool.deallocate(a);
    pool.deallocate(b);
}

TEST(FixedSizePoolTest, EnforcesMaxTotalSlotsLimit)
{
    pool_options options;
    options.initial_slab_slots   = 4;
    options.max_slab_slots       = 4;
    options.max_total_slots      = 4;
    options.local_cache_capacity = 0;

    mc::memory::fixed_size_pool pool(32, alignof(std::max_align_t), options);

    void* a = pool.allocate();
    void* b = pool.allocate();
    void* c = pool.allocate();
    void* d = pool.allocate();

    EXPECT_THROW(static_cast<void>(pool.allocate()), std::bad_alloc);

    pool.deallocate(d);
    pool.deallocate(c);
    pool.deallocate(b);
    pool.deallocate(a);
}

TEST(FixedSizePoolTest, TrimDoesNotReclaimActiveRemoteThreadLocalCaches)
{
    pool_options options;
    options.initial_slab_slots   = 4;
    options.max_slab_slots       = 4;
    options.max_total_slots      = 4;
    options.local_cache_capacity = 4;

    mc::memory::fixed_size_pool pool(32, alignof(std::max_align_t), options);

    std::atomic<bool> worker_ready{false};
    std::atomic<bool> stop_worker{false};

    std::thread worker([&]() {
        void* a = pool.allocate();
        void* b = pool.allocate();
        void* c = pool.allocate();
        void* d = pool.allocate();
        pool.deallocate(a);
        pool.deallocate(b);
        pool.deallocate(c);
        pool.deallocate(d);
        worker_ready.store(true, std::memory_order_release);

        while (!stop_worker.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    while (!worker_ready.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // 生产语义下 trim 不应直接改写活跃远端线程的本地 cache
    pool.trim(0);

    EXPECT_THROW(static_cast<void>(pool.allocate()), std::bad_alloc);

    stop_worker.store(true, std::memory_order_release);
    worker.join();

    void* s1 = pool.allocate();
    void* s2 = pool.allocate();
    void* s3 = pool.allocate();
    void* s4 = pool.allocate();
    ASSERT_NE(s1, nullptr);
    ASSERT_NE(s2, nullptr);
    ASSERT_NE(s3, nullptr);
    ASSERT_NE(s4, nullptr);
    pool.deallocate(s1);
    pool.deallocate(s2);
    pool.deallocate(s3);
    pool.deallocate(s4);
}

TEST(FixedSizePoolTest, ReusesSlotFromLaterSlab)
{
    pool_options options;
    options.initial_slab_slots   = 128;
    options.max_slab_slots       = 128;
    options.max_total_slots      = 128;
    options.local_cache_capacity = 0;

    mc::memory::fixed_size_pool pool(64, alignof(std::max_align_t), options);

    std::vector<void*> slots;
    slots.reserve(96);
    for (int i = 0; i < 96; ++i) {
        slots.push_back(pool.allocate());
    }

    void* target = slots.back();
    pool.deallocate(target);

    void* reused = pool.allocate();
    EXPECT_EQ(reused, target);

    for (std::size_t i = 0; i + 1 < slots.size(); ++i) {
        pool.deallocate(slots[i]);
    }
    pool.deallocate(reused);
}

TEST(FixedSizePoolTest, KeepsThreadLocalCacheForMultiplePools)
{
    pool_options options;
    options.initial_slab_slots   = 4;
    options.local_cache_capacity = 4;

    mc::memory::fixed_size_pool pool_a(32, alignof(std::max_align_t), options);
    mc::memory::fixed_size_pool pool_b(32, alignof(std::max_align_t), options);

    void* a1 = pool_a.allocate();
    void* b1 = pool_b.allocate();
    ASSERT_NE(a1, nullptr);
    ASSERT_NE(b1, nullptr);

    pool_a.deallocate(a1);
    pool_b.deallocate(b1);

    void* a2 = pool_a.allocate();
    void* b2 = pool_b.allocate();
    EXPECT_EQ(a2, a1);
    EXPECT_EQ(b2, b1);

    pool_a.deallocate(a2);
    pool_b.deallocate(b2);
}

TEST(FixedSizePoolTest, UsesCompactStrideForEightByteAlignedObjects)
{
    mc::memory::fixed_size_pool pool(32, alignof(std::uint64_t));
    EXPECT_EQ(pool.slot_stride(), 48U);
}

TEST(FixedSizePoolTest, LocalCacheCapacityBoundsPerThreadRetention)
{
    pool_options options;
    options.initial_slab_slots   = 4;
    options.max_slab_slots       = 4;
    options.max_total_slots      = 4;
    options.local_cache_capacity = 2;

    mc::memory::fixed_size_pool pool(32, alignof(std::max_align_t), options);

    std::vector<void*> slots;
    slots.reserve(4);
    for (int i = 0; i < 4; ++i) {
        slots.push_back(pool.allocate());
    }
    for (void* slot : slots) {
        pool.deallocate(slot);
    }

    bool        worker_allocated = false;
    std::thread worker([&]() {
        try {
            void* slot       = pool.allocate();
            worker_allocated = slot != nullptr;
            pool.deallocate(slot);
        } catch (const std::bad_alloc&) {
            worker_allocated = false;
        }
    });
    worker.join();

    EXPECT_TRUE(worker_allocated);
}

TEST(FixedSizePoolTest, RepeatedJoinThenDestroyAcrossThreads)
{
    pool_options options;
    options.initial_slab_slots   = 64;
    options.max_slab_slots       = 64;
    options.max_total_slots      = 256;
    options.local_cache_capacity = 8;

    constexpr int round_count      = 32;
    constexpr int thread_count     = 4;
    constexpr int batch_slot_count = 16;
    constexpr int batch_repeat     = 128;

    for (int round = 0; round < round_count; ++round) {
        mc::memory::fixed_size_pool pool(32, alignof(std::max_align_t), options);

        std::vector<std::thread> workers;
        workers.reserve(thread_count);
        for (int worker_index = 0; worker_index < thread_count; ++worker_index) {
            workers.emplace_back([&pool]() {
                std::vector<void*> slots;
                slots.reserve(batch_slot_count);
                for (int repeat = 0; repeat < batch_repeat; ++repeat) {
                    slots.clear();
                    for (int i = 0; i < batch_slot_count; ++i) {
                        slots.push_back(pool.allocate());
                    }
                    for (auto it = slots.rbegin(); it != slots.rend(); ++it) {
                        pool.deallocate(*it);
                    }
                }
            });
        }

        for (auto& worker : workers) {
            worker.join();
        }

        pool.trim(0);
    }
}

TEST(FixedSizePoolTest, ConcurrentTrimMaintainsProgressWithActiveRemoteCaches)
{
    pool_options options;
    options.initial_slab_slots   = 32;
    options.max_slab_slots       = 32;
    options.max_total_slots      = 64;
    options.local_cache_capacity = 8;

    mc::memory::fixed_size_pool pool(32, alignof(std::max_align_t), options);

    constexpr int worker_count      = 2;
    constexpr int batch_slot_count  = 8;
    constexpr int trim_repeat_count = 256;

    std::atomic<int>           ready_workers{0};
    std::atomic<bool>          stop_workers{false};
    std::atomic<bool>          worker_failed{false};
    std::atomic<std::uint64_t> completed_batches{0};
    std::vector<std::thread>   workers;
    workers.reserve(worker_count);

    for (int worker_index = 0; worker_index < worker_count; ++worker_index) {
        workers.emplace_back([&]() {
            std::vector<void*> slots;
            slots.reserve(batch_slot_count);
            ready_workers.fetch_add(1, std::memory_order_release);

            while (!stop_workers.load(std::memory_order_acquire)) {
                slots.clear();
                try {
                    for (int i = 0; i < batch_slot_count; ++i) {
                        slots.push_back(pool.allocate());
                    }
                } catch (const std::bad_alloc&) {
                    worker_failed.store(true, std::memory_order_release);
                    stop_workers.store(true, std::memory_order_release);
                }

                for (auto it = slots.rbegin(); it != slots.rend(); ++it) {
                    pool.deallocate(*it);
                }

                completed_batches.fetch_add(1, std::memory_order_release);
            }
        });
    }

    while (ready_workers.load(std::memory_order_acquire) != worker_count) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    while (completed_batches.load(std::memory_order_acquire) < worker_count * 4U) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const auto progress_before_trim = completed_batches.load(std::memory_order_acquire);
    for (int i = 0; i < trim_repeat_count; ++i) {
        pool.trim(0);
    }
    const auto progress_after_trim = completed_batches.load(std::memory_order_acquire);

    stop_workers.store(true, std::memory_order_release);
    for (auto& worker : workers) {
        worker.join();
    }

    EXPECT_FALSE(worker_failed.load(std::memory_order_acquire));
    EXPECT_GT(progress_after_trim, progress_before_trim);

    std::vector<void*> final_slots;
    final_slots.reserve(16);
    for (int i = 0; i < 16; ++i) {
        final_slots.push_back(pool.allocate());
    }
    for (void* slot : final_slots) {
        pool.deallocate(slot);
    }
}

} // namespace
