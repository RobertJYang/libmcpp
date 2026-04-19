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
#include <mc/engine/shm_service.h>
#include <mc/intrusive/offset_ptr.h>
#include <mc/shm/allocator.h>
#include <mc/shm/byte_string.h>
#include <mc/shm/container/map.h>

// mc::engine::shm_service ops
//
// 围绕 shm_service POD 的写入/读取/释放工具集。布局/语义参考 shm_service.h。
//
// 三层 API：
//
//   1. 单 POD ops（create / destroy / setter / CRC 刷新）
//   2. shm_service_map 类型别名：系统级 service 注册表的具体类型
//   3. attach 高层入口：以 (map, service_name, pid) 完成"已存在则接管 / 不存在则创建"
//      的原子语义
//
// 错误处理：所有函数 noexcept；分配失败返回 nullptr / false，绝不抛异常。

namespace mc::engine {

// ============================================================================
// 单 POD ops
// ============================================================================

// 在 SHM 中分配一个 shm_service POD，service_name 字符串拷贝进 SHM byte_string。
// 初始：state=alive、pid=initial_pid、epoch=initial_epoch、_reserved 全 0、CRC 已写入。
// 任一子分配失败回滚并返回 nullptr。
MC_API shm_service* shm_service_create(mc::shm::shm_allocator& alloc,
                                       std::string_view        service_name,
                                       std::uint32_t           initial_pid,
                                       std::uint64_t           initial_epoch = 1U) noexcept;

// 销毁 shm_service POD：释放 service_name byte_string + POD 自身。
// 不动 shm_service_map 中的索引（调用方在 attach/detach 流程内自行处理）。
// svc 为 nullptr 时为 no-op。
MC_API void shm_service_destroy(mc::shm::shm_allocator& alloc, shm_service* svc) noexcept;

// 单字段 setter：自动刷新 CRC。
MC_API void shm_service_set_pid(shm_service& svc, std::uint32_t pid) noexcept;
MC_API void shm_service_set_state(shm_service& svc, shm_service_state state) noexcept;

// epoch 自增 1，刷新 CRC。返回自增后的新值。
MC_API std::uint64_t shm_service_increment_epoch(shm_service& svc) noexcept;

// 重算并写入 svc.crc32_self。
// 只有直接操纵 svc 字段（绕过 ops API）后才需要手动调用。
MC_API void shm_service_refresh_crc(shm_service& svc) noexcept;

// 读取 service_name 为 string_view（svc 中 byte_string 为空时返回空 view）。
MC_API std::string_view shm_service_name(const shm_service& svc) noexcept;

// ============================================================================
// shm_service_map 类型别名
// ============================================================================
//
// 系统级 SHM 单例（well-known name 由 mcengine runtime 在更高层选定）：
// 跨所有 mcengine 进程共享，作为 service attach 的入口表。
//
// 容器选用 shm::container::map（skip list 实现，crash-safe，跨进程读写安全）。
// 比较器使用 byte_string_less（字节序），与 shm_storage_engine index 一致。

using shm_service_map = mc::shm::container::map<mc::shm::byte_string,
                                                mc::intrusive::offset_ptr<shm_service>,
                                                mc::shm::byte_string_less>;

// ============================================================================
// attach 高层入口
// ============================================================================
//
// 语义：
//   - service_name 已存在于 map：更新 pid + epoch++，进入 recover 路径（由调用方触发）
//   - service_name 不存在：分配新 shm_service POD（pid=current_pid, epoch=1）并入表
//
// 返回 attach 后的 shm_service*；任一步分配失败返回 nullptr。
//
// 注意：本函数只完成 attach 的"shm 层"动作。recover（重建 heap pool / 关系图）由
// shm_storage_engine.recover() 单独完成，由 mcengine service 启动逻辑串起来。
MC_API shm_service* shm_service_attach(mc::shm::shm_allocator& alloc,
                                       shm_service_map&        map,
                                       std::string_view        service_name,
                                       std::uint32_t           current_pid) noexcept;

}  // namespace mc::engine

#endif  // MC_ENGINE_SHM_SERVICE_OPS_H
