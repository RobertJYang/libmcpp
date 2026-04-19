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
#include <random>
#include <set>
#include <vector>

#include <mc/shm/allocator.h>
#include <mc/shm/container/set.h>

using mc::shm::container::container_op;
using mc::shm::container::journal_load_op;
using mc::shm::container::node_header_load_state;
using mc::shm::container::node_state;
using mc::shm::container::set;
using mc::shm::container::set_control;
using mc::shm::container::set_max_level;
using mc::shm::container::set_recover;
using mc::shm::shm_allocator;

namespace {

class shm_set_fixture : public ::testing::Test {
protected:
    static constexpr std::size_t arena_size = 4 * 1024 * 1024;

    void SetUp() override
    {
        m_buffer = std::aligned_alloc(4096, arena_size);
        ASSERT_NE(m_buffer, nullptr);
        ASSERT_TRUE(shm_allocator::initialize(m_buffer, arena_size));
        m_alloc = shm_allocator(m_buffer, arena_size);

        auto* mem = m_alloc.allocate(sizeof(set_control), alignof(set_control));
        ASSERT_NE(mem, nullptr);
        m_control  = new (mem) set_control();
        const auto tag = static_cast<std::uint32_t>(static_cast<std::byte*>(mem)
                                                    - static_cast<std::byte*>(m_buffer));
        set<int>::init(*m_control, tag);
    }

    void TearDown() override
    {
        m_control->~set_control();
        m_alloc.deallocate(m_control);
        std::free(m_buffer);
    }

    template <typename Key, typename Compare = std::less<Key>>
    std::vector<Key> collect(set<Key, Compare>& s) const
    {
        std::vector<Key> out;
        for (auto it = s.begin(); it != s.end(); ++it) {
            out.push_back(*it);
        }
        return out;
    }

    void*           m_buffer  = nullptr;
    shm_allocator   m_alloc;
    set_control*    m_control = nullptr;
};

} // namespace

TEST_F(shm_set_fixture, empty_on_init)
{
    set<int> s(*m_control, m_alloc);
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(0U, s.size());
    EXPECT_EQ(s.begin(), s.end());
    EXPECT_EQ(nullptr, s.find(42));
}

TEST_F(shm_set_fixture, single_insert_find_erase)
{
    set<int> s(*m_control, m_alloc);
    auto [p, inserted] = s.insert(42);
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(inserted);
    EXPECT_EQ(42, *p);
    EXPECT_EQ(1U, s.size());

    EXPECT_NE(nullptr, s.find(42));
    EXPECT_EQ(nullptr, s.find(43));

    EXPECT_TRUE(s.erase(42));
    EXPECT_EQ(0U, s.size());
    EXPECT_EQ(nullptr, s.find(42));
    EXPECT_FALSE(s.erase(42));
}

TEST_F(shm_set_fixture, duplicate_insert_returns_false)
{
    set<int> s(*m_control, m_alloc);
    auto [p1, ins1] = s.insert(7);
    EXPECT_TRUE(ins1);
    auto [p2, ins2] = s.insert(7);
    EXPECT_FALSE(ins2);
    EXPECT_EQ(p1, p2); // 同一个节点
    EXPECT_EQ(1U, s.size());
}

TEST_F(shm_set_fixture, ordered_iteration)
{
    set<int>                          s(*m_control, m_alloc);
    const std::vector<int>            input = {5, 2, 9, 1, 7, 3, 8, 4, 6, 0};
    for (auto v : input) {
        s.insert(v);
    }
    EXPECT_EQ(input.size(), s.size());

    const auto values = collect(s);
    ASSERT_EQ(input.size(), values.size());
    EXPECT_TRUE(std::is_sorted(values.begin(), values.end()));
    std::vector<int> sorted = input;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(sorted, values);
}

TEST_F(shm_set_fixture, reverse_order_iteration_via_std_less_greater)
{
    // 用 std::greater<int> 作为 Compare，应该从大到小
    // 独立的 control，避免复用 set<int> 的 control（key_size 相同但 Compare 不同无所谓）
    set<int, std::greater<int>> s(*m_control, m_alloc);
    const std::vector<int>      input = {5, 2, 9, 1, 7};
    for (auto v : input) {
        s.insert(v);
    }

    std::vector<int> collected;
    for (auto it = s.begin(); it != s.end(); ++it) {
        collected.push_back(*it);
    }
    EXPECT_EQ(std::vector<int>({9, 7, 5, 2, 1}), collected);
}

TEST_F(shm_set_fixture, erase_nonexistent_returns_false)
{
    set<int> s(*m_control, m_alloc);
    s.insert(1);
    s.insert(2);
    EXPECT_FALSE(s.erase(100));
    EXPECT_EQ(2U, s.size());
    EXPECT_EQ(std::vector<int>({1, 2}), collect(s));
}

TEST_F(shm_set_fixture, erase_head_and_tail_and_middle)
{
    set<int> s(*m_control, m_alloc);
    for (int v : {1, 2, 3, 4, 5}) {
        s.insert(v);
    }
    EXPECT_TRUE(s.erase(1));
    EXPECT_EQ(std::vector<int>({2, 3, 4, 5}), collect(s));
    EXPECT_TRUE(s.erase(5));
    EXPECT_EQ(std::vector<int>({2, 3, 4}), collect(s));
    EXPECT_TRUE(s.erase(3));
    EXPECT_EQ(std::vector<int>({2, 4}), collect(s));
    EXPECT_TRUE(s.erase(2));
    EXPECT_TRUE(s.erase(4));
    EXPECT_TRUE(s.empty());
}

TEST_F(shm_set_fixture, journal_clean_after_ops)
{
    set<int> s(*m_control, m_alloc);
    s.insert(1);
    EXPECT_EQ(container_op::none, journal_load_op(m_control->journal));
    s.insert(2);
    EXPECT_EQ(container_op::none, journal_load_op(m_control->journal));
    s.erase(1);
    EXPECT_EQ(container_op::none, journal_load_op(m_control->journal));
}

TEST_F(shm_set_fixture, node_state_linked_after_insert)
{
    set<int> s(*m_control, m_alloc);
    auto [p, inserted] = s.insert(123);
    ASSERT_TRUE(inserted);
    ASSERT_NE(p, nullptr);

    // 从 key_ptr 反推到 set_node_common 的 header 需走一次 find
    // 这里直接用 begin() 拿到的 iterator（node accessor 暴露在 iterator 上）
    auto it = s.begin();
    ASSERT_NE(it.node(), nullptr);
    EXPECT_EQ(node_state::linked, node_header_load_state(it.node()->header));
}

TEST_F(shm_set_fixture, stress_random_insert_erase_matches_std_set)
{
    set<int>        s(*m_control, m_alloc);
    std::set<int>   ref;
    std::mt19937    rng(0xC0FFEE);
    std::uniform_int_distribution<int> key_dist(0, 300);

    for (int iter = 0; iter < 2000; ++iter) {
        const int key = key_dist(rng);
        const int op  = rng() % 3; // 0/1 => insert, 2 => erase
        if (op < 2) {
            auto [p, ins]      = s.insert(key);
            const bool ref_ins = ref.insert(key).second;
            EXPECT_EQ(ref_ins, ins) << "iter=" << iter << " key=" << key;
            ASSERT_NE(p, nullptr);
        } else {
            const bool got = s.erase(key);
            const bool ref_e = ref.erase(key) > 0;
            EXPECT_EQ(ref_e, got) << "iter=" << iter << " key=" << key;
        }
        ASSERT_EQ(ref.size(), s.size());
    }

    // 遍历一致
    std::vector<int> got;
    for (auto v : s) {
        got.push_back(v);
    }
    EXPECT_TRUE(std::is_sorted(got.begin(), got.end()));
    EXPECT_EQ(std::vector<int>(ref.begin(), ref.end()), got);
    EXPECT_EQ(0U, s.degraded_count());
}

TEST_F(shm_set_fixture, recover_no_pending_is_idempotent)
{
    set<int> s(*m_control, m_alloc);
    for (int v : {3, 1, 4, 1, 5, 9, 2, 6}) {
        s.insert(v);
    }
    const auto before        = collect(s);
    const auto before_size   = s.size();
    const auto before_degraded = s.degraded_count();

    // 没有未提交 journal，recover 应该是幂等的
    set_recover(*m_control, [](const void* a, const void* b, std::size_t) -> int {
        const int ka = *static_cast<const int*>(a);
        const int kb = *static_cast<const int*>(b);
        return ka < kb ? -1 : (ka > kb ? 1 : 0);
    });

    EXPECT_EQ(before, collect(s));
    EXPECT_EQ(before_size, s.size());
    EXPECT_EQ(before_degraded, s.degraded_count());
}

TEST_F(shm_set_fixture, fsck_isolates_corrupted_node)
{
    set<int> s(*m_control, m_alloc);
    s.insert(1);
    s.insert(2);
    s.insert(3);

    // 找到 key==2 的节点并破坏其 header magic
    const int*  key_ptr = s.find(2);
    ASSERT_NE(key_ptr, nullptr);
    // key 在 set_node_common 后面（偏移依赖 level，使用 set_node_key_ptr 的反向会比较麻烦）
    // 简单做法：遍历并对比地址
    mc::shm::container::detail::set_node_common* target = nullptr;
    for (auto it = s.begin(); it != s.end(); ++it) {
        if (*it == 2) {
            target = it.node();
            break;
        }
    }
    ASSERT_NE(target, nullptr);
    target->header.magic ^= 0x1ULL;

    set_recover(*m_control, [](const void* a, const void* b, std::size_t) -> int {
        const int ka = *static_cast<const int*>(a);
        const int kb = *static_cast<const int*>(b);
        return ka < kb ? -1 : (ka > kb ? 1 : 0);
    });

    const auto values = collect(s);
    EXPECT_EQ(std::vector<int>({1, 3}), values);
    EXPECT_GE(s.degraded_count(), 1U);
}

TEST_F(shm_set_fixture, many_insert_then_erase_all)
{
    set<int> s(*m_control, m_alloc);
    for (int i = 0; i < 500; ++i) {
        auto [p, ins] = s.insert(i);
        ASSERT_TRUE(ins);
        ASSERT_NE(p, nullptr);
    }
    EXPECT_EQ(500U, s.size());
    int expected = 0;
    for (auto v : s) {
        EXPECT_EQ(expected++, v);
    }
    for (int i = 0; i < 500; ++i) {
        EXPECT_TRUE(s.erase(i)) << "i=" << i;
    }
    EXPECT_TRUE(s.empty());
}

TEST_F(shm_set_fixture, struct_pod_key)
{
    struct pod {
        std::uint32_t a;
        std::uint32_t b;
    };
    struct pod_less {
        bool operator()(const pod& x, const pod& y) const noexcept
        {
            if (x.a != y.a) return x.a < y.a;
            return x.b < y.b;
        }
    };

    // 重新初始化 control（key_size 与 int 不同）
    m_control->~set_control();
    m_control = new (m_control) set_control();
    const auto tag = static_cast<std::uint32_t>(static_cast<std::byte*>(
                                                    static_cast<void*>(m_control))
                                                - static_cast<std::byte*>(m_buffer));
    set<pod, pod_less>::init(*m_control, tag);

    set<pod, pod_less> s(*m_control, m_alloc);
    s.insert({2, 1});
    s.insert({1, 2});
    s.insert({2, 0});
    s.insert({1, 1});
    EXPECT_EQ(4U, s.size());
    std::vector<std::pair<std::uint32_t, std::uint32_t>> got;
    for (auto& k : s) {
        got.emplace_back(k.a, k.b);
    }
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> expected = {
        {1, 1}, {1, 2}, {2, 0}, {2, 1},
    };
    EXPECT_EQ(expected, got);

    EXPECT_TRUE(s.erase({1, 2}));
    EXPECT_NE(s.find({2, 0}), nullptr);
    EXPECT_EQ(s.find({1, 2}), nullptr);
}
