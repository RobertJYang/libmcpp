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
#include <array>
#include <cstdint>
#include <vector>

#include <mc/shm/allocator.h>
#include <mc/shm/container/list.h>

using mc::shm::container::container_op;
using mc::shm::container::journal_begin;
using mc::shm::container::journal_load_op;
using mc::shm::container::list;
using mc::shm::container::list_control;
using mc::shm::container::list_recover;
using mc::shm::container::node_header_load_state;
using mc::shm::container::node_state;
using mc::shm::ipc_mutex;
using mc::shm::shm_allocator;

namespace {

// 单进程测试 fixture：在堆上建一个模拟 arena，其中放置 list_control
class shm_list_fixture : public ::testing::Test {
protected:
    static constexpr std::size_t arena_size = 4 * 1024 * 1024; // 4MB

    void SetUp() override
    {
        m_buffer = std::aligned_alloc(4096, arena_size);
        ASSERT_NE(m_buffer, nullptr);
        ASSERT_TRUE(shm_allocator::initialize(m_buffer, arena_size));
        m_alloc = shm_allocator(m_buffer, arena_size);

        // 在 arena 内分配 list_control
        auto* mem = m_alloc.allocate(sizeof(list_control), alignof(list_control));
        ASSERT_NE(mem, nullptr);
        m_control = new (mem) list_control();
        // self_tag 用 control 在 arena 内的 offset 低 32 bit
        const auto tag = static_cast<std::uint32_t>(static_cast<std::byte*>(mem)
                                                    - static_cast<std::byte*>(m_buffer));
        list<int>::init(*m_control, tag);
    }

    void TearDown() override
    {
        m_control->~list_control();
        m_alloc.deallocate(m_control);
        std::free(m_buffer);
    }

    std::vector<int> collect(list<int>& l) const
    {
        std::vector<int> values;
        for (auto it = l.begin(); it != l.end(); ++it) {
            values.push_back(*it);
        }
        return values;
    }

    void*          m_buffer = nullptr;
    shm_allocator  m_alloc;
    list_control*  m_control = nullptr;
};

} // namespace

TEST_F(shm_list_fixture, empty_on_init)
{
    list<int> l(*m_control, m_alloc);
    EXPECT_TRUE(l.empty());
    EXPECT_EQ(0U, l.size());
    EXPECT_EQ(l.begin(), l.end());
}

TEST_F(shm_list_fixture, push_back_sequence)
{
    list<int> l(*m_control, m_alloc);
    ASSERT_NE(nullptr, l.push_back(1));
    ASSERT_NE(nullptr, l.push_back(2));
    ASSERT_NE(nullptr, l.push_back(3));

    EXPECT_EQ(3U, l.size());
    EXPECT_EQ(std::vector<int>({1, 2, 3}), collect(l));
}

TEST_F(shm_list_fixture, push_front_reverses_order)
{
    list<int> l(*m_control, m_alloc);
    l.push_front(1);
    l.push_front(2);
    l.push_front(3);

    EXPECT_EQ(std::vector<int>({3, 2, 1}), collect(l));
}

TEST_F(shm_list_fixture, mixed_push)
{
    list<int> l(*m_control, m_alloc);
    l.push_back(10);    // [10]
    l.push_front(5);    // [5, 10]
    l.push_back(20);    // [5, 10, 20]
    l.push_front(1);    // [1, 5, 10, 20]
    EXPECT_EQ(std::vector<int>({1, 5, 10, 20}), collect(l));
    EXPECT_EQ(4U, l.size());
}

TEST_F(shm_list_fixture, pop_front_and_back)
{
    list<int> l(*m_control, m_alloc);
    for (int i = 1; i <= 5; ++i) {
        l.push_back(i);
    }

    EXPECT_TRUE(l.pop_front());
    EXPECT_EQ(std::vector<int>({2, 3, 4, 5}), collect(l));

    EXPECT_TRUE(l.pop_back());
    EXPECT_EQ(std::vector<int>({2, 3, 4}), collect(l));

    EXPECT_TRUE(l.pop_front());
    EXPECT_TRUE(l.pop_front());
    EXPECT_TRUE(l.pop_back());
    EXPECT_TRUE(l.empty());
    EXPECT_FALSE(l.pop_front());
    EXPECT_FALSE(l.pop_back());
}

TEST_F(shm_list_fixture, erase_middle_node)
{
    list<int> l(*m_control, m_alloc);
    auto*     a = l.push_back(1);
    auto*     b = l.push_back(2);
    auto*     c = l.push_back(3);
    (void)a;
    (void)c;

    EXPECT_TRUE(l.erase(b));
    EXPECT_EQ(std::vector<int>({1, 3}), collect(l));
    EXPECT_EQ(2U, l.size());
}

TEST_F(shm_list_fixture, erase_head_and_tail)
{
    list<int> l(*m_control, m_alloc);
    auto*     a = l.push_back(1);
    l.push_back(2);
    auto* c = l.push_back(3);

    EXPECT_TRUE(l.erase(a));
    EXPECT_EQ(std::vector<int>({2, 3}), collect(l));

    EXPECT_TRUE(l.erase(c));
    EXPECT_EQ(std::vector<int>({2}), collect(l));
}

TEST_F(shm_list_fixture, iterator_forward)
{
    list<int> l(*m_control, m_alloc);
    for (int i = 0; i < 10; ++i) {
        l.push_back(i);
    }
    int expected = 0;
    for (auto& v : l) {
        EXPECT_EQ(expected++, v);
    }
    EXPECT_EQ(10, expected);
}

TEST_F(shm_list_fixture, journal_clean_after_push)
{
    list<int> l(*m_control, m_alloc);
    l.push_back(1);
    EXPECT_EQ(container_op::none, journal_load_op(m_control->journal));
    l.push_back(2);
    EXPECT_EQ(container_op::none, journal_load_op(m_control->journal));
    l.pop_front();
    EXPECT_EQ(container_op::none, journal_load_op(m_control->journal));
}

TEST_F(shm_list_fixture, node_state_linked_after_push)
{
    list<int> l(*m_control, m_alloc);
    auto* value = l.push_back(42);
    ASSERT_NE(value, nullptr);

    // value 前 32 字节是 list_node_common
    auto* header = reinterpret_cast<mc::shm::container::node_header*>(
        reinterpret_cast<std::byte*>(value) - 32);
    EXPECT_EQ(node_state::linked, node_header_load_state(*header));
}

// ==========================================================================
// recover 测试：直接注入"脏 journal"模拟崩溃后的状态
// ==========================================================================

TEST_F(shm_list_fixture, recover_rollback_pending_insert)
{
    list<int> l(*m_control, m_alloc);
    l.push_back(1);
    l.push_back(2);
    l.push_back(3);

    // 模拟：一个新节点 allocate + header init，但 journal 打到 pending，链接尚未完成
    //       (=> rollback 期望：新节点不出现在链表里)
    auto* tail      = reinterpret_cast<mc::shm::container::detail::list_node_common*>(
        static_cast<std::byte*>(m_buffer) + /*dummy*/ 0);
    (void)tail;

    void* mem = m_alloc.allocate(list<int>::node_size, list<int>::node_align);
    ASSERT_NE(mem, nullptr);
    auto* new_node = static_cast<mc::shm::container::detail::list_node_common*>(mem);
    mc::shm::container::node_header_init(new_node->header,
                                         mc::shm::container::list_node_magic,
                                         m_control->self_tag);
    new_node->prev_offset = 0;
    new_node->next_offset = 0;

    // 手动把节点打到 pending，并写一个未提交的 journal（模拟崩溃中途）
    mc::shm::container::node_header_transition(new_node->header, node_state::pending);

    const auto new_off = mc::shm::container::detail::self_offset_from(m_control, new_node);
    // saved_a = 原 tail_offset（head_offset 不变）；这里 anchor = 旧 tail
    const auto anchor_prev_off = m_control->tail_offset;
    const auto anchor_next_off = 0; // 原 tail 的 next 是 0
    const auto saved_a         = anchor_next_off; // 旧 tail.next = 0
    const auto saved_b         = anchor_prev_off; // 旧 tail（control.tail_offset）

    // misc: flag_at_tail（anchor_next_off == 0）
    const std::uint32_t flags = 0x2U;
    journal_begin(m_control->journal, container_op::insert,
                  static_cast<std::uint64_t>(new_off),
                  static_cast<std::uint64_t>(anchor_prev_off),
                  static_cast<std::uint64_t>(saved_a), static_cast<std::uint64_t>(saved_b),
                  static_cast<std::uint64_t>(anchor_next_off), flags);

    // 注意：这里刻意不做链接修改，模拟"Step B 执行到一半崩溃"
    // 此时 tail 的 next 还是 0（未改），head 也未改

    EXPECT_EQ(container_op::insert, journal_load_op(m_control->journal));

    // 触发 recover
    list_recover(*m_control);

    // 期望：rollback 后 journal 清零，链表保持 [1,2,3]
    EXPECT_EQ(container_op::none, journal_load_op(m_control->journal));
    EXPECT_EQ(std::vector<int>({1, 2, 3}), collect(l));
    EXPECT_EQ(node_state::degraded, node_header_load_state(new_node->header));
    EXPECT_GE(l.degraded_count(), 1U);

    // cleanup：把 leaked 节点的内存释放掉以免 TearDown 时 arena 不一致
    m_alloc.deallocate(new_node);
}

TEST_F(shm_list_fixture, recover_rollback_pending_erase)
{
    list<int> l(*m_control, m_alloc);
    auto*     v1 = l.push_back(1);
    auto*     v2 = l.push_back(2);
    auto*     v3 = l.push_back(3);
    (void)v1;
    (void)v3;

    // 手动模拟：对 v2 做到 "Step B 刚写完 journal、链接还没改"
    auto* node = reinterpret_cast<mc::shm::container::detail::list_node_common*>(
        reinterpret_cast<std::byte*>(v2) - 32);
    const auto target_off = mc::shm::container::detail::self_offset_from(m_control, node);
    const auto prev_off   = node->prev_offset;
    const auto next_off   = node->next_offset;

    mc::shm::container::node_header_transition(node->header, node_state::pending);
    journal_begin(m_control->journal, container_op::erase,
                  static_cast<std::uint64_t>(target_off),
                  static_cast<std::uint64_t>(prev_off),
                  static_cast<std::uint64_t>(prev_off), // saved_a = prev
                  static_cast<std::uint64_t>(next_off), // saved_b = next
                  /*extra=*/0, /*flags=*/0);

    // recover 应 rollback → v2 回到 linked 状态
    list_recover(*m_control);

    EXPECT_EQ(container_op::none, journal_load_op(m_control->journal));
    EXPECT_EQ(std::vector<int>({1, 2, 3}), collect(l));
    EXPECT_EQ(node_state::linked, node_header_load_state(node->header));
}

TEST_F(shm_list_fixture, recover_finish_linked_insert)
{
    list<int> l(*m_control, m_alloc);
    l.push_back(1);
    l.push_back(2);

    // 模拟：节点已经 linked、size 也 +1，只有 journal 没来得及 end
    void* mem = m_alloc.allocate(list<int>::node_size, list<int>::node_align);
    ASSERT_NE(mem, nullptr);
    auto* new_node = static_cast<mc::shm::container::detail::list_node_common*>(mem);
    mc::shm::container::node_header_init(new_node->header,
                                         mc::shm::container::list_node_magic,
                                         m_control->self_tag);
    const auto new_off = mc::shm::container::detail::self_offset_from(m_control, new_node);

    // 先用常规 insert 完成所有链接（这已经是完整 push_back 等价）
    mc::shm::container::detail::list_insert_after(*m_control, m_control->tail_offset, new_node);
    new (reinterpret_cast<std::byte*>(new_node) + 32) int(3);

    // 再人为重写一个 "未 end 的 journal"
    journal_begin(m_control->journal, container_op::insert,
                  static_cast<std::uint64_t>(new_off), 0, 0, 0, 0, 0);
    // node 已经是 linked

    list_recover(*m_control);

    EXPECT_EQ(container_op::none, journal_load_op(m_control->journal));
    EXPECT_EQ(std::vector<int>({1, 2, 3}), collect(l));
    EXPECT_EQ(node_state::linked, node_header_load_state(new_node->header));
}

TEST_F(shm_list_fixture, fsck_isolates_corrupted_node)
{
    list<int> l(*m_control, m_alloc);
    l.push_back(1);
    auto* v2 = l.push_back(2);
    l.push_back(3);

    // 破坏 v2 的 header magic
    auto* node = reinterpret_cast<mc::shm::container::detail::list_node_common*>(
        reinterpret_cast<std::byte*>(v2) - 32);
    node->header.magic ^= 0x1ULL;

    // 触发 recover → fsck 应 isolate v2
    list_recover(*m_control);

    // v2 应该不再出现在遍历中
    auto values = collect(l);
    EXPECT_EQ(2U, values.size());
    EXPECT_EQ(std::vector<int>({1, 3}), values);
    EXPECT_GE(l.degraded_count(), 1U);
}

TEST_F(shm_list_fixture, many_push_pop_stable)
{
    list<int> l(*m_control, m_alloc);
    for (int i = 0; i < 500; ++i) {
        l.push_back(i);
    }
    EXPECT_EQ(500U, l.size());
    int expected = 0;
    for (auto& v : l) {
        EXPECT_EQ(expected++, v);
    }

    // 弹一半
    for (int i = 0; i < 250; ++i) {
        EXPECT_TRUE(l.pop_front());
    }
    EXPECT_EQ(250U, l.size());

    expected = 250;
    for (auto& v : l) {
        EXPECT_EQ(expected++, v);
    }
}
