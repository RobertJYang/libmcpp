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

// shm_service ops 单元测试
//
// 覆盖：
//   - shm_service_create / destroy 基本回路
//   - shm_service_set_pid / set_state / increment_epoch + CRC 同步刷新
//   - shm_service_attach 三种路径：
//       * 首次创建（map 中无该 service_name）
//       * 已存在接管（map 中已有 → 更新 pid + epoch++）
//       * 同名多次 attach 的 epoch 累积语义

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdint>
#include <random>
#include <string_view>
#include <unistd.h>

#include "shm_service.h"
#include "shm_service_ops.h"
#include <mc/shm/allocator.h>
#include <mc/shm/container/map.h>
#include <mc/shm/region.h>

using mc::engine::shm_service;
using mc::engine::shm_service_abi_version;
using mc::engine::shm_service_attach;
using mc::engine::shm_service_check;
using mc::engine::shm_service_create;
using mc::engine::shm_service_destroy;
using mc::engine::shm_service_increment_epoch;
using mc::engine::shm_service_map;
using mc::engine::shm_service_name;
using mc::engine::shm_service_set_pid;
using mc::engine::shm_service_set_state;
using mc::engine::shm_service_state;
using mc::engine::shm_allocator;
using mc::shm::shm_region;
using mc::shm::shm_region_options;
using mc::shm::container::map_control;

namespace {

class shm_service_ops_fixture : public ::testing::Test {
protected:
    static constexpr std::size_t region_size = 4 * 1024 * 1024;

    void SetUp() override
    {
        std::random_device rd;
        std::mt19937       rng(rd());
        char               buf[128];
        std::snprintf(buf, sizeof(buf), "mc_shm_service_ops_test_%d_%u", ::getpid(), rng());

        shm_region_options opts;
        opts.segment_name  = mc::string(buf);
        opts.total_size    = region_size;
        opts.root_capacity = 16;

        m_region = shm_region(opts);
        ASSERT_TRUE(m_region.is_valid());
        m_alloc = m_region.user_arena();
        ASSERT_TRUE(m_alloc.is_valid());
    }

    // 在 SHM 中分配一个 map_control 并初始化为 shm_service_map 的形态。
    // 调用方负责在测试结束前销毁返回的 map_control（通过 destroy_map 辅助）。
    map_control* create_service_map_control()
    {
        auto* mem = m_alloc.allocate(sizeof(map_control), alignof(map_control));
        EXPECT_NE(mem, nullptr);
        auto*      ctrl = new (mem) map_control();
        const auto tag  = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(ctrl)
                                                    & 0xFFFFFFFFU);
        shm_service_map::init(*ctrl, tag);
        return ctrl;
    }

    void destroy_map_control(map_control* ctrl)
    {
        if (ctrl == nullptr) {
            return;
        }
        ctrl->~map_control();
        m_alloc.deallocate(ctrl);
    }

    shm_region    m_region;
    shm_allocator m_alloc;
};

}  // namespace

// ============================================================================
// shm_service_create / destroy
// ============================================================================

TEST_F(shm_service_ops_fixture, create_default_state)
{
    auto* svc = shm_service_create(m_alloc, "redfish", 12345U);
    ASSERT_NE(svc, nullptr);

    EXPECT_EQ(svc->abi_version, shm_service_abi_version);
    EXPECT_EQ(svc->state, static_cast<std::uint8_t>(shm_service_state::alive));
    EXPECT_EQ(svc->pid, 12345U);
    EXPECT_EQ(svc->epoch, 1U);
    EXPECT_EQ(shm_service_name(*svc), "redfish");
    EXPECT_TRUE(shm_service_check(*svc));

    shm_service_destroy(m_alloc, svc);
}

TEST_F(shm_service_ops_fixture, create_with_explicit_initial_epoch)
{
    auto* svc = shm_service_create(m_alloc, "ipmi", 99U, 7U);
    ASSERT_NE(svc, nullptr);
    EXPECT_EQ(svc->epoch, 7U);
    shm_service_destroy(m_alloc, svc);
}

TEST_F(shm_service_ops_fixture, destroy_null_is_noop)
{
    shm_service_destroy(m_alloc, nullptr);
}

TEST_F(shm_service_ops_fixture, destroy_releases_all)
{
    const std::size_t before = m_alloc.allocated_size();

    auto* svc = shm_service_create(m_alloc, "service-with-name", 1U);
    ASSERT_NE(svc, nullptr);
    shm_service_destroy(m_alloc, svc);

    EXPECT_EQ(m_alloc.allocated_size(), before);
}

// ============================================================================
// setter / epoch
// ============================================================================

TEST_F(shm_service_ops_fixture, set_pid_updates_field_and_crc)
{
    auto* svc = shm_service_create(m_alloc, "n", 1U);
    ASSERT_NE(svc, nullptr);

    shm_service_set_pid(*svc, 0xC0FFEEU);
    EXPECT_EQ(svc->pid, 0xC0FFEEU);
    EXPECT_TRUE(shm_service_check(*svc));

    shm_service_destroy(m_alloc, svc);
}

TEST_F(shm_service_ops_fixture, set_state_updates_field_and_crc)
{
    auto* svc = shm_service_create(m_alloc, "n", 1U);
    ASSERT_NE(svc, nullptr);

    shm_service_set_state(*svc, shm_service_state::detached);
    EXPECT_EQ(svc->state, static_cast<std::uint8_t>(shm_service_state::detached));
    EXPECT_TRUE(shm_service_check(*svc));

    shm_service_destroy(m_alloc, svc);
}

TEST_F(shm_service_ops_fixture, increment_epoch_returns_new_value)
{
    auto* svc = shm_service_create(m_alloc, "n", 1U, 5U);
    ASSERT_NE(svc, nullptr);

    EXPECT_EQ(shm_service_increment_epoch(*svc), 6U);
    EXPECT_EQ(shm_service_increment_epoch(*svc), 7U);
    EXPECT_EQ(svc->epoch, 7U);
    EXPECT_TRUE(shm_service_check(*svc));

    shm_service_destroy(m_alloc, svc);
}

// ============================================================================
// attach
// ============================================================================

TEST_F(shm_service_ops_fixture, attach_creates_when_absent)
{
    auto* ctrl = create_service_map_control();
    {
        shm_service_map map(*ctrl, m_alloc);
        EXPECT_TRUE(map.empty());

        shm_service* svc = shm_service_attach(m_alloc, map, "redfish", 1000U);
        ASSERT_NE(svc, nullptr);
        EXPECT_EQ(svc->pid, 1000U);
        EXPECT_EQ(svc->epoch, 1U);
        EXPECT_EQ(shm_service_name(*svc), "redfish");
        EXPECT_EQ(map.size(), 1U);
        EXPECT_TRUE(shm_service_check(*svc));

        // 清理 svc 与 map 中的 key（map 析构会处理 byte_string；svc 单独 destroy）
        shm_service_destroy(m_alloc, svc);
        map.clear();
    }
    destroy_map_control(ctrl);
}

TEST_F(shm_service_ops_fixture, attach_takes_over_existing_with_epoch_bump)
{
    auto* ctrl = create_service_map_control();
    {
        shm_service_map map(*ctrl, m_alloc);

        // 任意 pid 易与本机活进程冲突；本用例只验证 attach 写入语义，
        // 全程 force=true 跳过 liveness（liveness 行为由 takeover 套件覆盖）。
        shm_service* first = shm_service_attach(m_alloc, map, "ipmi", 1111U, /*force=*/true);
        ASSERT_NE(first, nullptr);
        EXPECT_EQ(first->epoch, 1U);

        shm_service* second = shm_service_attach(m_alloc, map, "ipmi", 2222U, /*force=*/true);
        ASSERT_NE(second, nullptr);
        EXPECT_EQ(second, first) << "同名 attach 必须返回同一 shm_service POD";
        EXPECT_EQ(second->pid, 2222U);
        EXPECT_EQ(second->epoch, 2U);
        EXPECT_EQ(map.size(), 1U);
        EXPECT_TRUE(shm_service_check(*second));

        shm_service_destroy(m_alloc, second);
        map.clear();
    }
    destroy_map_control(ctrl);
}

TEST_F(shm_service_ops_fixture, attach_multiple_distinct_services)
{
    auto* ctrl = create_service_map_control();
    {
        shm_service_map map(*ctrl, m_alloc);

        shm_service* a = shm_service_attach(m_alloc, map, "a", 1U);
        shm_service* b = shm_service_attach(m_alloc, map, "b", 2U);
        shm_service* c = shm_service_attach(m_alloc, map, "c", 3U);
        ASSERT_NE(a, nullptr);
        ASSERT_NE(b, nullptr);
        ASSERT_NE(c, nullptr);
        EXPECT_NE(a, b);
        EXPECT_NE(b, c);
        EXPECT_EQ(map.size(), 3U);
        EXPECT_EQ(shm_service_name(*a), "a");
        EXPECT_EQ(shm_service_name(*b), "b");
        EXPECT_EQ(shm_service_name(*c), "c");

        shm_service_destroy(m_alloc, a);
        shm_service_destroy(m_alloc, b);
        shm_service_destroy(m_alloc, c);
        map.clear();
    }
    destroy_map_control(ctrl);
}

TEST_F(shm_service_ops_fixture, attach_repeated_increments_epoch_each_time)
{
    auto* ctrl = create_service_map_control();
    {
        shm_service_map map(*ctrl, m_alloc);

        shm_service* svc = shm_service_attach(m_alloc, map, "svc", 100U, /*force=*/true);
        ASSERT_NE(svc, nullptr);
        EXPECT_EQ(svc->epoch, 1U);

        for (std::uint32_t i = 0; i < 5; ++i) {
            shm_service* again =
                shm_service_attach(m_alloc, map, "svc", 100U + i, /*force=*/true);
            EXPECT_EQ(again, svc);
        }
        EXPECT_EQ(svc->epoch, 6U);  // 1 + 5 次 ++

        shm_service_destroy(m_alloc, svc);
        map.clear();
    }
    destroy_map_control(ctrl);
}
