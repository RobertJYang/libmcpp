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
#include <mc/shm/container/set.h>
#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/region.h>

using mc::shm::container::container_op;
using mc::shm::container::journal_begin;
using mc::shm::container::journal_load_op;
using mc::shm::container::node_header_init;
using mc::shm::container::node_header_transition;
using mc::shm::container::node_state;
using mc::shm::container::set;
using mc::shm::container::set_control;
using mc::shm::container::set_max_level;
using mc::shm::container::set_node_magic;

namespace {

std::string make_unique_name(std::string_view prefix)
{
    const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::string(prefix) + "_" + std::to_string(getpid()) + "_" + std::to_string(now_ns);
}

inline constexpr const char* k_set_root_name = "mcset_ctrl";

class shm_set_crash_fixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_segment_name = make_unique_name("mc_shm_set_crash");
        mc::shm::shm_region_options options;
        options.segment_name = m_segment_name;
        options.total_size   = 2 * 1024 * 1024;
        m_region             = std::make_unique<mc::shm::shm_region>(options);
        ASSERT_TRUE(m_region->is_valid());

        auto  alloc = m_region->user_arena();
        auto* mem   = alloc.allocate(sizeof(set_control), alignof(set_control));
        ASSERT_NE(mem, nullptr);
        m_control      = new (mem) set_control();
        const auto tag = static_cast<std::uint32_t>(m_region->offset_of(mem));
        set<int>::init(*m_control, tag);

        ASSERT_TRUE(m_region->upsert_root(k_set_root_name, m_region->offset_of(mem),
                                          sizeof(set_control)));
    }

    void TearDown() override
    {
        m_region.reset();
        mc::shm::detail::shared_memory_backend::remove(m_segment_name);
    }

    std::vector<int> collect(set<int>& s) const
    {
        std::vector<int> out;
        for (auto it = s.begin(); it != s.end(); ++it) {
            out.push_back(*it);
        }
        return out;
    }

    std::string                          m_segment_name;
    std::unique_ptr<mc::shm::shm_region> m_region;
    set_control*                         m_control = nullptr;
};

struct child_set_context {
    std::unique_ptr<mc::shm::shm_region> region;
    set_control*                         control = nullptr;
};

child_set_context child_attach(const std::string& segment_name)
{
    child_set_context           ctx;
    mc::shm::shm_region_options options;
    options.segment_name = segment_name;
    options.open_mode    = mc::shm::detail::open_mode::open_existing;
    ctx.region           = std::make_unique<mc::shm::shm_region>(options);
    if (!ctx.region->is_valid()) {
        return ctx;
    }
    auto root = ctx.region->find_root(k_set_root_name);
    if (!root.has_value()) {
        return ctx;
    }
    ctx.control = static_cast<set_control*>(ctx.region->address_from_offset(root->offset));
    return ctx;
}

} // namespace

// ==========================================================================
// 子进程在 insert 中途崩溃：journal 已开（container_op::insert）、高层已 splice
// 但 layer 0 commit 尚未完成 → rollback
// ==========================================================================
TEST_F(shm_set_crash_fixture, child_crash_during_insert_rollbacks)
{
    auto     alloc = m_region->user_arena();
    set<int> s(*m_control, alloc);
    for (int v : {10, 20, 30}) {
        ASSERT_TRUE(s.insert(v).second);
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

        // prepare 一个 level=1 的新节点，key=25（会插在 20 和 30 之间）
        auto* node = mc::shm::container::detail::set_prepare_node(ctrl, child_alloc, 1U);
        if (node == nullptr) {
            ::_exit(3);
        }
        const int key_val = 25;
        std::memcpy(mc::shm::container::detail::set_node_key_ptr(node, 1U), &key_val,
                    sizeof(int));
        // forward_offset[0] / anchor_backup[0] = 原 pred(20).forward[0] (指向 30)
        // 这里简单 walk 第 0 层找到 20 的 offset
        std::int64_t pred_off = 0;
        std::int64_t cur      = ctrl.head_forward[0];
        while (cur != 0) {
            auto* n = static_cast<mc::shm::container::detail::set_node_common*>(
                mc::shm::container::detail::set_from_offset(&ctrl, cur));
            const int* k = static_cast<const int*>(
                mc::shm::container::detail::set_node_key_ptr(n, n->level));
            if (*k >= key_val) {
                break;
            }
            pred_off = cur;
            cur      = *mc::shm::container::detail::set_node_forward_ptr(n, 0U);
        }
        const std::int64_t anchor_next = cur; // 原 pred.forward[0] 指向的后继
        *mc::shm::container::detail::set_node_forward_ptr(node, 0U) = anchor_next;
        *mc::shm::container::detail::set_node_anchor_ptr(node, 0U)  = anchor_next;
        *mc::shm::container::detail::set_node_prev_ptr(node)        = pred_off;

        node_header_transition(node->header, node_state::pending);
        const std::int64_t new_off = mc::shm::container::detail::set_self_offset_from(&ctrl, node);
        journal_begin(ctrl.journal, container_op::insert,
                      static_cast<std::uint64_t>(new_off), 0, 0, 0, 0, /*misc=level*/ 1U);

        // 故意不 splice layer 0、不 state=linked、不 journal_end
        ::raise(SIGKILL);
        ::_exit(0);
    }

    int status = 0;
    ASSERT_EQ(::waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFSIGNALED(status));

    // 父进程触发 recover：再做一次 mutating 操作
    ASSERT_TRUE(s.insert(40).second);

    EXPECT_EQ(container_op::none, journal_load_op(m_control->journal));
    EXPECT_EQ(std::vector<int>({10, 20, 30, 40}), collect(s));
    // 子进程分配的节点应被 isolate（degraded）
    EXPECT_GE(s.degraded_count(), 1U);
}

// ==========================================================================
// 子进程在 erase 中途崩溃：journal 已开但 layer 0 还没摘 → rollback
// ==========================================================================
TEST_F(shm_set_crash_fixture, child_crash_during_erase_rollbacks)
{
    auto     alloc = m_region->user_arena();
    set<int> s(*m_control, alloc);
    for (int v : {10, 20, 30}) {
        ASSERT_TRUE(s.insert(v).second);
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

        // 找到 key==20 的节点
        std::int64_t cur = ctrl.head_forward[0];
        mc::shm::container::detail::set_node_common* target = nullptr;
        while (cur != 0) {
            auto* n = static_cast<mc::shm::container::detail::set_node_common*>(
                mc::shm::container::detail::set_from_offset(&ctrl, cur));
            const int* k = static_cast<const int*>(
                mc::shm::container::detail::set_node_key_ptr(n, n->level));
            if (*k == 20) {
                target = n;
                break;
            }
            cur = *mc::shm::container::detail::set_node_forward_ptr(n, 0U);
        }
        if (target == nullptr) {
            ::_exit(3);
        }

        node_header_transition(target->header, node_state::pending);
        const std::int64_t target_off
            = mc::shm::container::detail::set_self_offset_from(&ctrl, target);
        journal_begin(ctrl.journal, container_op::erase,
                      static_cast<std::uint64_t>(target_off), 0, 0, 0, 0,
                      /*misc=level*/ target->level);

        // 故意不摘 layer 0、不 state=free、不 journal_end
        ::raise(SIGKILL);
        ::_exit(0);
    }

    int status = 0;
    ASSERT_EQ(::waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFSIGNALED(status));

    const auto before_deg = s.degraded_count();

    // 父进程触发 recover：再做一次 mutating
    ASSERT_TRUE(s.insert(40).second);

    // rollback erase → 20 应仍在集合中
    EXPECT_EQ(container_op::none, journal_load_op(m_control->journal));
    EXPECT_EQ(std::vector<int>({10, 20, 30, 40}), collect(s));
    // erase rollback 不应增加 degraded
    EXPECT_EQ(before_deg, s.degraded_count());
    EXPECT_NE(nullptr, s.find(20));
}

// ==========================================================================
// 子进程破坏某节点 header → 父进程 fsck 应 isolate 该节点
// ==========================================================================
TEST_F(shm_set_crash_fixture, child_crash_corrupts_node_header_fsck_isolates)
{
    auto     alloc = m_region->user_arena();
    set<int> s(*m_control, alloc);
    for (int v : {10, 20, 30}) {
        ASSERT_TRUE(s.insert(v).second);
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
            auto* n = static_cast<mc::shm::container::detail::set_node_common*>(
                mc::shm::container::detail::set_from_offset(&ctrl, cur));
            const int* k = static_cast<const int*>(
                mc::shm::container::detail::set_node_key_ptr(n, n->level));
            if (*k == 20) {
                n->header.magic ^= 0x1ULL;
                break;
            }
            cur = *mc::shm::container::detail::set_node_forward_ptr(n, 0U);
        }
        ::raise(SIGKILL);
        ::_exit(0);
    }

    int status = 0;
    ASSERT_EQ(::waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFSIGNALED(status));

    const auto before_deg = s.degraded_count();
    ASSERT_TRUE(s.insert(40).second);

    EXPECT_EQ(container_op::none, journal_load_op(m_control->journal));
    const auto values = collect(s);
    // 20 被 isolate；剩 {10, 30, 40}
    EXPECT_EQ(std::vector<int>({10, 30, 40}), values);
    EXPECT_GT(s.degraded_count(), before_deg);
    EXPECT_EQ(nullptr, s.find(20));
}

// ==========================================================================
// 连续多轮崩溃，验证 set 反复 recover 仍能收敛
// ==========================================================================
TEST_F(shm_set_crash_fixture, repeated_child_crashes_still_converge)
{
    auto     alloc = m_region->user_arena();
    set<int> s(*m_control, alloc);
    for (int i = 0; i < 20; ++i) {
        ASSERT_TRUE(s.insert(i * 10).second);
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
                auto* node = mc::shm::container::detail::set_prepare_node(ctrl, child_alloc, 1U);
                if (node == nullptr) {
                    ::_exit(3);
                }
                const int key_val = 500 + round;
                std::memcpy(
                    mc::shm::container::detail::set_node_key_ptr(node, 1U), &key_val,
                    sizeof(int));
                *mc::shm::container::detail::set_node_forward_ptr(node, 0U) = 0;
                *mc::shm::container::detail::set_node_anchor_ptr(node, 0U)  = 0;
                *mc::shm::container::detail::set_node_prev_ptr(node)        = 0;
                node_header_transition(node->header, node_state::pending);
                journal_begin(ctrl.journal, container_op::insert,
                              static_cast<std::uint64_t>(
                                  mc::shm::container::detail::set_self_offset_from(&ctrl, node)),
                              0, 0, 0, 0, 1U);
            } else {
                // erase 中途崩溃：随便选 head_forward[0] 指向的节点
                const std::int64_t tgt = ctrl.head_forward[0];
                if (tgt != 0) {
                    auto* tn = static_cast<mc::shm::container::detail::set_node_common*>(
                        mc::shm::container::detail::set_from_offset(&ctrl, tgt));
                    node_header_transition(tn->header, node_state::pending);
                    journal_begin(ctrl.journal, container_op::erase,
                                  static_cast<std::uint64_t>(tgt), 0, 0, 0, 0, tn->level);
                }
            }
            ::raise(SIGKILL);
            ::_exit(0);
        }

        int status = 0;
        ASSERT_EQ(::waitpid(pid, &status, 0), pid);
        ASSERT_TRUE(WIFSIGNALED(status)) << "round=" << round;

        // 父进程触发 recover：做一次 mutating
        ASSERT_TRUE(s.insert(10000 + round).second) << "round=" << round;
        EXPECT_EQ(container_op::none, journal_load_op(m_control->journal))
            << "round=" << round;

        // 容器遍历应一直处于有序
        const auto values = collect(s);
        EXPECT_TRUE(std::is_sorted(values.begin(), values.end())) << "round=" << round;
    }
}
