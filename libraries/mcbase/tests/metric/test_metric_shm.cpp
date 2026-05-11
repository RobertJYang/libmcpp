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

#include <chrono>
#include <string>
#include <string_view>

#include <unistd.h>

#include <mc/metric/registry.h>
#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/shm_runtime.h>

namespace {

using mc::metric::descriptor;
using mc::metric::detail::make_counter_descriptor;
using mc::metric::detail::make_gauge_descriptor;
using mc::metric::label_set;
using mc::metric::registry;
using mc::metric::registry_options;

std::string make_unique_region_name(std::string_view prefix)
{
    const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::string(prefix) + "_" + std::to_string(getpid()) + "_" +
           std::to_string(now_ns);
}

class metric_shm_test : public ::testing::Test {
protected:
    void SetUp() override { m_region_name = make_unique_region_name("mc_metric_shm"); }

    void TearDown() override
    {
        mc::shm::detail::shared_memory_backend::remove(m_region_name);
    }

    mc::shm::runtime_options runtime_opts() const
    {
        mc::shm::runtime_options o;
        o.region_name = m_region_name;
        o.region_size = 4 * 1024 * 1024;
        return o;
    }

    registry_options registry_opts() const
    {
        registry_options o;
        o.capacity         = 256;
        o.arena_bytes      = 32 * 1024;
        o.hist_arena_bytes = 8 * 1024;
        o.name             = "test_shm";
        return o;
    }

    std::string m_region_name;
};

TEST_F(metric_shm_test, single_writer_single_reader_visible)
{
    mc::shm::shm_runtime rt(runtime_opts());
    ASSERT_TRUE(rt.is_valid());

    auto reg = registry::open_shm(rt, registry_opts());
    ASSERT_TRUE(reg);

    const auto d = make_counter_descriptor("mc.test.shm.counter", "", "", label_set{});
    auto       c = reg->counter_of(d);
    c.add(123);

    // 在同进程独立打开 second registry，必须看到同一份 metric
    auto reader = registry::open_shm(rt, registry_opts());
    ASSERT_TRUE(reader);
    auto snap = reader->collect();
    ASSERT_EQ(snap.size(), 1u);
    EXPECT_EQ(snap[0].name, "mc.test.shm.counter");
    EXPECT_EQ(snap[0].i_value, 123);
}

TEST_F(metric_shm_test, reopen_runtime_sees_existing_metrics)
{
    {
        mc::shm::shm_runtime rt(runtime_opts());
        ASSERT_TRUE(rt.is_valid());
        auto reg = registry::open_shm(rt, registry_opts());
        ASSERT_TRUE(reg);
        const auto d = make_gauge_descriptor("mc.test.shm.gauge", "", "", label_set{});
        auto       g = reg->gauge_of(d);
        g.set(77);
    }
    // 重新打开 runtime —— shm 仍存活
    {
        mc::shm::shm_runtime rt(runtime_opts());
        ASSERT_TRUE(rt.is_valid());
        auto reg = registry::open_shm(rt, registry_opts());
        ASSERT_TRUE(reg);
        auto snap = reg->collect();
        ASSERT_EQ(snap.size(), 1u);
        EXPECT_EQ(snap[0].name, "mc.test.shm.gauge");
        EXPECT_EQ(snap[0].i_value, 77);
    }
}

TEST_F(metric_shm_test, prune_stale_writers_marks_dead_owner_and_revives_on_restart)
{
    mc::shm::shm_runtime rt(runtime_opts());
    ASSERT_TRUE(rt.is_valid());

    // 写入者 1 注册 endpoint，建一个 counter 累加 5
    const auto ep1 = rt.register_endpoint("metric_writer", 16);
    ASSERT_TRUE(ep1);
    ASSERT_NE(ep1->endpoint_id, 0);
    ASSERT_TRUE(rt.mark_endpoint_running(*ep1));

    auto opts1            = registry_opts();
    opts1.owner_endpoint  = ep1->endpoint_id;
    opts1.writer_instance = ep1->instance_id;
    auto reg1             = registry::open_shm(rt, opts1);
    ASSERT_TRUE(reg1);

    const auto d  = make_counter_descriptor("mc.test.shm.bound", "", "", label_set{});
    auto       c1 = reg1->counter_of(d);
    c1.add(5);

    // 模拟进程死亡：abort_endpoint 把状态切到 aborting
    ASSERT_TRUE(rt.abort_endpoint(*ep1));

    // 监控侧：扫描 stale，应当标记 1 个
    EXPECT_EQ(reg1->prune_stale_writers(), 1u);
    {
        auto snap = reg1->collect();
        ASSERT_EQ(snap.size(), 1u);
        EXPECT_TRUE(snap[0].stale);
        EXPECT_EQ(snap[0].i_value, 5);
    }

    // 同 endpoint 名字重新注册：得到新 instance_id
    const auto ep2 = rt.register_endpoint("metric_writer", 16);
    ASSERT_TRUE(ep2);
    EXPECT_EQ(ep2->endpoint_id, ep1->endpoint_id);
    EXPECT_NE(ep2->instance_id, ep1->instance_id);
    ASSERT_TRUE(rt.mark_endpoint_running(*ep2));

    auto opts2            = registry_opts();
    opts2.owner_endpoint  = ep2->endpoint_id;
    opts2.writer_instance = ep2->instance_id;
    auto reg2             = registry::open_shm(rt, opts2);
    ASSERT_TRUE(reg2);

    auto c2 = reg2->counter_of(d); // 命中已有槽 → 接管 + 清 stale
    c2.add(3);

    {
        auto snap = reg2->collect();
        ASSERT_EQ(snap.size(), 1u);
        EXPECT_FALSE(snap[0].stale);
        EXPECT_EQ(snap[0].i_value, 8); // 累计语义：5 + 3
        EXPECT_EQ(snap[0].owner_endpoint, ep2->endpoint_id);
    }

    // 此时 prune 不应再标 stale（owner 仍活）
    EXPECT_EQ(reg2->prune_stale_writers(), 0u);
}

TEST_F(metric_shm_test, prune_stale_writers_is_noop_for_unbound_metrics)
{
    mc::shm::shm_runtime rt(runtime_opts());
    ASSERT_TRUE(rt.is_valid());

    // owner_endpoint 默认为 0 = 不绑定
    auto reg = registry::open_shm(rt, registry_opts());
    ASSERT_TRUE(reg);
    auto c = reg->counter_of(make_counter_descriptor("mc.test.unbound", "", "", label_set{}));
    c.add(1);

    EXPECT_EQ(reg->prune_stale_writers(), 0u);
    auto snap = reg->collect();
    ASSERT_EQ(snap.size(), 1u);
    EXPECT_FALSE(snap[0].stale);
}

TEST_F(metric_shm_test, prune_stale_writers_returns_zero_for_heap_registry)
{
    auto reg = registry::open_heap(registry_opts());
    ASSERT_TRUE(reg);
    auto c = reg->counter_of(make_counter_descriptor("mc.test.heap", "", "", label_set{}));
    c.add(1);
    EXPECT_EQ(reg->prune_stale_writers(), 0u);
}

TEST_F(metric_shm_test, conflicting_signature_returns_null)
{
    mc::shm::shm_runtime rt(runtime_opts());
    ASSERT_TRUE(rt.is_valid());

    auto reg1 = registry::open_shm(rt, registry_opts());
    ASSERT_TRUE(reg1);

    // 同名 + 不同 capacity → signature 不一致 → 拒绝
    registry_options bad = registry_opts();
    bad.capacity         = 1024;
    auto reg2            = registry::open_shm(rt, bad);
    EXPECT_FALSE(reg2);
}

} // namespace
