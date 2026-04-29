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

#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <unistd.h>

#include <mc/shm/allocator.h>
#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/region.h>
#include <mc/shm/string.h>
#include <mc/shm/string_view.h>

using mc::shm::shm_allocator;
using mc::shm::shm_region;
using mc::shm::shm_region_options;
using mc::shm::string;

namespace {

// 用真实 shm_region，以便 registry 生效，RAII 自动 destroy 路径能走通
class shm_string_region_fixture : public ::testing::Test {
protected:
    static constexpr std::size_t region_size = 4 * 1024 * 1024;

    void SetUp() override
    {
        std::random_device rd;
        std::mt19937       rng(rd());
        char               name[128];
        std::snprintf(name, sizeof(name), "mc_shm_string_test_%d_%lu", ::getpid(), static_cast<unsigned long>(rng()));

        shm_region_options opts;
        opts.segment_name  = mc::string(name);
        opts.total_size    = region_size;
        opts.root_capacity = 16;

        m_region = shm_region(opts);
        ASSERT_TRUE(m_region.is_valid());
        m_alloc = m_region.user_arena();
        ASSERT_TRUE(m_alloc.is_valid());
    }

    void TearDown() override
    {
        if (m_region.is_valid()) {
            mc::shm::detail::shared_memory_backend::remove(m_region.name());
        }
    }

    shm_region    m_region;
    shm_allocator m_alloc;
};

// 兼容老路径：使用裸 arena（不经 region、不注册 registry）
// 验证 destroy(alloc) 显式路径的独立可用性
class shm_string_bare_arena_fixture : public ::testing::Test {
protected:
    static constexpr std::size_t arena_size = 1 * 1024 * 1024;

    void SetUp() override
    {
        m_buffer = std::aligned_alloc(4096, arena_size);
        ASSERT_NE(m_buffer, nullptr);
        ASSERT_TRUE(shm_allocator::initialize(m_buffer, arena_size));
        m_alloc = shm_allocator(m_buffer, arena_size);
    }

    void TearDown() override
    {
        std::free(m_buffer);
    }

    void*         m_buffer = nullptr;
    shm_allocator m_alloc;
};

} // namespace

// =========================================================================
// 基础：默认构造 / create / view
// =========================================================================

TEST_F(shm_string_region_fixture, default_is_empty)
{
    string s;
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(0U, s.size());
    EXPECT_EQ(0U, s.capacity());
    EXPECT_EQ(nullptr, s.data());
    EXPECT_TRUE(s.buffer_intact());
}

TEST_F(shm_string_region_fixture, create_and_view)
{
    auto s = string::create(m_alloc, "hello");
    EXPECT_FALSE(s.empty());
    EXPECT_EQ(5U, s.size());
    EXPECT_EQ(5U, s.capacity());
    EXPECT_EQ("hello", s.std_view());
    EXPECT_EQ(0, std::memcmp("hello", s.data(), 5));
    EXPECT_TRUE(s.buffer_intact());
    // ~string() 自动释放
}

TEST_F(shm_string_region_fixture, create_empty_does_not_allocate)
{
    const auto before = m_alloc.allocated_size();
    auto       s      = string::create(m_alloc, "");
    const auto after  = m_alloc.allocated_size();
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(before, after);
}

TEST_F(shm_string_region_fixture, clone_produces_independent_copy)
{
    auto a = string::create(m_alloc, "copy me");
    auto b = a.clone(m_alloc);

    EXPECT_EQ(a.std_view(), b.std_view());
    EXPECT_NE(a.data(), b.data());
    EXPECT_EQ(a, b);

    // 销毁 b，a 仍然可读
    b.destroy();
    EXPECT_TRUE(b.empty());
    EXPECT_EQ("copy me", a.std_view());
}

// =========================================================================
// Move：offset_ptr 自动重算 offset；源被清空
// =========================================================================

TEST_F(shm_string_region_fixture, move_preserves_content_and_clears_source)
{
    auto       a           = string::create(m_alloc, "move me");
    const auto data_before = a.data();

    string b(std::move(a));
    EXPECT_EQ("move me", b.std_view());
    EXPECT_TRUE(a.empty());
    EXPECT_EQ(0U, a.size());
    // data 绝对地址不变
    EXPECT_EQ(data_before, b.data());
}

TEST_F(shm_string_region_fixture, move_assignment)
{
    auto a = string::create(m_alloc, "first");
    auto b = string::create(m_alloc, "second");
    b      = std::move(a);
    EXPECT_EQ("first", b.std_view());
    EXPECT_TRUE(a.empty());
}

TEST_F(shm_string_region_fixture, move_self_assignment_is_safe)
{
    auto s = string::create(m_alloc, "self");
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-move"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
#endif
    s = std::move(s);
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    EXPECT_EQ("self", s.std_view());
}

// =========================================================================
// RAII：析构自动释放；destroy() 幂等
// =========================================================================

TEST_F(shm_string_region_fixture, raii_destructor_frees_buffer)
{
    const auto baseline = m_alloc.allocated_size();
    {
        auto s = string::create(m_alloc, "temp");
        EXPECT_GT(m_alloc.allocated_size(), baseline);
    }
    EXPECT_EQ(baseline, m_alloc.allocated_size());
}

TEST_F(shm_string_region_fixture, many_roundtrips_do_not_leak)
{
    const auto baseline = m_alloc.allocated_size();
    for (int i = 0; i < 1000; ++i) {
        auto s = string::create(m_alloc, std::string("item_") + std::to_string(i));
        EXPECT_TRUE(s.buffer_intact());
    }
    EXPECT_EQ(baseline, m_alloc.allocated_size());
}

TEST_F(shm_string_region_fixture, idempotent_destroy)
{
    auto s = string::create(m_alloc, "x");
    s.destroy();
    EXPECT_TRUE(s.empty());
    s.destroy(); // 二次 destroy 安全
    EXPECT_TRUE(s.empty());
}

TEST_F(shm_string_region_fixture, explicit_destroy_with_alloc_also_works)
{
    auto s = string::create(m_alloc, "explicit");
    s.destroy(m_alloc);
    EXPECT_TRUE(s.empty());
}

// =========================================================================
// 比较
// =========================================================================

TEST_F(shm_string_region_fixture, equality_and_ordering)
{
    auto a = string::create(m_alloc, "apple");
    auto b = string::create(m_alloc, "banana");
    auto c = string::create(m_alloc, "apple");

    EXPECT_EQ(a, c);
    EXPECT_NE(a, b);

    EXPECT_LT(a, b);
    EXPECT_LE(a, c);
    EXPECT_GT(b, a);
    EXPECT_GE(c, a);

    EXPECT_EQ(a, std::string_view("apple"));
    EXPECT_NE(a, std::string_view("apples"));
    EXPECT_LT(a, std::string_view("banana"));
    EXPECT_LT(std::string_view("apple"), b);
}

TEST_F(shm_string_region_fixture, lexicographic_order_with_prefix)
{
    auto shorter = string::create(m_alloc, "app");
    auto longer  = string::create(m_alloc, "apple");
    EXPECT_LT(shorter, longer);
    EXPECT_FALSE(longer < shorter);
}

// =========================================================================
// 边界：长串 / 二进制内容
// =========================================================================

TEST_F(shm_string_region_fixture, long_string_and_binary_content)
{
    std::string big(20000, 'X');
    big[1000] = '\0';
    big[5000] = static_cast<char>(0xFF);

    auto s = string::create(m_alloc, big);
    ASSERT_EQ(big.size(), s.size());
    EXPECT_EQ(0, std::memcmp(big.data(), s.data(), big.size()));
    EXPECT_TRUE(s.buffer_intact());
}

// =========================================================================
// 存储在 shm slot 里（模拟容器 node 内嵌 string 成员）
// 验证 offset_ptr 跨"对象位置变化"仍然正确定位 buffer
// =========================================================================

TEST_F(shm_string_region_fixture, stored_in_shm_slot_survives_resolution)
{
    auto* slot_mem = m_alloc.allocate(sizeof(string), alignof(string));
    ASSERT_NE(slot_mem, nullptr);
    auto* slot = new (slot_mem) string{};

    *slot = string::create(m_alloc, "stored-in-slot");

    EXPECT_EQ("stored-in-slot", slot->std_view());
    EXPECT_TRUE(slot->buffer_intact());

    slot->~string(); // RAII 释放 buffer
    m_alloc.deallocate(slot_mem);
}

TEST_F(shm_string_region_fixture, move_from_stack_to_shm_slot)
{
    auto* slot_mem = m_alloc.allocate(sizeof(string), alignof(string));
    ASSERT_NE(slot_mem, nullptr);
    auto* slot = new (slot_mem) string{};

    auto tmp = string::create(m_alloc, "from-stack");
    *slot    = std::move(tmp);

    EXPECT_TRUE(tmp.empty());
    EXPECT_EQ("from-stack", slot->std_view());
    EXPECT_TRUE(slot->buffer_intact());

    slot->~string();
    m_alloc.deallocate(slot_mem);
}

// =========================================================================
// bare arena：string 未注册到 registry；destroy() 静默 leak，destroy(alloc) 生效
// =========================================================================

TEST_F(shm_string_bare_arena_fixture, destroy_noarg_on_unregistered_leaks_silently)
{
    const auto baseline = m_alloc.allocated_size();
    string     s        = string::create(m_alloc, "leaked");
    EXPECT_GT(m_alloc.allocated_size(), baseline);
    s.destroy(); // 找不到 region → 静默清零，buffer leaked
    EXPECT_TRUE(s.empty());
    EXPECT_GT(m_alloc.allocated_size(), baseline); // buffer 仍未回收
}

TEST_F(shm_string_bare_arena_fixture, destroy_with_alloc_on_unregistered_works)
{
    const auto baseline = m_alloc.allocated_size();
    string     s        = string::create(m_alloc, "explicit-free");
    EXPECT_GT(m_alloc.allocated_size(), baseline);
    s.destroy(m_alloc);
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(baseline, m_alloc.allocated_size());
}

TEST_F(shm_string_bare_arena_fixture, raii_on_unregistered_leaks_silently)
{
    const auto baseline = m_alloc.allocated_size();
    {
        auto s = string::create(m_alloc, "raii-on-bare");
        EXPECT_GT(m_alloc.allocated_size(), baseline);
    }
    // 未注册 → ~string() 静默 leak
    EXPECT_GT(m_alloc.allocated_size(), baseline);
}

// =========================================================================
// 多 region：registry 正确区分不同 region 的指针
// =========================================================================

TEST(shm_string_multi_region, destroy_routes_to_correct_region)
{
    std::random_device rd;
    std::mt19937       rng(rd());
    char               name_a[128];
    char               name_b[128];
    std::snprintf(name_a, sizeof(name_a), "mc_string_multi_a_%d_%lu", ::getpid(), static_cast<unsigned long>(rng()));
    std::snprintf(name_b, sizeof(name_b), "mc_string_multi_b_%d_%lu", ::getpid(), static_cast<unsigned long>(rng()));

    shm_region_options opt_a;
    opt_a.segment_name = mc::string(name_a);
    opt_a.total_size   = 2 * 1024 * 1024;
    shm_region region_a(opt_a);
    ASSERT_TRUE(region_a.is_valid());

    shm_region_options opt_b;
    opt_b.segment_name = mc::string(name_b);
    opt_b.total_size   = 2 * 1024 * 1024;
    shm_region region_b(opt_b);
    ASSERT_TRUE(region_b.is_valid());

    auto alloc_a = region_a.user_arena();
    auto alloc_b = region_b.user_arena();

    const auto base_a = alloc_a.allocated_size();
    const auto base_b = alloc_b.allocated_size();

    string sa = string::create(alloc_a, "in-A");
    string sb = string::create(alloc_b, "in-B");
    EXPECT_GT(alloc_a.allocated_size(), base_a);
    EXPECT_GT(alloc_b.allocated_size(), base_b);

    sa.destroy(); // 应路由到 region_a
    sb.destroy(); // 应路由到 region_b
    EXPECT_EQ(base_a, alloc_a.allocated_size());
    EXPECT_EQ(base_b, alloc_b.allocated_size());
}
