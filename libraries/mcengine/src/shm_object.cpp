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

#include <mc/engine/shm_object.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace mc::engine {

namespace {

// IEEE 802.3 反射多项式 0xEDB88320 的 table-based 实现
constexpr std::array<std::uint32_t, 256> build_crc32_table() noexcept
{
    std::array<std::uint32_t, 256> table{};
    constexpr std::uint32_t        poly = 0xEDB88320U;
    for (std::uint32_t i = 0; i < 256U; ++i) {
        std::uint32_t c = i;
        for (int j = 0; j < 8; ++j) {
            c = (c & 1U) ? (poly ^ (c >> 1)) : (c >> 1);
        }
        table[i] = c;
    }
    return table;
}

constexpr auto k_crc32_table = build_crc32_table();

// shm_object CRC 覆盖范围：从 object_id 字段开始（offset 8）到结构末尾
constexpr std::size_t k_shadow_crc_offset = offsetof(shm_object, object_id);
constexpr std::size_t k_shadow_crc_size   = sizeof(shm_object) - k_shadow_crc_offset;
static_assert(k_shadow_crc_size == 72U, "shm_object CRC 覆盖范围必须是 72B");

// slab CRC 覆盖范围辅助：从 0 开始到 slots 末尾，但跳过 crc32 字段本身
constexpr std::size_t k_property_slab_crc_field_offset = offsetof(property_slab, crc32);
constexpr std::size_t k_child_slab_crc_field_offset    = offsetof(child_slab, crc32);

// 通用：从 base + offset 起算 len 字节，更新累积 crc
inline std::uint32_t crc32_update(std::uint32_t crc, const std::uint8_t* p, std::size_t len) noexcept
{
    for (std::size_t i = 0; i < len; ++i) {
        crc = k_crc32_table[(crc ^ p[i]) & 0xFFU] ^ (crc >> 8);
    }
    return crc;
}

// slab 通用 CRC：跳过位于 [crc_field_offset, crc_field_offset+4) 的 crc32 字段本身
inline std::uint32_t slab_compute_crc_skip_field(const void* slab_ptr, std::size_t header_size,
                                                 std::size_t   crc_field_offset,
                                                 std::size_t   slot_payload_size) noexcept
{
    const auto*   base = static_cast<const std::uint8_t*>(slab_ptr);
    std::uint32_t crc  = 0xFFFFFFFFU;
    crc                = crc32_update(crc, base, crc_field_offset);  // 头部前段
    const std::size_t after_crc = crc_field_offset + sizeof(std::uint32_t);
    crc = crc32_update(crc, base + after_crc, header_size - after_crc + slot_payload_size);
    return crc ^ 0xFFFFFFFFU;
}

}  // namespace

std::uint32_t crc32_ieee(const void* data, std::size_t len) noexcept
{
    if (data == nullptr || len == 0U) {
        return 0U;
    }
    return crc32_update(0xFFFFFFFFU, static_cast<const std::uint8_t*>(data), len) ^ 0xFFFFFFFFU;
}

std::uint32_t shm_object_compute_crc(const shm_object& s) noexcept
{
    const auto* base = reinterpret_cast<const std::uint8_t*>(&s) + k_shadow_crc_offset;
    return crc32_ieee(base, k_shadow_crc_size);
}

bool shm_object_check(const shm_object& s) noexcept
{
    if (s.abi_version != shm_object_abi_version) {
        return false;
    }
    return shm_object_compute_crc(s) == s.crc32_self;
}

std::uint32_t property_slab_compute_crc(const property_slab& slab) noexcept
{
    return slab_compute_crc_skip_field(&slab, sizeof(property_slab),
                                       k_property_slab_crc_field_offset,
                                       static_cast<std::size_t>(slab.slot_count)
                                           * sizeof(property_slot));
}

bool property_slab_check(const property_slab& slab) noexcept
{
    if (slab.abi_version != shm_object_abi_version) {
        return false;
    }
    if (slab.slot_count > slab.slot_capacity) {
        return false;
    }
    return property_slab_compute_crc(slab) == slab.crc32;
}

std::uint32_t child_slab_compute_crc(const child_slab& slab) noexcept
{
    return slab_compute_crc_skip_field(
        &slab, sizeof(child_slab), k_child_slab_crc_field_offset,
        static_cast<std::size_t>(slab.slot_count)
            * sizeof(mc::intrusive::offset_ptr<shm_object>));
}

bool child_slab_check(const child_slab& slab) noexcept
{
    if (slab.abi_version != shm_object_abi_version) {
        return false;
    }
    if (slab.slot_count > slab.slot_capacity) {
        return false;
    }
    return child_slab_compute_crc(slab) == slab.crc32;
}

}  // namespace mc::engine
