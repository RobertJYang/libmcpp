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

#ifndef MC_ENGINE_SHM_SERVICE_H
#define MC_ENGINE_SHM_SERVICE_H

#include <cstddef>
#include <cstdint>

#include <mc/common.h>
#include <mc/intrusive/offset_ptr.h>
#include <mc/shm/byte_string.h>

// mc::engine::shm_service
//
// SHM 中持久化的 service 标识 POD。
//
// 一句话：service 在共享内存中的"first-class 表达"。
//   - service_name + pid + epoch 在 shm 中单一可信来源
//   - shm_object 以 offset_ptr<shm_service> 引用所属 service，本身不存 pid
//     （进程换代时只更新 shm_service.pid 一处，所有 shm_object 零修改）
//
// 生命周期：
//   - shm_service POD 由 attach 流程在 shm_service_map 中懒创建
//   - 进程 crash 不删除（pid 死活不影响 shm 存在性，由 mcengine 业务层判断）
//   - 接管：新进程 attach 同名 service → 直接更新 pid + epoch++
//
// shm 层只承诺：shm_service 的"存在性"独立于任何进程的生命周期。
// 不承诺：检测进程死活、广播 crash 事件、标记任何状态变化。

namespace mc::engine {

// ============================================================================
// ABI 版本（不兼容布局变化必须 bump）
// ============================================================================

inline constexpr std::uint16_t shm_service_abi_version = 1;

// ============================================================================
// state 枚举
// ============================================================================
//
// 注：crash 状态由"pid 已死"隐式表示，shm 层不显式标记。

enum class shm_service_state : std::uint8_t {
    alive    = 0,    // 正常持有
    detached = 1,    // 主动解除关联（保留枚举值）
};

// ============================================================================
// shm_service —— 主体
// ============================================================================
//
// 字段总览（offset / size）：
//    0  +2  abi_version
//    2  +1  state            shm_service_state
//    3  +1  _hdr_pad
//    4  +4  crc32_self       覆盖 pid..._reserved（不覆盖 abi_version/state/pad）
//    8  +4  pid              当前持有进程 pid（0 = 未持有）
//   12  +4  _pad32           epoch 8B 对齐填充
//   16  +8  epoch            每次 attach 时 ++，跨进程一致性版本号
//   24  +8  service_name     offset_ptr<byte_string>，标识 service 名称
//   32  +32 _reserved[4]     预留 32B 用于扩展（保持 64B 总长 + 8B 对齐）
// 总计 64B

struct shm_service {
    std::uint16_t                                   abi_version;
    std::uint8_t                                    state;
    std::uint8_t                                    _hdr_pad;
    std::uint32_t                                   crc32_self;
    std::uint32_t                                   pid;
    std::uint32_t                                   _pad32;
    std::uint64_t                                   epoch;
    mc::intrusive::offset_ptr<mc::shm::byte_string> service_name;
    std::uint64_t                                   _reserved[4];
};

static_assert(sizeof(shm_service) == 64U, "shm_service ABI 锁定 64B");
static_assert(alignof(shm_service) == 8U, "shm_service 必须 8B 对齐");

// ============================================================================
// CRC32（与 shm_object 共用 IEEE 802.3 多项式实现）
// ============================================================================

// 计算 shm_service 主体应有的 crc32（覆盖 pid 起到结构末尾，跳过 abi_version/state/pad/crc）
MC_API std::uint32_t shm_service_compute_crc(const shm_service& svc) noexcept;
MC_API bool          shm_service_check(const shm_service& svc) noexcept;

}  // namespace mc::engine

#endif  // MC_ENGINE_SHM_SERVICE_H
