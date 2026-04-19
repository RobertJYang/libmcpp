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
#include <cstdio>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

#include <mc/shm/allocator.h>
#include <mc/shm/container/map.h>
#include <mc/shm/container/set.h>
#include <mc/shm/region.h>
#include <mc/shm/string.h>

using mc::shm::shm_allocator;
using mc::shm::shm_region;
using mc::shm::shm_region_options;
using mc::shm::string;
using mc::shm::string_less;
using mc::shm::container::map;
using mc::shm::container::map_control;
using mc::shm::container::set;
using mc::shm::container::set_control;

namespace {

// -------------------------------------------------------------
// set<int> range queries
// -------------------------------------------------------------
class shm_set_range_fixture : public ::testing::Test {
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

    void fill_with(set<int>& s, std::initializer_list<int> xs)
    {
        for (int v : xs) s.insert(v);
    }

    void*         m_buffer  = nullptr;
    shm_allocator m_alloc;
    set_control*  m_control = nullptr;
};

} // namespace

// -------------------------------------------------------------
// set: empty / boundary
// -------------------------------------------------------------
TEST_F(shm_set_range_fixture, lower_upper_on_empty_returns_end)
{
    set<int> s(*m_control, m_alloc);
    EXPECT_TRUE(s.lower_bound(10) == s.cend());
    EXPECT_TRUE(s.upper_bound(10) == s.cend());
    auto range = s.equal_range(42);
    EXPECT_TRUE(range.first == s.cend());
    EXPECT_TRUE(range.second == s.cend());
}

TEST_F(shm_set_range_fixture, lower_key_exists_returns_iterator_to_key)
{
    set<int> s(*m_control, m_alloc);
    fill_with(s, {1, 3, 5, 7, 9});

    auto lb = s.lower_bound(5);
    ASSERT_TRUE(lb != s.cend());
    EXPECT_EQ(*lb, 5);

    auto ub = s.upper_bound(5);
    ASSERT_TRUE(ub != s.cend());
    EXPECT_EQ(*ub, 7);
}

TEST_F(shm_set_range_fixture, lower_key_missing_returns_first_greater)
{
    set<int> s(*m_control, m_alloc);
    fill_with(s, {1, 3, 5, 7, 9});

    auto lb = s.lower_bound(4);
    ASSERT_TRUE(lb != s.cend());
    EXPECT_EQ(*lb, 5);

    auto ub = s.upper_bound(4);
    ASSERT_TRUE(ub != s.cend());
    EXPECT_EQ(*ub, 5);

    auto range = s.equal_range(4);
    EXPECT_TRUE(range.first == range.second); // key 不存在，空区间
}

TEST_F(shm_set_range_fixture, lower_key_smaller_than_all_returns_begin)
{
    set<int> s(*m_control, m_alloc);
    fill_with(s, {10, 20, 30});

    auto lb = s.lower_bound(5);
    ASSERT_TRUE(lb == s.cbegin());
    EXPECT_EQ(*lb, 10);
}

TEST_F(shm_set_range_fixture, lower_key_greater_than_all_returns_end)
{
    set<int> s(*m_control, m_alloc);
    fill_with(s, {10, 20, 30});

    EXPECT_TRUE(s.lower_bound(100) == s.cend());
    EXPECT_TRUE(s.upper_bound(100) == s.cend());
}

TEST_F(shm_set_range_fixture, equal_range_on_existing_key_is_single_element)
{
    set<int> s(*m_control, m_alloc);
    fill_with(s, {1, 3, 5, 7});

    auto [lo, hi] = s.equal_range(3);
    ASSERT_TRUE(lo != s.cend());
    EXPECT_EQ(*lo, 3);
    ASSERT_TRUE(hi != s.cend());
    EXPECT_EQ(*hi, 5);

    int  count = 0;
    for (auto it = lo; it != hi; ++it) ++count;
    EXPECT_EQ(count, 1);
}

TEST_F(shm_set_range_fixture, iterate_range_lower_to_upper)
{
    set<int> s(*m_control, m_alloc);
    for (int i = 0; i < 100; ++i) s.insert(i);

    // [30, 50)
    std::vector<int> collected;
    for (auto it = s.lower_bound(30); it != s.lower_bound(50); ++it) {
        collected.push_back(*it);
    }
    ASSERT_EQ(collected.size(), 20U);
    for (std::size_t i = 0; i < collected.size(); ++i) {
        EXPECT_EQ(collected[i], static_cast<int>(30 + i));
    }
}

TEST_F(shm_set_range_fixture, random_vs_std_set_consistency)
{
    set<int> s(*m_control, m_alloc);
    std::set<int> ref;
    std::mt19937  rng(12345);

    for (int i = 0; i < 1000; ++i) {
        int v = static_cast<int>(rng() % 5000);
        if (ref.insert(v).second) {
            s.insert(v);
        }
    }

    for (int q = 0; q < 100; ++q) {
        int key = static_cast<int>(rng() % 6000);
        auto ref_lb = ref.lower_bound(key);
        auto shm_lb = s.lower_bound(key);
        if (ref_lb == ref.end()) {
            EXPECT_TRUE(shm_lb == s.cend()) << "key=" << key;
        } else {
            ASSERT_TRUE(shm_lb != s.cend()) << "key=" << key;
            EXPECT_EQ(*shm_lb, *ref_lb) << "key=" << key;
        }

        auto ref_ub = ref.upper_bound(key);
        auto shm_ub = s.upper_bound(key);
        if (ref_ub == ref.end()) {
            EXPECT_TRUE(shm_ub == s.cend());
        } else {
            ASSERT_TRUE(shm_ub != s.cend());
            EXPECT_EQ(*shm_ub, *ref_ub);
        }
    }
}

// -------------------------------------------------------------
// set<shm::string, string_less> heterogeneous range (with region)
// -------------------------------------------------------------
namespace {

class shm_set_string_range_fixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::random_device rd;
        std::mt19937       rng(rd());
        char               name[128];
        std::snprintf(name, sizeof(name), "mc_shm_set_range_%d_%u", ::getpid(), rng());

        shm_region_options opts;
        opts.segment_name  = mc::string(name);
        opts.total_size    = 4 * 1024 * 1024;
        opts.root_capacity = 16;

        m_region = shm_region(opts);
        ASSERT_TRUE(m_region.is_valid());
        m_alloc = m_region.user_arena();
        ASSERT_TRUE(m_alloc.is_valid());

        auto* mem = m_alloc.allocate(sizeof(set_control), alignof(set_control));
        ASSERT_NE(mem, nullptr);
        m_control  = new (mem) set_control();
        const auto tag = static_cast<std::uint32_t>(
            static_cast<std::byte*>(mem) - static_cast<std::byte*>(m_region.user_base()));
        set<string, string_less>::init(*m_control, tag);
    }

    void TearDown() override
    {
        {
            set<string, string_less> s(*m_control, m_alloc);
            s.clear();
        }
        m_control->~set_control();
        m_alloc.deallocate(m_control);
    }

    shm_region    m_region;
    shm_allocator m_alloc;
    set_control*  m_control = nullptr;
};

} // namespace

TEST_F(shm_set_string_range_fixture, prefix_range_via_lower_upper_bound)
{
    set<string, string_less> s(*m_control, m_alloc);
    auto ins = [&](std::string_view sv) { s.insert(string::create(m_alloc, sv)); };
    ins("/org/a/1");
    ins("/org/a/2");
    ins("/org/a/3");
    ins("/org/b/1");
    ins("/org/c/1");

    // [lower_bound("/org/a/"), lower_bound("/org/b/"))
    std::vector<std::string> collected;
    for (auto it = s.lower_bound(std::string_view{"/org/a/"});
         it != s.lower_bound(std::string_view{"/org/b/"}); ++it) {
        collected.push_back(std::string((*it).std_view()));
    }
    ASSERT_EQ(collected.size(), 3U);
    EXPECT_EQ(collected[0], "/org/a/1");
    EXPECT_EQ(collected[1], "/org/a/2");
    EXPECT_EQ(collected[2], "/org/a/3");
}

TEST_F(shm_set_string_range_fixture, lower_bound_probe_hetero_missing_key)
{
    set<string, string_less> s(*m_control, m_alloc);
    s.insert(string::create(m_alloc, "/a"));
    s.insert(string::create(m_alloc, "/c"));

    auto it = s.lower_bound(std::string_view{"/b"});
    ASSERT_TRUE(it != s.cend());
    EXPECT_EQ((*it).std_view(), std::string_view{"/c"});

    auto it_end = s.lower_bound(std::string_view{"/z"});
    EXPECT_TRUE(it_end == s.cend());
}

// -------------------------------------------------------------
// map<int,int> range queries
// -------------------------------------------------------------
namespace {

class shm_map_range_fixture : public ::testing::Test {
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
        m_control  = new (mem) map_control();
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

    void*         m_buffer  = nullptr;
    shm_allocator m_alloc;
    map_control*  m_control = nullptr;
};

} // namespace

TEST_F(shm_map_range_fixture, lower_upper_on_empty_returns_end)
{
    map<int, int> m(*m_control, m_alloc);
    EXPECT_TRUE(m.lower_bound(1) == m.cend());
    EXPECT_TRUE(m.upper_bound(1) == m.cend());
    auto range = m.equal_range(1);
    EXPECT_TRUE(range.first == m.cend());
    EXPECT_TRUE(range.second == m.cend());
}

TEST_F(shm_map_range_fixture, lower_bound_and_upper_bound_behavior)
{
    map<int, int> m(*m_control, m_alloc);
    for (int k = 0; k < 10; k += 2) m.try_emplace(k, k * 10);

    auto lb = m.lower_bound(4);
    ASSERT_TRUE(lb != m.cend());
    EXPECT_EQ(*((*lb).key), 4);
    EXPECT_EQ(*((*lb).value), 40);

    auto ub = m.upper_bound(4);
    ASSERT_TRUE(ub != m.cend());
    EXPECT_EQ(*((*ub).key), 6);

    // 不存在的 key
    auto lb2 = m.lower_bound(3);
    ASSERT_TRUE(lb2 != m.cend());
    EXPECT_EQ(*((*lb2).key), 4);
    auto ub2 = m.upper_bound(3);
    ASSERT_TRUE(ub2 != m.cend());
    EXPECT_EQ(*((*ub2).key), 4);
    EXPECT_TRUE(lb2 == ub2);
}

TEST_F(shm_map_range_fixture, equal_range_missing_key_is_empty_range)
{
    map<int, int> m(*m_control, m_alloc);
    m.try_emplace(1, 100);
    m.try_emplace(5, 500);

    auto [lo, hi] = m.equal_range(3);
    EXPECT_TRUE(lo == hi);
    ASSERT_TRUE(lo != m.cend());
    EXPECT_EQ(*((*lo).key), 5);
}

TEST_F(shm_map_range_fixture, iterate_sub_range_and_mutate_values)
{
    map<int, int> m(*m_control, m_alloc);
    for (int k = 0; k < 50; ++k) m.try_emplace(k, 0);

    auto lb = m.lower_bound(10);
    auto ub = m.lower_bound(20);

    int count = 0;
    for (auto it = lb; it != ub; ++it) {
        ++count;
        int k = *((*it).key);
        EXPECT_GE(k, 10);
        EXPECT_LT(k, 20);
    }
    EXPECT_EQ(count, 10);

    // 非 const 可变 find（通过 find 获取 mapped_ptr 改值）
    if (auto got = m.find(15)) {
        *got.value = 1500;
    }
    auto fixed = m.find(15);
    ASSERT_TRUE(fixed);
    EXPECT_EQ(*fixed.value, 1500);
}

TEST_F(shm_map_range_fixture, random_vs_std_map_consistency)
{
    map<int, int> m(*m_control, m_alloc);
    std::set<int> ref;
    std::mt19937  rng(98765);

    for (int i = 0; i < 500; ++i) {
        int v = static_cast<int>(rng() % 3000);
        if (ref.insert(v).second) {
            m.try_emplace(v, v + 1);
        }
    }

    for (int q = 0; q < 200; ++q) {
        int key = static_cast<int>(rng() % 3500);
        auto ref_lb = ref.lower_bound(key);
        auto shm_lb = m.lower_bound(key);
        if (ref_lb == ref.end()) {
            EXPECT_TRUE(shm_lb == m.cend()) << "key=" << key;
        } else {
            ASSERT_TRUE(shm_lb != m.cend()) << "key=" << key;
            EXPECT_EQ(*((*shm_lb).key), *ref_lb);
        }
    }
}

// -------------------------------------------------------------
// map<shm::string, int> heterogeneous range (with region)
// -------------------------------------------------------------
namespace {

class shm_map_string_range_fixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::random_device rd;
        std::mt19937       rng(rd());
        char               name[128];
        std::snprintf(name, sizeof(name), "mc_shm_map_range_%d_%u", ::getpid(), rng());

        shm_region_options opts;
        opts.segment_name  = mc::string(name);
        opts.total_size    = 4 * 1024 * 1024;
        opts.root_capacity = 16;

        m_region = shm_region(opts);
        ASSERT_TRUE(m_region.is_valid());
        m_alloc = m_region.user_arena();
        ASSERT_TRUE(m_alloc.is_valid());

        auto* mem = m_alloc.allocate(sizeof(map_control), alignof(map_control));
        ASSERT_NE(mem, nullptr);
        m_control  = new (mem) map_control();
        const auto tag = static_cast<std::uint32_t>(
            static_cast<std::byte*>(mem) - static_cast<std::byte*>(m_region.user_base()));
        map<string, int, string_less>::init(*m_control, tag);
    }

    void TearDown() override
    {
        {
            map<string, int, string_less> m(*m_control, m_alloc);
            m.clear();
        }
        m_control->~map_control();
        m_alloc.deallocate(m_control);
    }

    shm_region    m_region;
    shm_allocator m_alloc;
    map_control*  m_control = nullptr;
};

} // namespace

TEST_F(shm_map_string_range_fixture, prefix_scan_via_lower_bound_probe)
{
    map<string, int, string_less> m(*m_control, m_alloc);
    auto put = [&](std::string_view k, int v) {
        m.try_emplace(string::create(m_alloc, k), std::move(v));
    };
    put("/org/a/1", 1);
    put("/org/a/2", 2);
    put("/org/b/1", 10);
    put("/org/c/1", 100);

    std::vector<std::string> prefix_scan;
    for (auto it = m.lower_bound(std::string_view{"/org/a/"});
         it != m.lower_bound(std::string_view{"/org/b/"}); ++it) {
        prefix_scan.push_back(std::string((*it).key->std_view()));
    }
    ASSERT_EQ(prefix_scan.size(), 2U);
    EXPECT_EQ(prefix_scan[0], "/org/a/1");
    EXPECT_EQ(prefix_scan[1], "/org/a/2");
}

TEST_F(shm_map_string_range_fixture, equal_range_on_string_key)
{
    map<string, int, string_less> m(*m_control, m_alloc);
    m.try_emplace(string::create(m_alloc, "alpha"), 1);
    m.try_emplace(string::create(m_alloc, "beta"), 2);
    m.try_emplace(string::create(m_alloc, "gamma"), 3);

    auto [lo, hi] = m.equal_range(std::string_view{"beta"});
    ASSERT_TRUE(lo != m.cend());
    EXPECT_EQ((*lo).key->std_view(), std::string_view{"beta"});
    EXPECT_EQ(*((*lo).value), 2);
    ASSERT_TRUE(hi != m.cend());
    EXPECT_EQ((*hi).key->std_view(), std::string_view{"gamma"});

    int distance = 0;
    for (auto it = lo; it != hi; ++it) ++distance;
    EXPECT_EQ(distance, 1);
}
