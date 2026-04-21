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

#ifndef MC_METRIC_METRIC_TABLE_H
#define MC_METRIC_METRIC_TABLE_H

#include <cstddef>
#include <cstdint>
#include <functional>

#include <mc/metric/descriptor.h>
#include <mc/metric/sample.h>

#include "shm_layout.h"

namespace mc::metric::detail {

// 对底层 metric_table_header（位于 shm 或堆内存）的薄封装；可被 shm registry
// 与 heap registry 共用：堆模式只是把整段内存放在堆上，结构与 shm 完全一致。
class metric_table {
public:
    metric_table() noexcept = default;

    // 由 registry 在已分配/映射好的连续内存上构造（不接管所有权）
    explicit metric_table(void* base) noexcept : m_base(base)
    {}

    bool is_valid() const noexcept
    {
        return m_base != nullptr;
    }

    metric_table_header* header() noexcept
    {
        return reinterpret_cast<metric_table_header*>(m_base);
    }
    const metric_table_header* header() const noexcept
    {
        return reinterpret_cast<const metric_table_header*>(m_base);
    }

    metric_slot* slots() noexcept
    {
        return reinterpret_cast<metric_slot*>(_byte_ptr() + header()->slots_off);
    }
    const metric_slot* slots() const noexcept
    {
        return reinterpret_cast<const metric_slot*>(_byte_ptr() + header()->slots_off);
    }

    std::uint8_t* arena() noexcept
    {
        return _byte_ptr() + header()->arena_off;
    }
    const std::uint8_t* arena() const noexcept
    {
        return _byte_ptr() + header()->arena_off;
    }

    std::uint8_t* hist_arena() noexcept
    {
        return _byte_ptr() + header()->hist_arena_off;
    }
    const std::uint8_t* hist_arena() const noexcept
    {
        return _byte_ptr() + header()->hist_arena_off;
    }

    // 在表中定位 metric；首次出现时复制 descriptor 到 arena 并发布。
    // 返回的 slot 可能是 nullptr，表示哈希探测耗尽（overflow）或 arena 满。
    // 出参 created：true = 本次新建并发布了 descriptor，false = 已存在
    //
    // 已存在分支同时执行 owner 复活：若 slot.writer_instance 与传入不同，
    // 视为前一任 owner 已死亡、当前调用方接管。更新 owner_endpoint /
    // writer_instance 并清 stale；body / 描述符保持不变（counter 累计语义）
    metric_slot* upsert(const descriptor& d, std::uint16_t writer_endpoint,
                        std::uint32_t writer_instance, bool& created) noexcept;

    // 仅查找；不创建。读路径使用
    const metric_slot* find(metric_id_t id) const noexcept;

    // 给 histogram 准备并返回数据指针；若已绑定则直接返回现有指针
    hist_data_layout* ensure_hist_data(metric_slot* slot, const descriptor& d) noexcept;

    // 把 slot 转为可读的 sample（拍照，不持锁）
    void slot_to_sample(const metric_slot* slot, sample& out) const;

    // 遍历整张表，对每个有效槽生成 sample，附加到 out
    void collect_all(snapshot& out) const;

    std::uint64_t capacity() const noexcept
    {
        return header()->capacity;
    }
    std::uint64_t registered() const noexcept
    {
        return header()->registered.load(std::memory_order_acquire);
    }
    std::uint64_t overflow() const noexcept
    {
        return header()->overflow_drops.load(std::memory_order_acquire);
    }

    // 由 init_fn 在表内存清零之后调用，写入 header / 计算各 region 偏移
    static void initialize(void* base, const table_init_params& params) noexcept;

    // owner 存活性回调：返回 true 表示 (endpoint_id, instance_id) 仍然存活
    using liveness_probe = std::function<bool(std::uint16_t, std::uint32_t)>;

    // 遍历所有 slot，凡是 owner_endpoint!=0 且对应 (eid, instance) 已不存活的，
    // 把 slot.stale 置 1。返回新增 stale slot 数量
    std::size_t prune_stale_writers(const liveness_probe& probe) noexcept;

private:
    std::uint8_t* _byte_ptr() noexcept
    {
        return reinterpret_cast<std::uint8_t*>(m_base);
    }
    const std::uint8_t* _byte_ptr() const noexcept
    {
        return reinterpret_cast<const std::uint8_t*>(m_base);
    }

    // 在 descriptor arena 中线性 bump 出一段对齐的字节，返回偏移；不够则返回 0
    std::uint32_t _arena_alloc(std::uint64_t size, std::uint64_t align) noexcept;

    // 在 hist arena 中分配
    std::uint32_t _hist_arena_alloc(std::uint64_t size, std::uint64_t align) noexcept;

    // 把字符串拷入 arena，返回 (offset, len)；超长会被截断
    std::uint32_t _store_string(mc::string_view s, std::uint16_t& out_len) noexcept;

    // 完整写入 descriptor_record；失败返回 0（视为 arena 满）
    std::uint32_t _store_descriptor(const descriptor& d) noexcept;

    void* m_base = nullptr;
};

} // namespace mc::metric::detail

#endif // MC_METRIC_METRIC_TABLE_H
