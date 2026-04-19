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

// shm_object / property_slot / property_slab / child_slab POD 布局测试
//
// 范围：
//   1. sizeof / alignof 锁定（144B shm_object + 32B slot + 16B slab 头）
//   2. 字段 offset 锁定（防止 reorder 破坏 ABI）
//   3. CRC32 helper 基本正确性 + slab CRC 跳过自身字段验证
//   4. POD / triviality 性质

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <vector>

#include <mc/engine/shm_object.h>

using mc::engine::shm_ptr;
using mc::engine::child_slab;
using mc::engine::child_slab_alloc_size;
using mc::engine::child_slab_check;
using mc::engine::child_slab_compute_crc;
using mc::engine::shm_object;
using mc::engine::shm_object_abi_version;
using mc::engine::shm_object_check;
using mc::engine::shm_object_compute_crc;
using mc::engine::property_slab;
using mc::engine::property_slab_alloc_size;
using mc::engine::property_slab_check;
using mc::engine::property_slab_compute_crc;
using mc::engine::property_slot;
using mc::engine::property_type_tag;
namespace shadow_flags = mc::engine::shm_object_flags;

namespace {

// ============================================================================
// 1. sizeof / alignof
// ============================================================================

TEST(shm_object_layout, sizeof_shm_object_is_192)
{
    EXPECT_EQ(sizeof(shm_object), 192U);
    EXPECT_EQ(alignof(shm_object), 64U);
}

TEST(shm_object_layout, abi_version_is_v3)
{
    EXPECT_EQ(shm_object_abi_version, 3U);
}

TEST(shm_object_layout, sizeof_property_slot_is_32)
{
    EXPECT_EQ(sizeof(property_slot), 32U);
    EXPECT_EQ(alignof(property_slot), 8U);
}

TEST(shm_object_layout, sizeof_property_slab_header_is_16)
{
    EXPECT_EQ(sizeof(property_slab), 16U);
    EXPECT_EQ(alignof(property_slab), 8U);
}

TEST(shm_object_layout, sizeof_child_slab_header_is_16)
{
    EXPECT_EQ(sizeof(child_slab), 16U);
    EXPECT_EQ(alignof(child_slab), 8U);
}

// ============================================================================
// 2. 字段 offset
// ============================================================================

TEST(shm_object_layout, shm_object_field_offsets_locked)
{
    EXPECT_EQ(offsetof(shm_object, abi_version), 0U);
    EXPECT_EQ(offsetof(shm_object, flags), 2U);
    EXPECT_EQ(offsetof(shm_object, _hdr_pad), 3U);
    EXPECT_EQ(offsetof(shm_object, crc32_self), 4U);
    EXPECT_EQ(offsetof(shm_object, object_id), 8U);
    EXPECT_EQ(offsetof(shm_object, class_name), 16U);
    EXPECT_EQ(offsetof(shm_object, name), 24U);
    EXPECT_EQ(offsetof(shm_object, path), 32U);
    EXPECT_EQ(offsetof(shm_object, position), 40U);
    EXPECT_EQ(offsetof(shm_object, service), 48U);
    EXPECT_EQ(offsetof(shm_object, parent), 56U);
    EXPECT_EQ(offsetof(shm_object, properties), 64U);
    EXPECT_EQ(offsetof(shm_object, children), 72U);
    EXPECT_EQ(offsetof(shm_object, journal), 128U);
}

TEST(shm_object_layout, property_slot_field_offsets_locked)
{
    EXPECT_EQ(offsetof(property_slot, key_hash), 0U);
    EXPECT_EQ(offsetof(property_slot, key), 8U);
    EXPECT_EQ(offsetof(property_slot, type), 16U);
    EXPECT_EQ(offsetof(property_slot, v_int64), 24U);
    EXPECT_EQ(offsetof(property_slot, v_double), 24U);
    EXPECT_EQ(offsetof(property_slot, v_blob), 24U);
}

TEST(shm_object_layout, slab_headers_field_offsets_locked)
{
    EXPECT_EQ(offsetof(property_slab, abi_version), 0U);
    EXPECT_EQ(offsetof(property_slab, slot_capacity), 2U);
    EXPECT_EQ(offsetof(property_slab, slot_count), 4U);
    EXPECT_EQ(offsetof(property_slab, flags), 6U);
    EXPECT_EQ(offsetof(property_slab, crc32), 8U);

    EXPECT_EQ(offsetof(child_slab, abi_version), 0U);
    EXPECT_EQ(offsetof(child_slab, slot_capacity), 2U);
    EXPECT_EQ(offsetof(child_slab, slot_count), 4U);
    EXPECT_EQ(offsetof(child_slab, flags), 6U);
    EXPECT_EQ(offsetof(child_slab, crc32), 8U);
}

TEST(shm_object_layout, slab_alloc_sizes_correct)
{
    // property: 16B 头 + capacity * 32B
    EXPECT_EQ(property_slab_alloc_size(0), 16U);
    EXPECT_EQ(property_slab_alloc_size(1), 16U + 32U);
    EXPECT_EQ(property_slab_alloc_size(8), 16U + 8U * 32U);
    EXPECT_EQ(property_slab_alloc_size(64), 16U + 64U * 32U);

    // child: 16B 头 + capacity * 8B
    EXPECT_EQ(child_slab_alloc_size(0), 16U);
    EXPECT_EQ(child_slab_alloc_size(1), 16U + 8U);
    EXPECT_EQ(child_slab_alloc_size(8), 16U + 8U * 8U);
    EXPECT_EQ(child_slab_alloc_size(128), 16U + 128U * 8U);
}

// ============================================================================
// 3. POD 性质
// ============================================================================

TEST(shm_object_layout, types_are_trivially_destructible)
{
    EXPECT_TRUE(std::is_trivially_destructible_v<property_slot>);
    EXPECT_TRUE(std::is_trivially_destructible_v<property_slab>);
    EXPECT_TRUE(std::is_trivially_destructible_v<child_slab>);
    EXPECT_TRUE(std::is_trivially_destructible_v<shm_object>);
}

TEST(shm_object_layout, types_are_standard_layout)
{
    EXPECT_TRUE(std::is_standard_layout_v<property_slot>);
    EXPECT_TRUE(std::is_standard_layout_v<property_slab>);
    EXPECT_TRUE(std::is_standard_layout_v<child_slab>);
    EXPECT_TRUE(std::is_standard_layout_v<shm_object>);
}

// ============================================================================
// 4. CRC32 正确性
// ============================================================================

TEST(shm_object_crc, crc32_known_vectors)
{
    EXPECT_EQ(mc::engine::crc32_ieee("", 0), 0x00000000U);
    EXPECT_EQ(mc::engine::crc32_ieee("a", 1), 0xE8B7BE43U);
    EXPECT_EQ(mc::engine::crc32_ieee("123456789", 9), 0xCBF43926U);
}

TEST(shm_object_crc, shadow_compute_and_check)
{
    shm_object s{};
    s.abi_version = shm_object_abi_version;
    s.flags       = shadow_flags::alive;
    s.object_id   = 0xDEADBEEF12345678ULL;
    s.crc32_self  = shm_object_compute_crc(s);
    EXPECT_TRUE(shm_object_check(s));

    // 篡改 → CRC 应失配
    s.object_id = 0xCAFEBABE00000000ULL;
    EXPECT_FALSE(shm_object_check(s));
}

TEST(shm_object_crc, shadow_check_rejects_wrong_abi_version)
{
    shm_object s{};
    s.abi_version = shm_object_abi_version + 1;
    s.crc32_self  = shm_object_compute_crc(s);
    EXPECT_FALSE(shm_object_check(s));
}

TEST(shm_object_crc, property_slab_compute_skips_crc_field_itself)
{
    constexpr std::uint16_t capacity = 2;
    std::vector<std::byte>  buf(property_slab_alloc_size(capacity));
    auto*                   slab = new (buf.data()) property_slab{};
    slab->abi_version            = shm_object_abi_version;
    slab->slot_capacity          = capacity;
    slab->slot_count             = 2;

    auto fill_slot = [](property_slot& s, std::uint32_t key, std::int64_t v) noexcept {
        std::memset(&s, 0, sizeof(s));
        s.key_hash = key;
        s.type     = property_type_tag::int64;
        s.v_int64  = v;
    };
    fill_slot(slab->slots[0], 0xAABBCCDDU, 100);
    fill_slot(slab->slots[1], 0x11223344U, -200);

    slab->crc32 = property_slab_compute_crc(*slab);
    EXPECT_TRUE(property_slab_check(*slab));

    // 篡改 → 失配
    slab->slots[0].v_int64 = 999;
    EXPECT_FALSE(property_slab_check(*slab));
    slab->crc32 = property_slab_compute_crc(*slab);
    EXPECT_TRUE(property_slab_check(*slab));

    // crc32 字段本身的值不影响 compute 结果
    auto crc_before    = property_slab_compute_crc(*slab);
    slab->crc32        = 0xFFFFFFFFU;
    auto crc_after     = property_slab_compute_crc(*slab);
    EXPECT_EQ(crc_before, crc_after);
}

TEST(shm_object_crc, property_slab_check_rejects_count_exceeds_capacity)
{
    constexpr std::uint16_t capacity = 2;
    std::vector<std::byte>  buf(property_slab_alloc_size(capacity));
    auto*                   slab = new (buf.data()) property_slab{};
    slab->abi_version            = shm_object_abi_version;
    slab->slot_capacity          = capacity;
    slab->slot_count             = capacity + 1;
    // 不调用 compute_crc：count>capacity 时 compute 按 slot_count 读会越界访问
    // 已分配 buf 之外的 slot 区域。property_slab_check 先校验 count<=capacity，
    // 因此 crc 字段保持默认 0 不影响 EXPECT_FALSE 断言。
    EXPECT_FALSE(property_slab_check(*slab));
}

TEST(shm_object_crc, child_slab_compute_skips_crc_field_itself)
{
    constexpr std::uint16_t capacity = 3;
    std::vector<std::byte>  buf(child_slab_alloc_size(capacity));
    auto*                   slab = new (buf.data()) child_slab{};
    slab->abi_version            = shm_object_abi_version;
    slab->slot_capacity          = capacity;
    slab->slot_count             = 2;

    // 用两个虚拟地址填 offset_ptr（offset_ptr 是 self-relative，不需要真实对象）
    shm_object dummy_a{};
    shm_object dummy_b{};
    slab->slots[0] = shm_ptr<shm_object>(&dummy_a);
    slab->slots[1] = shm_ptr<shm_object>(&dummy_b);

    slab->crc32 = child_slab_compute_crc(*slab);
    EXPECT_TRUE(child_slab_check(*slab));

    // 篡改 slot 内容（指向不同地址）→ CRC 失配
    slab->slots[0] = shm_ptr<shm_object>(&dummy_b);
    EXPECT_FALSE(child_slab_check(*slab));
    slab->crc32 = child_slab_compute_crc(*slab);
    EXPECT_TRUE(child_slab_check(*slab));
}

TEST(shm_object_crc, child_slab_check_rejects_count_exceeds_capacity)
{
    constexpr std::uint16_t capacity = 2;
    std::vector<std::byte>  buf(child_slab_alloc_size(capacity));
    auto*                   slab = new (buf.data()) child_slab{};
    slab->abi_version            = shm_object_abi_version;
    slab->slot_capacity          = capacity;
    slab->slot_count             = capacity + 1;
    // 同 property_slab：count>capacity 时 compute 会越界，依赖 check 的容量预检拒绝。
    EXPECT_FALSE(child_slab_check(*slab));
}

TEST(shm_object_crc, empty_slabs_are_valid)
{
    {
        std::vector<std::byte> buf(property_slab_alloc_size(0));
        auto*                  slab = new (buf.data()) property_slab{};
        slab->abi_version           = shm_object_abi_version;
        slab->crc32                 = property_slab_compute_crc(*slab);
        EXPECT_TRUE(property_slab_check(*slab));
    }
    {
        std::vector<std::byte> buf(child_slab_alloc_size(0));
        auto*                  slab = new (buf.data()) child_slab{};
        slab->abi_version           = shm_object_abi_version;
        slab->crc32                 = child_slab_compute_crc(*slab);
        EXPECT_TRUE(child_slab_check(*slab));
    }
}

}  // namespace
