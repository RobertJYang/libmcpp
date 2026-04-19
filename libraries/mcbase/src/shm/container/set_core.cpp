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

#include "mc/shm/container/set.h"

#include <cstring>

#include "detail/skip_list_ops.h"

// set 的非模板核心实现：skip list 算法全部委托给 detail::skip_list_ops（与 map 共享）
// 本文件只保留：
//   1) set 专属节点布局访问器（prefix/forward/prev/anchor/key_ptr）
//   2) set 专属 ctx 构造与简单转发
//   3) set_control_init / set_recover 等生命周期 API

namespace mc::shm::container {

namespace {

using detail::set_node_common;
using detail::skip_list_ops_ctx;

constexpr std::size_t k_prefix_size = sizeof(set_node_common); // 32
constexpr std::size_t k_align       = 16;

inline std::size_t align_up(std::size_t v, std::size_t a) noexcept
{
    return (v + a - 1U) & ~(a - 1U);
}

// key_blob 紧随 anchor_backup；偏移 = k_prefix_size + (2L + 1) * 8
inline std::size_t key_off(std::uint32_t level) noexcept
{
    return k_prefix_size + (2U * static_cast<std::size_t>(level) + 1U) * sizeof(std::int64_t);
}

inline void* key_ptr(set_node_common* node) noexcept
{
    return reinterpret_cast<std::byte*>(node) + key_off(node->level);
}

inline const void* key_ptr(const set_node_common* node) noexcept
{
    return reinterpret_cast<const std::byte*>(node) + key_off(node->level);
}

// skip_list_ops 回调：从 node + level 定位 key
const void* set_key_of(const detail::skip_list_ops_ctx& /*ctx*/, const void* node, std::uint32_t level) noexcept
{
    return reinterpret_cast<const std::byte*>(node) + key_off(level);
}

// 构造 ctx（非 const 上下文，含原子字段的可写引用）
inline skip_list_ops_ctx make_ctx(set_control& ctrl) noexcept
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
    ctx.node_magic     = set_node_magic;
    ctx.prefix_size    = k_prefix_size;
    ctx.key_align      = ctrl.key_align;
    ctx.value_align    = 0;
    ctx.value_size     = 0;
    ctx.key_of         = &set_key_of;
    return ctx;
}

// const 版本：需要把 const 指针从 set_control 中取出；头层调用方保证只读路径
inline skip_list_ops_ctx make_ctx_const(const set_control& ctrl) noexcept
{
    auto& mut = const_cast<set_control&>(ctrl);
    return make_ctx(mut);
}

} // namespace

// ============================================================================
// 生命周期 / 恢复
// ============================================================================

void set_control_init(set_control& ctrl, std::uint32_t self_tag, std::size_t key_size, std::size_t key_align) noexcept
{
    journal_init(ctrl.journal);
    for (std::uint32_t i = 0; i < set_max_level; ++i) {
        ctrl.head_forward[i] = 0;
    }
    ctrl.self_tag  = self_tag;
    ctrl.key_size  = static_cast<std::uint32_t>(key_size);
    ctrl.key_align = static_cast<std::uint32_t>(key_align);
    ctrl.max_level = set_max_level;
    ctrl.size.store(0, std::memory_order_release);
    ctrl.degraded_count.store(0, std::memory_order_release);
    std::uint64_t seed = 0x9E3779B97F4A7C15ULL ^ (static_cast<std::uint64_t>(self_tag) << 1);
    if (seed == 0) {
        seed = 0x12345678ULL;
    }
    ctrl.rng_state.store(seed, std::memory_order_release);
    std::memset(ctrl._reserved, 0, sizeof(ctrl._reserved));
}

void set_recover(set_control& ctrl, set_compare_fn cmp) noexcept
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

std::size_t set_degraded_count(const set_control& ctrl) noexcept
{
    return ctrl.degraded_count.load(std::memory_order_acquire);
}

// ============================================================================
// detail：节点布局访问器 & 核心 API 转发
// ============================================================================

namespace detail {

std::size_t set_node_bytes(std::uint32_t level, std::size_t key_size) noexcept
{
    const std::size_t body = key_off(level) + key_size;
    return align_up(body, k_align);
}

std::size_t set_node_align(std::size_t key_align) noexcept
{
    return key_align > k_align ? key_align : k_align;
}

std::int64_t* set_node_forward_ptr(set_node_common* node, std::uint32_t i) noexcept
{
    return skip_list_forward_ptr(node, k_prefix_size, i);
}

const std::int64_t* set_node_forward_ptr(const set_node_common* node, std::uint32_t i) noexcept
{
    return skip_list_forward_ptr(node, k_prefix_size, i);
}

std::int64_t* set_node_prev_ptr(set_node_common* node) noexcept
{
    return skip_list_prev_ptr(node, k_prefix_size, node->level);
}

const std::int64_t* set_node_prev_ptr(const set_node_common* node) noexcept
{
    // 常量版本：skip_list_prev_ptr 只提供 void* 重载；手工做一次 const 反走
    return reinterpret_cast<const std::int64_t*>(
        skip_list_prev_ptr(const_cast<set_node_common*>(node), k_prefix_size, node->level));
}

std::int64_t* set_node_anchor_ptr(set_node_common* node, std::uint32_t i) noexcept
{
    return skip_list_anchor_ptr(node, k_prefix_size, node->level, i);
}

const std::int64_t* set_node_anchor_ptr(const set_node_common* node, std::uint32_t i) noexcept
{
    return skip_list_anchor_ptr(node, k_prefix_size, node->level, i);
}

void* set_node_key_ptr(set_node_common* node, std::uint32_t /*level_for_layout*/) noexcept
{
    return key_ptr(node);
}

const void* set_node_key_ptr(const set_node_common* node, std::uint32_t /*level_for_layout*/) noexcept
{
    return key_ptr(node);
}

set_node_common* set_find_predecessors(set_control& ctrl, const void* key, set_compare_fn cmp,
                                       std::int64_t* update_out) noexcept
{
    auto ctx = make_ctx(ctrl);
    return static_cast<set_node_common*>(skip_list_find_predecessors(ctx, key, cmp, update_out));
}

const set_node_common* set_find_const(const set_control& ctrl, const void* key, set_compare_fn cmp) noexcept
{
    auto ctx = make_ctx_const(ctrl);
    return static_cast<const set_node_common*>(skip_list_find_const(ctx, key, cmp));
}

const set_node_common* set_lower_bound_impl(const set_control& ctrl, const void* key, set_compare_fn cmp) noexcept
{
    auto ctx = make_ctx_const(ctrl);
    return static_cast<const set_node_common*>(skip_list_lower_bound(ctx, key, cmp));
}

const set_node_common* set_upper_bound_impl(const set_control& ctrl, const void* key, set_compare_fn cmp) noexcept
{
    auto ctx = make_ctx_const(ctrl);
    return static_cast<const set_node_common*>(skip_list_upper_bound(ctx, key, cmp));
}

const set_node_common* set_node_next_impl(const set_control& ctrl, const set_node_common* node) noexcept
{
    auto ctx = make_ctx_const(ctrl);
    return static_cast<const set_node_common*>(skip_list_node_next(ctx, node));
}

std::uint32_t set_random_level(set_control& ctrl) noexcept
{
    return skip_list_random_level(ctrl.rng_state, ctrl.max_level);
}

void set_insert_commit(set_control& ctrl, set_node_common* new_node, const std::int64_t* update_in) noexcept
{
    auto ctx = make_ctx(ctrl);
    skip_list_insert_commit(ctx, new_node, update_in);
}

bool set_erase_commit(set_control& ctrl, set_node_common* node, set_compare_fn cmp) noexcept
{
    auto ctx = make_ctx(ctrl);
    return skip_list_erase_commit(ctx, node, cmp);
}

void set_recover_cb(void* data) noexcept
{
    auto* cb = static_cast<set_recover_cb_data*>(data);
    if (cb == nullptr || cb->control == nullptr || cb->cmp == nullptr) {
        return;
    }
    set_recover(*cb->control, cb->cmp);
}

set_node_common* set_prepare_node(set_control& ctrl, shm_allocator& alloc, std::uint32_t level) noexcept
{
    if (level == 0 || level > ctrl.max_level) {
        return nullptr;
    }
    const std::size_t bytes = set_node_bytes(level, ctrl.key_size);
    const std::size_t align = set_node_align(ctrl.key_align);
    void*             mem   = alloc.allocate(bytes, align);
    if (mem == nullptr) {
        return nullptr;
    }
    std::memset(mem, 0, bytes);
    auto* node = static_cast<set_node_common*>(mem);
    node_header_init(node->header, set_node_magic, ctrl.self_tag);
    node->level = level;
    node->_pad  = 0;
    return node;
}

bool set_node_belongs_to(const set_control& ctrl, const set_node_common* node) noexcept
{
    return node != nullptr && node_header_check(node->header) && node->header.magic == set_node_magic &&
           node->header.owner_offset == ctrl.self_tag;
}

} // namespace detail

} // namespace mc::shm::container
