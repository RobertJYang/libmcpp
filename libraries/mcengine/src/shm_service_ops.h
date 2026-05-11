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

#ifndef MC_ENGINE_SHM_SERVICE_OPS_H
#define MC_ENGINE_SHM_SERVICE_OPS_H

#include <cstdint>
#include <string_view>

#include <mc/common.h>
#include <mc/shm/container/map.h>

#include "shm_object_ops.h"
#include "shm_service.h"

// shm_service POD 写读释放工具集。
// 所有函数 noexcept；分配失败返回 nullptr / false。

namespace mc::engine {

// 子分配失败回滚返回 nullptr。
MC_API shm_service* shm_service_create(shm_allocator& alloc, std::string_view service_name,
                                       std::uint32_t initial_pid,
                                       std::uint64_t initial_epoch = 1U) noexcept;
// 不动 shm_service_map 索引。svc 为 nullptr 时 no-op。
MC_API void shm_service_destroy(shm_allocator& alloc, shm_service* svc) noexcept;

MC_API void          shm_service_set_pid(shm_service& svc, std::uint32_t pid) noexcept;
MC_API void          shm_service_set_state(shm_service& svc, shm_service_state state) noexcept;
MC_API std::uint64_t shm_service_increment_epoch(shm_service& svc) noexcept;

// 仅在绕过 ops API 直改字段后调用。
MC_API void shm_service_refresh_crc(shm_service& svc) noexcept;

MC_API std::string_view shm_service_name(const shm_service& svc) noexcept;

// 跨进程 service 注册表。
using shm_service_map =
    mc::shm::container::map<shm_byte_string, shm_ptr<shm_service>, shm_byte_string_less>;

// 已存在则接管（pid 更新 + epoch++），不存在则创建（pid=current_pid, epoch=1）。
// force=false 且旧 owner pid 仍存活则拒绝并返回 nullptr。recover 由调用方触发。
MC_API shm_service* shm_service_attach(shm_allocator& alloc, shm_service_map& map,
                                       std::string_view service_name, std::uint32_t current_pid,
                                       bool force = false) noexcept;

}  // namespace mc::engine

#endif  // MC_ENGINE_SHM_SERVICE_OPS_H
