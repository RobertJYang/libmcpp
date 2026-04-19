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
#include <mc/engine/shm_object.h>  // shm_ptr / shm_byte_string 别名

namespace mc::engine {

// 不兼容布局变化必须 bump
inline constexpr std::uint16_t shm_service_abi_version = 1;

enum class shm_service_state : std::uint8_t {
    alive    = 0,
    detached = 1,
};

// shm_service 字段布局（offset / size，总 64B）：
//    0  +2  abi_version
//    2  +1  state
//    3  +1  _hdr_pad
//    4  +4  crc32_self       覆盖 pid..._reserved
//    8  +4  pid              0 = 未持有
//   12  +4  _pad32
//   16  +8  epoch            每次 attach 时 ++
//   24  +8  service_name
//   32  +32 _reserved[4]

struct shm_service {
    std::uint16_t            abi_version;
    std::uint8_t             state;
    std::uint8_t             _hdr_pad;
    std::uint32_t            crc32_self;
    std::uint32_t            pid;
    std::uint32_t            _pad32;
    std::uint64_t            epoch;
    shm_ptr<shm_byte_string> service_name;
    std::uint64_t            _reserved[4];
};

static_assert(sizeof(shm_service) == 64U, "shm_service ABI 锁定 64B");
static_assert(alignof(shm_service) == 8U, "shm_service 必须 8B 对齐");

MC_API std::uint32_t shm_service_compute_crc(const shm_service& svc) noexcept;
MC_API bool          shm_service_check(const shm_service& svc) noexcept;

} // namespace mc::engine

#endif // MC_ENGINE_SHM_SERVICE_H
