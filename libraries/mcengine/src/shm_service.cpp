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

#include "shm_service.h"

#include <cstddef>
#include <cstdint>

#include <mc/engine/shm_object.h>

namespace mc::engine {

namespace {

// shm_service CRC 覆盖范围：从 pid 字段开始（offset 8）到结构末尾，
// 跳过 abi_version/state/pad/crc32_self 这些前缀字段。
constexpr std::size_t k_service_crc_offset = offsetof(shm_service, pid);
constexpr std::size_t k_service_crc_size   = sizeof(shm_service) - k_service_crc_offset;
static_assert(k_service_crc_size == 56U, "shm_service CRC 覆盖范围必须是 56B");

} // namespace

std::uint32_t shm_service_compute_crc(const shm_service& svc) noexcept
{
    const auto* base = reinterpret_cast<const std::uint8_t*>(&svc) + k_service_crc_offset;
    return crc32_ieee(base, k_service_crc_size);
}

bool shm_service_check(const shm_service& svc) noexcept
{
    if (svc.abi_version != shm_service_abi_version) {
        return false;
    }
    return shm_service_compute_crc(svc) == svc.crc32_self;
}

} // namespace mc::engine
