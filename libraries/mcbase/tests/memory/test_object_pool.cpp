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

#include <mc/memory/object_pool.h>

namespace {

struct pool_test_object {
    inline static int s_ctor_count = 0;
    inline static int s_dtor_count = 0;

    int value{0};

    explicit pool_test_object(int v) : value(v)
    {
        ++s_ctor_count;
    }

    ~pool_test_object()
    {
        ++s_dtor_count;
    }
};

TEST(ObjectPoolTest, CreateDestroyAndReuseSlot)
{
    pool_test_object::s_ctor_count = 0;
    pool_test_object::s_dtor_count = 0;

    mc::memory::object_pool<pool_test_object> pool;

    auto* first = pool.create(1);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->value, 1);

    pool.destroy(first);

    auto* second = pool.create(2);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second, first);
    EXPECT_EQ(second->value, 2);

    pool.destroy(second);

    EXPECT_EQ(pool_test_object::s_ctor_count, 2);
    EXPECT_EQ(pool_test_object::s_dtor_count, 2);
}

TEST(ObjectPoolTest, ReserveProvidesCapacityWithoutConstructingObjects)
{
    pool_test_object::s_ctor_count = 0;
    pool_test_object::s_dtor_count = 0;

    mc::memory::object_pool<pool_test_object> pool;
    pool.reserve(8);

    EXPECT_EQ(pool_test_object::s_ctor_count, 0);
    EXPECT_EQ(pool_test_object::s_dtor_count, 0);

    auto* object = pool.create(42);
    ASSERT_NE(object, nullptr);
    EXPECT_EQ(object->value, 42);
    pool.destroy(object);
}

} // namespace
