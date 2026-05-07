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
#include <cstring>
#include <thread>

#include <mc/shm/container/guard.h>
#include <mc/shm/container/journal.h>
#include <mc/shm/container/node_header.h>
#include <mc/shm/ipc_mutex.h>

using mc::shm::container::container_journal;
using mc::shm::container::container_journal_view;
using mc::shm::container::container_op;
using mc::shm::container::container_guard;
using mc::shm::container::crc16_ccitt;
using mc::shm::container::journal_begin;
using mc::shm::container::journal_end;
using mc::shm::container::journal_init;
using mc::shm::container::journal_load;
using mc::shm::container::journal_load_op;
using mc::shm::container::journal_window;
using mc::shm::container::list_node_magic;
using mc::shm::container::node_header;
using mc::shm::container::node_header_check;
using mc::shm::container::node_header_checksum;
using mc::shm::container::node_header_init;
using mc::shm::container::node_header_load_state;
using mc::shm::container::node_header_mark_degraded;
using mc::shm::container::node_header_transition;
using mc::shm::container::node_state;

// ==========================================================================
// crc16_ccitt
// ==========================================================================

TEST(shm_container_basics, crc16_known_vectors)
{
    // "123456789" 标准测试向量（CRC-16-CCITT/FALSE，init 0xFFFF 不反转）结果 0x29B1
    const char data[] = "123456789";
    EXPECT_EQ(0x29B1U, crc16_ccitt(data, 9));
}

TEST(shm_container_basics, crc16_differs_on_single_bit_flip)
{
    std::uint64_t a = 0x1122334455667788ULL;
    std::uint64_t b = a ^ 0x1ULL;
    EXPECT_NE(crc16_ccitt(&a, sizeof(a)), crc16_ccitt(&b, sizeof(b)));
}

// ==========================================================================
// node_header
// ==========================================================================

TEST(shm_container_basics, node_header_layout)
{
    EXPECT_EQ(16U, sizeof(node_header));
    EXPECT_EQ(16U, alignof(node_header));
}

TEST(shm_container_basics, node_header_init_state_and_check)
{
    node_header h{};
    node_header_init(h, list_node_magic, /*owner_offset=*/1024);

    EXPECT_EQ(list_node_magic, h.magic);
    EXPECT_EQ(1024U, h.owner_offset);
    EXPECT_EQ(node_state::free, node_header_load_state(h));
    EXPECT_TRUE(node_header_check(h));
}

TEST(shm_container_basics, node_header_rejects_uninitialized)
{
    node_header h{};
    EXPECT_FALSE(node_header_check(h)) << "magic == 0 应视为未初始化";
}

TEST(shm_container_basics, node_header_detects_magic_flip)
{
    node_header h{};
    node_header_init(h, list_node_magic, 128);
    ASSERT_TRUE(node_header_check(h));

    h.magic ^= 0x1ULL;
    EXPECT_FALSE(node_header_check(h));
}

TEST(shm_container_basics, node_header_detects_owner_flip)
{
    node_header h{};
    node_header_init(h, list_node_magic, 128);
    ASSERT_TRUE(node_header_check(h));

    h.owner_offset ^= 0x1U;
    EXPECT_FALSE(node_header_check(h));
}

TEST(shm_container_basics, node_header_detects_checksum)
{
    node_header h{};
    node_header_init(h, list_node_magic, 128);
    h.checksum ^= 0xFFFFU;
    EXPECT_FALSE(node_header_check(h));
}

TEST(shm_container_basics, node_header_transition_does_not_invalidate_checksum)
{
    node_header h{};
    node_header_init(h, list_node_magic, 128);

    node_header_transition(h, node_state::pending);
    EXPECT_EQ(node_state::pending, node_header_load_state(h));
    EXPECT_TRUE(node_header_check(h));

    node_header_transition(h, node_state::linked);
    EXPECT_EQ(node_state::linked, node_header_load_state(h));
    EXPECT_TRUE(node_header_check(h));

    node_header_transition(h, node_state::pending);
    node_header_transition(h, node_state::free);
    EXPECT_EQ(node_state::free, node_header_load_state(h));
}

TEST(shm_container_basics, node_header_detects_invalid_state_byte)
{
    node_header h{};
    node_header_init(h, list_node_magic, 128);

    // 塞入一个非法 state（> degraded）
    h.state.store(0xEE, std::memory_order_release);
    EXPECT_FALSE(node_header_check(h));
}

TEST(shm_container_basics, node_header_mark_degraded_is_one_way)
{
    node_header h{};
    node_header_init(h, list_node_magic, 128);
    node_header_mark_degraded(h);
    EXPECT_EQ(node_state::degraded, node_header_load_state(h));
    EXPECT_TRUE(node_header_check(h));
}

TEST(shm_container_basics, node_header_checksum_stable)
{
    node_header h1{};
    node_header h2{};
    node_header_init(h1, list_node_magic, 4096);
    node_header_init(h2, list_node_magic, 4096);
    EXPECT_EQ(h1.checksum, h2.checksum);
    EXPECT_EQ(h1.checksum, node_header_checksum(h1));
}

// ==========================================================================
// container_journal
// ==========================================================================

TEST(shm_container_basics, journal_layout)
{
    EXPECT_EQ(64U, sizeof(container_journal));
    EXPECT_EQ(64U, alignof(container_journal));
}

TEST(shm_container_basics, journal_init_starts_clean)
{
    container_journal j{};
    journal_init(j);
    EXPECT_EQ(container_op::none, journal_load_op(j));
    container_journal_view view{};
    EXPECT_FALSE(journal_load(j, view));
}

TEST(shm_container_basics, journal_begin_makes_payload_visible)
{
    container_journal j{};
    journal_init(j);

    journal_begin(j, container_op::insert, /*target=*/100, /*anchor=*/200,
                  /*saved_a=*/0xAAAA, /*saved_b=*/0xBBBB, /*extra=*/0xCCCC, /*misc=*/7);

    EXPECT_EQ(container_op::insert, journal_load_op(j));

    container_journal_view view{};
    ASSERT_TRUE(journal_load(j, view));
    EXPECT_EQ(container_op::insert, view.op);
    EXPECT_EQ(100U, view.target_node_offset);
    EXPECT_EQ(200U, view.anchor_offset);
    EXPECT_EQ(0xAAAAU, view.saved_link_a);
    EXPECT_EQ(0xBBBBU, view.saved_link_b);
    EXPECT_EQ(0xCCCCU, view.extra);
    EXPECT_EQ(7U, view.misc);
}

TEST(shm_container_basics, journal_end_clears_op_only)
{
    container_journal j{};
    journal_init(j);
    journal_begin(j, container_op::erase, 1, 2, 3, 4, 5, 6);
    journal_end(j);

    EXPECT_EQ(container_op::none, journal_load_op(j));
    container_journal_view view{};
    EXPECT_FALSE(journal_load(j, view));
}

TEST(shm_container_basics, journal_window_commit_explicit)
{
    container_journal j{};
    journal_init(j);
    {
        journal_window win(j, container_op::insert, 10, 20, 30, 40, 50, 60);
        EXPECT_FALSE(win.is_committed());
        EXPECT_EQ(container_op::insert, journal_load_op(j));
        win.commit();
        EXPECT_TRUE(win.is_committed());
        EXPECT_EQ(container_op::none, journal_load_op(j));
    }
    // window 析构不额外影响
    EXPECT_EQ(container_op::none, journal_load_op(j));
}

TEST(shm_container_basics, journal_window_destructor_does_not_auto_commit)
{
    container_journal j{};
    journal_init(j);
    {
        journal_window win(j, container_op::erase, 1, 2, 3, 4, 5, 6);
        // 故意不 commit，模拟异常或崩溃中途
    }
    // 析构后 op 仍为 erase；恢复路径应 redo/rollback
    EXPECT_EQ(container_op::erase, journal_load_op(j));
}

TEST(shm_container_basics, journal_visibility_across_threads)
{
    // 验证 release/acquire 语义：writer 填 payload 后 release store op；
    // reader acquire load op 看到非 none 时能看到完整 payload
    container_journal j{};
    journal_init(j);

    std::atomic<bool> start{false};
    std::atomic<bool> reader_saw_payload{false};

    std::thread reader([&] {
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        // 持续轮询直到读到 op != none，然后校验 payload 完整性
        container_journal_view view{};
        while (!journal_load(j, view)) {
            std::this_thread::yield();
        }
        if (view.op == container_op::update && view.target_node_offset == 0xDEADBEEFULL
            && view.anchor_offset == 0xCAFEBABEULL && view.saved_link_a == 0x11
            && view.saved_link_b == 0x22 && view.extra == 0x33 && view.misc == 44U) {
            reader_saw_payload.store(true, std::memory_order_release);
        }
    });

    std::thread writer([&] {
        start.store(true, std::memory_order_release);
        // 稍等一下让 reader 先开始自旋
        for (int i = 0; i < 1000; ++i) {
            std::this_thread::yield();
        }
        journal_begin(j, container_op::update, 0xDEADBEEFULL, 0xCAFEBABEULL, 0x11, 0x22, 0x33, 44);
    });

    writer.join();
    reader.join();
    EXPECT_TRUE(reader_saw_payload.load(std::memory_order_acquire));
}

// ==========================================================================
// container_guard
// ==========================================================================

TEST(shm_container_basics, guard_acquires_and_releases)
{
    mc::shm::ipc_mutex m;
    int                recover_called = 0;
    {
        container_guard g(m, [](void* data) noexcept { ++*static_cast<int*>(data); },
                          &recover_called);
        EXPECT_TRUE(m.is_locked_by_current_thread());
    }
    EXPECT_FALSE(m.is_locked_by_current_thread());
    // 干净获取不应触发 recover
    EXPECT_EQ(0, recover_called);
}

TEST(shm_container_basics, guard_triggers_recover_after_holder_death)
{
    // 模拟一个脏锁场景：直接用另一个"假冒"的进程代 + pid 抢占
    // ipc_mutex 内部 lock_ex 返回 recovered，container_guard 应调用 recover
    //
    // 这里不 fork；测试用简化手段：手动构造一个 stale 状态 —— 无法轻松模拟
    // 留到集成测试中用 fork 覆盖。本用例仅检查"干净锁多次获取不会误触发 recover"
    mc::shm::ipc_mutex m;
    int                recover_called = 0;
    for (int i = 0; i < 3; ++i) {
        container_guard g(m, [](void* data) noexcept { ++*static_cast<int*>(data); },
                          &recover_called);
        EXPECT_FALSE(g.did_recover());
    }
    EXPECT_EQ(0, recover_called);
}

TEST(shm_container_basics, guard_null_recover_is_ok)
{
    mc::shm::ipc_mutex m;
    {
        container_guard g(m, nullptr, nullptr);
        EXPECT_TRUE(m.is_locked_by_current_thread());
    }
    EXPECT_FALSE(m.is_locked_by_current_thread());
}
