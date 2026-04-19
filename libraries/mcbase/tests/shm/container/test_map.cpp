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
#include <cstdint>
#include <map>
#include <random>
#include <vector>

#include <mc/shm/allocator.h>
#include <mc/shm/container/map.h>

using mc::shm::container::container_op;
using mc::shm::container::journal_load_op;
using mc::shm::container::map;
using mc::shm::container::map_control;
using mc::shm::container::map_recover;
using mc::shm::shm_allocator;

namespace {

class shm_map_fixture : public ::testing::Test {
protected:
    static constexpr std::size_t arena_size = 4 * 1024 * 1024;

    void SetUp() override
    {
        m_buffer = std::aligned_alloc(4096, arena_size);
        ASSERT_NE(m_buffer, nullptr);
        ASSERT_TRUE(shm_allocator::initialize(m_buffer, arena_size));
        m_alloc = shm_allocator(m_buffer, arena_size);

        auto* mem = m_alloc.allocate(sizeof(map_control), alignof(map_control));
        ASSERT_NE(mem, nullptr);
        m_control = new (mem) map_control();
        const auto tag = static_cast<std::uint32_t>(static_cast<std::byte*>(mem)
                                                    - static_cast<std::byte*>(m_buffer));
        map<int, int>::init(*m_control, tag);
    }

    void TearDown() override
    {
        m_control->~map_control();
        m_alloc.deallocate(m_control);
        std::free(m_buffer);
    }

    template <typename K, typename V, typename Cmp = std::less<K>>
    std::vector<std::pair<K, V>> collect(map<K, V, Cmp>& m) const
    {
        std::vector<std::pair<K, V>> out;
        for (auto it = m.begin(); it != m.end(); ++it) {
            auto p = *it;
            out.emplace_back(*p.key, *p.value);
        }
        return out;
    }

    void*         m_buffer  = nullptr;
    shm_allocator m_alloc;
    map_control*  m_control = nullptr;
};

} // namespace

TEST_F(shm_map_fixture, empty_on_init)
{
    map<int, int> m(*m_control, m_alloc);
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(0U, m.size());
    EXPECT_EQ(m.begin(), m.end());
    EXPECT_FALSE(m.find(1));
}

TEST_F(shm_map_fixture, try_emplace_and_find)
{
    map<int, int> m(*m_control, m_alloc);
    auto [p, inserted] = m.try_emplace(42, 100);
    ASSERT_TRUE(inserted);
    ASSERT_NE(p.key, nullptr);
    ASSERT_NE(p.value, nullptr);
    EXPECT_EQ(42, *p.key);
    EXPECT_EQ(100, *p.value);

    auto q = m.find(42);
    ASSERT_TRUE(q);
    EXPECT_EQ(42, *q.key);
    EXPECT_EQ(100, *q.value);

    EXPECT_FALSE(m.find(43));
}

TEST_F(shm_map_fixture, try_emplace_duplicate_returns_false_and_existing)
{
    map<int, int> m(*m_control, m_alloc);
    auto [p1, i1] = m.try_emplace(7, 111);
    EXPECT_TRUE(i1);
    auto [p2, i2] = m.try_emplace(7, 999);
    EXPECT_FALSE(i2);
    EXPECT_EQ(p1.key, p2.key);
    EXPECT_EQ(p1.value, p2.value);
    EXPECT_EQ(111, *p2.value); // value 未被改写
    EXPECT_EQ(1U, m.size());
}

TEST_F(shm_map_fixture, ordered_iteration)
{
    map<int, int>    m(*m_control, m_alloc);
    const std::vector<std::pair<int, int>> inputs = {
        {5, 50}, {2, 20}, {9, 90}, {1, 10}, {7, 70},
    };
    for (auto& kv : inputs) {
        m.try_emplace(kv.first, kv.second);
    }
    EXPECT_EQ(inputs.size(), m.size());
    auto collected = collect(m);
    auto sorted    = inputs;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(sorted, collected);
}

TEST_F(shm_map_fixture, mutate_value_through_find)
{
    map<int, int> m(*m_control, m_alloc);
    m.try_emplace(1, 10);
    auto p = m.find(1);
    ASSERT_TRUE(p);
    *p.value = 42;

    auto q = m.find(1);
    ASSERT_TRUE(q);
    EXPECT_EQ(42, *q.value);
}

TEST_F(shm_map_fixture, erase_nonexistent_returns_false)
{
    map<int, int> m(*m_control, m_alloc);
    m.try_emplace(1, 1);
    m.try_emplace(2, 2);
    EXPECT_FALSE(m.erase(100));
    EXPECT_EQ(2U, m.size());
}

TEST_F(shm_map_fixture, erase_head_tail_middle)
{
    map<int, int> m(*m_control, m_alloc);
    for (int i = 1; i <= 5; ++i) {
        m.try_emplace(i, i * 10);
    }
    EXPECT_TRUE(m.erase(1));
    EXPECT_TRUE(m.erase(5));
    EXPECT_TRUE(m.erase(3));
    auto collected = collect(m);
    const std::vector<std::pair<int, int>> expected = {{2, 20}, {4, 40}};
    EXPECT_EQ(expected, collected);
    EXPECT_EQ(2U, m.size());
}

TEST_F(shm_map_fixture, clear_empties)
{
    map<int, int> m(*m_control, m_alloc);
    for (int i = 0; i < 50; ++i) {
        m.try_emplace(i, i);
    }
    EXPECT_EQ(50U, m.size());
    m.clear();
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.begin(), m.end());
}

TEST_F(shm_map_fixture, journal_clean_after_ops)
{
    map<int, int> m(*m_control, m_alloc);
    m.try_emplace(1, 1);
    EXPECT_EQ(container_op::none, journal_load_op(m_control->journal));
    m.try_emplace(2, 2);
    EXPECT_EQ(container_op::none, journal_load_op(m_control->journal));
    m.erase(1);
    EXPECT_EQ(container_op::none, journal_load_op(m_control->journal));
}

TEST_F(shm_map_fixture, recover_no_pending_is_idempotent)
{
    map<int, int> m(*m_control, m_alloc);
    for (int v : {3, 1, 4, 1, 5, 9, 2, 6}) {
        m.try_emplace(v, v * 2);
    }
    const auto before        = collect(m);
    const auto before_size   = m.size();
    const auto before_degraded = m.degraded_count();

    map_recover(*m_control, [](const void* a, const void* b, std::size_t) -> int {
        const int ka = *static_cast<const int*>(a);
        const int kb = *static_cast<const int*>(b);
        return ka < kb ? -1 : (ka > kb ? 1 : 0);
    });

    EXPECT_EQ(before, collect(m));
    EXPECT_EQ(before_size, m.size());
    EXPECT_EQ(before_degraded, m.degraded_count());
}

TEST_F(shm_map_fixture, stress_random_matches_std_map)
{
    map<int, int>   m(*m_control, m_alloc);
    std::map<int, int> ref;
    std::mt19937    rng(0xC0FFEE);
    std::uniform_int_distribution<int> key_dist(0, 300);

    for (int iter = 0; iter < 2000; ++iter) {
        const int key = key_dist(rng);
        const int op  = rng() % 4;
        if (op < 3) {
            auto [p, ins]      = m.try_emplace(key, key * 7);
            const bool ref_ins = ref.insert({key, key * 7}).second;
            EXPECT_EQ(ref_ins, ins) << "iter=" << iter;
            ASSERT_TRUE(p);
        } else {
            const bool got   = m.erase(key);
            const bool ref_e = ref.erase(key) > 0;
            EXPECT_EQ(ref_e, got) << "iter=" << iter;
        }
        ASSERT_EQ(ref.size(), m.size());
    }

    std::vector<std::pair<int, int>> got;
    for (auto it = m.begin(); it != m.end(); ++it) {
        auto p = *it;
        got.emplace_back(*p.key, *p.value);
    }
    std::vector<std::pair<int, int>> expected(ref.begin(), ref.end());
    EXPECT_EQ(expected, got);
    EXPECT_EQ(0U, m.degraded_count());
}

TEST_F(shm_map_fixture, pod_struct_value)
{
    struct payload {
        std::uint32_t a;
        std::uint32_t b;
        bool operator==(const payload& o) const noexcept { return a == o.a && b == o.b; }
    };

    m_control->~map_control();
    m_control = new (m_control) map_control();
    const auto tag = static_cast<std::uint32_t>(static_cast<std::byte*>(
                                                    static_cast<void*>(m_control))
                                                - static_cast<std::byte*>(m_buffer));
    map<int, payload>::init(*m_control, tag);

    map<int, payload> m(*m_control, m_alloc);
    m.try_emplace(1, payload{10, 20});
    m.try_emplace(2, payload{30, 40});
    auto p = m.find(2);
    ASSERT_TRUE(p);
    EXPECT_EQ(30U, p.value->a);
    EXPECT_EQ(40U, p.value->b);
    EXPECT_TRUE(m.erase(1));
    EXPECT_EQ(1U, m.size());
}
