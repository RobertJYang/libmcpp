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
#include <mc/shm/container/set.h>
#include <mc/shm/region.h>
#include <mc/shm/string.h>

using mc::shm::shm_allocator;
using mc::shm::shm_region;
using mc::shm::shm_region_options;
using mc::shm::string;
using mc::shm::string_less;
using mc::shm::container::set;
using mc::shm::container::set_control;

namespace {

// set<shm::string> 需要 region registry 生效（erase 时析构 string 会反查 region）
class shm_set_string_fixture : public ::testing::Test {
protected:
    static constexpr std::size_t region_size = 4 * 1024 * 1024;

    void SetUp() override
    {
        std::random_device rd;
        std::mt19937       rng(rd());
        char               name[128];
        std::snprintf(name, sizeof(name), "mc_set_str_%d_%lu", ::getpid(), static_cast<unsigned long>(rng()));

        shm_region_options opts;
        opts.segment_name  = mc::string(name);
        opts.total_size    = region_size;
        opts.root_capacity = 16;

        m_region = shm_region(opts);
        ASSERT_TRUE(m_region.is_valid());
        m_alloc = m_region.user_arena();
        ASSERT_TRUE(m_alloc.is_valid());

        void* mem = m_alloc.allocate(sizeof(set_control), alignof(set_control));
        ASSERT_NE(mem, nullptr);
        m_control = new (mem) set_control();

        auto*      user_base = static_cast<std::byte*>(m_region.user_base());
        const auto tag       = static_cast<std::uint32_t>(static_cast<std::byte*>(mem) - user_base);
        set<string, string_less>::init(*m_control, tag);
    }

    void TearDown() override
    {
        // set<string>::clear 不显式调用——由 TearDown 销毁 region 时一起回收
        // 但要保证 m_control 本身先析构（持锁状态不能跨 region 销毁）
        if (m_control != nullptr) {
            // 走一次 clear 释放所有 string 的二级 buffer（更干净）
            set<string, string_less> s(*m_control, m_alloc);
            s.clear();
            m_control->~set_control();
            m_alloc.deallocate(m_control);
            m_control = nullptr;
        }
    }

    std::vector<std::string> collect(set<string, string_less>& s) const
    {
        std::vector<std::string> out;
        for (auto it = s.begin(); it != s.end(); ++it) {
            out.emplace_back((*it).std_view());
        }
        return out;
    }

    shm_region    m_region;
    shm_allocator m_alloc;
    set_control*  m_control = nullptr;
};

} // namespace

// =========================================================================
// 基础：insert 支持 rvalue / heterogeneous find / erase
// =========================================================================

TEST_F(shm_set_string_fixture, insert_rvalue_transfers_ownership)
{
    set<string, string_less> s(*m_control, m_alloc);

    auto tmp = string::create(m_alloc, "/xyz/openbmc");
    ASSERT_FALSE(tmp.empty());
    auto [kp, inserted] = s.insert(std::move(tmp));
    ASSERT_NE(kp, nullptr);
    EXPECT_TRUE(inserted);
    // tmp 已移动：内容清空
    EXPECT_TRUE(tmp.empty());
    EXPECT_EQ("/xyz/openbmc", kp->std_view());
    EXPECT_EQ(1U, s.size());
}

TEST_F(shm_set_string_fixture, insert_duplicate_returns_existing)
{
    set<string, string_less> s(*m_control, m_alloc);
    auto [k1, i1] = s.insert(string::create(m_alloc, "/a"));
    ASSERT_TRUE(i1);
    auto dup      = string::create(m_alloc, "/a");
    auto before   = m_alloc.allocated_size();
    auto [k2, i2] = s.insert(std::move(dup));
    EXPECT_FALSE(i2);
    EXPECT_EQ(k1, k2);
    // dup 被函数接管：rvalue 虽然没进入 set，但必须已释放，否则 buffer 泄漏
    // 实际当前语义：失败时 dup 尚未 move，因此其 buffer 会在本函数作用域末自动 destroy
    (void)before;
    EXPECT_EQ(1U, s.size());
}

TEST_F(shm_set_string_fixture, ordered_by_lexicographic)
{
    set<string, string_less>       s(*m_control, m_alloc);
    const std::vector<std::string> input = {
        "/xyz/openbmc/a/b", "/xyz/openbmc/a", "/xyz/openbmc/c", "/a", "/zzz",
    };
    for (const auto& p : input) {
        s.insert(string::create(m_alloc, p));
    }
    EXPECT_EQ(input.size(), s.size());
    auto                     values = collect(s);
    std::vector<std::string> sorted = input;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(sorted, values);
}

TEST_F(shm_set_string_fixture, heterogeneous_find_by_string_view)
{
    set<string, string_less> s(*m_control, m_alloc);
    s.insert(string::create(m_alloc, "/a"));
    s.insert(string::create(m_alloc, "/b"));
    s.insert(string::create(m_alloc, "/c"));

    const string* hit = s.find(std::string_view("/b"));
    ASSERT_NE(hit, nullptr);
    EXPECT_EQ("/b", hit->std_view());

    EXPECT_EQ(nullptr, s.find(std::string_view("/missing")));
}

TEST_F(shm_set_string_fixture, heterogeneous_erase_by_string_view_releases_buffer)
{
    set<string, string_less> s(*m_control, m_alloc);
    s.insert(string::create(m_alloc, "/keep"));
    s.insert(string::create(m_alloc, "/drop/xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"));
    const auto peak_allocated = m_alloc.allocated_size();

    EXPECT_TRUE(s.erase(std::string_view("/drop/xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx")));
    EXPECT_EQ(1U, s.size());

    // buffer 应已被释放（allocated_size 下降）；节点本身仍占位（free 但未 deallocate）
    EXPECT_LT(m_alloc.allocated_size(), peak_allocated);

    // 再查找应返回 nullptr
    EXPECT_EQ(nullptr, s.find(std::string_view("/drop/xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx")));
    // 保留项仍可见
    EXPECT_NE(nullptr, s.find(std::string_view("/keep")));
}

TEST_F(shm_set_string_fixture, erase_nonexistent_returns_false)
{
    set<string, string_less> s(*m_control, m_alloc);
    s.insert(string::create(m_alloc, "/hello"));
    EXPECT_FALSE(s.erase(std::string_view("/world")));
    EXPECT_EQ(1U, s.size());
}

TEST_F(shm_set_string_fixture, clear_empties_and_releases_buffers)
{
    set<string, string_less> s(*m_control, m_alloc);
    for (int i = 0; i < 50; ++i) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "/obj/%03d/extra_payload_to_force_allocation", i);
        s.insert(string::create(m_alloc, buf));
    }
    EXPECT_EQ(50U, s.size());
    const auto before = m_alloc.allocated_size();
    s.clear();
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(0U, s.size());
    // clear 至少释放了所有 string buffer
    EXPECT_LT(m_alloc.allocated_size(), before);
    // begin == end
    EXPECT_EQ(s.begin(), s.end());
}

TEST_F(shm_set_string_fixture, iteration_over_views)
{
    set<string, string_less> s(*m_control, m_alloc);
    s.insert(string::create(m_alloc, "/xyz/obj/0"));
    s.insert(string::create(m_alloc, "/xyz/obj/1"));
    s.insert(string::create(m_alloc, "/xyz/obj/2"));

    std::vector<std::string> seen;
    for (const auto& k : s) {
        seen.emplace_back(k.std_view());
    }
    const std::vector<std::string> expected = {
        "/xyz/obj/0",
        "/xyz/obj/1",
        "/xyz/obj/2",
    };
    EXPECT_EQ(expected, seen);
}

TEST_F(shm_set_string_fixture, stress_mixed_insert_erase)
{
    set<string, string_less> s(*m_control, m_alloc);
    std::mt19937             rng(0xBEEF);
    std::vector<std::string> live;

    for (int i = 0; i < 400; ++i) {
        const int op = rng() % 3;
        if (op != 0 || live.empty()) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "/obj/%05lu", static_cast<unsigned long>(rng() % 1000));
            const std::string key = buf;
            auto [kp, ins]        = s.insert(string::create(m_alloc, key));
            if (ins) {
                live.push_back(key);
            }
        } else {
            const std::size_t idx = rng() % live.size();
            const std::string k   = live[idx];
            live[idx]             = live.back();
            live.pop_back();
            EXPECT_TRUE(s.erase(std::string_view(k)));
        }
        ASSERT_EQ(live.size(), s.size());
    }

    // 最后 clear，确保所有 buffer 释放
    s.clear();
    EXPECT_TRUE(s.empty());
}
