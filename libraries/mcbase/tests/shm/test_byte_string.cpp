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
#include <string_view>
#include <unistd.h>
#include <vector>

#include <mc/shm/allocator.h>
#include <mc/shm/byte_string.h>
#include <mc/shm/region.h>

using mc::shm::byte_string;
using mc::shm::byte_string_less;
using mc::shm::shm_allocator;
using mc::shm::shm_region;
using mc::shm::shm_region_options;

namespace {

class shm_byte_string_fixture : public ::testing::Test {
protected:
    static constexpr std::size_t region_size = 4 * 1024 * 1024;

    void SetUp() override
    {
        std::random_device rd;
        std::mt19937       rng(rd());
        char               name[128];
        std::snprintf(name, sizeof(name), "mc_shm_bstr_test_%d_%lu", ::getpid(), static_cast<unsigned long>(rng()));

        shm_region_options opts;
        opts.segment_name  = mc::string(name);
        opts.total_size    = region_size;
        opts.root_capacity = 16;

        m_region = shm_region(opts);
        ASSERT_TRUE(m_region.is_valid());
        m_alloc = m_region.user_arena();
        ASSERT_TRUE(m_alloc.is_valid());
    }

    shm_region    m_region;
    shm_allocator m_alloc;
};

} // namespace

TEST_F(shm_byte_string_fixture, default_is_empty)
{
    byte_string b;
    EXPECT_TRUE(b.empty());
    EXPECT_EQ(b.size(), 0U);
    EXPECT_EQ(b.data(), nullptr);
    EXPECT_TRUE(b.buffer_intact());
}

TEST_F(shm_byte_string_fixture, create_from_string_view)
{
    std::string_view sv{"/org/openbmc/object/0"};
    auto             b = byte_string::create(m_alloc, sv);
    EXPECT_FALSE(b.empty());
    EXPECT_EQ(b.size(), sv.size());
    EXPECT_TRUE(b.buffer_intact());
    EXPECT_EQ(b.as_string_view(), sv);
}

TEST_F(shm_byte_string_fixture, create_from_raw_bytes_with_embedded_zero)
{
    // interface + '\0' + path 拼接 key：含内嵌 NUL，长度显式给出
    const char        raw[]  = {'i', 'f', '\0', '/', 'a', '/', 'b'};
    const std::size_t rawlen = sizeof(raw);

    auto b = byte_string::create(m_alloc, raw, rawlen);
    ASSERT_FALSE(b.empty());
    EXPECT_EQ(b.size(), rawlen);
    ASSERT_NE(b.data(), nullptr);
    EXPECT_EQ(std::memcmp(b.data(), raw, rawlen), 0);

    // string_view 观察：允许内嵌 '\0'
    auto sv = b.as_string_view();
    EXPECT_EQ(sv.size(), rawlen);
    EXPECT_EQ(sv[2], '\0');
}

TEST_F(shm_byte_string_fixture, create_empty_returns_empty)
{
    auto b1 = byte_string::create(m_alloc, std::string_view{});
    EXPECT_TRUE(b1.empty());
    auto b2 = byte_string::create(m_alloc, nullptr, 0);
    EXPECT_TRUE(b2.empty());
    // data=nullptr size!=0 仍返回空串（不会崩）
    auto b3 = byte_string::create(m_alloc, nullptr, 5);
    EXPECT_TRUE(b3.empty());
}

TEST_F(shm_byte_string_fixture, clone_copies_content_independent_buffer)
{
    std::string payload(256, 'A');
    for (std::size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<char>(i & 0xFF);
    }
    auto src = byte_string::create(m_alloc, payload);
    ASSERT_FALSE(src.empty());

    auto dup = src.clone(m_alloc);
    ASSERT_FALSE(dup.empty());
    EXPECT_EQ(dup.size(), src.size());
    EXPECT_EQ(src, dup);
    EXPECT_NE(src.data(), dup.data());
}

TEST_F(shm_byte_string_fixture, move_transfers_ownership)
{
    auto a = byte_string::create(m_alloc, "foo");
    ASSERT_FALSE(a.empty());
    const auto* const a_data = a.data();

    byte_string b(std::move(a));
    EXPECT_TRUE(a.empty());
    EXPECT_EQ(b.size(), 3U);
    EXPECT_EQ(b.data(), a_data);

    byte_string c;
    c = std::move(b);
    EXPECT_TRUE(b.empty());
    EXPECT_EQ(c.size(), 3U);
    EXPECT_EQ(c, std::string_view{"foo"});
}

TEST_F(shm_byte_string_fixture, move_self_assign_is_noop)
{
    auto  a    = byte_string::create(m_alloc, "abc");
    auto& self = a;
    a          = std::move(self);
    EXPECT_EQ(a.size(), 3U);
    EXPECT_EQ(a, std::string_view{"abc"});
}

TEST_F(shm_byte_string_fixture, move_assign_releases_existing_buffer)
{
    auto a = byte_string::create(m_alloc, std::string(128, 'x'));
    auto b = byte_string::create(m_alloc, std::string(64, 'y'));
    ASSERT_FALSE(a.empty());
    ASSERT_FALSE(b.empty());

    // 将 b 移动给 a，a 原 buffer 应被释放
    a = std::move(b);
    EXPECT_EQ(a.size(), 64U);

    // 再往 arena 里分配应该能复用空间（间接验证）
    std::vector<byte_string> pool;
    for (int i = 0; i < 100; ++i) {
        pool.push_back(byte_string::create(m_alloc, "ok"));
        ASSERT_FALSE(pool.back().empty());
    }
}

TEST_F(shm_byte_string_fixture, destroy_with_alloc_frees_and_resets)
{
    auto b = byte_string::create(m_alloc, "hello");
    ASSERT_FALSE(b.empty());
    b.destroy(m_alloc);
    EXPECT_TRUE(b.empty());
    EXPECT_EQ(b.data(), nullptr);
    EXPECT_TRUE(b.buffer_intact());
}

TEST_F(shm_byte_string_fixture, destroy_idempotent)
{
    auto b = byte_string::create(m_alloc, "hello");
    b.destroy();
    b.destroy();
    b.destroy(m_alloc);
    EXPECT_TRUE(b.empty());
}

TEST_F(shm_byte_string_fixture, destroy_via_registry_default)
{
    // 验证 ~byte_string 析构路径能够通过 registry 反查 region 找到 allocator
    // 大量分配 + 析构不泄漏
    for (int i = 0; i < 2000; ++i) {
        auto b = byte_string::create(m_alloc, std::string(32, 'z'));
        ASSERT_FALSE(b.empty());
    }
    // 跑完后 arena 可继续分配
    auto final_b = byte_string::create(m_alloc, "alive");
    EXPECT_FALSE(final_b.empty());
}

TEST_F(shm_byte_string_fixture, compare_equal_and_less)
{
    auto a = byte_string::create(m_alloc, "apple");
    auto b = byte_string::create(m_alloc, "banana");
    auto c = byte_string::create(m_alloc, "apple");

    EXPECT_TRUE(a == c);
    EXPECT_TRUE(a != b);
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(b > a);
    EXPECT_FALSE(a < c);
    EXPECT_TRUE(a <= c);
    EXPECT_TRUE(a >= c);
}

TEST_F(shm_byte_string_fixture, compare_with_string_view)
{
    auto a = byte_string::create(m_alloc, "zeta");
    EXPECT_TRUE(a == std::string_view{"zeta"});
    EXPECT_TRUE(std::string_view{"zeta"} == a);
    EXPECT_TRUE(a < std::string_view{"zzz"});
    EXPECT_TRUE(std::string_view{"aaa"} < a);
}

TEST_F(shm_byte_string_fixture, compare_byte_wise_with_embedded_nul)
{
    // "a\0b" (3B) vs "a" (1B)：按字节比较应视 3B 者更大（长度更长且 prefix 相同）
    const char left[] = {'a', '\0', 'b'};
    auto       a      = byte_string::create(m_alloc, left, sizeof(left));
    auto       b      = byte_string::create(m_alloc, "a");
    EXPECT_TRUE(b < a);
    EXPECT_FALSE(a == b);
}

TEST_F(shm_byte_string_fixture, byte_string_less_heterogeneous)
{
    auto             a = byte_string::create(m_alloc, "aa");
    auto             b = byte_string::create(m_alloc, "bb");
    byte_string_less cmp;
    EXPECT_TRUE(cmp(a, b));
    EXPECT_FALSE(cmp(b, a));
    EXPECT_TRUE(cmp(a, std::string_view{"bb"}));
    EXPECT_FALSE(cmp(std::string_view{"bb"}, a));
    EXPECT_TRUE(cmp(std::string_view{"aa"}, b));
}

TEST_F(shm_byte_string_fixture, buffer_intact_detects_corruption)
{
    auto b = byte_string::create(m_alloc, "check");
    ASSERT_TRUE(b.buffer_intact());
    // 破坏 magic
    auto* raw   = const_cast<std::byte*>(b.data()) - byte_string::buffer_header_size;
    auto* magic = reinterpret_cast<std::uint32_t*>(raw);
    *magic      = 0xDEADBEEFU;
    EXPECT_FALSE(b.buffer_intact());
    // destroy 应该静默跳过（不崩溃）
    b.destroy(m_alloc);
    EXPECT_TRUE(b.empty());
}

TEST_F(shm_byte_string_fixture, large_payload_roundtrip)
{
    std::string big(1 << 14, '\0'); // 16K
    for (std::size_t i = 0; i < big.size(); ++i) {
        big[i] = static_cast<char>((i * 37U) & 0xFFU);
    }
    auto b = byte_string::create(m_alloc, big);
    ASSERT_FALSE(b.empty());
    EXPECT_EQ(b.size(), big.size());
    EXPECT_EQ(std::memcmp(b.data(), big.data(), big.size()), 0);
}
