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
#include <unistd.h>
#include <vector>

#include <mc/intrusive.h>
#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/region.h>

namespace {

struct set_node : mc::intrusive::set_base_hook<> {
    explicit set_node(int v) : value(v)
    {}

    int value = 0;

    bool operator<(const set_node& other) const
    {
        return value < other.value;
    }
};

using shm_set = mc::intrusive::set<set_node, mc::intrusive::constant_time_size<true>>;

std::string make_unique_name(std::string_view prefix)
{
    const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::string(prefix) + "_" + std::to_string(getpid()) + "_" + std::to_string(now_ns);
}

TEST(shm_intrusive_test, intrusive_set_can_be_built_in_shared_memory)
{
    const auto segment_name = make_unique_name("mc_shm_intrusive");
    auto cleanup = [&]() { mc::shm::detail::shared_memory_backend::remove(segment_name); };

    mc::shm::shm_region_options options;
    options.segment_name = segment_name;
    options.total_size = 256 * 1024;

    mc::shm::shm_region region(options);
    ASSERT_TRUE(region.is_valid());
    auto allocator = region.user_arena();
    ASSERT_TRUE(allocator.is_valid());

    auto* root = allocator.construct<shm_set>();
    auto* first = allocator.construct<set_node>(3);
    auto* second = allocator.construct<set_node>(1);
    auto* third = allocator.construct<set_node>(2);
    ASSERT_NE(root, nullptr);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(third, nullptr);

    root->insert(*first);
    root->insert(*second);
    root->insert(*third);
    ASSERT_TRUE(region.upsert_root("numbers", region.offset_of(root), sizeof(*root), mc::shm::root_kind::intrusive_root));

    options.open_mode = mc::shm::detail::open_mode::open_existing;
    mc::shm::shm_region reopened(options);
    ASSERT_TRUE(reopened.is_valid());

    auto root_record = reopened.find_root("numbers");
    ASSERT_TRUE(root_record.has_value());
    auto* reopened_root = reopened.ptr_from_offset<shm_set>(root_record->offset);
    ASSERT_NE(reopened_root, nullptr);

    std::vector<int> values;
    for (const auto& node : *reopened_root) {
        values.push_back(node.value);
    }

    EXPECT_EQ(values, (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(reopened_root->size(), 3U);

    cleanup();
}

} // namespace
