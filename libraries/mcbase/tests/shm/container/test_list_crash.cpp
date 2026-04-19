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
#include <csignal>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include <mc/shm/allocator.h>
#include <mc/shm/container/list.h>
#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/region.h>

using mc::shm::container::container_op;
using mc::shm::container::journal_begin;
using mc::shm::container::journal_load_op;
using mc::shm::container::list;
using mc::shm::container::list_control;
using mc::shm::container::list_node_magic;
using mc::shm::container::node_header_init;
using mc::shm::container::node_header_load_state;
using mc::shm::container::node_header_transition;
using mc::shm::container::node_state;

namespace {

std::string make_unique_name(std::string_view prefix)
{
    const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::string(prefix) + "_" + std::to_string(getpid()) + "_" + std::to_string(now_ns);
}

inline constexpr const char* k_crash_root_name = "mclist_ctrl";

class shm_list_crash_fixture : public ::testing::Test {
protected:

    void SetUp() override
    {
        m_segment_name = make_unique_name("mc_shm_list_crash");
        mc::shm::shm_region_options options;
        options.segment_name = m_segment_name;
        options.total_size   = 2 * 1024 * 1024;
        m_region             = std::make_unique<mc::shm::shm_region>(options);
        ASSERT_TRUE(m_region->is_valid());

        auto alloc = m_region->user_arena();
        auto* mem  = alloc.allocate(sizeof(list_control), alignof(list_control));
        ASSERT_NE(mem, nullptr);
        m_control = new (mem) list_control();

        const auto tag = static_cast<std::uint32_t>(m_region->offset_of(mem));
        list<int>::init(*m_control, tag);

        ASSERT_TRUE(m_region->upsert_root(k_crash_root_name, m_region->offset_of(mem),
                                          sizeof(list_control)));
    }

    void TearDown() override
    {
        m_region.reset();
        mc::shm::detail::shared_memory_backend::remove(m_segment_name);
    }

    std::vector<int> collect(list<int>& l) const
    {
        std::vector<int> values;
        for (auto it = l.begin(); it != l.end(); ++it) {
            values.push_back(*it);
        }
        return values;
    }

    std::string                          m_segment_name;
    std::unique_ptr<mc::shm::shm_region> m_region;
    list_control*                        m_control = nullptr;
};

// 子进程 helper：重新 attach region 并返回 list_control*
struct child_region_context {
    std::unique_ptr<mc::shm::shm_region> region;
    list_control*                        control = nullptr;
};

child_region_context child_attach(const std::string& segment_name)
{
    child_region_context ctx;
    mc::shm::shm_region_options options;
    options.segment_name = segment_name;
    options.open_mode    = mc::shm::detail::open_mode::open_existing;
    ctx.region = std::make_unique<mc::shm::shm_region>(options);
    if (!ctx.region->is_valid()) {
        return ctx;
    }
    auto root = ctx.region->find_root(k_crash_root_name);
    if (!root.has_value()) {
        return ctx;
    }
    ctx.control = static_cast<list_control*>(ctx.region->address_from_offset(root->offset));
    return ctx;
}

} // namespace

// ==========================================================================
// child_crash_during_insert_rollbacks_and_recovers
// ==========================================================================
//
// 子进程：持锁 + allocate 新节点 + journal_begin(insert)，然后 SIGKILL
// 父进程：再做任何 mutating 操作时 container_guard 应识别 recovered → rollback
// 预期：链表内容保持 [1,2,3]；节点被标 degraded
TEST_F(shm_list_crash_fixture, child_crash_during_insert_rollbacks)
{
    auto alloc = m_region->user_arena();
    list<int> l(*m_control, alloc);
    ASSERT_NE(nullptr, l.push_back(1));
    ASSERT_NE(nullptr, l.push_back(2));
    ASSERT_NE(nullptr, l.push_back(3));

    pid_t pid = ::fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        auto ctx = child_attach(m_segment_name);
        if (ctx.control == nullptr) {
            ::_exit(2);
        }
        auto& ctrl = *ctx.control;
        auto  child_alloc = ctx.region->user_arena();

        // 模拟 push_back 的"Step A/B 之后、Step C 之前"崩溃
        ctrl.mutex.lock_ex();

        void* mem = child_alloc.allocate(list<int>::node_size, list<int>::node_align);
        if (mem == nullptr) {
            ::_exit(3);
        }
        auto* node = static_cast<mc::shm::container::detail::list_node_common*>(mem);
        node_header_init(node->header, list_node_magic, ctrl.self_tag);
        node->prev_offset = 0;
        node->next_offset = 0;

        node_header_transition(node->header, node_state::pending);

        const auto new_off = mc::shm::container::detail::self_offset_from(&ctrl, node);
        const auto anchor_prev_off = ctrl.tail_offset;
        const auto anchor_next_off = std::int64_t{0};
        const auto saved_a         = anchor_next_off;
        const auto saved_b         = anchor_prev_off;
        const std::uint32_t flags  = 0x2U; // flag_at_tail

        journal_begin(ctrl.journal, container_op::insert,
                      static_cast<std::uint64_t>(new_off),
                      static_cast<std::uint64_t>(anchor_prev_off),
                      static_cast<std::uint64_t>(saved_a),
                      static_cast<std::uint64_t>(saved_b),
                      static_cast<std::uint64_t>(anchor_next_off), flags);

        // 故意不做链接修改、不 state=linked、不 journal_end
        ::raise(SIGKILL);
        ::_exit(0);
    }

    int status = 0;
    ASSERT_EQ(::waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFSIGNALED(status));
    EXPECT_EQ(WTERMSIG(status), SIGKILL);

    // 父进程：container_guard 在下次 lock_ex 会拿到 recovered，自动触发 recover
    // 触发方式：push_back 一个新元素
    ASSERT_NE(nullptr, l.push_back(4));

    // 期望：[1,2,3] 被保留，然后追加 4；journal 干净
    EXPECT_EQ(container_op::none, journal_load_op(m_control->journal));
    EXPECT_EQ(std::vector<int>({1, 2, 3, 4}), collect(l));
    EXPECT_GE(l.degraded_count(), 1U); // 子进程 alloc 的节点被 rollback + isolate
}

// ==========================================================================
// child_crash_during_erase_rollbacks
// ==========================================================================
//
// 子进程：持锁 + 对 tail 节点打 pending + journal_begin(erase)，然后 SIGKILL
// 父进程：recover 应 rollback，链表保持 [1,2,3]
TEST_F(shm_list_crash_fixture, child_crash_during_erase_rollbacks)
{
    auto alloc = m_region->user_arena();
    list<int> l(*m_control, alloc);
    l.push_back(1);
    l.push_back(2);
    l.push_back(3);

    pid_t pid = ::fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        auto ctx = child_attach(m_segment_name);
        if (ctx.control == nullptr) {
            ::_exit(2);
        }
        auto& ctrl = *ctx.control;
        ctrl.mutex.lock_ex();

        // 找到 tail 节点（值 3）
        const auto tail_off = ctrl.tail_offset;
        if (tail_off == 0) {
            ::_exit(3);
        }
        auto* tail_node = static_cast<mc::shm::container::detail::list_node_common*>(
            mc::shm::container::detail::from_offset(&ctrl, tail_off));

        const auto prev_off = tail_node->prev_offset;
        const auto next_off = tail_node->next_offset;

        node_header_transition(tail_node->header, node_state::pending);
        const std::uint32_t flags = 0x2U; // flag_at_tail (next==0)
        journal_begin(ctrl.journal, container_op::erase,
                      static_cast<std::uint64_t>(tail_off),
                      static_cast<std::uint64_t>(prev_off),
                      static_cast<std::uint64_t>(prev_off),
                      static_cast<std::uint64_t>(next_off), 0, flags);

        // 故意不做链接修改、不 journal_end
        ::raise(SIGKILL);
        ::_exit(0);
    }

    int status = 0;
    ASSERT_EQ(::waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFSIGNALED(status));

    // 父进程：触发 recover
    const auto before_deg = l.degraded_count();
    l.pop_front(); // 触发锁，recover 应 rollback erase

    // rollback erase → tail 节点应回到 linked；pop_front 又弹了 head (1)
    // 最终 [2, 3]
    EXPECT_EQ(container_op::none, journal_load_op(m_control->journal));
    EXPECT_EQ(std::vector<int>({2, 3}), collect(l));
    EXPECT_EQ(l.degraded_count(), before_deg); // rollback 成功，无 isolate
}

// ==========================================================================
// child_crash_corrupts_node_header_fsck_isolates
// ==========================================================================
//
// 子进程：持锁 + 破坏 tail 节点的 magic + 立即 SIGKILL（journal 保持干净）
// 父进程：下一次持锁时 container_guard 触发 recover → fsck 扫出损坏并 isolate
TEST_F(shm_list_crash_fixture, child_crash_corrupts_node_and_fsck_isolates)
{
    auto alloc = m_region->user_arena();
    list<int> l(*m_control, alloc);
    l.push_back(1);
    l.push_back(2);
    l.push_back(3);

    pid_t pid = ::fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        auto ctx = child_attach(m_segment_name);
        if (ctx.control == nullptr) {
            ::_exit(2);
        }
        auto& ctrl = *ctx.control;
        ctrl.mutex.lock_ex();

        // 破坏 tail 节点的 magic
        const auto tail_off = ctrl.tail_offset;
        if (tail_off == 0) {
            ::_exit(3);
        }
        auto* tail_node = static_cast<mc::shm::container::detail::list_node_common*>(
            mc::shm::container::detail::from_offset(&ctrl, tail_off));
        tail_node->header.magic ^= 0x1ULL;

        ::raise(SIGKILL);
        ::_exit(0);
    }

    int status = 0;
    ASSERT_EQ(::waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFSIGNALED(status));

    // 父进程触发 recover
    const auto before_deg = l.degraded_count();
    l.pop_front();

    // fsck isolate tail（值 3）；pop_front 弹掉 head (1)；剩 [2]
    EXPECT_EQ(container_op::none, journal_load_op(m_control->journal));
    EXPECT_EQ(std::vector<int>({2}), collect(l));
    EXPECT_GT(l.degraded_count(), before_deg);
}

// ==========================================================================
// repeated_child_crashes_still_converge
// ==========================================================================
//
// 连续多轮 child crash（混合 insert / erase 中途崩溃），验证父进程每轮
// 都能正确 recover 并继续操作，链表保持合理状态
TEST_F(shm_list_crash_fixture, repeated_child_crashes_still_converge)
{
    auto alloc = m_region->user_arena();
    list<int> l(*m_control, alloc);
    for (int i = 0; i < 10; ++i) {
        l.push_back(i);
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

            // 轮次内随机选 insert / erase 模拟
            if (round % 2 == 0) {
                auto child_alloc = ctx.region->user_arena();
                void* mem = child_alloc.allocate(list<int>::node_size, list<int>::node_align);
                if (mem == nullptr) {
                    ::_exit(3);
                }
                auto* node = static_cast<mc::shm::container::detail::list_node_common*>(mem);
                node_header_init(node->header, list_node_magic, ctrl.self_tag);
                node->prev_offset = 0;
                node->next_offset = 0;
                node_header_transition(node->header, node_state::pending);
                journal_begin(ctrl.journal, container_op::insert,
                              static_cast<std::uint64_t>(
                                  mc::shm::container::detail::self_offset_from(&ctrl, node)),
                              static_cast<std::uint64_t>(ctrl.tail_offset), 0,
                              static_cast<std::uint64_t>(ctrl.tail_offset), 0, 0x2U);
            } else if (ctrl.tail_offset != 0) {
                auto* tail_node = static_cast<mc::shm::container::detail::list_node_common*>(
                    mc::shm::container::detail::from_offset(&ctrl, ctrl.tail_offset));
                node_header_transition(tail_node->header, node_state::pending);
                journal_begin(ctrl.journal, container_op::erase,
                              static_cast<std::uint64_t>(ctrl.tail_offset),
                              static_cast<std::uint64_t>(tail_node->prev_offset),
                              static_cast<std::uint64_t>(tail_node->prev_offset),
                              static_cast<std::uint64_t>(tail_node->next_offset), 0, 0x2U);
            }
            ::raise(SIGKILL);
            ::_exit(0);
        }

        int status = 0;
        ASSERT_EQ(::waitpid(pid, &status, 0), pid);
        ASSERT_TRUE(WIFSIGNALED(status)) << "round=" << round;

        // 父进程 push_back 触发 recover
        ASSERT_NE(nullptr, l.push_back(1000 + round)) << "round=" << round;
        EXPECT_EQ(container_op::none, journal_load_op(m_control->journal)) << "round=" << round;
    }
}
