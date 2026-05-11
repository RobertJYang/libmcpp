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

#include <thread>
#include <vector>

#include <mc/metric.h>

namespace {

void increment_call_site()
{
    MC_METRIC_COUNTER("mc.test.macro.requests").add(1);
}

void increment_with_labels(int code)
{
    if (code == 200) {
        MC_METRIC_COUNTER("mc.test.macro.responses", {"code", "200"}).add(1);
    } else {
        MC_METRIC_COUNTER("mc.test.macro.responses", {"code", "500"}).add(1);
    }
}

void observe_latency(double v)
{
    MC_METRIC_HISTOGRAM_BUCKETS(latency_buckets, 0.001, 0.01, 0.1, 1.0);
    MC_METRIC_HISTOGRAM("mc.test.macro.latency", latency_buckets).observe(v);
}

class metric_macros_test : public ::testing::Test {
protected:
    void SetUp() override { mc::metric::reset_default_registry_for_test(); }
    void TearDown() override { mc::metric::reset_default_registry_for_test(); }
};

TEST_F(metric_macros_test, counter_macro_records_to_default_registry)
{
    for (int i = 0; i < 1000; ++i) {
        increment_call_site();
    }
    auto snap = mc::metric::default_registry().collect();
    bool found = false;
    for (const auto& s : snap) {
        if (s.name == "mc.test.macro.requests") {
            EXPECT_EQ(s.i_value, 1000);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(metric_macros_test, counter_macro_with_labels_creates_distinct_metrics)
{
    for (int i = 0; i < 5; ++i) increment_with_labels(200);
    for (int i = 0; i < 3; ++i) increment_with_labels(500);

    auto snap = mc::metric::default_registry().collect();
    int  found_200 = 0;
    int  found_500 = 0;
    for (const auto& s : snap) {
        if (s.name == "mc.test.macro.responses") {
            ASSERT_EQ(s.labels.size(), 1u);
            if (s.labels[0].second == "200") {
                EXPECT_EQ(s.i_value, 5);
                ++found_200;
            } else if (s.labels[0].second == "500") {
                EXPECT_EQ(s.i_value, 3);
                ++found_500;
            }
        }
    }
    EXPECT_EQ(found_200, 1);
    EXPECT_EQ(found_500, 1);
}

TEST_F(metric_macros_test, histogram_macro_observes_into_buckets)
{
    observe_latency(0.0005);
    observe_latency(0.05);
    observe_latency(0.5);
    observe_latency(5.0);
    auto snap = mc::metric::default_registry().collect();
    bool found = false;
    for (const auto& s : snap) {
        if (s.name == "mc.test.macro.latency") {
            EXPECT_EQ(s.hist_total_count, 4u);
            ASSERT_EQ(s.hist_buckets.size(), 5u); // 4 bounds + +inf
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(metric_macros_test, macro_is_safe_under_concurrent_first_call)
{
    constexpr int            n_threads = 8;
    constexpr int            per_thr   = 10000;
    std::vector<std::thread> threads;
    threads.reserve(n_threads);
    for (int t = 0; t < n_threads; ++t) {
        threads.emplace_back([]() {
            for (int i = 0; i < per_thr; ++i) {
                MC_METRIC_COUNTER("mc.test.macro.race").add(1);
            }
        });
    }
    for (auto& th : threads) th.join();

    auto snap = mc::metric::default_registry().collect();
    bool found = false;
    for (const auto& s : snap) {
        if (s.name == "mc.test.macro.race") {
            EXPECT_EQ(s.i_value, n_threads * per_thr);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

} // namespace
