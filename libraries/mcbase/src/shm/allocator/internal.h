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

#ifndef MC_SHM_SRC_ALLOCATOR_INTERNAL_H
#define MC_SHM_SRC_ALLOCATOR_INTERNAL_H

#include <atomic>
#include <cstddef>
#include <cstdint>

#include <mc/shm/allocator.h>
#include <mc/shm/ipc_mutex.h>

namespace mc::shm::detail {

// ================ 常量与参数 ================

// TLSF: 最小 class 32B（2^5），最多 2^(5+27) = ~4GB 单 arena
constexpr std::uint32_t tlsf_fl_shift         = 5;
constexpr std::uint32_t tlsf_fl_count         = 27;
constexpr std::uint32_t tlsf_sl_log2          = 4;
constexpr std::uint32_t tlsf_sl_count         = 1u << tlsf_sl_log2; // 16
constexpr std::size_t   tlsf_align            = 16;                 // block 必须 16B 对齐
constexpr std::size_t   tlsf_min_block_size   = 32;                 // header 16B + 至少 16B 可复用

// block flags（低 2 位为标志，size 一定 4 字节对齐）
constexpr std::uint64_t block_flag_free      = 1ULL << 0;
constexpr std::uint64_t block_flag_prev_free = 1ULL << 1;
constexpr std::uint64_t block_flag_mask      = 0x3ULL;
constexpr std::uint64_t block_size_mask      = ~0x3ULL;

// arena magic
constexpr std::uint32_t arena_magic   = 0x4d43414cU; // "MCAL"
constexpr std::uint32_t arena_version = 2;

// ================ TLSF block header ================

// 前 16 字节为 allocated 与 free 共用的 header；后 16 字节仅 free 时使用
struct tlsf_block_header {
    std::uint64_t prev_phys_offset;
    std::uint64_t size_and_flags;

    // free-list 双向链表指针，仅 free 时有效；allocated 时复用为用户 payload
    std::uint64_t next_free_offset;
    std::uint64_t prev_free_offset;
};

constexpr std::size_t tlsf_block_header_overhead = 16;

// ================ slab ================

constexpr std::uint32_t slab_max_classes         = 16;
constexpr std::size_t   slab_chunk_size          = 64 * 1024;
constexpr std::uint64_t slab_chunk_magic         = 0x4d43534843484b56ULL; // "MCSHCHKV"
constexpr std::size_t   slab_max_class_size_default = 128;

// slab 条带化：每个 size_class 拆分 N 个独立 free-list 头（stripe），
// 每个 stripe 独占 cache line 避免 false sharing，按 slot_off hash 分发。
// 降低跨进程 CAS 热点：N 进程 ÷ N stripe ≈ 每 stripe 1 进程平均争用。
// 必须是 2 的幂以便用位与代替取模。
constexpr std::uint32_t slab_stripe_count = 16;
static_assert((slab_stripe_count & (slab_stripe_count - 1)) == 0,
              "slab_stripe_count 必须是 2 的幂");

// slab slot 内 header: 仅 "free 时" 使用，存 next_free 指针
// allocated slot 的前 8 字节让给用户
struct slab_slot_link {
    std::uint64_t next_free_offset;
};

// 一个 chunk（从 TLSF 批发的 64KB 块）
// free_count 为 atomic：slab 快路径下 pop/push 可并发 fetch_add/fetch_sub，
// 不精确时由 slab_recover 从 bitmap 重建纠正
struct slab_chunk_header {
    std::uint64_t              chunk_magic;
    std::uint32_t              class_id;
    std::uint32_t              slot_size;
    std::uint32_t              slot_count;
    std::atomic<std::uint32_t> free_count;
    std::uint64_t              next_chunk_offset; // 同 class 下一个 chunk
    std::uint64_t              slots_base_offset; // chunk header 之后的 slot 数组起点
};

// 单个条带的 Treiber stack 头：ABA 通过高 16 bit 版本号防护，低 48 bit 存 slot offset
// 每个 stripe 独占 64B cache line，避免相邻 stripe 之间 false sharing
struct alignas(64) slab_stripe {
    std::atomic<std::uint64_t> free_slot_top;  // (version << 48) | offset
    std::uint8_t               _pad[56];
};
static_assert(sizeof(slab_stripe) == 64, "slab_stripe 必须独占单个 cache line");

// size_class 控制块：read-mostly 元数据 + N 个 per-stripe Treiber stack
// _pad_head 把 stripes 推到 64B 边界，避免元数据与 stripe[0] 共享 cache line
struct slab_class_control {
    std::uint32_t class_size;         // 用户可见对象 size 上限
    std::uint32_t slot_size;          // 实际 slot 大小（>= class_size，至少 16B）
    std::uint64_t chunks_head_offset; // 该 class 所有 chunk 的单向链表头（按 class 归属）
    std::uint8_t  _pad_head[48];      // 对齐到 64B
    slab_stripe   stripes[slab_stripe_count];
};
static_assert(sizeof(slab_class_control) == 64 + 64 * slab_stripe_count,
              "slab_class_control 布局错误");

// slab 总控制块
struct slab_control {
    std::uint32_t      class_count;
    std::uint32_t      _reserved;
    slab_class_control classes[slab_max_classes];
};

// ================ journal ================
//
// journal 的 commit 点只有一个：`op` 从 none 切换到非 none 的原子 store。
// `op == none`       → recover 直接返回（payload 字段无意义）
// `op != none`       → payload 字段一定是 begin 时写入的值（release-acquire 配对保证可见性）
//
// 每种 op 对 arg0..arg4 / misc 的语义如下（union 风格）：
//
// tlsf_alloc (从 free-list pop 一个 block，准备转成 allocated)
//   arg0 = block_offset              被 pop 的 block
//   arg1 = saved_size_and_flags      修改前 block.size_and_flags（回滚用）
//   arg2 = next_phys_offset          若 !=0, 需要同步 next 的 prev_free 位
//   arg3 = saved_next_size_and_flags next 修改前 flags（回滚用）
//   arg4 = 0
//   misc = 0
//
// tlsf_split (将一个大 block 分出 remainder 挂回 free-list)
//   arg0 = block_offset              被 split 的 block
//   arg1 = saved_size_and_flags      split 前 block.size_and_flags
//   arg2 = remainder_offset          remainder 起点
//   arg3 = remainder_size            remainder 大小（含 header）
//   arg4 = next_phys_after_remainder !=0 时需要更新其 prev_phys_offset → remainder
//   misc = 0
//
// tlsf_free (把 allocated block 插回 free-list)
//   arg0 = block_offset
//   arg1 = saved_size_and_flags
//   arg2 = next_phys_offset          需要更新其 prev_free 位
//   arg3 = saved_next_size_and_flags
//   arg4 = 0
//   misc = 0
//
// tlsf_coalesce (合并 primary 与某个邻居 block)
//   arg0 = primary_offset
//   arg1 = primary_saved_size_and_flags
//   arg2 = neighbor_offset           被吞并的邻居
//   arg3 = neighbor_saved_size_and_flags
//   arg4 = post_neighbor_phys_offset 合并后需要更新其 prev_phys
//   misc = direction                 0 = merge forward (next), 1 = merge backward (prev)
//
// slab_wholesale (从 TLSF 批发一个 chunk 并串入 class free-list)
//   arg0 = chunk_offset              从 TLSF 拿到的 chunk 起点
//   arg1 = chunk_block_size          TLSF block 大小（回滚时 free 用）
//   arg2 = slot_count
//   arg3 = slots_base_offset
//   arg4 = old_free_slot_top         修改前 class.free_slot_top
//   misc = class_id

enum class journal_op : std::uint8_t {
    none           = 0,
    tlsf_alloc     = 1,
    tlsf_split     = 2,
    tlsf_free      = 3,
    tlsf_coalesce  = 4,
    slab_wholesale = 5,
};

// journal 放在单 cacheline，避免 false sharing；字段按 op 不同含义不同
struct alignas(64) allocator_journal {
    std::atomic<std::uint8_t> op;
    std::uint8_t              _pad0[7];
    std::uint64_t             arg0;
    std::uint64_t             arg1;
    std::uint64_t             arg2;
    std::uint64_t             arg3;
    std::uint64_t             arg4;
    std::uint32_t             misc;
    std::uint8_t              _pad1[12];
};

static_assert(sizeof(allocator_journal) == 64, "journal 必须占用单个 cacheline");

// ================ journal 写入协议包装 ================
//
// 严格的 3 步：
//   1. 填 payload（顺序不敏感；崩溃在此阶段无害，op 仍为 none）
//   2. `release` store op = 非 none（此后 payload 对 recover() 可见）
//   3. 实际修改共享结构
//   4. `release` store op = none（修改稳定后关闭 journal）
//
// recover() 通过 `acquire` load op 读到 payload 的稳定快照。

inline void journal_reset(allocator_journal& j) noexcept
{
    j.arg0 = 0;
    j.arg1 = 0;
    j.arg2 = 0;
    j.arg3 = 0;
    j.arg4 = 0;
    j.misc = 0;
    j.op.store(static_cast<std::uint8_t>(journal_op::none), std::memory_order_release);
}

inline void journal_begin(allocator_journal& j, journal_op op,
                          std::uint64_t a0, std::uint64_t a1,
                          std::uint64_t a2 = 0, std::uint64_t a3 = 0,
                          std::uint64_t a4 = 0, std::uint32_t misc = 0) noexcept
{
    j.arg0 = a0;
    j.arg1 = a1;
    j.arg2 = a2;
    j.arg3 = a3;
    j.arg4 = a4;
    j.misc = misc;
    // release: 保证 payload 写入对任何 acquire-load 到 op != none 的读者都可见
    j.op.store(static_cast<std::uint8_t>(op), std::memory_order_release);
}

inline void journal_end(allocator_journal& j) noexcept
{
    j.op.store(static_cast<std::uint8_t>(journal_op::none), std::memory_order_release);
}

inline journal_op journal_load(const allocator_journal& j) noexcept
{
    return static_cast<journal_op>(j.op.load(std::memory_order_acquire));
}

// RAII 包装：构造即 begin，析构即 end；只在正常路径用于标识"修改窗口"
// 崩溃路径由 arena_guard 检测 ipc_mutex::recovered 后驱动 recover() 处理。
class journal_window {
public:
    journal_window(allocator_journal& j, journal_op op,
                   std::uint64_t a0, std::uint64_t a1,
                   std::uint64_t a2 = 0, std::uint64_t a3 = 0,
                   std::uint64_t a4 = 0, std::uint32_t misc = 0) noexcept
        : m_j(&j)
    {
        journal_begin(*m_j, op, a0, a1, a2, a3, a4, misc);
    }

    ~journal_window()
    {
        if (m_j != nullptr) {
            journal_end(*m_j);
        }
    }

    journal_window(const journal_window&)            = delete;
    journal_window& operator=(const journal_window&) = delete;

private:
    allocator_journal* m_j;
};

// ================ TLSF 控制块 ================

struct tlsf_control {
    std::uint32_t fl_bitmap;
    std::uint32_t _reserved;
    std::uint32_t sl_bitmap[tlsf_fl_count];
    // free_list[fl][sl] 存放该 class 的第一个 free block 的 offset（0 表示空）
    std::uint64_t free_list[tlsf_fl_count][tlsf_sl_count];
};

// ================ arena header（整个 arena 的元数据头） ================

struct alignas(64) arena_header {
    std::uint32_t magic;
    std::uint32_t version;
    ipc_mutex     arena_lock;

    std::uint64_t arena_size;         // arena 总字节数（含 header）
    std::uint64_t user_offset;        // TLSF 可管理区域起点（相对 arena base）
    std::uint64_t user_size;          // TLSF 可管理区域大小

    std::atomic<std::uint64_t> live_bytes;
    std::atomic<std::uint64_t> peak_bytes;

    // recover_epoch：每完成一次 recover +1。slab 快路径读 epoch 前后比对，
    // 若期间发生 recover 则撤销本次分配（bitmap_clear + 让 caller 走慢路径重试），
    // 避免 "in-flight 快路径 + 并发 recover" 导致 double-allocate
    std::atomic<std::uint64_t> recover_epoch;

    std::uint8_t _pad_before_journal[8];

    allocator_journal journal;
    tlsf_control      tlsf;
    slab_control      slab;
};

// ================ 工具函数 ================

template <typename T>
inline T* ptr_at(void* base, std::uint64_t offset) noexcept
{
    if (offset == 0 || base == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<T*>(static_cast<std::byte*>(base) + offset);
}

template <typename T>
inline const T* ptr_at(const void* base, std::uint64_t offset) noexcept
{
    if (offset == 0 || base == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<const T*>(static_cast<const std::byte*>(base) + offset);
}

inline std::uint64_t offset_of(const void* base, const void* ptr) noexcept
{
    if (base == nullptr || ptr == nullptr) {
        return 0;
    }
    return static_cast<std::uint64_t>(static_cast<const std::byte*>(ptr) - static_cast<const std::byte*>(base));
}

inline std::size_t tlsf_block_size(const tlsf_block_header* blk) noexcept
{
    return static_cast<std::size_t>(blk->size_and_flags & block_size_mask);
}

inline bool tlsf_block_is_free(const tlsf_block_header* blk) noexcept
{
    return (blk->size_and_flags & block_flag_free) != 0;
}

inline bool tlsf_block_prev_is_free(const tlsf_block_header* blk) noexcept
{
    return (blk->size_and_flags & block_flag_prev_free) != 0;
}

inline void tlsf_set_size(tlsf_block_header* blk, std::size_t size, bool is_free, bool prev_free) noexcept
{
    blk->size_and_flags = (static_cast<std::uint64_t>(size) & block_size_mask)
                        | (is_free ? block_flag_free : 0ULL)
                        | (prev_free ? block_flag_prev_free : 0ULL);
}

inline void tlsf_set_free_flag(tlsf_block_header* blk, bool is_free) noexcept
{
    if (is_free) {
        blk->size_and_flags |= block_flag_free;
    } else {
        blk->size_and_flags &= ~block_flag_free;
    }
}

inline void tlsf_set_prev_free_flag(tlsf_block_header* blk, bool prev_free) noexcept
{
    if (prev_free) {
        blk->size_and_flags |= block_flag_prev_free;
    } else {
        blk->size_and_flags &= ~block_flag_prev_free;
    }
}

inline std::size_t align_up(std::size_t value, std::size_t alignment) noexcept
{
    return (value + alignment - 1) & ~(alignment - 1);
}

inline std::size_t align_down(std::size_t value, std::size_t alignment) noexcept
{
    return value & ~(alignment - 1);
}

// 位操作：fls64 找最高位；ffs32 找最低位
inline std::uint32_t fls64(std::uint64_t x) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
    return x == 0 ? 0 : static_cast<std::uint32_t>(63U - static_cast<std::uint32_t>(__builtin_clzll(x)));
#else
    std::uint32_t r = 0;
    while (x > 1) {
        x >>= 1;
        ++r;
    }
    return r;
#endif
}

inline std::uint32_t ffs32(std::uint32_t x) noexcept
{
#if defined(__GNUC__) || defined(__clang__)
    return x == 0 ? 32U : static_cast<std::uint32_t>(__builtin_ctz(x));
#else
    std::uint32_t r = 0;
    if (x == 0) {
        return 32;
    }
    while ((x & 1U) == 0) {
        x >>= 1;
        ++r;
    }
    return r;
#endif
}

} // namespace mc::shm::detail

#endif // MC_SHM_SRC_ALLOCATOR_INTERNAL_H
