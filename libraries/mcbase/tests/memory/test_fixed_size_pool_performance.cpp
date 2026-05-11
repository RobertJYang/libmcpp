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

#include <algorithm>

#include <boost/pool/object_pool.hpp>
#include <boost/pool/pool.hpp>

#include <mc/memory/fixed_size_pool.h>
#include <mc/memory/object_pool.h>

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

namespace {

// 默认约 5e4 次/场景；精细对比可设 MC_FIXED_SIZE_POOL_BENCH_ITERS。
// boost::object_pool 同步路径极慢，默认对其单独封顶，避免拖垮全量测试；全量对比设
// MC_FIXED_SIZE_POOL_BENCH_BOOST_FULL=1。
std::size_t bench_iteration_count()
{
    const char* env = std::getenv("MC_FIXED_SIZE_POOL_BENCH_ITERS");
    if (env != nullptr && env[0] != '\0') {
        const auto parsed = std::strtoull(env, nullptr, 10);
        return parsed < 1000 ? 1000 : static_cast<std::size_t>(parsed);
    }
    return 50000;
}

std::size_t boost_object_pool_ops(std::size_t count)
{
    const char* full = std::getenv("MC_FIXED_SIZE_POOL_BENCH_BOOST_FULL");
    if (full != nullptr && full[0] != '\0' && std::strcmp(full, "0") != 0) {
        return count;
    }
    return std::min(count, std::size_t{8192});
}

struct benchmark_node {
    std::uint64_t a{0};
    std::uint64_t b{0};
    std::uint64_t c{0};
    std::uint64_t d{0};
};

template <typename Fn>
long long measure_us(Fn&& fn)
{
    const auto start = std::chrono::steady_clock::now();
    fn();
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}

TEST(FixedSizePoolPerformanceTest, DISABLED_single_thread_allocate_deallocate)
{
    const std::size_t count     = bench_iteration_count();
    const std::size_t boost_ops = boost_object_pool_ops(count);

    mc::memory::fixed_size_pool::options pooled_opts;
    pooled_opts.initial_slab_slots   = 256;
    pooled_opts.max_slab_slots       = 4096;
    pooled_opts.max_total_slots      = 0;
    pooled_opts.local_cache_capacity = 256;

    mc::memory::fixed_size_pool          pooled(sizeof(benchmark_node), alignof(benchmark_node), pooled_opts);
    mc::memory::fixed_size_pool::options central_only_opts = pooled_opts;
    central_only_opts.local_cache_capacity                 = 0;
    mc::memory::fixed_size_pool central_only(sizeof(benchmark_node), alignof(benchmark_node), central_only_opts);

    auto pooled_us = measure_us([&]() {
        std::vector<void*> slots;
        slots.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            slots.push_back(pooled.allocate());
        }
        for (void* slot : slots) {
            pooled.deallocate(slot);
        }
    });

    auto central_only_us = measure_us([&]() {
        std::vector<void*> slots;
        slots.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            slots.push_back(central_only.allocate());
        }
        for (void* slot : slots) {
            central_only.deallocate(slot);
        }
    });

    auto boost_malloc_free_us = measure_us([&]() {
        boost::object_pool<benchmark_node> pool;
        std::vector<benchmark_node*>       slots;
        slots.reserve(boost_ops);
        for (std::size_t i = 0; i < boost_ops; ++i) {
            slots.push_back(pool.malloc());
        }
        for (auto* slot : slots) {
            pool.free(slot);
        }
    });

    auto boost_construct_destroy_us = measure_us([&]() {
        boost::object_pool<benchmark_node> pool;
        std::vector<benchmark_node*>       slots;
        slots.reserve(boost_ops);
        for (std::size_t i = 0; i < boost_ops; ++i) {
            slots.push_back(pool.construct());
        }
        for (auto* slot : slots) {
            pool.destroy(slot);
        }
    });

    auto boost_pool_malloc_free_us = measure_us([&]() {
        boost::pool<>      pool(sizeof(benchmark_node), 256, 4096);
        std::vector<void*> slots;
        slots.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            slots.push_back(pool.malloc());
        }
        for (void* slot : slots) {
            pool.free(slot);
        }
    });

    auto new_delete_us = measure_us([&]() {
        std::vector<benchmark_node*> slots;
        slots.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            slots.push_back(new benchmark_node{});
        }
        for (auto* slot : slots) {
            delete slot;
        }
    });

    std::cout << "single-thread allocate/deallocate (mc/boost::pool/new: " << count
              << " ops; boost::object_pool: " << boost_ops << " ops)" << std::endl;
    std::cout << "fixed_size_pool tls: " << pooled_us << " us" << std::endl;
    std::cout << "fixed_size_pool central-only: " << central_only_us << " us" << std::endl;
    std::cout << "boost::pool malloc/free: " << boost_pool_malloc_free_us << " us" << std::endl;
    std::cout << "boost::object_pool malloc/free: " << boost_malloc_free_us << " us" << std::endl;
    std::cout << "boost::object_pool construct/destroy: " << boost_construct_destroy_us << " us" << std::endl;
    std::cout << "new/delete: " << new_delete_us << " us" << std::endl;
}

TEST(FixedSizePoolPerformanceTest, DISABLED_object_pool_versus_raw_fixed_size_pool)
{
    const std::size_t count     = bench_iteration_count();
    const std::size_t boost_ops = boost_object_pool_ops(count);

    mc::memory::fixed_size_pool::options opts;
    opts.initial_slab_slots   = 256;
    opts.max_slab_slots       = 4096;
    opts.max_total_slots      = 0;
    opts.local_cache_capacity = 256;

    mc::memory::fixed_size_pool             raw_pool(sizeof(benchmark_node), alignof(benchmark_node), opts);
    mc::memory::object_pool<benchmark_node> typed_pool(opts);

    auto raw_us = measure_us([&]() {
        std::vector<void*> slots;
        slots.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            void* mem = raw_pool.allocate();
            slots.push_back(new (mem) benchmark_node{});
        }
        for (void* slot : slots) {
            static_cast<benchmark_node*>(slot)->~benchmark_node();
            raw_pool.deallocate(slot);
        }
    });

    auto typed_us = measure_us([&]() {
        std::vector<benchmark_node*> slots;
        slots.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            slots.push_back(typed_pool.create());
        }
        for (auto* slot : slots) {
            typed_pool.destroy(slot);
        }
    });

    auto boost_typed_us = measure_us([&]() {
        boost::object_pool<benchmark_node> pool;
        std::vector<benchmark_node*>       slots;
        slots.reserve(boost_ops);
        for (std::size_t i = 0; i < boost_ops; ++i) {
            slots.push_back(pool.construct());
        }
        for (auto* slot : slots) {
            pool.destroy(slot);
        }
    });

    std::cout << "object_pool vs raw fixed_size_pool (mc: " << count << " ops; boost::object_pool: " << boost_ops
              << " ops)" << std::endl;
    std::cout << "raw fixed_size_pool: " << raw_us << " us" << std::endl;
    std::cout << "object_pool: " << typed_us << " us" << std::endl;
    std::cout << "boost::object_pool: " << boost_typed_us << " us" << std::endl;
}

TEST(FixedSizePoolPerformanceTest, DISABLED_remote_reclaim_contention)
{
    const std::size_t per_thread_ops = std::max<std::size_t>(10000, bench_iteration_count() / 2);

    mc::memory::fixed_size_pool::options opts;
    opts.initial_slab_slots   = 256;
    opts.max_slab_slots       = 4096;
    opts.max_total_slots      = 0;
    opts.local_cache_capacity = 256;

    mc::memory::fixed_size_pool pool(sizeof(benchmark_node), alignof(benchmark_node), opts);

    auto worker_fn = [&pool, per_thread_ops]() {
        std::vector<void*> slots;
        slots.reserve(per_thread_ops);
        for (std::size_t i = 0; i < per_thread_ops; ++i) {
            slots.push_back(pool.allocate());
        }
        for (void* slot : slots) {
            pool.deallocate(slot);
        }
    };

    const auto workload_us = measure_us([&]() {
        std::thread t1(worker_fn);
        std::thread t2(worker_fn);
        t1.join();
        t2.join();
    });

    const auto trim_us = measure_us([&]() {
        pool.trim(0);
    });

    std::cout << "remote reclaim contention (" << per_thread_ops * 2 << " ops)" << std::endl;
    std::cout << "workload: " << workload_us << " us" << std::endl;
    std::cout << "trim: " << trim_us << " us" << std::endl;
}

TEST(FixedSizePoolPerformanceTest, DISABLED_trim_maintenance_observation)
{
    struct observation_case {
        const char* label;
        std::size_t thread_count;
        std::size_t per_thread_ops;
        std::size_t local_cache_capacity;
    };

    const std::size_t      base_ops = std::max<std::size_t>(2048, bench_iteration_count() / 8);
    const observation_case cases[]  = {
        {"single-thread small", 1, base_ops, 32},
        {"four-thread medium", 4, base_ops, 32},
        {"four-thread larger-cache", 4, base_ops, 128},
    };

    for (const auto& observation : cases) {
        mc::memory::fixed_size_pool::options opts;
        opts.initial_slab_slots   = 256;
        opts.max_slab_slots       = 4096;
        opts.max_total_slots      = 0;
        opts.local_cache_capacity = observation.local_cache_capacity;

        mc::memory::fixed_size_pool pool(sizeof(benchmark_node), alignof(benchmark_node), opts);

        const auto workload_us = measure_us([&]() {
            std::vector<std::thread> workers;
            workers.reserve(observation.thread_count);
            for (std::size_t worker_index = 0; worker_index < observation.thread_count; ++worker_index) {
                workers.emplace_back([&pool, &observation]() {
                    std::vector<void*> slots;
                    slots.reserve(observation.per_thread_ops);
                    for (std::size_t i = 0; i < observation.per_thread_ops; ++i) {
                        slots.push_back(pool.allocate());
                    }
                    for (void* slot : slots) {
                        pool.deallocate(slot);
                    }
                });
            }
            for (auto& worker : workers) {
                worker.join();
            }
        });

        std::size_t released_slots = 0;
        const auto  trim_us        = measure_us([&]() {
            released_slots = pool.trim(0);
        });

        std::cout << "trim observation: " << observation.label << std::endl;
        std::cout << "threads: " << observation.thread_count << ", per-thread ops: " << observation.per_thread_ops
                  << ", local_cache_capacity: " << observation.local_cache_capacity << std::endl;
        std::cout << "workload: " << workload_us << " us" << std::endl;
        std::cout << "trim: " << trim_us << " us" << std::endl;
        std::cout << "released slots: " << released_slots << std::endl;
    }
}

} // namespace
