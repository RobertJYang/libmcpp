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
#include <string>
#include <vector>

#include <unistd.h>

#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/region.h>

namespace {

std::string make_unique_name(std::string_view prefix)
{
    const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::string(prefix) + "_" + std::to_string(getpid()) + "_" + std::to_string(now_ns);
}

class shm_region_test : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_segment_name = make_unique_name("mc_shm_region");
    }

    void TearDown() override
    {
        mc::shm::detail::shared_memory_backend::remove(m_segment_name);
    }

    std::string m_segment_name;
};

TEST_F(shm_region_test, region_registers_and_reopens_roots)
{
    mc::shm::shm_region_options options;
    options.segment_name = m_segment_name;
    options.total_size = 256 * 1024;
    options.root_capacity = 8;

    mc::shm::shm_region region(options);
    ASSERT_TRUE(region.is_valid());
    auto allocator = region.user_arena();
    ASSERT_TRUE(allocator.is_valid());

    auto* value = allocator.construct<int>(42);
    ASSERT_NE(value, nullptr);

    ASSERT_TRUE(region.upsert_root("answer", region.offset_of(value), sizeof(*value), mc::shm::root_kind::opaque, 7));
    auto local_root = region.find_root("answer");
    ASSERT_TRUE(local_root.has_value());
    EXPECT_EQ(local_root->generation, 7U);

    options.open_mode = mc::shm::detail::open_mode::open_existing;
    mc::shm::shm_region reopened(options);
    ASSERT_TRUE(reopened.is_valid());

    auto reopened_root = reopened.find_root("answer");
    ASSERT_TRUE(reopened_root.has_value());
    auto* reopened_value = reopened.ptr_from_offset<int>(reopened_root->offset);
    ASSERT_NE(reopened_value, nullptr);
    EXPECT_EQ(*reopened_value, 42);

    const auto roots = reopened.list_roots();
    ASSERT_EQ(roots.size(), 1U);
    EXPECT_EQ(roots.front().name, "answer");

    EXPECT_TRUE(reopened.erase_root("answer"));
    EXPECT_FALSE(reopened.find_root("answer").has_value());
}

TEST_F(shm_region_test, multiple_roots_can_be_listed_and_erased)
{
    mc::shm::shm_region_options options;
    options.segment_name  = m_segment_name;
    options.total_size    = 256 * 1024;
    options.root_capacity = 8;

    mc::shm::shm_region region(options);
    ASSERT_TRUE(region.is_valid());
    auto allocator = region.user_arena();
    ASSERT_TRUE(allocator.is_valid());

    struct entry {
        std::string name;
        int         value;
    };
    std::vector<entry> entries{{"alpha", 1}, {"beta", 2}, {"gamma", 3}};

    for (const auto& e : entries) {
        auto* p = allocator.construct<int>(e.value);
        ASSERT_NE(p, nullptr);
        ASSERT_TRUE(region.upsert_root(e.name, region.offset_of(p), sizeof(*p)));
    }

    const auto listed = region.list_roots();
    EXPECT_EQ(listed.size(), entries.size());
    for (const auto& e : entries) {
        auto rec = region.find_root(e.name);
        ASSERT_TRUE(rec.has_value()) << e.name;
        auto* val = region.ptr_from_offset<int>(rec->offset);
        ASSERT_NE(val, nullptr);
        EXPECT_EQ(*val, e.value);
    }

    EXPECT_TRUE(region.erase_root("beta"));
    EXPECT_FALSE(region.find_root("beta").has_value());
    EXPECT_TRUE(region.find_root("alpha").has_value());
    EXPECT_TRUE(region.find_root("gamma").has_value());

    // erase 不存在的 root 返回 false。
    EXPECT_FALSE(region.erase_root("unknown"));
}

TEST_F(shm_region_test, contains_and_offset_of_respect_region_bounds)
{
    mc::shm::shm_region_options options;
    options.segment_name = m_segment_name;
    options.total_size   = 128 * 1024;

    mc::shm::shm_region region(options);
    ASSERT_TRUE(region.is_valid());
    auto allocator = region.user_arena();
    ASSERT_TRUE(allocator.is_valid());

    auto* p = allocator.allocate(64);
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(region.contains(p));
    const auto offset = region.offset_of(p);
    EXPECT_EQ(region.ptr_from_offset<void>(offset), p);

    int stack_value = 0;
    EXPECT_FALSE(region.contains(&stack_value));
    allocator.deallocate(p);
}

TEST_F(shm_region_test, upsert_root_rejects_oversized_name)
{
    mc::shm::shm_region_options options;
    options.segment_name = m_segment_name;
    options.total_size   = 64 * 1024;

    mc::shm::shm_region region(options);
    ASSERT_TRUE(region.is_valid());
    auto allocator = region.user_arena();
    ASSERT_TRUE(allocator.is_valid());

    auto* value = allocator.construct<int>(0);
    ASSERT_NE(value, nullptr);

    // 长度超过 64 应被拒绝。
    const std::string too_long(mc::shm::shm_region::max_root_name_size + 4, 'x');
    EXPECT_FALSE(region.upsert_root(too_long, region.offset_of(value), sizeof(*value)));
    EXPECT_FALSE(region.find_root(too_long).has_value());
}

} // namespace
