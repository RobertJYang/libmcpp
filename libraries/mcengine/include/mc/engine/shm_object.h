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

// mc::engine::shm_object
//
// SHM 中持久化的对象本体——崩溃一致的 POD 视图。
//
// **分层归属**：本类型属于 mcengine 业务层（描述 abstract_object 的持久化形态）。
// mcbase/shm 只提供 byte_string / container / runtime 等纯共享内存机制，不应该
// 知道 shm_object 的存在。本头文件依赖 mcbase 的 offset_ptr + byte_string，
// 但反向 mcbase 不依赖 mcengine。
//
// 设计契约：
//   D1 SHM 对象是真理之源；heap 对象是 runtime 实例
//   D2 重建走 mc::engine::class_factory
//   D3 properties 用"内嵌 slab + copy-on-grow"
//   D4 parent/children **独立持久化**（managed_objects 是生命周期关系，不可从
//      path 派生；path 仅是逻辑组织字段）
//   D5 interface / observer / property::extension_data 不入 SHM
//
// 布局原则：
//   - sizeof 不刻意凑某个值；只保证 abi_version 在 offset 0 + 字段 offset 稳定
//     + 8B 对齐 + 加字段时 bump abi_version
//   - 修改 property/child 只动对应 slab + 自身 CRC，不动 shm_object 主体

namespace mc::engine {

// ============================================================================
// ABI 版本（不兼容布局变化必须 bump）
// ============================================================================

inline constexpr std::uint16_t shm_object_abi_version = 2;

// 前向声明：shm_service 的具体类型在 shm_service.h 中定义；
// shm_object 仅以 offset_ptr 形式引用 service，不需要完整定义。
struct shm_service;

// ============================================================================
// property 值类型 tag（u8 编码）
// ============================================================================

enum class property_type_tag : std::uint8_t {
    null    = 0,
    int64   = 1,
    string  = 2,  // 通过 offset_ptr<byte_string> 引用 SHM 内字符串
    bytes   = 3,
    double_ = 4,
    // 5..255 预留
};

inline const char* property_type_tag_name(property_type_tag t) noexcept
{
    switch (t) {
        case property_type_tag::null:    return "null";
        case property_type_tag::int64:   return "int64";
        case property_type_tag::string:  return "string";
        case property_type_tag::bytes:   return "bytes";
        case property_type_tag::double_: return "double";
    }
    return "unknown";
}

// ============================================================================
// property_slot —— 单个 property 的 32B 固定槽
// ============================================================================

struct property_slot {
    std::uint32_t                                   key_hash;
    std::uint32_t                                   _pad0;
    mc::intrusive::offset_ptr<mc::shm::byte_string> key;
    property_type_tag                               type;
    std::uint8_t                                    _pad1[3];
    std::uint32_t                                   _pad2;
    union {
        std::int64_t                                    v_int64;
        double                                          v_double;
        mc::intrusive::offset_ptr<mc::shm::byte_string> v_blob;  // string + bytes 共用
    };
};

static_assert(sizeof(property_slot) == 32U, "property_slot ABI 锁定 32B");
static_assert(alignof(property_slot) == 8U, "property_slot 必须 8B 对齐");

// ============================================================================
// property_slab —— 一次性分配的连续 property 槽位数组
// ============================================================================
//
// 写入语义：原地写 + 重算 crc32（无 journal；上层提供事务边界）
// 容量不足：alloc 新 slab（capacity ×= 2）→ 复制旧 → 切换 shadow.properties → free 旧
// 删除 slot：将最后一个 slot move 到删除位 + slot_count--（O(1) swap-and-pop）

struct property_slab {
    std::uint16_t abi_version;    // = shm_object_abi_version
    std::uint16_t slot_capacity;
    std::uint16_t slot_count;
    std::uint16_t flags;          // bit0=under_repair；其他位预留
    std::uint32_t crc32;          // 覆盖 abi_version..slots[slot_count-1]，跳过自身字段
    std::uint32_t _pad;
    property_slot slots[];        // 柔性数组；分配大小 = sizeof(property_slab) + capacity * 32
};

static_assert(sizeof(property_slab) == 16U, "property_slab 头部 ABI 锁定 16B");
static_assert(alignof(property_slab) == 8U, "property_slab 必须 8B 对齐");

constexpr std::size_t property_slab_alloc_size(std::uint16_t capacity) noexcept
{
    return sizeof(property_slab) + static_cast<std::size_t>(capacity) * sizeof(property_slot);
}

// ============================================================================
// child_slab —— 一次性分配的连续 child 引用数组
// ============================================================================
//
// 与 property_slab 同模式：inline 数组 + copy-on-grow + CRC。
// 每槽 8B：仅 offset_ptr<shm_object>。
//
// 之所以独立设计 child_slab 而非借用 property_slab：
//   - children 是生命周期关系（owner→child 强引用），删除 child 必须级联 destroy
//   - properties 是属性快照，删除只是 unset 字段
//   - 类型不同，迭代/CRC 边界不同，混用会导致接口含糊

struct child_slab {
    std::uint16_t                            abi_version;
    std::uint16_t                            slot_capacity;
    std::uint16_t                            slot_count;
    std::uint16_t                            flags;
    std::uint32_t                            crc32;
    std::uint32_t                            _pad;
    mc::intrusive::offset_ptr<struct shm_object> slots[];  // 柔性数组；8B/slot
};

static_assert(sizeof(child_slab) == 16U, "child_slab 头部 ABI 锁定 16B");
static_assert(alignof(child_slab) == 8U, "child_slab 必须 8B 对齐");

constexpr std::size_t child_slab_alloc_size(std::uint16_t capacity) noexcept
{
    return sizeof(child_slab)
           + static_cast<std::size_t>(capacity)
                 * sizeof(mc::intrusive::offset_ptr<struct shm_object>);
}

// ============================================================================
// shm_object —— 主体
// ============================================================================
//
// 字段总览（offset / size）：
//    0  +2  abi_version
//    2  +1  flags            bit0=alive, bit1=isolated, bit2=under_repair
//    3  +1  _hdr_pad
//    4  +4  crc32_self       覆盖 object_id..children（不覆盖被指 payload）
//    8  +8  object_id
//   16  +8  class_name       offset_ptr<byte_string>
//   24  +8  name             offset_ptr<byte_string>
//   32  +8  path             offset_ptr<byte_string>   // 逻辑组织字段，非 ownership
//   40  +8  position         offset_ptr<byte_string>
//   48  +8  service          offset_ptr<shm_service>，null = 未归属任一 service
//   56  +8  parent           offset_ptr<shm_object>，null = 根（生命周期 owner）
//   64  +8  properties       offset_ptr<property_slab>，null = 无 property
//   72  +8  children         offset_ptr<child_slab>，null = 无 child
// 总计 80B
//
// 字段语义提醒：
//   - **parent / children = 生命周期关系**（parent destroy 时级联 destroy children）
//   - **path = 逻辑组织字段**（命名空间标签，与 ownership 完全独立）
//   - **service = 归属关系**（"哪个 service 在持有此对象"，pid 由 service->pid 提供，
//     单一来源；shm_object 不存 pid）
//   - 不能从 path 前缀枚举推 children！

namespace shm_object_flags {
inline constexpr std::uint8_t alive        = 1U << 0;
inline constexpr std::uint8_t isolated     = 1U << 1;  // class_name 未注册 / CRC 失败 → 隔离
inline constexpr std::uint8_t under_repair = 1U << 2;  // reconcile 期间临时标志
}  // namespace shm_object_flags

struct shm_object {
    std::uint16_t abi_version;
    std::uint8_t  flags;
    std::uint8_t  _hdr_pad;
    std::uint32_t crc32_self;
    std::uint64_t object_id;

    mc::intrusive::offset_ptr<mc::shm::byte_string> class_name;
    mc::intrusive::offset_ptr<mc::shm::byte_string> name;
    mc::intrusive::offset_ptr<mc::shm::byte_string> path;
    mc::intrusive::offset_ptr<mc::shm::byte_string> position;
    mc::intrusive::offset_ptr<shm_service>          service;     // 所属 service（null=未归属）
    mc::intrusive::offset_ptr<shm_object>           parent;      // 生命周期 owner（null=根）
    mc::intrusive::offset_ptr<property_slab>        properties;
    mc::intrusive::offset_ptr<child_slab>           children;    // 生命周期 children
};

static_assert(sizeof(shm_object) == 80U, "shm_object ABI 锁定 80B");
static_assert(alignof(shm_object) == 8U, "shm_object 必须 8B 对齐");

// ============================================================================
// CRC32（IEEE 802.3 多项式 0xEDB88320，反射形式）
// ============================================================================
//
// shadow / property_slab / child_slab 都用此 CRC 保护
// table-based 实现，每字节 1 次 lookup，算力可忽略

MC_API std::uint32_t crc32_ieee(const void* data, std::size_t len) noexcept;

// 计算 shm_object 主体应有的 crc32（覆盖 object_id..children）
MC_API std::uint32_t shm_object_compute_crc(const shm_object& s) noexcept;
MC_API bool          shm_object_check(const shm_object& s) noexcept;

// 计算 property_slab 应有的 crc32（覆盖 abi_version..slots[slot_count-1]，跳过 crc32 字段本身）
MC_API std::uint32_t property_slab_compute_crc(const property_slab& slab) noexcept;
MC_API bool          property_slab_check(const property_slab& slab) noexcept;

// 计算 child_slab 应有的 crc32（同上）
MC_API std::uint32_t child_slab_compute_crc(const child_slab& slab) noexcept;
MC_API bool          child_slab_check(const child_slab& slab) noexcept;

}  // namespace mc::engine

#endif  // MC_ENGINE_SHM_OBJECT_H
