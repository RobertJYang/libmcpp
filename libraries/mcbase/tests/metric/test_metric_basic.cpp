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
#include <cmath>
#include <cstdint>

#include <mc/metric.h>
#include <mc/metric/registry.h>

namespace {

using mc::metric::descriptor;
using mc::metric::detail::make_counter_descriptor;
using mc::metric::detail::make_gauge_descriptor;
using mc::metric::detail::make_histogram_descriptor;
using mc::metric::detail::make_up_down_counter_descriptor;
using mc::metric::label_set;
using mc::metric::registry;
using mc::metric::registry_options;

class registry_basic_test : public ::testing::Test {
protected:
    void SetUp() override
    {
        registry_options opts;
        opts.capacity         = 256;
        opts.arena_bytes      = 16 * 1024;
        opts.hist_arena_bytes = 8 * 1024;
        opts.name             = "test_basic";
        m_registry            = registry::open_heap(opts);
        ASSERT_TRUE(m_registry);
    }

    std::unique_ptr<registry> m_registry;
};

TEST_F(registry_basic_test, counter_basic_add_and_collect)
{
    const auto desc = make_counter_descriptor("mc.test.counter_a", "1", "test counter a",
                                              label_set{});
    auto       c    = m_registry->counter_of(desc);
    ASSERT_TRUE(c.is_valid());
    c.add();
    c.add(2);
    c.add(7);
    EXPECT_EQ(c.value(), 10);

    auto snap = m_registry->collect();
    ASSERT_EQ(snap.size(), 1u);
    EXPECT_EQ(snap[0].name, "mc.test.counter_a");
    EXPECT_EQ(snap[0].unit, "1");
    EXPECT_EQ(snap[0].help, "test counter a");
    EXPECT_EQ(snap[0].i_value, 10);
    EXPECT_EQ(snap[0].k, mc::metric::kind::counter);
}

TEST_F(registry_basic_test, counter_idempotent_lookup_returns_same_slot)
{
    const auto d = make_counter_descriptor("mc.test.same", "", "", label_set{});
    auto a = m_registry->counter_of(d);
    auto b = m_registry->counter_of(d);
    ASSERT_TRUE(a.is_valid());
    ASSERT_TRUE(b.is_valid());
    a.add(3);
    b.add(4);
    EXPECT_EQ(a.value(), 7);
    EXPECT_EQ(b.value(), 7);
    EXPECT_EQ(m_registry->registered_count(), 1u);
}

TEST_F(registry_basic_test, labels_make_distinct_slots_but_label_order_does_not)
{
    const auto d1 = make_counter_descriptor("mc.test.lbl", "", "",
                                            label_set{{"interface", "X"}, {"method", "do"}});
    const auto d2 = make_counter_descriptor("mc.test.lbl", "", "",
                                            label_set{{"method", "do"}, {"interface", "X"}});
    const auto d3 = make_counter_descriptor("mc.test.lbl", "", "",
                                            label_set{{"interface", "Y"}, {"method", "do"}});

    auto c1 = m_registry->counter_of(d1);
    auto c2 = m_registry->counter_of(d2);
    auto c3 = m_registry->counter_of(d3);
    c1.add(1);
    c2.add(2);
    c3.add(10);

    EXPECT_EQ(c1.value(), 3);   // d1 == d2
    EXPECT_EQ(c2.value(), 3);
    EXPECT_EQ(c3.value(), 10);
    EXPECT_EQ(m_registry->registered_count(), 2u);
}

TEST_F(registry_basic_test, up_down_counter_can_decrement)
{
    const auto d = make_up_down_counter_descriptor("mc.test.queue_depth", "", "", label_set{});
    auto       c = m_registry->up_down_counter_of(d);
    c.add_signed(5);
    c.add_signed(-2);
    EXPECT_EQ(c.value(), 3);
}

TEST_F(registry_basic_test, gauge_set_and_read)
{
    const auto d = make_gauge_descriptor("mc.test.cpu_pct", "", "", label_set{});
    auto       g = m_registry->gauge_of(d);
    g.set(42);
    EXPECT_EQ(g.value(), 42);
    g.set(7);
    EXPECT_EQ(g.value(), 7);
}

TEST_F(registry_basic_test, gauge_double_roundtrip)
{
    const auto d = make_gauge_descriptor("mc.test.temp_c", "", "", label_set{});
    auto       g = m_registry->gauge_of(d);
    g.set_double(36.5);
    EXPECT_DOUBLE_EQ(g.value_double(), 36.5);
}

TEST_F(registry_basic_test, histogram_observe_distributes_into_buckets)
{
    static const double bounds[] = {0.01, 0.1, 1.0, 10.0};
    const auto          d        = make_histogram_descriptor("mc.test.lat_s", "s", "",
                                                             label_set{}, bounds, 4);
    auto                h        = m_registry->histogram_of(d);
    ASSERT_TRUE(h.is_valid());
    h.observe(0.005);
    h.observe(0.05);
    h.observe(0.5);
    h.observe(5.0);
    h.observe(50.0);
    EXPECT_EQ(h.total_count(), 5u);
    EXPECT_DOUBLE_EQ(h.sum(), 0.005 + 0.05 + 0.5 + 5.0 + 50.0);
    EXPECT_EQ(h.bucket_count_at(0), 1u);
    EXPECT_EQ(h.bucket_count_at(1), 1u);
    EXPECT_EQ(h.bucket_count_at(2), 1u);
    EXPECT_EQ(h.bucket_count_at(3), 1u);
    EXPECT_EQ(h.bucket_count_at(4), 1u); // +inf

    auto snap = m_registry->collect();
    ASSERT_EQ(snap.size(), 1u);
    ASSERT_EQ(snap[0].hist_buckets.size(), 5u);
    EXPECT_DOUBLE_EQ(snap[0].hist_buckets[0].upper_bound, 0.01);
    EXPECT_EQ(snap[0].hist_buckets[0].cumulative_count, 1u);
    EXPECT_EQ(snap[0].hist_buckets[1].cumulative_count, 2u);
    EXPECT_EQ(snap[0].hist_buckets[2].cumulative_count, 3u);
    EXPECT_EQ(snap[0].hist_buckets[3].cumulative_count, 4u);
    EXPECT_TRUE(std::isinf(snap[0].hist_buckets[4].upper_bound));
    EXPECT_EQ(snap[0].hist_buckets[4].cumulative_count, 5u);
}

TEST_F(registry_basic_test, capacity_overflow_returns_invalid_handles)
{
    registry_options small_opts;
    small_opts.capacity         = 4;       // 仅 4 槽
    small_opts.arena_bytes      = 16 * 1024;
    small_opts.hist_arena_bytes = 4096;
    auto small = registry::open_heap(small_opts);
    ASSERT_TRUE(small);

    int valid = 0;
    int invalid = 0;
    for (int i = 0; i < 64; ++i) {
        std::string name = "mc.test.overflow." + std::to_string(i);
        auto        d    = make_counter_descriptor(mc::string_view(name.data(), name.size()), "",
                                                   "", label_set{});
        auto        c    = small->counter_of(d);
        if (c.is_valid()) ++valid;
        else ++invalid;
    }
    EXPECT_LE(valid, 4);     // 不会超过容量
    EXPECT_GT(invalid, 0);   // 至少出现过 overflow
    EXPECT_GT(small->overflow_count(), 0u);
}

TEST_F(registry_basic_test, descriptor_arena_overflow_does_not_crash)
{
    registry_options opts;
    opts.capacity         = 256;
    opts.arena_bytes      = 4096; // 极小 arena
    opts.hist_arena_bytes = 4096;
    auto             r = registry::open_heap(opts);
    ASSERT_TRUE(r);

    // 一直创建带不同长 label 的 metric，arena 会很快撑爆
    int created_with_desc = 0;
    for (int i = 0; i < 256; ++i) {
        std::string name = "mc.test.arena.metric_with_long_name_" + std::to_string(i);
        std::string lbl  = "long_label_value_" + std::to_string(i);
        auto        d    = make_counter_descriptor(mc::string_view(name.data(), name.size()), "",
                                                   "",
                                                   label_set{{"key", mc::string_view(lbl.data(),
                                                                                     lbl.size())}});
        auto        c    = r->counter_of(d);
        if (c.is_valid()) {
            c.add(1);
        }
        if (i < 8) ++created_with_desc;
    }
    auto snap = r->collect();
    EXPECT_GT(snap.size(), 0u);
    EXPECT_GT(r->overflow_count(), 0u);
}

TEST(default_registry_test, default_registry_is_lazy_heap)
{
    mc::metric::reset_default_registry_for_test();
    auto& r = mc::metric::default_registry();
    auto  d = make_counter_descriptor("mc.test.default", "", "", label_set{});
    auto  c = r.counter_of(d);
    c.add(5);
    EXPECT_EQ(c.value(), 5);
    mc::metric::reset_default_registry_for_test();
}

} // namespace
