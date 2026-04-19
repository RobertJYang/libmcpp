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

#ifndef MC_SHM_SRC_ALLOCATOR_SLAB_H
#define MC_SHM_SRC_ALLOCATOR_SLAB_H

#include "internal.h"

namespace mc::shm::detail {

// 初始化 slab 控制块（默认 7 个 class: 16/24/32/48/64/96/128）
bool slab_init(void* base, arena_header* arena) noexcept;

// 无锁快路径分配：仅 CAS pop，不持 arena_lock；stack 空返回 0（由 shm_allocator
// 持锁走 slab_try_allocate 的 wholesale 路径）
std::uint64_t slab_fast_allocate(void* base, arena_header* arena, std::size_t size,
                                 std::size_t alignment) noexcept;

// 无锁快路径释放：payload 对应 chunk 有 slab magic 则 bitmap_clear + CAS push，
// 返回 true；不属 slab 返回 false（由 shm_allocator 持锁走 TLSF 释放）
bool slab_fast_deallocate(void* base, arena_header* arena, std::uint64_t payload_offset) noexcept;

// 持 arena_lock 慢路径：快路径失败时触发 wholesale，再试快路径
// 成功返回 payload offset；不能走 slab 返回 0（由调用者 fallback 到 TLSF）
std::uint64_t slab_try_allocate(void* base, arena_header* arena, std::size_t size, std::size_t alignment) noexcept;

// 持 arena_lock 慢路径 deallocate：与快路径逻辑等价（保留该 API 兼容旧调用点）
bool slab_try_deallocate(void* base, arena_header* arena, std::uint64_t payload_offset) noexcept;

// Phase 3 实装
void slab_recover(void* base, arena_header* arena) noexcept;

// 一致性扫描
integrity_status slab_check_integrity(const void* base, const arena_header* arena) noexcept;

// 选择 slot_size 合适的 class；size 超出范围时返回 UINT32_MAX
std::uint32_t slab_find_class(const slab_control& ctrl, std::size_t size, std::size_t alignment) noexcept;

// 给定 slot offset，找到所属 chunk header；失败返回 nullptr
slab_chunk_header* slab_chunk_of_slot(void* base, arena_header* arena, std::uint64_t slot_offset) noexcept;

} // namespace mc::shm::detail

#endif // MC_SHM_SRC_ALLOCATOR_SLAB_H
