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
#include <thread>
#include <vector>

#include <mc/metric/registry.h>

namespace {

using mc::metric::descriptor;
using mc::metric::detail::make_counter_descriptor;
using mc::metric::detail::make_histogram_descriptor;
using mc::metric::label_set;
using mc::metric::registry;
using mc::metric::registry_options;

TEST(registry_concurrent_test, multi_thread_counter_first_create_race_converges)
{
    registry_options opts;
    opts.capacity         = 1024;
    opts.arena_bytes      = 64 * 1024;
    opts.hist_arena_bytes = 4096;
    auto r = registry::open_heap(opts);
    ASSERT_TRUE(r);

    constexpr int        thread_count = 16;
    constexpr int        per_thread   = 50000;
    std::atomic<int>     ready{0};
    std::atomic<bool>    go{false};
    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back([&]() {
            ready.fetch_add(1);
            while (!go.load()) {}
            const auto d = make_counter_descriptor("mc.test.concurrent.shared", "", "",
                                                   label_set{});
            auto       c = r->counter_of(d);
            for (int i = 0; i < per_thread; ++i) c.add();
        });
    }
    while (ready.load() < thread_count) {}
    go.store(true);
    for (auto& th : threads) th.join();

    const auto d = make_counter_descriptor("mc.test.concurrent.shared", "", "", label_set{});
    auto       c = r->counter_of(d);
    EXPECT_EQ(c.value(), thread_count * per_thread);
    EXPECT_EQ(r->registered_count(), 1u);
}

TEST(registry_concurrent_test, multi_thread_distinct_labels_get_distinct_slots)
{
    registry_options opts;
    opts.capacity         = 4096;
    opts.arena_bytes      = 256 * 1024;
    opts.hist_arena_bytes = 4096;
    auto r = registry::open_heap(opts);
    ASSERT_TRUE(r);

    constexpr int        thread_count = 8;
    constexpr int        per_thread   = 200;
    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < per_thread; ++i) {
                std::string lbl_value = std::to_string(t) + "_" + std::to_string(i);
                const auto  d         = make_counter_descriptor(
                    "mc.test.concurrent.distinct", "", "",
                    label_set{{"id", mc::string_view(lbl_value.data(), lbl_value.size())}});
                auto c = r->counter_of(d);
                c.add(1);
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(r->registered_count(), static_cast<std::size_t>(thread_count * per_thread));
    auto snap = r->collect();
    std::uint64_t total = 0;
    for (const auto& s : snap) total += s.i_value;
    EXPECT_EQ(total, thread_count * per_thread);
}

TEST(registry_concurrent_test, histogram_concurrent_observe_count_matches)
{
    registry_options opts;
    opts.capacity         = 256;
    opts.arena_bytes      = 16 * 1024;
    opts.hist_arena_bytes = 16 * 1024;
    auto r = registry::open_heap(opts);
    ASSERT_TRUE(r);

    static const double bounds[] = {0.1, 1.0, 10.0};
    const auto          d        = make_histogram_descriptor("mc.test.concurrent.hist", "", "",
                                                             label_set{}, bounds, 3);

    constexpr int        thread_count = 8;
    constexpr int        per_thread   = 20000;
    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back([&]() {
            auto h = r->histogram_of(d);
            for (int i = 0; i < per_thread; ++i) {
                h.observe(0.5);
            }
        });
    }
    for (auto& th : threads) th.join();

    auto h = r->histogram_of(d);
    EXPECT_EQ(h.total_count(), static_cast<std::uint64_t>(thread_count * per_thread));
    EXPECT_DOUBLE_EQ(h.sum(), thread_count * per_thread * 0.5);
    EXPECT_EQ(h.bucket_count_at(1), static_cast<std::uint64_t>(thread_count * per_thread));
}

} // namespace
