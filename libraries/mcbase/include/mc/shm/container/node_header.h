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

#ifndef MC_SHM_CONTAINER_NODE_HEADER_H
#define MC_SHM_CONTAINER_NODE_HEADER_H

#include <atomic>
#include <cstdint>
#include <mc/common.h>

// 共享内存 crash-safe 容器的节点头部
//
// 设计要点：
//  - 每个节点以 16B header 开头，保护节点的归属、类型、状态
//  - magic 标识容器类型（list / set / map 等），fsck 时用于甄别节点
//  - owner_offset 反向指向容器控制块，用于归属校验与 dangling 检测
//  - state 表达节点当前是否处于某个 journal op 的中间态
//  - checksum 保护 header 前三字段的一致性（crc16-ccitt）

namespace mc::shm::container {

// ---- 预定义 node magic ----
//
// 每种容器使用固定的 8 字节 ASCII 字面量作为 magic，便于 hex dump 阅读
// 升级节点布局时 bump version（低字节）以避免老节点被误判为当前版本
constexpr std::uint64_t list_node_magic = 0x4D434C53'4E4F4431ULL; // "MCLSNOD1"
constexpr std::uint64_t set_node_magic  = 0x4D435253'4E4F4431ULL; // "MCRSNOD1"
constexpr std::uint64_t map_node_magic  = 0x4D434D50'4E4F4431ULL; // "MCMPNOD1"

// ---- 节点状态 ----
//
// 状态转换：
//   free  →  pending  →  linked    （insert 路径）
//   linked → pending  →  free      （erase 路径）
// 任意节点若被 recover 判定 header checksum 失配 / 不变量违反，
// 则单向进入 degraded；一旦 degraded 不再回到其它状态（避免损坏扩散）
enum class node_state : std::uint8_t {
    free     = 0,
    linked   = 1,
    pending  = 2,
    degraded = 3,
};

inline const char* node_state_name(node_state s) noexcept
{
    switch (s) {
        case node_state::free:
            return "free";
        case node_state::linked:
            return "linked";
        case node_state::pending:
            return "pending";
        case node_state::degraded:
            return "degraded";
    }
    return "unknown";
}

// ---- 16B 节点头 ----
//
// 布局严格 16B 对齐。checksum 只覆盖 "初始化后不变" 的静态字段
// (magic + owner_offset)，保护 header 不被非法写入破坏；state 用 atomic
// 独立管理，transition 是单原子 store，不会产生"部分更新"窗口
//
// 字段总计 8 + 4 + 2 + 1 + 1 = 16 字节，无 padding
struct alignas(16) node_header {
    std::uint64_t             magic;        // 类型 magic；0 视为未初始化
    std::uint32_t             owner_offset; // 容器控制块在 arena 内的 offset
    std::uint16_t             checksum;     // crc16-ccitt(magic, owner_offset)
    std::atomic<std::uint8_t> state;        // node_state；release/acquire 可见
    std::uint8_t              _reserved;    // 对齐 + 预留扩展位
};

static_assert(sizeof(node_header) == 16, "node_header 必须占 16 字节");
static_assert(alignof(node_header) == 16, "node_header 必须 16 字节对齐");

// ---- checksum 算法 ----
//
// crc16-ccitt (poly 0x1021, init 0xFFFF)；算力便宜、对静态字段足够强
// 覆盖 magic + owner_offset 共 12 字节
MC_API std::uint16_t crc16_ccitt(const void* data, std::size_t len) noexcept;

// 计算 header 应该的 checksum（基于 magic + owner_offset 两个静态字段）
MC_API std::uint16_t node_header_checksum(const node_header& h) noexcept;

// 初始化一个全新节点的 header，state = free
// 写入顺序：静态字段 → checksum → state（release store，保证此前字段可见）
MC_API void node_header_init(node_header& h, std::uint64_t magic, std::uint32_t owner_offset) noexcept;

// 状态转换（单原子 release store），不改变 checksum
// 调用者保证 new_state 是合法 transition（见 node_state 注释）
MC_API void node_header_transition(node_header& h, node_state new_state) noexcept;

// 读 state（acquire load）
MC_API node_state node_header_load_state(const node_header& h) noexcept;

// 校验 header 完整性：magic 合法 + checksum 匹配 + state 值在合法范围
// 返回 false 意味着 header 被破坏或节点未正确初始化；recover 路径应 isolate
MC_API bool node_header_check(const node_header& h) noexcept;

// 单向把节点标记为 degraded
// 仅限 recover 持容器锁时使用；degraded 状态一旦标记不再回到其他状态
MC_API void node_header_mark_degraded(node_header& h) noexcept;

} // namespace mc::shm::container

#endif // MC_SHM_CONTAINER_NODE_HEADER_H
