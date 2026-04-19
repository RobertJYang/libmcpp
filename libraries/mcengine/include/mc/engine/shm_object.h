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

#ifndef MC_ENGINE_SHM_OBJECT_H
#define MC_ENGINE_SHM_OBJECT_H

#include <cstddef>
#include <cstdint>

#include <mc/common.h>
#include <mc/intrusive/offset_ptr.h>
#include <mc/shm/byte_string.h>
#include <mc/shm/container/journal.h>

namespace mc::engine {

// SHM 类型短别名（mcengine 内通用）
template <typename T>
using shm_ptr              = mc::intrusive::offset_ptr<T>;
using shm_byte_string      = mc::shm::byte_string;
using shm_byte_string_less = mc::shm::byte_string_less;

// 不兼容布局变化必须 bump
inline constexpr std::uint16_t shm_object_abi_version = 3;

struct shm_service;

enum class property_type_tag : std::uint8_t {
    null    = 0,
    int64   = 1,
    string  = 2,
    bytes   = 3,
    double_ = 4,
};

MC_API const char* property_type_tag_name(property_type_tag t) noexcept;

struct property_slot {
    std::uint32_t                                   key_hash;
    std::uint32_t                                   _pad0;
    shm_ptr<shm_byte_string> key;
    property_type_tag        type;
    std::uint8_t             _pad1[3];
    std::uint32_t            _pad2;
    union {
        std::int64_t             v_int64;
        double                   v_double;
        shm_ptr<shm_byte_string> v_blob;
    };
};

static_assert(sizeof(property_slot) == 32U, "property_slot ABI 锁定 32B");
static_assert(alignof(property_slot) == 8U, "property_slot 必须 8B 对齐");

struct property_slab {
    std::uint16_t abi_version;
    std::uint16_t slot_capacity;
    std::uint16_t slot_count;
    std::uint16_t flags;
    std::uint32_t crc32;
    std::uint32_t _pad;
    property_slot slots[];
};

static_assert(sizeof(property_slab) == 16U, "property_slab 头部 ABI 锁定 16B");
static_assert(alignof(property_slab) == 8U, "property_slab 必须 8B 对齐");

constexpr std::size_t property_slab_alloc_size(std::uint16_t capacity) noexcept
{
    return sizeof(property_slab) + static_cast<std::size_t>(capacity) * sizeof(property_slot);
}

struct child_slab {
    std::uint16_t           abi_version;
    std::uint16_t           slot_capacity;
    std::uint16_t           slot_count;
    std::uint16_t           flags;
    std::uint32_t           crc32;
    std::uint32_t           _pad;
    shm_ptr<struct shm_object> slots[];
};

static_assert(sizeof(child_slab) == 16U, "child_slab 头部 ABI 锁定 16B");
static_assert(alignof(child_slab) == 8U, "child_slab 必须 8B 对齐");

constexpr std::size_t child_slab_alloc_size(std::uint16_t capacity) noexcept
{
    return sizeof(child_slab) +
           static_cast<std::size_t>(capacity) * sizeof(shm_ptr<struct shm_object>);
}

// shm_object 字段布局（offset / size，总 192B）：
//    0  +2  abi_version
//    2  +1  flags            bit0=alive, bit1=isolated, bit2=under_repair
//    3  +1  _hdr_pad
//    4  +4  crc32_self       覆盖 object_id..children
//    8  +8  object_id
//   16  +8  class_name
//   24  +8  name
//   32  +8  path
//   40  +8  position
//   48  +8  service          null = 未归属
//   56  +8  parent           null = 根
//   64  +8  properties
//   72  +8  children
//   80 +48  _padding
//  128 +64  journal          container_journal（写者私有事务簿，CRC 不覆盖）

namespace shm_object_flags {
inline constexpr std::uint8_t alive        = 1U << 0;
inline constexpr std::uint8_t isolated     = 1U << 1;
inline constexpr std::uint8_t under_repair = 1U << 2;
} // namespace shm_object_flags

struct shm_object {
    std::uint16_t abi_version;
    std::uint8_t  flags;
    std::uint8_t  _hdr_pad;
    std::uint32_t crc32_self;
    std::uint64_t object_id;

    shm_ptr<shm_byte_string> class_name;
    shm_ptr<shm_byte_string> name;
    shm_ptr<shm_byte_string> path;
    shm_ptr<shm_byte_string> position;
    shm_ptr<shm_service>     service;
    shm_ptr<shm_object>      parent;
    shm_ptr<property_slab>   properties;
    shm_ptr<child_slab>      children;

    mc::shm::container::container_journal journal;
};

static_assert(sizeof(shm_object) == 192U, "shm_object ABI 锁定 192B");
static_assert(alignof(shm_object) == 64U, "shm_object 因内嵌 64B journal 须 64B 对齐");
static_assert(offsetof(shm_object, journal) == 128U, "journal 因 container_journal alignas(64) 落在 offset 128");

MC_API std::uint32_t crc32_ieee(const void* data, std::size_t len) noexcept;

MC_API std::uint32_t shm_object_compute_crc(const shm_object& s) noexcept;
MC_API bool          shm_object_check(const shm_object& s) noexcept;

MC_API std::uint32_t property_slab_compute_crc(const property_slab& slab) noexcept;
MC_API bool          property_slab_check(const property_slab& slab) noexcept;

MC_API std::uint32_t child_slab_compute_crc(const child_slab& slab) noexcept;
MC_API bool          child_slab_check(const child_slab& slab) noexcept;

} // namespace mc::engine

#endif // MC_ENGINE_SHM_OBJECT_H
