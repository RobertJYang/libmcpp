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

#include <mc/memory/size_class_pool.h>

TEST(SizeClassPoolTest, default_config_has_five_classes_and_routes_by_request_size)
{
    mc::memory::size_class_pool::options opts;
    opts.alignment                                = 8;
    opts.min_slot_size                            = 16;
    opts.max_slot_size                            = 256;
    opts.default_class_options.initial_slab_slots = 4;

    mc::memory::size_class_pool pool(opts);
    EXPECT_EQ(pool.get_stats().active_classes, 5U);

    // 同时持有：1→最小档、17→更大一档、129→最大档，应来自不同 fixed_size_pool，指针互异
    void* small  = pool.try_acquire(1);
    void* medium = pool.try_acquire(17);
    void* large  = pool.try_acquire(129);
    ASSERT_NE(small, nullptr);
    ASSERT_NE(medium, nullptr);
    ASSERT_NE(large, nullptr);
    EXPECT_NE(small, medium);
    EXPECT_NE(medium, large);
    EXPECT_NE(small, large);

    EXPECT_TRUE(pool.try_release(small, 1));
    EXPECT_TRUE(pool.try_release(medium, 17));
    EXPECT_TRUE(pool.try_release(large, 129));

    // 129 与 200 同属最大档，释放后再分配应复用同一块
    void* from_129 = pool.try_acquire(129);
    ASSERT_NE(from_129, nullptr);
    EXPECT_TRUE(pool.try_release(from_129, 129));
    void* from_200 = pool.try_acquire(200);
    ASSERT_NE(from_200, nullptr);
    EXPECT_EQ(from_200, from_129);
    EXPECT_TRUE(pool.try_release(from_200, 200));
}

TEST(SizeClassPoolTest, reuses_slot_within_same_class)
{
    mc::memory::size_class_pool::options opts;
    opts.min_slot_size                              = 16;
    opts.max_slot_size                              = 256;
    opts.default_class_options.initial_slab_slots   = 4;
    opts.default_class_options.local_cache_capacity = 4;

    mc::memory::size_class_pool pool(opts);

    void* first = pool.try_acquire(17);
    ASSERT_NE(first, nullptr);
    EXPECT_TRUE(pool.try_release(first, 24));

    void* reused = pool.try_acquire(31);
    EXPECT_EQ(reused, first);
    EXPECT_TRUE(pool.try_release(reused, 32));
}

TEST(SizeClassPoolTest, oversized_request_uses_system_allocator)
{
    mc::memory::size_class_pool::options opts;
    opts.max_slot_size = 128;
    opts.min_slot_size = 16;
    opts.alignment     = 8;

    mc::memory::size_class_pool pool(opts);

    auto st = pool.try_acquire_with_status(256);
    ASSERT_NE(st.ptr, nullptr);
    EXPECT_FALSE(st.reused);
    EXPECT_TRUE(pool.try_release(st.ptr, 256));
}

TEST(SizeClassPoolTest, reports_fresh_and_reused_acquisitions)
{
    mc::memory::size_class_pool::options opts;
    opts.max_slot_size                              = 128;
    opts.min_slot_size                              = 16;
    opts.default_class_options.initial_slab_slots   = 4;
    opts.default_class_options.local_cache_capacity = 0;

    mc::memory::size_class_pool pool(opts);

    auto first = pool.try_acquire_with_status(24);
    ASSERT_NE(first.ptr, nullptr);
    EXPECT_FALSE(first.reused);
    EXPECT_TRUE(pool.try_release(first.ptr, 24));

    auto reused = pool.try_acquire_with_status(31);
    ASSERT_NE(reused.ptr, nullptr);
    EXPECT_EQ(reused.ptr, first.ptr);
    EXPECT_TRUE(reused.reused);
    EXPECT_TRUE(pool.try_release(reused.ptr, 32));
}

TEST(SizeClassPoolTest, trim_and_clear_release_slab_storage)
{
    mc::memory::size_class_pool::options opts;
    opts.max_slot_size                              = 128;
    opts.min_slot_size                              = 16;
    opts.default_class_options.initial_slab_slots   = 8;
    opts.default_class_options.local_cache_capacity = 0;

    mc::memory::size_class_pool pool(opts);
    std::vector<void*>          slots;
    for (int i = 0; i < 9; ++i) {
        slots.push_back(pool.try_acquire(8));
    }
    for (void* slot : slots) {
        EXPECT_TRUE(pool.try_release(slot, 8));
    }

    EXPECT_GT(pool.trim(0), 0U);
    pool.clear();
    EXPECT_EQ(pool.get_stats().active_classes, 4U);
}
