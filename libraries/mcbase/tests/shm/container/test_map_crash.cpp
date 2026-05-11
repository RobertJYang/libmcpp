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
#include <chrono>
#include <csignal>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include <mc/shm/allocator.h>
#include <mc/shm/container/map.h>
#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/region.h>

using mc::shm::container::container_op;
using mc::shm::container::journal_begin;
using mc::shm::container::journal_load_op;
using mc::shm::container::map;
using mc::shm::container::map_control;
using mc::shm::container::map_node_magic;
using mc::shm::container::node_header_transition;
using mc::shm::container::node_state;

namespace {

std::string make_unique_name(std::string_view prefix)
{
    const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::string(prefix) + "_" + std::to_string(getpid()) + "_" + std::to_string(now_ns);
}

inline constexpr const char* k_map_root_name = "mcmap_ctrl";

class shm_map_crash_fixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_segment_name = make_unique_name("mc_shm_map_crash");
        mc::shm::shm_region_options options;
        options.segment_name = m_segment_name;
        options.total_size   = 2 * 1024 * 1024;
        m_region             = std::make_unique<mc::shm::shm_region>(options);
        ASSERT_TRUE(m_region->is_valid());

        auto  alloc = m_region->user_arena();
        auto* mem   = alloc.allocate(sizeof(map_control), alignof(map_control));
        ASSERT_NE(mem, nullptr);
        m_control      = new (mem) map_control();
        const auto tag = static_cast<std::uint32_t>(m_region->offset_of(mem));
        map<int, int>::init(*m_control, tag);

        ASSERT_TRUE(m_region->upsert_root(k_map_root_name, m_region->offset_of(mem), sizeof(map_control)));
    }

    void TearDown() override
    {
        m_region.reset();
        mc::shm::detail::shared_memory_backend::remove(m_segment_name);
    }

    std::vector<std::pair<int, int>> collect(map<int, int>& m) const
    {
        std::vector<std::pair<int, int>> out;
        for (auto it = m.begin(); it != m.end(); ++it) {
            auto p = *it;
            out.emplace_back(*p.key, *p.value);
        }
        return out;
    }

    std::string                          m_segment_name;
    std::unique_ptr<mc::shm::shm_region> m_region;
    map_control*                         m_control = nullptr;
};

struct child_map_context {
    std::unique_ptr<mc::shm::shm_region> region;
    map_control*                         control = nullptr;
};

child_map_context child_attach(const std::string& segment_name)
{
    child_map_context           ctx;
    mc::shm::shm_region_options options;
    options.segment_name = segment_name;
    options.open_mode    = mc::shm::detail::open_mode::open_existing;
    ctx.region           = std::make_unique<mc::shm::shm_region>(options);
    if (!ctx.region->is_valid()) {
        return ctx;
    }
    auto root = ctx.region->find_root(k_map_root_name);
    if (!root.has_value()) {
        return ctx;
    }
    ctx.control = static_cast<map_control*>(ctx.region->address_from_offset(root->offset));
    return ctx;
}

} // namespace

// ==========================================================================
// 子进程在 try_emplace 中途崩溃：journal 已开，高层 splice 尚未完成 → rollback
// ==========================================================================
TEST_F(shm_map_crash_fixture, child_crash_during_insert_rollbacks)
{
    auto          alloc = m_region->user_arena();
    map<int, int> m(*m_control, alloc);
    for (int v : {10, 20, 30}) {
        ASSERT_TRUE(m.try_emplace(v, v * 100).second);
    }

    pid_t pid = ::fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        auto ctx = child_attach(m_segment_name);
        if (ctx.control == nullptr) {
            ::_exit(2);
        }
        auto& ctrl        = *ctx.control;
        auto  child_alloc = ctx.region->user_arena();

        ctrl.mutex.lock_ex();

        auto* node = mc::shm::container::detail::map_prepare_node(ctrl, child_alloc, 1U);
        if (node == nullptr) {
            ::_exit(3);
        }
        const int key_val   = 25;
        const int value_val = 2500;
        std::memcpy(mc::shm::container::detail::map_node_key_ptr(node, 1U, ctrl.key_align), &key_val, sizeof(int));
        std::memcpy(
            mc::shm::container::detail::map_node_value_ptr(node, 1U, ctrl.key_size, ctrl.key_align, ctrl.value_align),
            &value_val, sizeof(int));

        // 手工找到 key==20 的节点作为 layer 0 pred
        std::int64_t pred_off = 0;
        std::int64_t cur      = ctrl.head_forward[0];
        while (cur != 0) {
            auto* n = static_cast<mc::shm::container::detail::map_node_common*>(
                mc::shm::container::detail::map_from_offset(&ctrl, cur));
            const int* k =
                static_cast<const int*>(mc::shm::container::detail::map_node_key_ptr(n, n->level, ctrl.key_align));
            if (*k >= key_val) {
                break;
            }
            pred_off = cur;
            cur      = *mc::shm::container::detail::map_node_forward_ptr(n, 0U);
        }
        const std::int64_t anchor_next                              = cur;
        *mc::shm::container::detail::map_node_forward_ptr(node, 0U) = anchor_next;
        *mc::shm::container::detail::map_node_anchor_ptr(node, 0U)  = anchor_next;
        *mc::shm::container::detail::map_node_prev_ptr(node)        = pred_off;

        node_header_transition(node->header, node_state::pending);
        const std::int64_t new_off = mc::shm::container::detail::map_self_offset_from(&ctrl, node);
        journal_begin(ctrl.journal, container_op::insert, static_cast<std::uint64_t>(new_off), 0, 0, 0, 0,
                      /*misc=level*/ 1U);

        ::raise(SIGKILL);
        ::_exit(0);
    }

    int status = 0;
    ASSERT_EQ(::waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFSIGNALED(status));

    // 父进程触发 recover：再做一次 mutating
    ASSERT_TRUE(m.try_emplace(40, 4000).second);

    EXPECT_EQ(container_op::none, journal_load_op(m_control->journal));
    EXPECT_EQ((std::vector<std::pair<int, int>>{{10, 1000}, {20, 2000}, {30, 3000}, {40, 4000}}), collect(m));
    EXPECT_GE(m.degraded_count(), 1U);
}

// ==========================================================================
// 子进程在 erase 中途崩溃：journal 已开但 layer 0 尚未摘 → rollback
// ==========================================================================
TEST_F(shm_map_crash_fixture, child_crash_during_erase_rollbacks)
{
    auto          alloc = m_region->user_arena();
    map<int, int> m(*m_control, alloc);
    for (int v : {10, 20, 30}) {
        ASSERT_TRUE(m.try_emplace(v, v * 100).second);
    }

    pid_t pid = ::fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        auto ctx = child_attach(m_segment_name);
        if (ctx.control == nullptr) {
            ::_exit(2);
        }
        auto& ctrl = *ctx.control;
        ctrl.mutex.lock_ex();

        std::int64_t                                 cur    = ctrl.head_forward[0];
        mc::shm::container::detail::map_node_common* target = nullptr;
        while (cur != 0) {
            auto* n = static_cast<mc::shm::container::detail::map_node_common*>(
                mc::shm::container::detail::map_from_offset(&ctrl, cur));
            const int* k =
                static_cast<const int*>(mc::shm::container::detail::map_node_key_ptr(n, n->level, ctrl.key_align));
            if (*k == 20) {
                target = n;
                break;
            }
            cur = *mc::shm::container::detail::map_node_forward_ptr(n, 0U);
        }
        if (target == nullptr) {
            ::_exit(3);
        }

        node_header_transition(target->header, node_state::pending);
        const std::int64_t target_off = mc::shm::container::detail::map_self_offset_from(&ctrl, target);
        journal_begin(ctrl.journal, container_op::erase, static_cast<std::uint64_t>(target_off), 0, 0, 0, 0,
                      target->level);

        ::raise(SIGKILL);
        ::_exit(0);
    }

    int status = 0;
    ASSERT_EQ(::waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFSIGNALED(status));

    const auto before_deg = m.degraded_count();
    ASSERT_TRUE(m.try_emplace(40, 4000).second);

    EXPECT_EQ(container_op::none, journal_load_op(m_control->journal));
    EXPECT_EQ((std::vector<std::pair<int, int>>{{10, 1000}, {20, 2000}, {30, 3000}, {40, 4000}}), collect(m));
    EXPECT_EQ(before_deg, m.degraded_count());
    auto found = m.find(20);
    ASSERT_TRUE(found);
    EXPECT_EQ(2000, *found.value);
}

// ==========================================================================
// 子进程破坏某节点 header → 父进程 fsck 应 isolate 该节点
// ==========================================================================
TEST_F(shm_map_crash_fixture, child_crash_corrupts_node_header_fsck_isolates)
{
    auto          alloc = m_region->user_arena();
    map<int, int> m(*m_control, alloc);
    for (int v : {10, 20, 30}) {
        ASSERT_TRUE(m.try_emplace(v, v * 100).second);
    }

    pid_t pid = ::fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        auto ctx = child_attach(m_segment_name);
        if (ctx.control == nullptr) {
            ::_exit(2);
        }
        auto& ctrl = *ctx.control;
        ctrl.mutex.lock_ex();

        std::int64_t cur = ctrl.head_forward[0];
        while (cur != 0) {
            auto* n = static_cast<mc::shm::container::detail::map_node_common*>(
                mc::shm::container::detail::map_from_offset(&ctrl, cur));
            const int* k =
                static_cast<const int*>(mc::shm::container::detail::map_node_key_ptr(n, n->level, ctrl.key_align));
            if (*k == 20) {
                n->header.magic ^= 0x1ULL;
                break;
            }
            cur = *mc::shm::container::detail::map_node_forward_ptr(n, 0U);
        }
        ::raise(SIGKILL);
        ::_exit(0);
    }

    int status = 0;
    ASSERT_EQ(::waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFSIGNALED(status));

    const auto before_deg = m.degraded_count();
    ASSERT_TRUE(m.try_emplace(40, 4000).second);

    EXPECT_EQ(container_op::none, journal_load_op(m_control->journal));
    const auto values = collect(m);
    // 20 被 isolate；剩 {10, 30, 40}
    EXPECT_EQ((std::vector<std::pair<int, int>>{{10, 1000}, {30, 3000}, {40, 4000}}), values);
    EXPECT_GT(m.degraded_count(), before_deg);
    EXPECT_FALSE(m.find(20));
}

// ==========================================================================
// 连续多轮崩溃：验证 map 反复 recover 仍然收敛（有序 + 无 dirty journal）
// ==========================================================================
TEST_F(shm_map_crash_fixture, repeated_child_crashes_still_converge)
{
    auto          alloc = m_region->user_arena();
    map<int, int> m(*m_control, alloc);
    for (int i = 0; i < 20; ++i) {
        ASSERT_TRUE(m.try_emplace(i * 10, i).second);
    }

    for (int round = 0; round < 5; ++round) {
        pid_t pid = ::fork();
        ASSERT_NE(pid, -1);
        if (pid == 0) {
            auto ctx = child_attach(m_segment_name);
            if (ctx.control == nullptr) {
                ::_exit(2);
            }
            auto& ctrl = *ctx.control;
            ctrl.mutex.lock_ex();

            if (round % 2 == 0) {
                // insert 中途崩溃
                auto  child_alloc = ctx.region->user_arena();
                auto* node        = mc::shm::container::detail::map_prepare_node(ctrl, child_alloc, 1U);
                if (node == nullptr) {
                    ::_exit(3);
                }
                const int key_val   = 500 + round;
                const int value_val = round;
                std::memcpy(mc::shm::container::detail::map_node_key_ptr(node, 1U, ctrl.key_align), &key_val,
                            sizeof(int));
                std::memcpy(mc::shm::container::detail::map_node_value_ptr(node, 1U, ctrl.key_size, ctrl.key_align,
                                                                           ctrl.value_align),
                            &value_val, sizeof(int));
                *mc::shm::container::detail::map_node_forward_ptr(node, 0U) = 0;
                *mc::shm::container::detail::map_node_anchor_ptr(node, 0U)  = 0;
                *mc::shm::container::detail::map_node_prev_ptr(node)        = 0;
                node_header_transition(node->header, node_state::pending);
                journal_begin(ctrl.journal, container_op::insert,
                              static_cast<std::uint64_t>(mc::shm::container::detail::map_self_offset_from(&ctrl, node)),
                              0, 0, 0, 0, 1U);
            } else {
                // erase 中途崩溃
                const std::int64_t tgt = ctrl.head_forward[0];
                if (tgt != 0) {
                    auto* tn = static_cast<mc::shm::container::detail::map_node_common*>(
                        mc::shm::container::detail::map_from_offset(&ctrl, tgt));
                    node_header_transition(tn->header, node_state::pending);
                    journal_begin(ctrl.journal, container_op::erase, static_cast<std::uint64_t>(tgt), 0, 0, 0, 0,
                                  tn->level);
                }
            }
            ::raise(SIGKILL);
            ::_exit(0);
        }

        int status = 0;
        ASSERT_EQ(::waitpid(pid, &status, 0), pid);
        ASSERT_TRUE(WIFSIGNALED(status)) << "round=" << round;

        ASSERT_TRUE(m.try_emplace(10000 + round, round).second) << "round=" << round;
        EXPECT_EQ(container_op::none, journal_load_op(m_control->journal)) << "round=" << round;

        const auto values = collect(m);
        EXPECT_TRUE(std::is_sorted(values.begin(), values.end(),
                                   [](const auto& a, const auto& b) {
            return a.first < b.first;
        })) << "round="
            << round;
    }
}
