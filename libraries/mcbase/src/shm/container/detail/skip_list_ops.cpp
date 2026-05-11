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

#include "skip_list_ops.h"

#include <cstring>

// skip_list_ops 实现（set_core / map_core 共享）
//
// 详见 skip_list_ops.h 的设计说明与 docs/plans/2026-04-17-shm-crash-safe-containers-design.md

namespace mc::shm::container::detail {

namespace {

constexpr std::uint32_t k_max_supported_level = 16; // 栈上 update[] 的上限；实际不超过 max_level

// 给定 predecessor offset（0 = head），读某层的 forward 值
inline std::int64_t forward_at(const skip_list_ops_ctx& ctx, std::int64_t predecessor_off, std::uint32_t i) noexcept
{
    if (predecessor_off == 0) {
        return ctx.head_forward[i];
    }
    void* pred = skip_list_from_offset(ctx.control_base, predecessor_off);
    return *skip_list_forward_ptr(pred, ctx.prefix_size, i);
}

// 写某层的 forward 值（head 或节点）
inline void forward_store(const skip_list_ops_ctx& ctx, std::int64_t predecessor_off, std::uint32_t i,
                          std::int64_t value) noexcept
{
    if (predecessor_off == 0) {
        ctx.head_forward[i] = value;
    } else {
        void* pred                                       = skip_list_from_offset(ctx.control_base, predecessor_off);
        *skip_list_forward_ptr(pred, ctx.prefix_size, i) = value;
    }
}

// 节点合法性：header ok + magic 匹配 + owner_offset 匹配
inline bool node_valid(const skip_list_ops_ctx& ctx, const void* n) noexcept
{
    const auto& h = skip_list_node_hdr(n);
    return node_header_check(h) && h.magic == ctx.node_magic && h.owner_offset == ctx.self_tag;
}

inline void* node_at(const skip_list_ops_ctx& ctx, std::int64_t off) noexcept
{
    return off == 0 ? nullptr : skip_list_from_offset(ctx.control_base, off);
}

} // namespace

// ============================================================================
// find
// ============================================================================

void* skip_list_find_predecessors(const skip_list_ops_ctx& ctx, const void* key, skip_list_compare_fn cmp,
                                  std::int64_t* update_out) noexcept
{
    const std::uint32_t max_level = ctx.max_level;
    const std::size_t   key_size  = ctx.key_size;
    std::int64_t        pred_off  = 0;
    void*               found     = nullptr;

    for (std::int32_t i = static_cast<std::int32_t>(max_level) - 1; i >= 0; --i) {
        const std::uint32_t lvl     = static_cast<std::uint32_t>(i);
        std::int64_t        cur_off = (pred_off == 0) ? ctx.head_forward[lvl]
                                                      : *skip_list_forward_ptr(node_at(ctx, pred_off), ctx.prefix_size, lvl);
        while (cur_off != 0) {
            void* cur = node_at(ctx, cur_off);
            if (cur == nullptr || !node_valid(ctx, cur)) {
                break;
            }
            const void* cur_key = ctx.key_of(ctx, cur, skip_list_node_level(cur));
            const int   c       = cmp(cur_key, key, key_size);
            if (c < 0) {
                pred_off = cur_off;
                cur_off  = *skip_list_forward_ptr(cur, ctx.prefix_size, lvl);
            } else {
                if (c == 0 && lvl == 0) {
                    found = cur;
                }
                break;
            }
        }
        update_out[lvl] = pred_off;
    }
    return found;
}

const void* skip_list_find_const(const skip_list_ops_ctx& ctx, const void* key, skip_list_compare_fn cmp) noexcept
{
    const std::uint32_t max_level = ctx.max_level;
    const std::size_t   key_size  = ctx.key_size;
    std::int64_t        pred_off  = 0;

    for (std::int32_t i = static_cast<std::int32_t>(max_level) - 1; i >= 0; --i) {
        const std::uint32_t lvl     = static_cast<std::uint32_t>(i);
        std::int64_t        cur_off = (pred_off == 0) ? ctx.head_forward[lvl]
                                                      : *skip_list_forward_ptr(node_at(ctx, pred_off), ctx.prefix_size, lvl);
        while (cur_off != 0) {
            const void* cur = node_at(ctx, cur_off);
            if (cur == nullptr || !node_valid(ctx, cur)) {
                break;
            }
            const void* cur_key = ctx.key_of(ctx, cur, skip_list_node_level(cur));
            const int   c       = cmp(cur_key, key, key_size);
            if (c < 0) {
                pred_off = cur_off;
                cur_off  = *skip_list_forward_ptr(cur, ctx.prefix_size, lvl);
            } else {
                if (c == 0 && lvl == 0) {
                    return cur;
                }
                break;
            }
        }
    }
    return nullptr;
}

// 与 skip_list_find_predecessors 同构，但只读且不需要 update[]
// 返回 layer-0 上 "第一个不小于 key 的节点" 所在位置（命中或严格大于）
// 路径跟踪方式：扫描每层 predecessor（< key 中最大），最后在 layer 0 走一步 forward
const void* skip_list_lower_bound(const skip_list_ops_ctx& ctx, const void* key, skip_list_compare_fn cmp) noexcept
{
    const std::uint32_t max_level = ctx.max_level;
    const std::size_t   key_size  = ctx.key_size;
    std::int64_t        pred_off  = 0;
    const void*         candidate = nullptr;

    for (std::int32_t i = static_cast<std::int32_t>(max_level) - 1; i >= 0; --i) {
        const std::uint32_t lvl     = static_cast<std::uint32_t>(i);
        std::int64_t        cur_off = (pred_off == 0) ? ctx.head_forward[lvl]
                                                      : *skip_list_forward_ptr(node_at(ctx, pred_off), ctx.prefix_size, lvl);
        while (cur_off != 0) {
            const void* cur = node_at(ctx, cur_off);
            if (cur == nullptr || !node_valid(ctx, cur)) {
                break;
            }
            const void* cur_key = ctx.key_of(ctx, cur, skip_list_node_level(cur));
            const int   c       = cmp(cur_key, key, key_size);
            if (c < 0) {
                pred_off = cur_off;
                cur_off  = *skip_list_forward_ptr(cur, ctx.prefix_size, lvl);
            } else {
                if (lvl == 0) {
                    candidate = cur;
                }
                break;
            }
        }
    }
    return candidate;
}

// upper_bound：第一个 > key 的节点
// 等同于 lower_bound(key)，若该节点 key 与 query 相等则再走一步 forward[0]
const void* skip_list_upper_bound(const skip_list_ops_ctx& ctx, const void* key, skip_list_compare_fn cmp) noexcept
{
    const void* lb = skip_list_lower_bound(ctx, key, cmp);
    if (lb == nullptr) {
        return nullptr;
    }
    const void* lb_key = ctx.key_of(ctx, lb, skip_list_node_level(lb));
    if (cmp(lb_key, key, ctx.key_size) != 0) {
        return lb;
    }
    const std::int64_t nxt = *skip_list_forward_ptr(lb, ctx.prefix_size, 0U);
    if (nxt == 0) {
        return nullptr;
    }
    const void* nxt_node = node_at(const_cast<skip_list_ops_ctx&>(ctx), nxt);
    if (nxt_node == nullptr || !node_valid(ctx, nxt_node)) {
        return nullptr;
    }
    return nxt_node;
}

const void* skip_list_node_next(const skip_list_ops_ctx& ctx, const void* node) noexcept
{
    if (node == nullptr) {
        return nullptr;
    }
    const std::int64_t nxt = *skip_list_forward_ptr(node, ctx.prefix_size, 0U);
    if (nxt == 0) {
        return nullptr;
    }
    const void* nxt_node = node_at(const_cast<skip_list_ops_ctx&>(ctx), nxt);
    if (nxt_node == nullptr || !node_valid(ctx, nxt_node)) {
        return nullptr;
    }
    return nxt_node;
}

// ============================================================================
// insert / erase commit
// ============================================================================

void skip_list_insert_commit(const skip_list_ops_ctx& ctx, void* node, const std::int64_t* update) noexcept
{
    const std::int64_t  new_off = skip_list_self_offset(ctx.control_base, node);
    const std::uint32_t level   = skip_list_node_level(node);
    auto&               header  = skip_list_node_hdr(node);

    node_header_transition(header, node_state::pending);

    journal_begin(*ctx.journal, container_op::insert, static_cast<std::uint64_t>(new_off), 0, 0, 0, 0, level);

    for (std::int32_t i = static_cast<std::int32_t>(level) - 1; i > 0; --i) {
        forward_store(ctx, update[i], static_cast<std::uint32_t>(i), new_off);
    }
    forward_store(ctx, update[0], 0U, new_off);

    const std::int64_t next_at_layer0 = *skip_list_forward_ptr(node, ctx.prefix_size, 0U);
    if (next_at_layer0 != 0) {
        void* nxt = node_at(ctx, next_at_layer0);
        if (nxt != nullptr) {
            *skip_list_prev_ptr(nxt, ctx.prefix_size, skip_list_node_level(nxt)) = new_off;
        }
    }

    node_header_transition(header, node_state::linked);
    ctx.size->fetch_add(1, std::memory_order_acq_rel);
    journal_end(*ctx.journal);
}

bool skip_list_erase_commit(const skip_list_ops_ctx& ctx, void* target, skip_list_compare_fn cmp) noexcept
{
    if (target == nullptr || !node_valid(ctx, target)) {
        return false;
    }
    auto& header = skip_list_node_hdr(target);
    if (node_header_load_state(header) != node_state::linked) {
        return false;
    }

    std::int64_t update[k_max_supported_level] = {0};
    const void*  target_key                    = ctx.key_of(ctx, target, skip_list_node_level(target));
    (void)skip_list_find_predecessors(ctx, target_key, cmp, update);

    const std::int64_t  target_off = skip_list_self_offset(ctx.control_base, target);
    const std::uint32_t level      = skip_list_node_level(target);

    node_header_transition(header, node_state::pending);

    journal_begin(*ctx.journal, container_op::erase, static_cast<std::uint64_t>(target_off), 0, 0, 0, 0, level);

    for (std::int32_t i = static_cast<std::int32_t>(level) - 1; i > 0; --i) {
        const std::uint32_t lvl = static_cast<std::uint32_t>(i);
        if (forward_at(ctx, update[lvl], lvl) == target_off) {
            forward_store(ctx, update[lvl], lvl, *skip_list_forward_ptr(target, ctx.prefix_size, lvl));
        }
    }
    if (forward_at(ctx, update[0], 0U) == target_off) {
        const std::int64_t next_off = *skip_list_forward_ptr(target, ctx.prefix_size, 0U);
        forward_store(ctx, update[0], 0U, next_off);
        if (next_off != 0) {
            void* nxt = node_at(ctx, next_off);
            if (nxt != nullptr) {
                *skip_list_prev_ptr(nxt, ctx.prefix_size, skip_list_node_level(nxt)) = update[0];
            }
        }
    }

    node_header_transition(header, node_state::free);
    ctx.size->fetch_sub(1, std::memory_order_acq_rel);
    journal_end(*ctx.journal);
    return true;
}

// ============================================================================
// recover
// ============================================================================

void skip_list_recover_insert(const skip_list_ops_ctx& ctx, std::uint64_t target_offset,
                              skip_list_compare_fn cmp) noexcept
{
    void* target = node_at(ctx, static_cast<std::int64_t>(target_offset));
    if (target == nullptr) {
        return;
    }
    auto& header = skip_list_node_hdr(target);
    if (!node_valid(ctx, target)) {
        if (node_header_check(header)) {
            node_header_mark_degraded(header);
            ctx.degraded_count->fetch_add(1, std::memory_order_acq_rel);
        }
        return;
    }

    const std::uint32_t level      = skip_list_node_level(target);
    const std::int64_t  target_off = static_cast<std::int64_t>(target_offset);

    std::int64_t update[k_max_supported_level] = {0};
    const void*  target_key                    = ctx.key_of(ctx, target, level);
    (void)skip_list_find_predecessors(ctx, target_key, cmp, update);

    const bool layer0_done = (forward_at(ctx, update[0], 0U) == target_off);

    if (layer0_done) {
        for (std::uint32_t i = 1; i < level; ++i) {
            if (forward_at(ctx, update[i], i) == *skip_list_anchor_ptr(target, ctx.prefix_size, level, i)) {
                forward_store(ctx, update[i], i, target_off);
            }
        }
        node_header_transition(header, node_state::linked);
        ctx.size->fetch_add(1, std::memory_order_acq_rel);
        return;
    }

    for (std::uint32_t i = 1; i < level; ++i) {
        if (forward_at(ctx, update[i], i) == target_off) {
            forward_store(ctx, update[i], i, *skip_list_anchor_ptr(target, ctx.prefix_size, level, i));
        }
    }
    node_header_mark_degraded(header);
    ctx.degraded_count->fetch_add(1, std::memory_order_acq_rel);
}

void skip_list_recover_erase(const skip_list_ops_ctx& ctx, std::uint64_t target_offset,
                             skip_list_compare_fn cmp) noexcept
{
    void* target = node_at(ctx, static_cast<std::int64_t>(target_offset));
    if (target == nullptr) {
        return;
    }
    auto& header = skip_list_node_hdr(target);
    if (!node_valid(ctx, target)) {
        if (node_header_check(header)) {
            node_header_mark_degraded(header);
            ctx.degraded_count->fetch_add(1, std::memory_order_acq_rel);
        }
        return;
    }

    const std::uint32_t level      = skip_list_node_level(target);
    const std::int64_t  target_off = static_cast<std::int64_t>(target_offset);
    const auto          state      = node_header_load_state(header);

    std::int64_t update[k_max_supported_level] = {0};
    const void*  target_key                    = ctx.key_of(ctx, target, level);
    (void)skip_list_find_predecessors(ctx, target_key, cmp, update);

    const bool layer0_unlinked = (forward_at(ctx, update[0], 0U) != target_off);

    if (layer0_unlinked && state == node_state::free) {
        for (std::uint32_t i = 1; i < level; ++i) {
            if (forward_at(ctx, update[i], i) == target_off) {
                forward_store(ctx, update[i], i, *skip_list_forward_ptr(target, ctx.prefix_size, i));
            }
        }
        ctx.size->fetch_sub(1, std::memory_order_acq_rel);
        return;
    }

    for (std::uint32_t i = 1; i < level; ++i) {
        if (forward_at(ctx, update[i], i) == *skip_list_forward_ptr(target, ctx.prefix_size, i)) {
            forward_store(ctx, update[i], i, target_off);
        }
    }
    if (!layer0_unlinked && state != node_state::linked) {
        node_header_transition(header, node_state::linked);
    }
}

// ============================================================================
// fsck
// ============================================================================

void skip_list_fsck_scan(const skip_list_ops_ctx& ctx) noexcept
{
    const std::uint32_t max_level      = ctx.max_level;
    const std::size_t   hint_size      = ctx.size->load(std::memory_order_acquire);
    const std::size_t   scan_limit     = hint_size * 4 + 1024;
    std::size_t         degraded_added = 0;

    std::size_t  counted = 0;
    std::int64_t prev    = 0;
    std::int64_t cur     = ctx.head_forward[0];
    std::size_t  guard   = 0;

    while (cur != 0) {
        if (++guard > scan_limit) {
            if (prev == 0) {
                ctx.head_forward[0] = 0;
            } else {
                *skip_list_forward_ptr(node_at(ctx, prev), ctx.prefix_size, 0U) = 0;
            }
            break;
        }
        void* n = node_at(ctx, cur);
        if (n == nullptr) {
            break;
        }
        const bool header_ok = node_valid(ctx, n);
        const bool state_ok  = header_ok && node_header_load_state(skip_list_node_hdr(n)) == node_state::linked;

        if (!header_ok || !state_ok) {
            const std::uint32_t lvl  = skip_list_node_level(n);
            const std::int64_t  next = lvl > 0 ? *skip_list_forward_ptr(n, ctx.prefix_size, 0U) : 0;
            if (prev == 0) {
                ctx.head_forward[0] = next;
            } else {
                *skip_list_forward_ptr(node_at(ctx, prev), ctx.prefix_size, 0U) = next;
            }
            if (next != 0) {
                void* nxt = node_at(ctx, next);
                if (nxt != nullptr) {
                    *skip_list_prev_ptr(nxt, ctx.prefix_size, skip_list_node_level(nxt)) = prev;
                }
            }
            if (header_ok) {
                node_header_mark_degraded(skip_list_node_hdr(n));
            }
            ++degraded_added;
            cur = next;
            continue;
        }

        *skip_list_prev_ptr(n, ctx.prefix_size, skip_list_node_level(n)) = prev;
        prev                                                             = cur;
        cur = *skip_list_forward_ptr(n, ctx.prefix_size, 0U);
        ++counted;
    }

    for (std::uint32_t lvl = 1; lvl < max_level; ++lvl) {
        std::int64_t p = 0;
        std::int64_t c = ctx.head_forward[lvl];
        std::size_t  g = 0;
        while (c != 0) {
            if (++g > scan_limit) {
                if (p == 0) {
                    ctx.head_forward[lvl] = 0;
                } else {
                    *skip_list_forward_ptr(node_at(ctx, p), ctx.prefix_size, lvl) = 0;
                }
                break;
            }
            void* n = node_at(ctx, c);
            if (n == nullptr) {
                break;
            }
            const bool ok = node_valid(ctx, n) && node_header_load_state(skip_list_node_hdr(n)) == node_state::linked &&
                            skip_list_node_level(n) > lvl;
            if (!ok) {
                const std::int64_t next = (n != nullptr && skip_list_node_level(n) > lvl)
                                              ? *skip_list_forward_ptr(n, ctx.prefix_size, lvl)
                                              : 0;
                if (p == 0) {
                    ctx.head_forward[lvl] = next;
                } else {
                    *skip_list_forward_ptr(node_at(ctx, p), ctx.prefix_size, lvl) = next;
                }
                c = next;
                continue;
            }
            p = c;
            c = *skip_list_forward_ptr(n, ctx.prefix_size, lvl);
        }
    }

    ctx.size->store(counted, std::memory_order_release);
    if (degraded_added > 0) {
        ctx.degraded_count->fetch_add(degraded_added, std::memory_order_acq_rel);
    }
}

// ============================================================================
// random_level
// ============================================================================

std::uint32_t skip_list_random_level(std::atomic<std::uint64_t>& rng_state, std::uint32_t max_level) noexcept
{
    std::uint64_t s = rng_state.load(std::memory_order_relaxed);
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    if (s == 0) {
        s = 0x1ULL;
    }
    rng_state.store(s, std::memory_order_relaxed);

    std::uint32_t level = 1;
    std::uint64_t bits  = s;
    while ((bits & 1U) != 0 && level < max_level) {
        ++level;
        bits >>= 1;
    }
    return level;
}

} // namespace mc::shm::container::detail
