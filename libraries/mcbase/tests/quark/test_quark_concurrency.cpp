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

#include <array>
#include <atomic>
#include <random>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include <mc/quark.h>

using namespace mc;

namespace {

constexpr std::size_t k_thread_count    = 8U;
constexpr std::size_t k_operations_each = 5000U;

const std::array<mc::quark, 16> g_pool = {
    "foo.concurrency.pool.00"_q, "foo.concurrency.pool.01"_q, "foo.concurrency.pool.02"_q,
    "foo.concurrency.pool.03"_q, "foo.concurrency.pool.04"_q, "foo.concurrency.pool.05"_q,
    "foo.concurrency.pool.06"_q, "foo.concurrency.pool.07"_q, "foo.concurrency.pool.08"_q,
    "foo.concurrency.pool.09"_q, "foo.concurrency.pool.10"_q, "foo.concurrency.pool.11"_q,
    "foo.concurrency.pool.12"_q, "foo.concurrency.pool.13"_q, "foo.concurrency.pool.14"_q,
    "foo.concurrency.pool.15"_q,
};

const std::array<mc::string_view, 16> g_pool_views = {
    mc::string_view("foo.concurrency.pool.00"), mc::string_view("foo.concurrency.pool.01"),
    mc::string_view("foo.concurrency.pool.02"), mc::string_view("foo.concurrency.pool.03"),
    mc::string_view("foo.concurrency.pool.04"), mc::string_view("foo.concurrency.pool.05"),
    mc::string_view("foo.concurrency.pool.06"), mc::string_view("foo.concurrency.pool.07"),
    mc::string_view("foo.concurrency.pool.08"), mc::string_view("foo.concurrency.pool.09"),
    mc::string_view("foo.concurrency.pool.10"), mc::string_view("foo.concurrency.pool.11"),
    mc::string_view("foo.concurrency.pool.12"), mc::string_view("foo.concurrency.pool.13"),
    mc::string_view("foo.concurrency.pool.14"), mc::string_view("foo.concurrency.pool.15"),
};

} // namespace

TEST(quark_concurrency, parallel_try_from_returns_stable_id)
{
    std::atomic<bool>        start{false};
    std::vector<std::thread> threads;
    threads.reserve(k_thread_count);

    std::vector<std::vector<mc::quark::id_type>> per_thread(k_thread_count);
    for (auto& v : per_thread) {
        v.reserve(k_operations_each);
    }

    for (std::size_t t = 0U; t < k_thread_count; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937                               rng(static_cast<std::uint32_t>(t * 7919U + 13U));
            std::uniform_int_distribution<std::size_t> dist(0U, g_pool_views.size() - 1U);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (std::size_t i = 0U; i < k_operations_each; ++i) {
                const auto idx = dist(rng);
                const auto q   = mc::quark::try_from(g_pool_views[idx]);
                per_thread[t].push_back(q.id());
            }
        });
    }

    start.store(true, std::memory_order_release);
    for (auto& th : threads) {
        th.join();
    }

    for (std::size_t t = 0U; t < k_thread_count; ++t) {
        for (std::size_t i = 0U; i < per_thread[t].size(); ++i) {
            EXPECT_NE(per_thread[t][i], mc::quark::invalid_id);
        }
    }

    std::unordered_set<mc::quark::id_type> unique_ids;
    for (const auto& q : g_pool) {
        EXPECT_TRUE(unique_ids.insert(q.id()).second) << "duplicate id for " << q.view();
    }
    EXPECT_EQ(unique_ids.size(), g_pool.size());
}

TEST(quark_concurrency, all_observed_ids_resolve_to_original_string)
{
    std::vector<std::thread> threads;
    threads.reserve(4U);
    std::atomic<bool> start{false};

    for (std::size_t t = 0U; t < 4U; ++t) {
        threads.emplace_back([&]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (std::size_t i = 0U; i < 1000U; ++i) {
                for (std::size_t k = 0U; k < g_pool.size(); ++k) {
                    auto q = mc::quark::try_from(g_pool_views[k]);
                    ASSERT_EQ(q.id(), g_pool[k].id());
                    ASSERT_EQ(std::string(q.view().data(), q.size()),
                              std::string(g_pool_views[k].data(), g_pool_views[k].size()));
                }
            }
        });
    }

    start.store(true, std::memory_order_release);
    for (auto& th : threads) {
        th.join();
    }
}
