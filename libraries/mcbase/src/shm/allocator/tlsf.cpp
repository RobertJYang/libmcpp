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

#include "tlsf.h"

namespace mc::shm::detail {
namespace {

// ================ class index 映射 ================

// size 下边界：size < small_threshold 的全部归 fl=0
constexpr std::size_t small_threshold = 1ULL << (tlsf_fl_shift + tlsf_sl_log2); // 512

void mapping_insert(std::size_t size, std::uint32_t& fl, std::uint32_t& sl) noexcept
{
    if (size < small_threshold) {
        fl = 0;
        sl = static_cast<std::uint32_t>(size >> tlsf_fl_shift);
    } else {
        std::uint32_t raw_fl = fls64(static_cast<std::uint64_t>(size));
        sl                   = static_cast<std::uint32_t>((size >> (raw_fl - tlsf_sl_log2)) & (tlsf_sl_count - 1));
        fl                   = raw_fl - tlsf_fl_shift;
    }
}

// 分配用：向上舍入到下一个 class 边界以确保找到的 class 下限一定 >= size
bool mapping_search(std::size_t size, std::uint32_t& fl, std::uint32_t& sl) noexcept
{
    if (size >= small_threshold) {
        std::uint32_t raw_fl = fls64(static_cast<std::uint64_t>(size));
        std::size_t   round  = (1ULL << (raw_fl - tlsf_sl_log2)) - 1;
        size += round;
    } else {
        // fl=0 子档粒度为 1 << tlsf_fl_shift (=32 B)；向上取整保证命中 class 下限 >= size
        constexpr std::size_t small_quantum = 1ULL << tlsf_fl_shift;
        size = (size + small_quantum - 1) & ~(small_quantum - 1);
    }
    if (size == 0) {
        fl = 0;
        sl = 0;
        return true;
    }
    mapping_insert(size, fl, sl);
    return fl < tlsf_fl_count;
}

// 在 (fl, sl) 处及之上找第一个非空 class
bool search_suitable_class(const tlsf_control& ctrl, std::uint32_t& fl, std::uint32_t& sl) noexcept
{
    std::uint32_t sl_map = ctrl.sl_bitmap[fl] & (~0U << sl);
    if (sl_map == 0) {
        // 该 fl 中 >= sl 的都为空，需要升一层 fl
        std::uint32_t fl_map = ctrl.fl_bitmap & (~0U << (fl + 1));
        if (fl_map == 0) {
            return false;
        }
        fl     = ffs32(fl_map);
        sl_map = ctrl.sl_bitmap[fl];
    }
    sl = ffs32(sl_map);
    return true;
}

// ================ free_list 维护 ================

void insert_free_block(void* base, arena_header* arena, tlsf_block_header* blk) noexcept
{
    std::size_t   bsz = tlsf_block_size(blk);
    std::uint32_t fl  = 0;
    std::uint32_t sl  = 0;
    mapping_insert(bsz, fl, sl);

    auto& ctrl     = arena->tlsf;
    auto  blk_off  = offset_of(base, blk);
    auto  head_off = ctrl.free_list[fl][sl];

    blk->next_free_offset = head_off;
    blk->prev_free_offset = 0;
    if (head_off != 0) {
        auto* head                 = ptr_at<tlsf_block_header>(base, head_off);
        head->prev_free_offset     = blk_off;
    }
    ctrl.free_list[fl][sl] = blk_off;
    ctrl.fl_bitmap |= (1U << fl);
    ctrl.sl_bitmap[fl] |= (1U << sl);
}

void remove_free_block(void* base, arena_header* arena, tlsf_block_header* blk) noexcept
{
    std::size_t   bsz = tlsf_block_size(blk);
    std::uint32_t fl  = 0;
    std::uint32_t sl  = 0;
    mapping_insert(bsz, fl, sl);

    auto& ctrl     = arena->tlsf;
    auto  next_off = blk->next_free_offset;
    auto  prev_off = blk->prev_free_offset;

    if (prev_off != 0) {
        auto* prev             = ptr_at<tlsf_block_header>(base, prev_off);
        prev->next_free_offset = next_off;
    }
    if (next_off != 0) {
        auto* next             = ptr_at<tlsf_block_header>(base, next_off);
        next->prev_free_offset = prev_off;
    }

    if (ctrl.free_list[fl][sl] == offset_of(base, blk)) {
        ctrl.free_list[fl][sl] = next_off;
        if (next_off == 0) {
            ctrl.sl_bitmap[fl] &= ~(1U << sl);
            if (ctrl.sl_bitmap[fl] == 0) {
                ctrl.fl_bitmap &= ~(1U << fl);
            }
        }
    }

    blk->next_free_offset = 0;
    blk->prev_free_offset = 0;
}

// ================ block 物理导航 ================

tlsf_block_header* next_phys_block(void* base, arena_header* arena, tlsf_block_header* blk) noexcept
{
    auto off       = offset_of(base, blk);
    auto size      = tlsf_block_size(blk);
    auto next_off  = off + size;
    auto arena_end = arena->user_offset + arena->user_size;
    if (next_off >= arena_end) {
        return nullptr;
    }
    return ptr_at<tlsf_block_header>(base, next_off);
}

tlsf_block_header* prev_phys_block(void* base, tlsf_block_header* blk) noexcept
{
    if (blk->prev_phys_offset == 0) {
        return nullptr;
    }
    return ptr_at<tlsf_block_header>(base, blk->prev_phys_offset);
}

// 可能对 block 进行 split：如果 block 比 split_size 大足够多，剩余部分作为新 free block
// split_size 必须是 tlsf_align 的倍数
// 返回拆分后原 block 的新 size；拆出的新 free block 已经 insert
tlsf_block_header* split_block_if_possible(void* base, arena_header* arena, tlsf_block_header* blk,
                                           std::size_t want_size) noexcept
{
    std::size_t bsz       = tlsf_block_size(blk);
    std::size_t remainder = bsz - want_size;
    if (remainder < tlsf_min_block_size) {
        return nullptr;
    }

    auto* remainder_blk = reinterpret_cast<tlsf_block_header*>(
        reinterpret_cast<std::byte*>(blk) + want_size);
    bool prev_free = tlsf_block_prev_is_free(blk);
    tlsf_set_size(blk, want_size, tlsf_block_is_free(blk), prev_free);

    remainder_blk->prev_phys_offset = offset_of(base, blk);
    tlsf_set_size(remainder_blk, remainder, true, !tlsf_block_is_free(blk) ? false : true);
    remainder_blk->next_free_offset = 0;
    remainder_blk->prev_free_offset = 0;

    // 更新 remainder 之后的 block（如存在）的 prev_phys_offset 和 prev_free 标志
    auto* after = next_phys_block(base, arena, remainder_blk);
    if (after != nullptr) {
        after->prev_phys_offset = offset_of(base, remainder_blk);
        tlsf_set_prev_free_flag(after, true);
    }

    insert_free_block(base, arena, remainder_blk);
    return remainder_blk;
}

// 合并相邻的 free block：把 blk 与 blk 之后的 free 邻居合并；要求调用者已从 free_list 摘除 blk
// 返回合并后的 block size
void absorb_next_if_free(void* base, arena_header* arena, tlsf_block_header* blk) noexcept
{
    auto* next = next_phys_block(base, arena, blk);
    if (next == nullptr || !tlsf_block_is_free(next)) {
        return;
    }
    remove_free_block(base, arena, next);
    std::size_t new_size = tlsf_block_size(blk) + tlsf_block_size(next);
    tlsf_set_size(blk, new_size, tlsf_block_is_free(blk), tlsf_block_prev_is_free(blk));

    // 更新再之后 block 的 prev_phys_offset
    auto* after = next_phys_block(base, arena, blk);
    if (after != nullptr) {
        after->prev_phys_offset = offset_of(base, blk);
    }
}

tlsf_block_header* absorb_prev_if_free(void* base, arena_header* arena, tlsf_block_header* blk) noexcept
{
    if (!tlsf_block_prev_is_free(blk)) {
        return blk;
    }
    auto* prev = prev_phys_block(base, blk);
    if (prev == nullptr || !tlsf_block_is_free(prev)) {
        return blk;
    }
    remove_free_block(base, arena, prev);
    std::size_t new_size = tlsf_block_size(prev) + tlsf_block_size(blk);
    tlsf_set_size(prev, new_size, true, tlsf_block_prev_is_free(prev));

    auto* after = next_phys_block(base, arena, prev);
    if (after != nullptr) {
        after->prev_phys_offset = offset_of(base, prev);
    }
    return prev;
}

} // namespace

// ================ 公共 API 实现 ================

std::size_t tlsf_required_block_size(std::size_t size, std::size_t alignment) noexcept
{
    // block 本身 16 字节 header 开销
    std::size_t total = tlsf_block_header_overhead + size;
    // 至少满足 tlsf_min_block_size
    if (total < tlsf_min_block_size) {
        total = tlsf_min_block_size;
    }
    // alignment > 默认时需要额外空间让出 prefix
    if (alignment > tlsf_align) {
        total += alignment;
    }
    // 对齐到 tlsf_align
    total = align_up(total, tlsf_align);
    return total;
}

std::uint64_t tlsf_payload_offset(std::uint64_t block_offset) noexcept
{
    if (block_offset == 0) {
        return 0;
    }
    return block_offset + tlsf_block_header_overhead;
}

std::uint64_t tlsf_block_offset_from_payload(std::uint64_t payload_offset) noexcept
{
    if (payload_offset == 0 || payload_offset < tlsf_block_header_overhead) {
        return 0;
    }
    return payload_offset - tlsf_block_header_overhead;
}

bool tlsf_init(void* base, arena_header* arena) noexcept
{
    if (base == nullptr || arena == nullptr) {
        return false;
    }

    auto& ctrl     = arena->tlsf;
    ctrl.fl_bitmap = 0;
    for (std::uint32_t i = 0; i < tlsf_fl_count; ++i) {
        ctrl.sl_bitmap[i] = 0;
        for (std::uint32_t j = 0; j < tlsf_sl_count; ++j) {
            ctrl.free_list[i][j] = 0;
        }
    }

    // 把整个 user 区域作为首个 free block
    std::size_t user_offset = arena->user_offset;
    std::size_t user_size   = arena->user_size;
    if (user_size < tlsf_min_block_size) {
        return false;
    }

    // user_offset 必须 tlsf_align 对齐
    std::size_t aligned_off = align_up(user_offset, tlsf_align);
    if (aligned_off != user_offset) {
        // 调整 user_offset，损失前段
        std::size_t shift = aligned_off - user_offset;
        if (user_size <= shift) {
            return false;
        }
        arena->user_offset = aligned_off;
        arena->user_size   = user_size - shift;
        user_offset        = aligned_off;
        user_size          = arena->user_size;
    }

    // block size 向下舍入到 tlsf_align
    std::size_t block_size = align_down(user_size, tlsf_align);
    if (block_size < tlsf_min_block_size) {
        return false;
    }
    arena->user_size = block_size;

    auto* first = ptr_at<tlsf_block_header>(base, static_cast<std::uint64_t>(user_offset));
    first->prev_phys_offset = 0;
    tlsf_set_size(first, block_size, true, false);
    first->next_free_offset = 0;
    first->prev_free_offset = 0;

    insert_free_block(base, arena, first);
    return true;
}

std::uint64_t tlsf_allocate(void* base, arena_header* arena, std::size_t size, std::size_t alignment) noexcept
{
    if (base == nullptr || arena == nullptr || size == 0) {
        return 0;
    }
    if (alignment < tlsf_align) {
        alignment = tlsf_align;
    }

    std::size_t total = tlsf_required_block_size(size, alignment);
    if (total == 0) {
        return 0;
    }

    std::uint32_t fl = 0;
    std::uint32_t sl = 0;
    if (!mapping_search(total, fl, sl)) {
        return 0;
    }
    if (fl >= tlsf_fl_count) {
        return 0;
    }
    if (!search_suitable_class(arena->tlsf, fl, sl)) {
        return 0;
    }

    std::uint64_t blk_off = arena->tlsf.free_list[fl][sl];
    if (blk_off == 0) {
        return 0;
    }
    auto* blk = ptr_at<tlsf_block_header>(base, blk_off);

    // journal_window 声明 "正在修改"，崩溃后 arena_guard 的 recover() 会触发 full-repair
    journal_window jw{arena->journal, journal_op::tlsf_alloc, blk_off, blk->size_and_flags};
    remove_free_block(base, arena, blk);

    std::size_t blk_size = tlsf_block_size(blk);

    // 如果请求大对齐，可能需要 split 出 prefix
    // 对齐计算基于 arena 内 offset（不是绝对地址），确保跨进程 mmap 到不同
    // 虚拟地址时，chunk 的 arena-offset 保持不变（否则 slab 按绝对地址 mask
    // 反查 chunk 会失败）
    std::uint64_t payload_off = blk_off + tlsf_block_header_overhead;
    std::size_t   prefix_gap  = 0;
    if (alignment > tlsf_align) {
        std::uint64_t aligned_off = align_up(payload_off, static_cast<std::uint64_t>(alignment));
        prefix_gap                = static_cast<std::size_t>(aligned_off - payload_off);
        if (prefix_gap != 0) {
            if (prefix_gap < tlsf_min_block_size) {
                std::uint64_t next_aligned = align_up(aligned_off + 1,
                                                      static_cast<std::uint64_t>(alignment));
                prefix_gap                 = static_cast<std::size_t>(next_aligned - payload_off);
                aligned_off                = next_aligned;
            }
        }
        (void)aligned_off;
        if (prefix_gap > 0) {
            // 确保前缀至少能成为一个合法 free block
            if (prefix_gap < tlsf_min_block_size) {
                // 理论上不会发生；保险起见放弃该 alignment 路径
                insert_free_block(base, arena, blk);
                return 0;
            }
            // 检查剩余够不够
            if (blk_size < prefix_gap + tlsf_min_block_size) {
                insert_free_block(base, arena, blk);
                return 0;
            }

            // 把 blk 前 prefix_gap 变成独立 free block
            auto* prefix_blk = blk;
            auto* new_blk    = reinterpret_cast<tlsf_block_header*>(
                reinterpret_cast<std::byte*>(blk) + prefix_gap);

            tlsf_set_size(prefix_blk, prefix_gap, true, tlsf_block_prev_is_free(prefix_blk));
            new_blk->prev_phys_offset = offset_of(base, prefix_blk);
            tlsf_set_size(new_blk, blk_size - prefix_gap, false /*will allocate*/, true);
            new_blk->next_free_offset = 0;
            new_blk->prev_free_offset = 0;

            // 更新再之后 block 的 prev_phys_offset
            auto* after = next_phys_block(base, arena, new_blk);
            if (after != nullptr) {
                after->prev_phys_offset = offset_of(base, new_blk);
            }

            insert_free_block(base, arena, prefix_blk);

            blk         = new_blk;
            blk_off     = offset_of(base, blk);
            blk_size    = tlsf_block_size(blk);
            payload_off = blk_off + tlsf_block_header_overhead;
        }
    }

    // 标记 blk 为 allocated
    tlsf_set_size(blk, blk_size, false, tlsf_block_prev_is_free(blk));

    // 尝试 split 尾部
    if (blk_size >= total + tlsf_min_block_size) {
        split_block_if_possible(base, arena, blk, total);
        blk_size = tlsf_block_size(blk);
    }

    // 通知物理后继：前驱现在是 allocated
    auto* after = next_phys_block(base, arena, blk);
    if (after != nullptr) {
        tlsf_set_prev_free_flag(after, false);
    }

    // 更新统计
    std::uint64_t live = arena->live_bytes.fetch_add(blk_size, std::memory_order_acq_rel) + blk_size;
    std::uint64_t peak = arena->peak_bytes.load(std::memory_order_acquire);
    while (live > peak && !arena->peak_bytes.compare_exchange_weak(peak, live, std::memory_order_acq_rel)) {
        // loop
    }

    return payload_off;
}

void tlsf_deallocate(void* base, arena_header* arena, std::uint64_t payload_offset) noexcept
{
    if (base == nullptr || arena == nullptr || payload_offset == 0) {
        return;
    }
    std::uint64_t blk_off = tlsf_block_offset_from_payload(payload_offset);
    if (blk_off == 0) {
        return;
    }
    auto* blk = ptr_at<tlsf_block_header>(base, blk_off);
    if (tlsf_block_is_free(blk)) {
        // 已是 free，忽略（double-free）
        return;
    }

    std::size_t freed_size = tlsf_block_size(blk);

    // journal_window 声明 "正在释放"，崩溃后 arena_guard 的 recover() 会 full-repair
    journal_window jw{arena->journal, journal_op::tlsf_free, blk_off, blk->size_and_flags};

    // 先置 free 标志
    tlsf_set_size(blk, tlsf_block_size(blk), true, tlsf_block_prev_is_free(blk));

    // 合并前驱（若前驱 free）
    blk = absorb_prev_if_free(base, arena, blk);
    // 合并后继
    absorb_next_if_free(base, arena, blk);

    // insert 到 free_list
    insert_free_block(base, arena, blk);

    // 通知物理后继：前驱现在 free
    auto* after = next_phys_block(base, arena, blk);
    if (after != nullptr) {
        tlsf_set_prev_free_flag(after, true);
    }

    arena->live_bytes.fetch_sub(freed_size, std::memory_order_acq_rel);
}

void tlsf_recover(void* base, arena_header* arena) noexcept
{
    if (base == nullptr || arena == nullptr) {
        return;
    }

    journal_op op = journal_load(arena->journal);
    if (op == journal_op::none) {
        // 无脏标志，无需恢复
        return;
    }

    // full-repair 策略：利用"单 8B 字段写是原子"的硬件保证，
    // 从物理链条重建 free-list / bitmap / live_bytes。
    // 代价 O(N)，换取实现极其简单且可证明的可靠性。

    auto& ctrl     = arena->tlsf;
    ctrl.fl_bitmap = 0;
    for (std::uint32_t i = 0; i < tlsf_fl_count; ++i) {
        ctrl.sl_bitmap[i] = 0;
        for (std::uint32_t j = 0; j < tlsf_sl_count; ++j) {
            ctrl.free_list[i][j] = 0;
        }
    }

    std::uint64_t live     = 0;
    std::uint64_t off      = arena->user_offset;
    std::uint64_t end      = arena->user_offset + arena->user_size;
    std::uint64_t prev_off = 0;
    bool          prev_free = false;

    while (off < end) {
        auto* blk = ptr_at<tlsf_block_header>(base, off);
        std::size_t bsz = tlsf_block_size(blk);

        if (bsz < tlsf_min_block_size || off + bsz > end) {
            // 物理链条结构性损坏，repair 无法继续
            // 保留 journal.op != none 作为"需要人工介入"的信号
            return;
        }

        // 修正 prev_phys_offset 与 prev_free 标志（可能在 split/coalesce 中间态时不一致）
        blk->prev_phys_offset = prev_off;
        tlsf_set_prev_free_flag(blk, prev_free);

        if (tlsf_block_is_free(blk)) {
            blk->next_free_offset = 0;
            blk->prev_free_offset = 0;
            insert_free_block(base, arena, blk);
        } else {
            // 如果是 slab chunk，其 live_bytes 由 slab_recover 根据 bitmap 重算；tlsf_recover 只算非 slab 的部分
            auto  payload_off = off + tlsf_block_header_overhead;
            auto* maybe_chunk = ptr_at<slab_chunk_header>(base, payload_off);
            bool  is_slab_chunk = (maybe_chunk != nullptr && maybe_chunk->chunk_magic == slab_chunk_magic
                                   && maybe_chunk->class_id < arena->slab.class_count);
            if (!is_slab_chunk) {
                live += bsz;
            }
        }

        prev_off  = off;
        prev_free = tlsf_block_is_free(blk);
        off += bsz;
    }

    if (off != end) {
        // 最终 off 不等于 end，说明最后一个 block 越界；停止修复，保留 dirty flag
        return;
    }

    arena->live_bytes.store(live, std::memory_order_release);
    // peak_bytes 不回退，保持历史最大值
    // 注意：本函数不在此清除 journal flag，允许 slab_recover 在同一 journal 窗口内继续补 live_bytes。
    // journal_end 统一由 caller（arena_guard / repair）在两个 recover 都完成后调用。
}

integrity_status tlsf_check_integrity(const void* base, const arena_header* arena) noexcept
{
    if (base == nullptr || arena == nullptr || arena->magic != arena_magic) {
        return integrity_status::invalid_arena;
    }

    std::uint64_t off      = arena->user_offset;
    std::uint64_t end      = arena->user_offset + arena->user_size;
    std::uint64_t prev_off = 0;
    bool          prev_free = false;

    while (off < end) {
        const auto* blk = ptr_at<tlsf_block_header>(base, off);
        std::size_t bsz = tlsf_block_size(blk);
        if (bsz < tlsf_min_block_size || off + bsz > end) {
            return integrity_status::block_corrupted;
        }
        if (blk->prev_phys_offset != prev_off) {
            return integrity_status::block_corrupted;
        }
        if (tlsf_block_prev_is_free(blk) != prev_free) {
            return integrity_status::tlsf_inconsistent;
        }
        prev_off  = off;
        prev_free = tlsf_block_is_free(blk);
        off += bsz;
    }

    if (off != end) {
        return integrity_status::block_corrupted;
    }

    return integrity_status::ok;
}

} // namespace mc::shm::detail
