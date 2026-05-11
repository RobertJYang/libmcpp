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

#ifndef MC_SHM_SRC_CONTAINER_DETAIL_SKIP_LIST_OPS_H
#define MC_SHM_SRC_CONTAINER_DETAIL_SKIP_LIST_OPS_H

#include <atomic>
#include <cstddef>
#include <cstdint>

#include <mc/shm/container/journal.h>
#include <mc/shm/container/node_header.h>

// skip_list_ops
//
// 被 set_core.cpp 与 map_core.cpp 共享的 skip list 算法核心（find predecessors、
// insert/erase commit、journal redo/rollback、fsck 扫描）。
//
// 通过一个运行期 context 描述节点布局与 control 字段，避免把算法模板化，从而保持
// 二进制只膨胀一次。所有 container 约束：
//   - 节点前 16B 必须是 node_header（magic / owner_offset / state）
//   - 紧接 uint32 level，再 4B 无用 _pad（共 prefix_size = 32B）
//   - forward_offset[L] 紧贴 prefix，prev_offset 在 forward 之后，
//     anchor_backup[L] 在 prev 之后
//   - key 的位置由 context 提供一个函数指针计算（set / map 有所不同）
// 所有 offset 是 self-relative to control_base。

namespace mc::shm::container::detail {

using skip_list_compare_fn = int (*)(const void* a, const void* b, std::size_t key_size);

struct skip_list_ops_ctx;

// 取 node key 的函数指针（set: key 紧随 anchor_backup；map: 额外对齐 value_align）
// 通过 ctx 访问 key_align / value_align 等 container 特有字段
using skip_list_key_of_fn = const void* (*)(const skip_list_ops_ctx& ctx, const void* node, std::uint32_t level);

struct skip_list_ops_ctx {
    // control 起始地址（from_offset / self_offset_from 的基准）
    void* control_base;
    // control.head_forward 数组首地址（长度 max_level）
    std::int64_t* head_forward;
    // control.journal
    container_journal* journal;
    // control.size / degraded_count（atomic）
    std::atomic<std::uint64_t>* size;
    std::atomic<std::uint64_t>* degraded_count;
    // control.self_tag
    std::uint32_t self_tag;
    // control.key_size
    std::uint32_t key_size;
    // control.max_level（当前统一为 12）
    std::uint32_t max_level;
    // 节点 magic 常量（set_node_magic / map_node_magic）
    std::uint64_t node_magic;
    // 节点公共前缀字节数（set_node_common / map_node_common 都是 32）
    std::size_t prefix_size;
    // key 对齐（map 的 key_of 需要）；set 为 1
    std::uint32_t key_align;
    // value 对齐（map 专用）；set 为 0
    std::uint32_t value_align;
    // value 大小（map 专用，用于计算 value 相对 key 的偏移）；set 为 0
    std::uint32_t value_size;
    // 取 key 指针
    skip_list_key_of_fn key_of;
};

// node_level_of：读节点的 uint32 level 字段（prefix 内固定 offset = sizeof(node_header)）
inline std::uint32_t skip_list_node_level(const void* node) noexcept
{
    return *reinterpret_cast<const std::uint32_t*>(reinterpret_cast<const std::byte*>(node) + sizeof(node_header));
}

inline node_header& skip_list_node_hdr(void* node) noexcept
{
    return *reinterpret_cast<node_header*>(node);
}

inline const node_header& skip_list_node_hdr(const void* node) noexcept
{
    return *reinterpret_cast<const node_header*>(node);
}

// ---- offset 换算 ----
inline std::int64_t skip_list_self_offset(const void* control, const void* node) noexcept
{
    return static_cast<std::int64_t>(reinterpret_cast<const std::byte*>(node) -
                                     reinterpret_cast<const std::byte*>(control));
}

inline void* skip_list_from_offset(void* control, std::int64_t offset) noexcept
{
    return reinterpret_cast<std::byte*>(control) + offset;
}

inline const void* skip_list_from_offset(const void* control, std::int64_t offset) noexcept
{
    return reinterpret_cast<const std::byte*>(control) + offset;
}

// ---- 变长字段访问（offset 纯靠 prefix_size + level）----
inline std::int64_t* skip_list_forward_ptr(void* node, std::size_t prefix_size, std::uint32_t i) noexcept
{
    return reinterpret_cast<std::int64_t*>(reinterpret_cast<std::byte*>(node) + prefix_size +
                                           static_cast<std::size_t>(i) * sizeof(std::int64_t));
}

inline const std::int64_t* skip_list_forward_ptr(const void* node, std::size_t prefix_size, std::uint32_t i) noexcept
{
    return reinterpret_cast<const std::int64_t*>(reinterpret_cast<const std::byte*>(node) + prefix_size +
                                                 static_cast<std::size_t>(i) * sizeof(std::int64_t));
}

inline std::int64_t* skip_list_prev_ptr(void* node, std::size_t prefix_size, std::uint32_t level) noexcept
{
    return reinterpret_cast<std::int64_t*>(reinterpret_cast<std::byte*>(node) + prefix_size +
                                           static_cast<std::size_t>(level) * sizeof(std::int64_t));
}

inline std::int64_t* skip_list_anchor_ptr(void* node, std::size_t prefix_size, std::uint32_t level,
                                          std::uint32_t i) noexcept
{
    return reinterpret_cast<std::int64_t*>(reinterpret_cast<std::byte*>(node) + prefix_size +
                                           (static_cast<std::size_t>(level) + 1U) * sizeof(std::int64_t) +
                                           static_cast<std::size_t>(i) * sizeof(std::int64_t));
}

inline const std::int64_t* skip_list_anchor_ptr(const void* node, std::size_t prefix_size, std::uint32_t level,
                                                std::uint32_t i) noexcept
{
    return reinterpret_cast<const std::int64_t*>(reinterpret_cast<const std::byte*>(node) + prefix_size +
                                                 (static_cast<std::size_t>(level) + 1U) * sizeof(std::int64_t) +
                                                 static_cast<std::size_t>(i) * sizeof(std::int64_t));
}

// ---- 算法 API ----

// 找每层 predecessor；返回 layer-0 上的命中节点（如果存在）；未找到返回 nullptr
void* skip_list_find_predecessors(const skip_list_ops_ctx& ctx, const void* key, skip_list_compare_fn cmp,
                                  std::int64_t* update_out) noexcept;

const void* skip_list_find_const(const skip_list_ops_ctx& ctx, const void* key, skip_list_compare_fn cmp) noexcept;

// lower_bound：返回第一个 >= key 的节点；若全部小于 key，返回 nullptr
// 行为：相同 key 命中时返回该节点；否则返回 predecessor 的 forward[0]
const void* skip_list_lower_bound(const skip_list_ops_ctx& ctx, const void* key, skip_list_compare_fn cmp) noexcept;

// upper_bound：返回第一个 > key 的节点；若全部 <= key，返回 nullptr
// 行为：相同 key 命中时跳过命中节点，返回其 forward[0]
const void* skip_list_upper_bound(const skip_list_ops_ctx& ctx, const void* key, skip_list_compare_fn cmp) noexcept;

// 给定任意已链入节点，返回 layer-0 上的下一个节点（或 nullptr）。供 range 迭代使用
const void* skip_list_node_next(const skip_list_ops_ctx& ctx, const void* node) noexcept;

// 提交新节点插入；调用方保证 node 已初始化（header/level/forward/anchor/prev 已写入）
void skip_list_insert_commit(const skip_list_ops_ctx& ctx, void* node, const std::int64_t* update_in) noexcept;

// 删除：找 target 的 predecessors，unlink，标 free；节点内存不归还 allocator
bool skip_list_erase_commit(const skip_list_ops_ctx& ctx, void* node, skip_list_compare_fn cmp) noexcept;

// 崩溃恢复：redo / rollback insert
void skip_list_recover_insert(const skip_list_ops_ctx& ctx, std::uint64_t target_offset,
                              skip_list_compare_fn cmp) noexcept;

// 崩溃恢复：redo / rollback erase
void skip_list_recover_erase(const skip_list_ops_ctx& ctx, std::uint64_t target_offset,
                             skip_list_compare_fn cmp) noexcept;

// fsck 扫描（第 0 层 + 高层），isolate 损坏节点
void skip_list_fsck_scan(const skip_list_ops_ctx& ctx) noexcept;

// xorshift64 随机层高（p = 0.5）
std::uint32_t skip_list_random_level(std::atomic<std::uint64_t>& rng_state, std::uint32_t max_level) noexcept;

} // namespace mc::shm::container::detail

#endif // MC_SHM_SRC_CONTAINER_DETAIL_SKIP_LIST_OPS_H
