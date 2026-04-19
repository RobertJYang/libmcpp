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

#include "slab.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "tlsf.h"

namespace mc::shm::detail {
namespace {

// 默认 7 档：16 / 24 / 32 / 48 / 64 / 96 / 128 B
// slot_size = max(class_size, 16B)（保证 free 时 next_free_offset 存得下）
constexpr std::uint32_t default_class_sizes[] = {16, 24, 32, 48, 64, 96, 128};
constexpr std::uint32_t default_class_count   = sizeof(default_class_sizes) / sizeof(default_class_sizes[0]);

// free_slot_top 字段编码：高 16 bit 版本号（ABA 防护），低 48 bit slot offset
constexpr std::uint64_t slot_top_offset_mask  = 0x0000'FFFF'FFFF'FFFFULL;
constexpr std::uint32_t slot_top_version_shift = 48;

constexpr std::uint32_t slab_stripe_mask = slab_stripe_count - 1;

// stripe_of：按 slot 地址 hash 到 stripe，保证同一 slot 的 push/pop/recover 归属相同 stripe
// slot 至少 16B 对齐，低 4 位恒为 0，右移 4 位提高位熵
inline std::uint32_t stripe_of(std::uint64_t slot_offset) noexcept
{
    return static_cast<std::uint32_t>((slot_offset >> 4) & slab_stripe_mask);
}

// 起始 stripe 选择：thread-local round-robin，初值混入 pid 避免所有进程集中到 stripe 0
// 返回 [0, slab_stripe_count)，每次调用 +1，多次 allocate 自然扫描全部 stripe
inline std::uint32_t next_start_stripe() noexcept
{
    static thread_local std::uint32_t cursor =
        static_cast<std::uint32_t>(::getpid()) * 2654435761U;
    return cursor++ & slab_stripe_mask;
}

std::uint32_t slot_size_for_class(std::uint32_t class_size) noexcept
{
    // slot 对齐到 16B，保证 user payload 满足 alignof(max_align_t)
    std::uint32_t slot = class_size < 16 ? 16U : class_size;
    return static_cast<std::uint32_t>(align_up(slot, 16));
}

// bitmap 保留 512B：支持最多 4096 slot，实际 64KB chunk 上最小 slot_size=16 → 4061 slot 用 508B，足够
constexpr std::size_t slab_bitmap_reserve = 512;

std::size_t chunk_header_only_size() noexcept
{
    return align_up(sizeof(slab_chunk_header), 16);
}

std::size_t chunk_prefix_size() noexcept
{
    return align_up(chunk_header_only_size() + slab_bitmap_reserve, 16);
}

// 返回 chunk 内 bitmap 起点（紧跟 chunk_header）
std::uint8_t* chunk_bitmap(void* base, const slab_chunk_header* chunk) noexcept
{
    auto off = offset_of(base, chunk) + chunk_header_only_size();
    return ptr_at<std::uint8_t>(base, off);
}

// slot_offset → slot 在 chunk 内的 index；UINT32_MAX 代表非法
std::uint32_t slot_index_in_chunk(const slab_chunk_header* chunk, std::uint64_t slot_offset) noexcept
{
    if (slot_offset < chunk->slots_base_offset) {
        return UINT32_MAX;
    }
    auto rel = slot_offset - chunk->slots_base_offset;
    if (chunk->slot_size == 0 || rel % chunk->slot_size != 0) {
        return UINT32_MAX;
    }
    auto idx = rel / chunk->slot_size;
    if (idx >= chunk->slot_count) {
        return UINT32_MAX;
    }
    return static_cast<std::uint32_t>(idx);
}

// ---- bitmap 字节级原子操作 ----
// 跨进程并发下，bit-0 与 bit-1 同 byte 的 set/clear 必须用原子 RMW，
// 否则 plain |= / &= 会丢失另一个 bit 的修改

bool bitmap_is_set_atomic(const std::uint8_t* bm, std::uint32_t idx) noexcept
{
    auto byte = __atomic_load_n(&bm[idx >> 3], __ATOMIC_ACQUIRE);
    return (byte >> (idx & 7U)) & 1U;
}

void bitmap_set_atomic(std::uint8_t* bm, std::uint32_t idx) noexcept
{
    std::uint8_t mask = static_cast<std::uint8_t>(1U << (idx & 7U));
    __atomic_fetch_or(&bm[idx >> 3], mask, __ATOMIC_ACQ_REL);
}

void bitmap_clear_atomic(std::uint8_t* bm, std::uint32_t idx) noexcept
{
    std::uint8_t mask = static_cast<std::uint8_t>(~(1U << (idx & 7U)));
    __atomic_fetch_and(&bm[idx >> 3], mask, __ATOMIC_ACQ_REL);
}

// 用于 slab_recover 在持 arena_lock 时的非原子访问（recover 是串行的）
bool bitmap_is_set_plain(const std::uint8_t* bm, std::uint32_t idx) noexcept
{
    return (bm[idx >> 3] >> (idx & 7U)) & 1U;
}

// slot_offset → 所属 chunk（O(1) mask 反查，对齐到 slab_chunk_size）
// 使用 "arena 内 offset" 对齐（而不是绝对地址对齐），这保证跨进程 mmap 到不同
// 虚拟地址时反查结果一致。TLSF allocate 以 offset 对齐分配 chunk，保证 chunk_off
// 是 slab_chunk_size 的整数倍。
slab_chunk_header* chunk_from_slot_offset(void* base, std::uint64_t slot_offset,
                                          std::uint64_t arena_size) noexcept
{
    if (slot_offset == 0) {
        return nullptr;
    }
    auto chunk_off = slot_offset & ~(static_cast<std::uint64_t>(slab_chunk_size) - 1);
    if (chunk_off + sizeof(slab_chunk_header) > arena_size) {
        return nullptr;
    }
    auto* chunk = ptr_at<slab_chunk_header>(base, chunk_off);
    if (chunk == nullptr || chunk->chunk_magic != slab_chunk_magic) {
        return nullptr;
    }
    return chunk;
}

// ---- Treiber stack 真 CAS pop / push（per-stripe 版本） ----
//
// slot offset 必须落在 arena 的 user 区间内，否则视为损坏（崩溃残留 / 位污染）
inline bool slot_offset_in_arena(const arena_header* arena, std::uint64_t offset) noexcept
{
    if (arena == nullptr || offset == 0) {
        return false;
    }
    const std::uint64_t lo = arena->user_offset;
    const std::uint64_t hi = arena->user_offset + arena->user_size;
    return offset >= lo && (offset + sizeof(slab_slot_link)) <= hi;
}

// pop_stripe: 从指定 stripe 弹出栈顶 slot；stack 空或栈顶 offset 异常返回 0
//   - 条带化后每次只争用单个 stripe 的 free_slot_top，不同 stripe 之间互不干扰
//   - 调用者负责 pop 后的 bitmap_set_atomic（见 fast_allocate）
//   - 防御：若 free_slot_top 中 offset 不在 arena 范围内（崩溃进程残留非法值），
//     直接返回 0，交由慢路径 recover 重建；这避免 ptr_at 解引用导致 SIGBUS
std::uint64_t pop_stripe(void* base, arena_header* arena, slab_stripe& stripe) noexcept
{
    std::uint64_t old_top = stripe.free_slot_top.load(std::memory_order_acquire);
    while (true) {
        std::uint64_t offset = old_top & slot_top_offset_mask;
        if (offset == 0) {
            return 0;
        }
        if (!slot_offset_in_arena(arena, offset)) {
            // free_slot_top 被污染：放弃快路径，走慢路径 + arena_guard → recover
            return 0;
        }
        auto*         slot    = ptr_at<slab_slot_link>(base, offset);
        std::uint64_t next    = slot->next_free_offset;
        std::uint64_t version = (old_top >> slot_top_version_shift) + 1U;
        std::uint64_t new_top = (version << slot_top_version_shift) | (next & slot_top_offset_mask);

        if (stripe.free_slot_top.compare_exchange_weak(old_top, new_top,
                                                       std::memory_order_acq_rel,
                                                       std::memory_order_acquire)) {
            return offset;
        }
    }
}

// push_stripe: CAS 把 slot 挂到指定 stripe 的栈顶
void push_stripe(void* base, slab_stripe& stripe, std::uint64_t slot_offset) noexcept
{
    auto*         slot    = ptr_at<slab_slot_link>(base, slot_offset);
    std::uint64_t old_top = stripe.free_slot_top.load(std::memory_order_acquire);
    while (true) {
        slot->next_free_offset = old_top & slot_top_offset_mask;
        std::uint64_t version  = (old_top >> slot_top_version_shift) + 1U;
        std::uint64_t new_top  = (version << slot_top_version_shift) | (slot_offset & slot_top_offset_mask);

        if (stripe.free_slot_top.compare_exchange_weak(old_top, new_top,
                                                       std::memory_order_acq_rel,
                                                       std::memory_order_acquire)) {
            return;
        }
    }
}

// 持 arena_lock 下从 TLSF 批发 chunk（内部 tlsf_allocate 需要 journal）；
// 成功时用 CAS 把整条新 slot 链 prepend 到 free_slot_top（不破坏并发快路径的 push/pop）
//
// 崩溃安全：journal_op::slab_wholesale 事务保护，commit 点为 chunks_head_offset 的更新
std::uint64_t wholesale_chunk(void* base, arena_header* arena, std::uint32_t class_id) noexcept
{
    auto& cls = arena->slab.classes[class_id];
    if (cls.slot_size == 0) {
        return 0;
    }

    // TLSF 分配 slab_chunk_size 字节作为 chunk，alignment = slab_chunk_size 保证绝对地址对齐
    std::uint64_t chunk_payload_off = tlsf_allocate(base, arena, slab_chunk_size, slab_chunk_size);
    if (chunk_payload_off == 0) {
        return 0;
    }

    auto        blk_off = tlsf_block_offset_from_payload(chunk_payload_off);
    auto*       blk     = ptr_at<tlsf_block_header>(base, blk_off);
    std::size_t bsize   = tlsf_block_size(blk);

    // 事务 begin：记录 chunk_off 与 bsize，供 slab_recover 回滚未挂链的 chunk
    journal_window jw{arena->journal, journal_op::slab_wholesale, chunk_payload_off,
                      static_cast<std::uint64_t>(bsize),
                      0, 0, 0, class_id};

    // 构造 chunk header（magic 最后写，作为 chunk header 的 visibility 屏障）
    auto* chunk              = ptr_at<slab_chunk_header>(base, chunk_payload_off);
    chunk->class_id          = class_id;
    chunk->slot_size         = cls.slot_size;
    chunk->next_chunk_offset = cls.chunks_head_offset;

    const std::size_t   prefix_sz  = chunk_prefix_size();
    const std::uint64_t slots_base = chunk_payload_off + prefix_sz;
    const std::size_t   usable     = slab_chunk_size - prefix_sz;
    const std::uint32_t slot_count = static_cast<std::uint32_t>(usable / cls.slot_size);
    chunk->slot_count        = slot_count;
    chunk->free_count.store(slot_count, std::memory_order_relaxed);
    chunk->slots_base_offset = slots_base;

    // 初始化 bitmap 为全 0（所有 slot 为 free）
    std::memset(chunk_bitmap(base, chunk), 0, slab_bitmap_reserve);

    // 写入 chunk_magic：此后 tlsf_recover 会把该 chunk 视为 slab chunk 跳过 live_bytes 计数
    chunk->chunk_magic = slab_chunk_magic;

    // 条带化：按 stripe_of(slot_off) 把 slot 分成 N 条本地链，每条 prepend 到对应 stripe
    // 崩溃一致性：slot 归属固定由 slot_off hash 决定，recover 按相同 hash 重建，
    // 不会因崩溃时未完成所有 stripe 的 prepend 而丢失 slot
    std::uint64_t local_head[slab_stripe_count] = {0};
    std::uint64_t local_tail[slab_stripe_count] = {0};
    for (std::uint32_t i = 0; i < slot_count; ++i) {
        std::uint64_t slot_off = slots_base + static_cast<std::uint64_t>(i) * cls.slot_size;
        std::uint32_t s        = stripe_of(slot_off);
        auto*         slot     = ptr_at<slab_slot_link>(base, slot_off);
        slot->next_free_offset = local_head[s];
        if (local_tail[s] == 0) {
            local_tail[s] = slot_off;
        }
        local_head[s] = slot_off;
    }

    // 每个 stripe 独立做 CAS prepend
    for (std::uint32_t s = 0; s < slab_stripe_count; ++s) {
        if (local_head[s] == 0) {
            continue;
        }
        auto*         tail_slot = ptr_at<slab_slot_link>(base, local_tail[s]);
        auto&         stripe    = cls.stripes[s];
        std::uint64_t old_top   = stripe.free_slot_top.load(std::memory_order_acquire);
        while (true) {
            tail_slot->next_free_offset = old_top & slot_top_offset_mask;
            std::uint64_t version       = (old_top >> slot_top_version_shift) + 1U;
            std::uint64_t new_top       = (version << slot_top_version_shift)
                                        | (local_head[s] & slot_top_offset_mask);
            if (stripe.free_slot_top.compare_exchange_weak(old_top, new_top,
                                                           std::memory_order_acq_rel,
                                                           std::memory_order_acquire)) {
                break;
            }
        }
    }

    // commit 点：把 chunk 挂入 class chunks 链表；挂链后对所有 reader 可见
    cls.chunks_head_offset = chunk_payload_off;

    // 最后把 TLSF 批发的 chunk overhead 从 live_bytes 扣除，使 live_bytes 反映用户视角
    // （tlsf_allocate 时已 +bsize，wholesale 不把 chunk 计入用户 live，故 -bsize 相抵）
    arena->live_bytes.fetch_sub(bsize, std::memory_order_acq_rel);

    return chunk_payload_off;
}

// pop 成功后的收尾：bitmap_set + live_bytes 累加 + peak 更新
// 调用者保证 slot_off != 0 且属于 class_id 指定的 class
void slot_finalize_allocated(void* base, arena_header* arena, std::uint32_t class_size,
                             std::uint64_t slot_off) noexcept
{
    std::uint64_t arena_bytes = arena->user_offset + arena->user_size;
    if (auto* chunk = chunk_from_slot_offset(base, slot_off, arena_bytes); chunk != nullptr) {
        if (auto idx = slot_index_in_chunk(chunk, slot_off); idx != UINT32_MAX) {
            bitmap_set_atomic(chunk_bitmap(base, chunk), idx);
        }
        // free_count 仅作辅助计数，不精确时由 slab_recover 从 bitmap 重建
        auto cur = chunk->free_count.load(std::memory_order_relaxed);
        if (cur > 0) {
            chunk->free_count.fetch_sub(1, std::memory_order_relaxed);
        }
    }
    std::uint64_t live = arena->live_bytes.fetch_add(class_size, std::memory_order_acq_rel) + class_size;
    std::uint64_t peak = arena->peak_bytes.load(std::memory_order_acquire);
    while (live > peak && !arena->peak_bytes.compare_exchange_weak(peak, live, std::memory_order_acq_rel)) {
        // loop until peak >= live
    }
}

} // namespace

bool slab_init(void* /*base*/, arena_header* arena) noexcept
{
    if (arena == nullptr) {
        return false;
    }
    arena->slab.class_count = default_class_count;
    for (std::uint32_t i = 0; i < slab_max_classes; ++i) {
        auto& cls              = arena->slab.classes[i];
        cls.class_size         = i < default_class_count ? default_class_sizes[i] : 0;
        cls.slot_size          = i < default_class_count ? slot_size_for_class(cls.class_size) : 0;
        cls.chunks_head_offset = 0;
        for (std::uint32_t s = 0; s < slab_stripe_count; ++s) {
            cls.stripes[s].free_slot_top.store(0, std::memory_order_relaxed);
        }
    }
    return true;
}

std::uint32_t slab_find_class(const slab_control& ctrl, std::size_t size, std::size_t alignment) noexcept
{
    if (ctrl.class_count == 0) {
        return UINT32_MAX;
    }
    // slab 只处理默认对齐（<= 16B）；更大对齐走 TLSF
    if (alignment > 16) {
        return UINT32_MAX;
    }
    for (std::uint32_t i = 0; i < ctrl.class_count; ++i) {
        if (ctrl.classes[i].class_size >= size) {
            return i;
        }
    }
    return UINT32_MAX;
}

slab_chunk_header* slab_chunk_of_slot(void* base, arena_header* arena, std::uint64_t slot_offset) noexcept
{
    if (slot_offset == 0 || arena == nullptr) {
        return nullptr;
    }
    std::uint64_t arena_bytes = arena->user_offset + arena->user_size;
    auto*         chunk       = chunk_from_slot_offset(base, slot_offset, arena_bytes);
    if (chunk == nullptr) {
        return nullptr;
    }
    // 验证 slot 对齐到 slot_size 边界
    std::uint64_t begin = chunk->slots_base_offset;
    std::uint64_t end   = begin + static_cast<std::uint64_t>(chunk->slot_count) * chunk->slot_size;
    if (slot_offset < begin || slot_offset >= end) {
        return nullptr;
    }
    std::uint64_t rel = slot_offset - begin;
    if (rel % chunk->slot_size != 0) {
        return nullptr;
    }
    return chunk;
}

// ---- 无锁快路径 ----
//
// slab_fast_allocate 不持 arena_lock。CAS pop 成功返回 slot；stack 空返回 0
// （调用者负责持锁做 wholesale 后再重试）。
//
// 崩溃一致性 / in-flight race：
//   1) 快路径进程崩溃（不持 arena_lock）不会把 journal 置脏，因此不触发 recover，
//      不会干扰其他进程。崩溃点若在 pop 和 bitmap_set 之间，slot 状态
//      (bitmap=0, 不在 stack) 属于 "孤立 slot"；下次 slab_recover 被触发
//      （例如有持锁进程崩溃）时，从 bitmap=0 扫描会重新 push 回 stack。
//   2) 其他进程持 arena_lock 做 TLSF/wholesale 时崩溃：arena_guard 在下一
//      个 lock_ex() 返回 recovered 时触发 recover，recover 完成后
//      recover_epoch +1。本函数在 pop 前后记录 epoch，若发生变化则
//      视为 in-flight 被 recover 干扰，撤销本次分配（让 caller 走慢路径）。
//      撤销安全性：pop 后未 bitmap_set，slot 仍为 bitmap=0；recover 已按
//      bitmap 重建 stack，slot 要么已被 push 回 stack，要么孤立；两种情况
//      下次快路径/recover 均能正确回收，不泄漏、不 double-alloc。
std::uint64_t slab_fast_allocate(void* base, arena_header* arena, std::size_t size,
                                 std::size_t alignment) noexcept
{
    if (arena == nullptr || base == nullptr) {
        return 0;
    }
    std::uint32_t class_id = slab_find_class(arena->slab, size, alignment);
    if (class_id == UINT32_MAX) {
        return 0;
    }
    auto& cls = arena->slab.classes[class_id];

    std::uint64_t e0 = arena->recover_epoch.load(std::memory_order_acquire);

    // 条带化 pop：从 round-robin 起始 stripe 扫描；空则顺延到下一个 stripe
    // 全部 stripe 均空时返回 0（交给慢路径走 wholesale）
    std::uint32_t start    = next_start_stripe();
    std::uint64_t slot_off = 0;
    for (std::uint32_t k = 0; k < slab_stripe_count; ++k) {
        std::uint32_t s = (start + k) & slab_stripe_mask;
        slot_off        = pop_stripe(base, arena, cls.stripes[s]);
        if (slot_off != 0) {
            break;
        }
    }
    if (slot_off == 0) {
        return 0;
    }

    // 检测 in-flight race：pop 成功后若 epoch 变动，说明 recover 在此期间运行，
    // 可能已把 slot 误 push 回 stripe（若 recover 看到 bitmap=0）。为避免 double-alloc，
    // 保守地放弃本次分配，让 caller 走慢路径重试
    std::uint64_t e1 = arena->recover_epoch.load(std::memory_order_acquire);
    if (e1 != e0) {
        // slot_off 已从 stripe 弹出但未 bitmap_set；state = (bitmap=0, 不在 stripe)。
        // recover 若已把它 push 回去，现在对应 stripe 里会有一份 slot_off；bitmap 仍为 0。
        // 下次 fast_allocate 会把它正常 pop 出来复用，不泄漏。若 recover 没看到 slot_off
        // （时间窗口错开），则 slot_off 现在是孤立的（bitmap=0 但不在任何 stripe），下次
        // 任何 slab_recover 调用都会把它 push 回对应 stripe，仍不泄漏。
        return 0;
    }

    slot_finalize_allocated(base, arena, cls.class_size, slot_off);
    return slot_off;
}

// deallocate 无锁快路径：payload 对应 chunk 有 slab magic 则认为属 slab 管辖，
// 清 bitmap + CAS push 完成释放，返回 true；否则返回 false（交给 TLSF 处理）
bool slab_fast_deallocate(void* base, arena_header* arena, std::uint64_t payload_offset) noexcept
{
    if (arena == nullptr || base == nullptr || payload_offset == 0) {
        return false;
    }
    auto* chunk = slab_chunk_of_slot(base, arena, payload_offset);
    if (chunk == nullptr) {
        return false;
    }
    if (chunk->class_id >= arena->slab.class_count) {
        return false;
    }
    auto idx = slot_index_in_chunk(chunk, payload_offset);
    if (idx == UINT32_MAX) {
        return false;
    }

    // 双 free 检测：bitmap 已是 clear 意味着 slot 已在 stack，直接返回 true（吞掉重复释放）
    auto* bm = chunk_bitmap(base, chunk);
    if (!bitmap_is_set_atomic(bm, idx)) {
        return true;
    }
    // 顺序：先 clear bitmap（标记为"free"候选），再 CAS push
    // 崩溃在 clear-bitmap 之后、CAS push 之前：slot 状态 (bitmap=0, 不在 stripe) 与 pop
    // 崩溃场景一致，recover 会按 stripe_of(slot_off) 重新 push 到相应 stripe，slot 不泄漏
    bitmap_clear_atomic(bm, idx);

    // stripe 归属由 slot_off hash 固定，与 allocate 时 pop 的 stripe 无关
    // 保证 recover 重建后同一 slot 总是落在相同 stripe，不会"迷路"
    auto& cls = arena->slab.classes[chunk->class_id];
    push_stripe(base, cls.stripes[stripe_of(payload_offset)], payload_offset);

    // free_count 辅助计数（不精确由 recover 纠正）
    auto cur = chunk->free_count.load(std::memory_order_relaxed);
    if (cur < chunk->slot_count) {
        chunk->free_count.fetch_add(1, std::memory_order_relaxed);
    }

    arena->live_bytes.fetch_sub(arena->slab.classes[chunk->class_id].class_size, std::memory_order_acq_rel);
    return true;
}

// 持锁版本：保持原有 API，仅多一层"快路径失败 → wholesale → 再走快路径"的封装
// 调用方（shm_allocator::allocate 慢路径）保证已持 arena_lock
std::uint64_t slab_try_allocate(void* base, arena_header* arena, std::size_t size, std::size_t alignment) noexcept
{
    // 先试快路径（锁内也可用；可能在上锁瞬间有其他进程 wholesale 过）
    std::uint64_t off = slab_fast_allocate(base, arena, size, alignment);
    if (off != 0) {
        return off;
    }
    // 适合 slab 的 size/alignment 才走 wholesale；否则交给 TLSF
    std::uint32_t class_id = slab_find_class(arena->slab, size, alignment);
    if (class_id == UINT32_MAX) {
        return 0;
    }
    if (wholesale_chunk(base, arena, class_id) == 0) {
        return 0;
    }
    return slab_fast_allocate(base, arena, size, alignment);
}

// 持锁版本 deallocate：与快路径逻辑一致（CAS 本就支持持锁 caller）
bool slab_try_deallocate(void* base, arena_header* arena, std::uint64_t payload_offset) noexcept
{
    return slab_fast_deallocate(base, arena, payload_offset);
}

namespace {

// 判断 chunk_off 是否已经挂入指定 class 的 chunks 链表
bool chunk_is_linked(const void* base, const slab_class_control& cls, std::uint64_t chunk_off) noexcept
{
    std::uint64_t cur = cls.chunks_head_offset;
    // 链表长度限制：避免坏链导致死循环
    for (std::uint32_t guard = 0; guard < 1U << 20; ++guard) {
        if (cur == 0) {
            return false;
        }
        if (cur == chunk_off) {
            return true;
        }
        const auto* chunk = ptr_at<slab_chunk_header>(base, cur);
        if (chunk == nullptr || chunk->chunk_magic != slab_chunk_magic) {
            return false;
        }
        cur = chunk->next_chunk_offset;
    }
    return false;
}

// slab_wholesale 事务未提交时的回滚：把半成品 chunk 还给 TLSF
// 调用者必须保证 arena->journal.op == slab_wholesale 且 chunk 未挂入 chunks_head
void rollback_wholesale(void* base, arena_header* arena, std::uint64_t chunk_payload_off,
                        std::uint64_t bsize) noexcept
{
    if (base == nullptr || arena == nullptr || chunk_payload_off == 0) {
        return;
    }
    // tlsf_recover 对 chunk_magic == slab_chunk_magic 的块跳过 live_bytes 计数；
    // 此处要把 chunk 还给 TLSF，tlsf_deallocate 内部会 -bsize，因此需要先 +bsize 补齐
    auto* chunk = ptr_at<slab_chunk_header>(base, chunk_payload_off);
    if (chunk != nullptr && chunk->chunk_magic == slab_chunk_magic) {
        arena->live_bytes.fetch_add(bsize, std::memory_order_acq_rel);
        // 清 magic，避免 tlsf_deallocate 后残留"看起来像 slab chunk"的物理块
        chunk->chunk_magic = 0;
    }
    // 若 chunk_magic 未写入，tlsf_recover 已把 bsize 计入 live_bytes，直接 tlsf_deallocate 即可
    tlsf_deallocate(base, arena, chunk_payload_off);
}

} // namespace

void slab_recover(void* base, arena_header* arena) noexcept
{
    if (base == nullptr || arena == nullptr) {
        return;
    }
    // journal 干净 → 无需 recover；避免重复 fetch_add 导致 live_bytes 翻倍
    journal_op op = journal_load(arena->journal);
    if (op == journal_op::none) {
        return;
    }

    // slab_wholesale 事务特判：若 chunk 未挂入 class chunks 链表，说明 commit 点之前崩溃，
    // 需要把 chunk 还给 TLSF 以避免 64KB 泄漏。
    if (op == journal_op::slab_wholesale) {
        std::uint64_t chunk_off = arena->journal.arg0;
        std::uint64_t bsize     = arena->journal.arg1;
        std::uint32_t class_id  = arena->journal.misc;
        if (chunk_off != 0 && class_id < arena->slab.class_count) {
            const auto& cls = arena->slab.classes[class_id];
            if (!chunk_is_linked(base, cls, chunk_off)) {
                rollback_wholesale(base, arena, chunk_off, bsize);
            }
        }
    }

    // 以 bitmap 为 ground truth，重建每个 class 每个 stripe 的 free-list 与 chunk.free_count，
    // 并把 slab 部分的 live_bytes 补回到 arena->live_bytes（tlsf_recover 已跳过 slab chunks）。
    // 注意此处持 arena_lock 串行，用 plain 操作而非 atomic。
    // 条带化：按 stripe_of(slot_off) 把 free slot 分发到对应 stripe，
    // 与 fast_deallocate/wholesale 的 hash 规则严格一致，保证崩溃前后 slot 归属不变。
    std::uint64_t slab_live = 0;
    for (std::uint32_t i = 0; i < arena->slab.class_count; ++i) {
        auto& cls = arena->slab.classes[i];
        for (std::uint32_t s = 0; s < slab_stripe_count; ++s) {
            cls.stripes[s].free_slot_top.store(0, std::memory_order_relaxed);
        }

        std::uint32_t class_used = 0;
        std::uint64_t chunk_off  = cls.chunks_head_offset;
        while (chunk_off != 0) {
            auto* chunk = ptr_at<slab_chunk_header>(base, chunk_off);
            if (chunk == nullptr || chunk->chunk_magic != slab_chunk_magic) {
                break;
            }
            auto*         bm            = chunk_bitmap(base, chunk);
            std::uint32_t free_in_chunk = 0;
            for (std::uint32_t idx = 0; idx < chunk->slot_count; ++idx) {
                if (!bitmap_is_set_plain(bm, idx)) {
                    std::uint64_t slot_off =
                        chunk->slots_base_offset + static_cast<std::uint64_t>(idx) * chunk->slot_size;
                    std::uint32_t s       = stripe_of(slot_off);
                    auto&         stripe  = cls.stripes[s];
                    std::uint64_t old_top =
                        stripe.free_slot_top.load(std::memory_order_relaxed) & slot_top_offset_mask;
                    auto* slot             = ptr_at<slab_slot_link>(base, slot_off);
                    slot->next_free_offset = old_top;
                    // recover 单线程无 ABA 风险，version 保持 0
                    stripe.free_slot_top.store(slot_off & slot_top_offset_mask, std::memory_order_release);
                    ++free_in_chunk;
                }
            }
            chunk->free_count.store(free_in_chunk, std::memory_order_relaxed);
            class_used += chunk->slot_count - free_in_chunk;
            chunk_off = chunk->next_chunk_offset;
        }
        slab_live += static_cast<std::uint64_t>(class_used) * cls.class_size;
    }
    arena->live_bytes.fetch_add(slab_live, std::memory_order_acq_rel);
}

integrity_status slab_check_integrity(const void* base, const arena_header* arena) noexcept
{
    if (base == nullptr || arena == nullptr) {
        return integrity_status::invalid_arena;
    }

    for (std::uint32_t i = 0; i < arena->slab.class_count; ++i) {
        const auto& cls       = arena->slab.classes[i];
        std::uint64_t chunk_off = cls.chunks_head_offset;
        while (chunk_off != 0) {
            const auto* chunk = ptr_at<slab_chunk_header>(base, chunk_off);
            if (chunk->chunk_magic != slab_chunk_magic) {
                return integrity_status::slab_inconsistent;
            }
            if (chunk->class_id != i || chunk->slot_size != cls.slot_size) {
                return integrity_status::slab_inconsistent;
            }
            chunk_off = chunk->next_chunk_offset;
        }
    }
    return integrity_status::ok;
}

} // namespace mc::shm::detail
