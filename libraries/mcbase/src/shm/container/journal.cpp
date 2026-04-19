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

#include "mc/shm/container/journal.h"

#include <cstring>

namespace mc::shm::container {

void journal_init(container_journal& j) noexcept
{
    j.target_node_offset = 0;
    j.anchor_offset      = 0;
    j.saved_link_a       = 0;
    j.saved_link_b       = 0;
    j.extra              = 0;
    j.misc               = 0;
    std::memset(j._pad0, 0, sizeof(j._pad0));
    std::memset(j._pad1, 0, sizeof(j._pad1));
    j.op.store(static_cast<std::uint8_t>(container_op::none), std::memory_order_release);
}

void journal_begin(container_journal& j, container_op op, std::uint64_t target,
                   std::uint64_t anchor, std::uint64_t saved_a, std::uint64_t saved_b,
                   std::uint64_t extra, std::uint32_t misc) noexcept
{
    j.target_node_offset = target;
    j.anchor_offset      = anchor;
    j.saved_link_a       = saved_a;
    j.saved_link_b       = saved_b;
    j.extra              = extra;
    j.misc               = misc;
    j.op.store(static_cast<std::uint8_t>(op), std::memory_order_release);
}

void journal_end(container_journal& j) noexcept
{
    j.op.store(static_cast<std::uint8_t>(container_op::none), std::memory_order_release);
}

bool journal_load(const container_journal& j, container_journal_view& out) noexcept
{
    const auto raw = j.op.load(std::memory_order_acquire);
    if (raw == 0) {
        return false;
    }

    // op != none 表示 release store 已经生效，上面所有字段一定可见
    out.op                 = (raw <= static_cast<std::uint8_t>(container_op::update))
                                 ? static_cast<container_op>(raw)
                                 : container_op::none;
    out.target_node_offset = j.target_node_offset;
    out.anchor_offset      = j.anchor_offset;
    out.saved_link_a       = j.saved_link_a;
    out.saved_link_b       = j.saved_link_b;
    out.extra              = j.extra;
    out.misc               = j.misc;
    return out.op != container_op::none;
}

container_op journal_load_op(const container_journal& j) noexcept
{
    const auto raw = j.op.load(std::memory_order_acquire);
    if (raw > static_cast<std::uint8_t>(container_op::update)) {
        return container_op::none;
    }
    return static_cast<container_op>(raw);
}

journal_window::journal_window(container_journal& j, container_op op, std::uint64_t target,
                               std::uint64_t anchor, std::uint64_t saved_a, std::uint64_t saved_b,
                               std::uint64_t extra, std::uint32_t misc) noexcept
    : m_journal(j)
{
    journal_begin(m_journal, op, target, anchor, saved_a, saved_b, extra, misc);
}

void journal_window::commit() noexcept
{
    if (m_committed) {
        return;
    }
    journal_end(m_journal);
    m_committed = true;
}

} // namespace mc::shm::container
