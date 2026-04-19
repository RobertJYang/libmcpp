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
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

#include <mc/shm/allocator.h>
#include <mc/shm/container/map.h>
#include <mc/shm/region.h>
#include <mc/shm/string.h>

using mc::shm::shm_allocator;
using mc::shm::shm_region;
using mc::shm::shm_region_options;
using mc::shm::string;
using mc::shm::string_less;
using mc::shm::container::map;
using mc::shm::container::map_control;

namespace {

class shm_map_string_fixture : public ::testing::Test {
protected:
    static constexpr std::size_t region_size = 4 * 1024 * 1024;

    void SetUp() override
    {
        std::random_device rd;
        std::mt19937       rng(rd());
        char               name[128];
        std::snprintf(name, sizeof(name), "mc_map_str_%d_%u", ::getpid(), rng());

        shm_region_options opts;
        opts.segment_name  = mc::string(name);
        opts.total_size    = region_size;
        opts.root_capacity = 16;

        m_region = shm_region(opts);
        ASSERT_TRUE(m_region.is_valid());
        m_alloc = m_region.user_arena();
        ASSERT_TRUE(m_alloc.is_valid());

        void* mem = m_alloc.allocate(sizeof(map_control), alignof(map_control));
        ASSERT_NE(mem, nullptr);
        m_control = new (mem) map_control();
        auto* user_base = static_cast<std::byte*>(m_region.user_base());
        const auto tag  = static_cast<std::uint32_t>(static_cast<std::byte*>(mem) - user_base);
        map<string, string, string_less>::init(*m_control, tag);
    }

    void TearDown() override
    {
        if (m_control != nullptr) {
            map<string, string, string_less> m(*m_control, m_alloc);
            m.clear();
            m_control->~map_control();
            m_alloc.deallocate(m_control);
            m_control = nullptr;
        }
    }

    shm_region    m_region;
    shm_allocator m_alloc;
    map_control*  m_control = nullptr;
};

} // namespace

TEST_F(shm_map_string_fixture, try_emplace_rvalue_transfers_ownership)
{
    map<string, string, string_less> m(*m_control, m_alloc);

    auto k = string::create(m_alloc, "/xyz/obj/0");
    auto v = string::create(m_alloc, "hello");
    auto [p, ins] = m.try_emplace(std::move(k), std::move(v));
    ASSERT_TRUE(ins);
    EXPECT_TRUE(k.empty());
    EXPECT_TRUE(v.empty());
    EXPECT_EQ("/xyz/obj/0", p.key->std_view());
    EXPECT_EQ("hello", p.value->std_view());
    EXPECT_EQ(1U, m.size());
}

TEST_F(shm_map_string_fixture, heterogeneous_find_by_string_view)
{
    map<string, string, string_less> m(*m_control, m_alloc);
    m.try_emplace(string::create(m_alloc, "/a"), string::create(m_alloc, "A"));
    m.try_emplace(string::create(m_alloc, "/b"), string::create(m_alloc, "B"));
    m.try_emplace(string::create(m_alloc, "/c"), string::create(m_alloc, "C"));

    auto p = m.find(std::string_view("/b"));
    ASSERT_TRUE(p);
    EXPECT_EQ("/b", p.key->std_view());
    EXPECT_EQ("B", p.value->std_view());

    EXPECT_FALSE(m.find(std::string_view("/missing")));
}

TEST_F(shm_map_string_fixture, heterogeneous_erase_releases_key_and_value_buffers)
{
    map<string, string, string_less> m(*m_control, m_alloc);
    m.try_emplace(string::create(m_alloc, "/keep"), string::create(m_alloc, "K"));
    m.try_emplace(string::create(m_alloc,
                                 "/drop/xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"),
                   string::create(m_alloc, "DROPPED_VALUE_PAYLOAD_FOR_SIZE_CHECKXXXX"));
    const auto peak = m_alloc.allocated_size();

    EXPECT_TRUE(m.erase(std::string_view(
        "/drop/xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx")));
    EXPECT_EQ(1U, m.size());
    EXPECT_LT(m_alloc.allocated_size(), peak);

    auto p = m.find(std::string_view("/keep"));
    ASSERT_TRUE(p);
    EXPECT_EQ("K", p.value->std_view());
}

TEST_F(shm_map_string_fixture, mutate_value_inplace_via_find)
{
    map<string, string, string_less> m(*m_control, m_alloc);
    m.try_emplace(string::create(m_alloc, "/obj"), string::create(m_alloc, "old"));
    auto p = m.find(std::string_view("/obj"));
    ASSERT_TRUE(p);
    EXPECT_EQ("old", p.value->std_view());

    // 注意：shm::string 不可 resize，需通过 destroy + 重新 create 替换。
    // 这里演示"以更短的新串 replace"的典型 pattern（业务层保证 mutex 串行）
    p.value->destroy(m_alloc);
    new (p.value) string(string::create(m_alloc, "new_value_text"));

    auto q = m.find(std::string_view("/obj"));
    ASSERT_TRUE(q);
    EXPECT_EQ("new_value_text", q.value->std_view());
}

TEST_F(shm_map_string_fixture, clear_releases_all_buffers)
{
    map<string, string, string_less> m(*m_control, m_alloc);
    for (int i = 0; i < 30; ++i) {
        char kbuf[64];
        char vbuf[64];
        std::snprintf(kbuf, sizeof(kbuf), "/obj/%03d/key_pad", i);
        std::snprintf(vbuf, sizeof(vbuf), "value_%03d_padded_buf", i);
        m.try_emplace(string::create(m_alloc, kbuf), string::create(m_alloc, vbuf));
    }
    EXPECT_EQ(30U, m.size());
    const auto before = m_alloc.allocated_size();
    m.clear();
    EXPECT_TRUE(m.empty());
    EXPECT_LT(m_alloc.allocated_size(), before);
}

TEST_F(shm_map_string_fixture, iteration_in_order)
{
    map<string, string, string_less> m(*m_control, m_alloc);
    const std::vector<std::string> keys = {"/z", "/a", "/m", "/b"};
    for (const auto& k : keys) {
        m.try_emplace(string::create(m_alloc, k), string::create(m_alloc, k + "_v"));
    }

    std::vector<std::string> seen;
    for (auto it = m.begin(); it != m.end(); ++it) {
        auto p = *it;
        seen.emplace_back(p.key->std_view());
    }
    std::vector<std::string> sorted = keys;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(sorted, seen);
}
