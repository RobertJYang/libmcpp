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

#include "mc/shm/container/map.h"

#include <cstring>

#include "detail/skip_list_ops.h"

// map 的非模板核心：skip list 算法委托给 detail::skip_list_ops（与 set 共享）
// 本文件仅保留：
//   1) map 专属节点布局（包含 value_blob 对齐计算）
//   2) map 专属 ctx 构造与简单转发
//   3) map_control_init / map_recover 等生命周期 API

namespace mc::shm::container {

namespace {

using detail::map_node_common;
using detail::skip_list_ops_ctx;

constexpr std::size_t k_prefix_size = sizeof(map_node_common); // 32
constexpr std::size_t k_align       = 16;

inline std::size_t align_up(std::size_t v, std::size_t a) noexcept
{
    return (v + a - 1U) & ~(a - 1U);
}

// key 起始偏移：prefix + forward[L] + prev + anchor[L]，再对齐到 key_align
inline std::size_t key_offset(std::uint32_t level, std::size_t key_align) noexcept
{
    const std::size_t after_links = k_prefix_size + (2U * static_cast<std::size_t>(level) + 1U) * sizeof(std::int64_t);
    return align_up(after_links, key_align == 0 ? 1U : key_align);
}

// value 起始偏移：key_offset + key_size，再对齐到 value_align
inline std::size_t value_offset(std::uint32_t level, std::size_t key_size, std::size_t key_align,
                                std::size_t value_align) noexcept
{
    return align_up(key_offset(level, key_align) + key_size, value_align == 0 ? 1U : value_align);
}

inline void* key_ptr_impl(map_node_common* node, std::uint32_t level, std::size_t key_align) noexcept
{
    return reinterpret_cast<std::byte*>(node) + key_offset(level, key_align);
}

inline const void* key_ptr_impl(const map_node_common* node, std::uint32_t level, std::size_t key_align) noexcept
{
    return reinterpret_cast<const std::byte*>(node) + key_offset(level, key_align);
}

inline void* value_ptr_impl(map_node_common* node, std::uint32_t level, std::size_t key_size, std::size_t key_align,
                            std::size_t value_align) noexcept
{
    return reinterpret_cast<std::byte*>(node) + value_offset(level, key_size, key_align, value_align);
}

inline const void* value_ptr_impl(const map_node_common* node, std::uint32_t level, std::size_t key_size,
                                  std::size_t key_align, std::size_t value_align) noexcept
{
    return reinterpret_cast<const std::byte*>(node) + value_offset(level, key_size, key_align, value_align);
}

// skip_list_ops 回调：定位 key（依赖 ctx.key_align）
const void* map_key_of(const detail::skip_list_ops_ctx& ctx, const void* node, std::uint32_t level) noexcept
{
    return reinterpret_cast<const std::byte*>(node) + key_offset(level, ctx.key_align);
}

inline skip_list_ops_ctx make_ctx(map_control& ctrl) noexcept
{
    skip_list_ops_ctx ctx{};
    ctx.control_base   = &ctrl;
    ctx.head_forward   = ctrl.head_forward;
    ctx.journal        = &ctrl.journal;
    ctx.size           = &ctrl.size;
    ctx.degraded_count = &ctrl.degraded_count;
    ctx.self_tag       = ctrl.self_tag;
    ctx.key_size       = ctrl.key_size;
    ctx.max_level      = ctrl.max_level;
    ctx.node_magic     = map_node_magic;
    ctx.prefix_size    = k_prefix_size;
    ctx.key_align      = ctrl.key_align;
    ctx.value_align    = ctrl.value_align;
    ctx.value_size     = ctrl.value_size;
    ctx.key_of         = &map_key_of;
    return ctx;
}

inline skip_list_ops_ctx make_ctx_const(const map_control& ctrl) noexcept
{
    auto& mut = const_cast<map_control&>(ctrl);
    return make_ctx(mut);
}

} // namespace

// ============================================================================
// 生命周期 / 恢复
// ============================================================================

void map_control_init(map_control& ctrl, std::uint32_t self_tag, std::size_t key_size, std::size_t key_align,
                      std::size_t value_size, std::size_t value_align) noexcept
{
    journal_init(ctrl.journal);
    for (std::uint32_t i = 0; i < map_max_level; ++i) {
        ctrl.head_forward[i] = 0;
    }
    ctrl.self_tag    = self_tag;
    ctrl.key_size    = static_cast<std::uint32_t>(key_size);
    ctrl.key_align   = static_cast<std::uint32_t>(key_align);
    ctrl.value_size  = static_cast<std::uint32_t>(value_size);
    ctrl.value_align = static_cast<std::uint32_t>(value_align);
    ctrl.max_level   = map_max_level;
    ctrl.size.store(0, std::memory_order_release);
    ctrl.degraded_count.store(0, std::memory_order_release);
    std::uint64_t seed = 0x9E3779B97F4A7C15ULL ^ (static_cast<std::uint64_t>(self_tag) << 1);
    if (seed == 0) {
        seed = 0x12345678ULL;
    }
    ctrl.rng_state.store(seed, std::memory_order_release);
    std::memset(ctrl._reserved, 0, sizeof(ctrl._reserved));
}

void map_recover(map_control& ctrl, map_compare_fn cmp) noexcept
{
    if (cmp == nullptr) {
        return;
    }
    auto                   ctx = make_ctx(ctrl);
    container_journal_view view{};
    if (journal_load(ctrl.journal, view)) {
        switch (view.op) {
            case container_op::insert:
                detail::skip_list_recover_insert(ctx, view.target_node_offset, cmp);
                break;
            case container_op::erase:
                detail::skip_list_recover_erase(ctx, view.target_node_offset, cmp);
                break;
            case container_op::update:
            case container_op::none:
            default:
                break;
        }
        journal_end(ctrl.journal);
    }
    detail::skip_list_fsck_scan(ctx);
}

std::size_t map_degraded_count(const map_control& ctrl) noexcept
{
    return ctrl.degraded_count.load(std::memory_order_acquire);
}

// ============================================================================
// detail：节点布局 & core API 转发
// ============================================================================

namespace detail {

std::size_t map_node_bytes(std::uint32_t level, std::size_t key_size, std::size_t key_align, std::size_t value_size,
                           std::size_t value_align) noexcept
{
    const std::size_t val_off = value_offset(level, key_size, key_align, value_align);
    const std::size_t body    = val_off + value_size;
    return align_up(body, k_align);
}

std::size_t map_node_align(std::size_t key_align, std::size_t value_align) noexcept
{
    std::size_t a = k_align;
    if (key_align > a) {
        a = key_align;
    }
    if (value_align > a) {
        a = value_align;
    }
    return a;
}

std::int64_t* map_node_forward_ptr(map_node_common* node, std::uint32_t i) noexcept
{
    return skip_list_forward_ptr(node, k_prefix_size, i);
}
const std::int64_t* map_node_forward_ptr(const map_node_common* node, std::uint32_t i) noexcept
{
    return skip_list_forward_ptr(node, k_prefix_size, i);
}
std::int64_t* map_node_prev_ptr(map_node_common* node) noexcept
{
    return skip_list_prev_ptr(node, k_prefix_size, node->level);
}
const std::int64_t* map_node_prev_ptr(const map_node_common* node) noexcept
{
    return reinterpret_cast<const std::int64_t*>(
        skip_list_prev_ptr(const_cast<map_node_common*>(node), k_prefix_size, node->level));
}
std::int64_t* map_node_anchor_ptr(map_node_common* node, std::uint32_t i) noexcept
{
    return skip_list_anchor_ptr(node, k_prefix_size, node->level, i);
}
const std::int64_t* map_node_anchor_ptr(const map_node_common* node, std::uint32_t i) noexcept
{
    return skip_list_anchor_ptr(node, k_prefix_size, node->level, i);
}

void* map_node_key_ptr(map_node_common* node, std::uint32_t level, std::size_t key_align) noexcept
{
    return key_ptr_impl(node, level, key_align);
}
const void* map_node_key_ptr(const map_node_common* node, std::uint32_t level, std::size_t key_align) noexcept
{
    return key_ptr_impl(node, level, key_align);
}
void* map_node_value_ptr(map_node_common* node, std::uint32_t level, std::size_t key_size, std::size_t key_align,
                         std::size_t value_align) noexcept
{
    return value_ptr_impl(node, level, key_size, key_align, value_align);
}
const void* map_node_value_ptr(const map_node_common* node, std::uint32_t level, std::size_t key_size,
                               std::size_t key_align, std::size_t value_align) noexcept
{
    return value_ptr_impl(node, level, key_size, key_align, value_align);
}

map_node_common* map_find_predecessors(map_control& ctrl, const void* key, map_compare_fn cmp,
                                       std::int64_t* update_out) noexcept
{
    auto ctx = make_ctx(ctrl);
    return static_cast<map_node_common*>(skip_list_find_predecessors(ctx, key, cmp, update_out));
}

const map_node_common* map_find_const(const map_control& ctrl, const void* key, map_compare_fn cmp) noexcept
{
    auto ctx = make_ctx_const(ctrl);
    return static_cast<const map_node_common*>(skip_list_find_const(ctx, key, cmp));
}

const map_node_common* map_lower_bound_impl(const map_control& ctrl, const void* key, map_compare_fn cmp) noexcept
{
    auto ctx = make_ctx_const(ctrl);
    return static_cast<const map_node_common*>(skip_list_lower_bound(ctx, key, cmp));
}

const map_node_common* map_upper_bound_impl(const map_control& ctrl, const void* key, map_compare_fn cmp) noexcept
{
    auto ctx = make_ctx_const(ctrl);
    return static_cast<const map_node_common*>(skip_list_upper_bound(ctx, key, cmp));
}

const map_node_common* map_node_next_impl(const map_control& ctrl, const map_node_common* node) noexcept
{
    auto ctx = make_ctx_const(ctrl);
    return static_cast<const map_node_common*>(skip_list_node_next(ctx, node));
}

std::uint32_t map_random_level(map_control& ctrl) noexcept
{
    return skip_list_random_level(ctrl.rng_state, ctrl.max_level);
}

void map_insert_commit(map_control& ctrl, map_node_common* new_node, const std::int64_t* update_in) noexcept
{
    auto ctx = make_ctx(ctrl);
    skip_list_insert_commit(ctx, new_node, update_in);
}

bool map_erase_commit(map_control& ctrl, map_node_common* node, map_compare_fn cmp) noexcept
{
    auto ctx = make_ctx(ctrl);
    return skip_list_erase_commit(ctx, node, cmp);
}

void map_recover_cb(void* data) noexcept
{
    auto* cb = static_cast<map_recover_cb_data*>(data);
    if (cb == nullptr || cb->control == nullptr || cb->cmp == nullptr) {
        return;
    }
    map_recover(*cb->control, cb->cmp);
}

map_node_common* map_prepare_node(map_control& ctrl, shm_allocator& alloc, std::uint32_t level) noexcept
{
    if (level == 0 || level > ctrl.max_level) {
        return nullptr;
    }
    const std::size_t bytes = map_node_bytes(level, ctrl.key_size, ctrl.key_align, ctrl.value_size, ctrl.value_align);
    const std::size_t align = map_node_align(ctrl.key_align, ctrl.value_align);
    void*             mem   = alloc.allocate(bytes, align);
    if (mem == nullptr) {
        return nullptr;
    }
    std::memset(mem, 0, bytes);
    auto* node = static_cast<map_node_common*>(mem);
    node_header_init(node->header, map_node_magic, ctrl.self_tag);
    node->level = level;
    node->_pad  = 0;
    return node;
}

bool map_node_belongs_to(const map_control& ctrl, const map_node_common* node) noexcept
{
    return node != nullptr && node_header_check(node->header) && node->header.magic == map_node_magic &&
           node->header.owner_offset == ctrl.self_tag;
}

} // namespace detail

} // namespace mc::shm::container
