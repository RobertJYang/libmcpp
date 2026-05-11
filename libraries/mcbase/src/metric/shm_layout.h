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

#ifndef MC_METRIC_SHM_LAYOUT_H
#define MC_METRIC_SHM_LAYOUT_H

#include <atomic>
#include <cstddef>
#include <cstdint>

#include <mc/metric/label_set.h>
#include <mc/metric/metric_kind.h>

namespace mc::metric::detail {

constexpr std::uint32_t metric_table_magic   = 0x4D435452U; // 'MCTR'
constexpr std::uint32_t metric_table_version = 1U;

// 表头放在共享内存起始处；之后依次紧跟 slots、descriptor arena、histogram arena
struct alignas(64) metric_table_header {
    std::atomic<std::uint32_t> magic;
    std::atomic<std::uint32_t> version;
    std::uint64_t              capacity;          // 槽数，2 的幂
    std::uint64_t              slots_off;         // 相对 header 起始
    std::uint64_t              arena_off;         // descriptor arena
    std::uint64_t              arena_size;
    std::atomic<std::uint64_t> arena_used;        // bump pointer
    std::uint64_t              hist_arena_off;
    std::uint64_t              hist_arena_size;
    std::atomic<std::uint64_t> hist_arena_used;
    std::atomic<std::uint64_t> registered;
    std::atomic<std::uint64_t> overflow_drops;
    std::atomic<std::uint64_t> writer_endpoint_recovered; // for future
    // 留给将来扩展（采样率、quota 等）
    std::uint64_t              reserved[4];
};

static_assert(sizeof(metric_table_header) == 128,
              "metric_table_header 必须保持 128 字节，避免后续布局漂移");

// slot 是底层热路径数据，按 64 字节对齐避免跨 cache-line false-sharing
struct alignas(64) metric_slot {
    std::atomic<std::uint64_t> metric_id;       // 0 = 空槽；非 0 = FNV-1a(name,labels)
    std::atomic<std::uint8_t>  kind_byte;       // mc::metric::kind 的底层 uint8
    std::atomic<std::uint8_t>  stale;           // 1 = owner endpoint 已被识别为死亡
    std::atomic<std::uint16_t> owner_endpoint;  // 0 = 不绑定
    std::atomic<std::uint32_t> writer_instance; // 与 shm_runtime endpoint instance_id 配对
    std::atomic<std::uint32_t> descriptor_off;  // 偏移到 arena；0 = 尚未写入
    std::uint32_t              _pad0;
    std::atomic<std::int64_t>  body;            // counter/up_down/gauge 数值
    std::atomic<std::uint32_t> hist_data_off;   // 偏移到 hist arena；仅 histogram
    std::uint32_t              _pad1;
    std::atomic<std::uint64_t> updates;         // 单调递增写入次数
    std::uint64_t              _tail_pad[2];    // 凑齐 64 字节
};

static_assert(sizeof(metric_slot) == 64, "metric_slot 必须是 64 字节");

// arena 中的描述符固定部分；后跟变长字符串
constexpr std::size_t arena_max_string = 96; // 单个名字/标签字段最大字节
constexpr std::size_t arena_max_labels = label_set::max_labels;

struct alignas(8) descriptor_record {
    std::uint8_t  kind;
    std::uint8_t  label_count;
    std::uint16_t name_len;
    std::uint16_t unit_len;
    std::uint16_t help_len;
    std::uint32_t name_off;
    std::uint32_t unit_off;
    std::uint32_t help_off;
    std::uint32_t label_key_off[arena_max_labels];
    std::uint32_t label_value_off[arena_max_labels];
    std::uint16_t label_key_len[arena_max_labels];
    std::uint16_t label_value_len[arena_max_labels];
};

// hist arena 中的数据头：与 mc/metric/histogram.h 中 histogram_data 完全一致，
// 之后紧跟 bucket 边界数组与 bucket count 数组
struct alignas(8) hist_data_layout {
    std::uint32_t              bucket_count;
    std::uint32_t              _pad;
    std::atomic<std::uint64_t> count;
    std::atomic<std::uint64_t> sum_bits;
    // followed by:
    //   double                       bounds[bucket_count];
    //   std::atomic<std::uint64_t>   counts[bucket_count + 1];
};

// arena 申请用的进程内传参（init_fn 是裸函数指针，不能捕获），通过 thread_local 通信
struct table_init_params {
    std::uint64_t capacity;
    std::uint64_t arena_size;
    std::uint64_t hist_arena_size;
};

inline table_init_params& current_init_params() noexcept
{
    thread_local table_init_params s_params{};
    return s_params;
}

// 根据 options 计算应分配的 ctrl_size
inline std::size_t compute_table_size(std::uint64_t capacity, std::uint64_t arena_size,
                                      std::uint64_t hist_arena_size) noexcept
{
    return sizeof(metric_table_header) +
           static_cast<std::size_t>(capacity) * sizeof(metric_slot) +
           static_cast<std::size_t>(arena_size) +
           static_cast<std::size_t>(hist_arena_size);
}

// shm_runtime 的 signature 用于跨进程"形状一致性"校验
inline std::uint64_t compute_table_signature(std::uint64_t capacity, std::uint64_t arena_size,
                                             std::uint64_t hist_arena_size) noexcept
{
    // [63:48] capacity_log2  [47:24] arena_kib  [23:0] hist_arena_kib
    std::uint64_t cap_log = 0;
    while ((1ULL << cap_log) < capacity) ++cap_log;
    const std::uint64_t arena_kib       = arena_size / 1024U;
    const std::uint64_t hist_arena_kib  = hist_arena_size / 1024U;
    return (cap_log << 48) | ((arena_kib & 0xFFFFFFULL) << 24) | (hist_arena_kib & 0xFFFFFFULL);
}

inline std::uint64_t round_up_pow2(std::uint64_t v) noexcept
{
    std::uint64_t r = 1;
    while (r < v) r <<= 1;
    return r;
}

} // namespace mc::metric::detail

#endif // MC_METRIC_SHM_LAYOUT_H
