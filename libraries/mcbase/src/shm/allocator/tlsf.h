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

#ifndef MC_SHM_SRC_ALLOCATOR_TLSF_H
#define MC_SHM_SRC_ALLOCATOR_TLSF_H

#include <mc/common.h>
#include "internal.h"

namespace mc::shm::detail {

// ================ TLSF 核心接口 ================
//
// 以下所有函数都假设调用者已经持有 arena->arena_lock（除 tlsf_check_integrity 外）。

// 初始化 TLSF 控制块与首个 free block
// arena_base: arena 起点；arena 中已完成 arena_header 的原地构造
// 返回 true 表示成功
MC_API bool tlsf_init(void* arena_base, arena_header* arena) noexcept;

// 分配 size 字节，对齐 alignment；返回 block payload 的 offset（相对 arena base）
// 失败时返回 0
MC_API std::uint64_t tlsf_allocate(void* arena_base, arena_header* arena,
                            std::size_t size, std::size_t alignment) noexcept;

// 按 payload offset 释放
MC_API void tlsf_deallocate(void* arena_base, arena_header* arena, std::uint64_t payload_offset) noexcept;

// 根据 journal 执行 recover
void tlsf_recover(void* arena_base, arena_header* arena) noexcept;

// 一致性扫描；返回 ok 或具体错误
integrity_status tlsf_check_integrity(const void* arena_base, const arena_header* arena) noexcept;

// ================ 内部工具（供 slab / journal 使用） ================

// 计算可分配的 block 总大小（含 header），根据 user 请求 size + alignment
std::size_t tlsf_required_block_size(std::size_t size, std::size_t alignment) noexcept;

// block 起点 offset → payload offset
MC_API std::uint64_t tlsf_payload_offset(std::uint64_t block_offset) noexcept;

// payload offset → block 起点 offset
MC_API std::uint64_t tlsf_block_offset_from_payload(std::uint64_t payload_offset) noexcept;

} // namespace mc::shm::detail

#endif // MC_SHM_SRC_ALLOCATOR_TLSF_H
