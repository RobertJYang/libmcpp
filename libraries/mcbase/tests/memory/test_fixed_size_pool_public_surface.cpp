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

#include <mc/memory/fixed_size_pool.h>
#include <mc/memory/object_pool.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace {

template <typename T, typename = void>
struct has_refill_batch_slots : std::false_type {};

template <typename T>
struct has_refill_batch_slots<T, std::void_t<decltype(std::declval<T&>().refill_batch_slots)>> : std::true_type {};

template <typename T, typename = void>
struct has_max_empty_slabs : std::false_type {};

template <typename T>
struct has_max_empty_slabs<T, std::void_t<decltype(std::declval<T&>().max_empty_slabs)>> : std::true_type {};

template <typename T, typename = void>
struct has_enable_safe_checks : std::false_type {};

template <typename T>
struct has_enable_safe_checks<T, std::void_t<decltype(std::declval<T&>().enable_safe_checks)>> : std::true_type {};

template <typename T, typename = void>
struct has_enable_observability : std::false_type {};

template <typename T>
struct has_enable_observability<T, std::void_t<decltype(std::declval<T&>().enable_observability)>> : std::true_type {};

template <typename T, typename = void>
struct has_stats_type : std::false_type {};

template <typename T>
struct has_stats_type<T, std::void_t<typename T::stats>> : std::true_type {};

template <typename T, typename = void>
struct has_snapshot : std::false_type {};

template <typename T>
struct has_snapshot<T, std::void_t<decltype(std::declval<const T&>().snapshot())>> : std::true_type {};

template <typename T, typename = void>
struct has_request_trim : std::false_type {};

template <typename T>
struct has_request_trim<T, std::void_t<decltype(std::declval<T&>().request_trim())>> : std::true_type {};

template <typename T, typename = void>
struct has_poll_maintenance : std::false_type {};

template <typename T>
struct has_poll_maintenance<T, std::void_t<decltype(std::declval<T&>().poll_maintenance())>> : std::true_type {};

struct public_surface_object {
    int value{0};
};

TEST(FixedSizePoolPublicSurfaceTest, OptionsShouldStayMinimal)
{
    using options = mc::memory::fixed_size_pool::options;

    EXPECT_FALSE(has_refill_batch_slots<options>::value);
    EXPECT_FALSE(has_max_empty_slabs<options>::value);
    EXPECT_FALSE(has_enable_safe_checks<options>::value);
    EXPECT_FALSE(has_enable_observability<options>::value);
}

TEST(FixedSizePoolPublicSurfaceTest, PoolShouldNotExposeDiagnosticApis)
{
    using pool = mc::memory::fixed_size_pool;

    EXPECT_FALSE(has_stats_type<pool>::value);
    EXPECT_FALSE(has_snapshot<pool>::value);
    EXPECT_FALSE(has_request_trim<pool>::value);
    EXPECT_FALSE(has_poll_maintenance<pool>::value);
}

TEST(FixedSizePoolPublicSurfaceTest, ObjectPoolShouldNotExposeDiagnosticApis)
{
    using pool = mc::memory::object_pool<public_surface_object>;

    EXPECT_FALSE(has_stats_type<pool>::value);
    EXPECT_FALSE(has_snapshot<pool>::value);
    EXPECT_FALSE(has_request_trim<pool>::value);
    EXPECT_FALSE(has_poll_maintenance<pool>::value);
}

} // namespace
