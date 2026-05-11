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

#ifndef MC_METRIC_COUNTER_H
#define MC_METRIC_COUNTER_H

#include <atomic>
#include <cstdint>

#include <mc/common.h>

namespace mc::metric {

// counter 句柄
class MC_API counter {
public:
    counter() noexcept = default;

    // 内部使用构造
    explicit counter(std::atomic<std::int64_t>* slot) noexcept : m_slot(slot) {}

    bool is_valid() const noexcept { return m_slot != nullptr; }

    MC_ALWAYS_INLINE void add(std::uint64_t v = 1) noexcept
    {
        if (MC_UNLIKELY(m_slot == nullptr)) {
            return;
        }
        m_slot->fetch_add(static_cast<std::int64_t>(v), std::memory_order_relaxed);
    }

    // 用于可增可减的计数器
    MC_ALWAYS_INLINE void add_signed(std::int64_t v) noexcept
    {
        if (MC_UNLIKELY(m_slot == nullptr)) {
            return;
        }
        m_slot->fetch_add(v, std::memory_order_relaxed);
    }

    // 返回当前值
    std::int64_t value() const noexcept
    {
        return m_slot != nullptr ? m_slot->load(std::memory_order_relaxed) : 0;
    }

private:
    std::atomic<std::int64_t>* m_slot = nullptr;
};

} // namespace mc::metric

#endif // MC_METRIC_COUNTER_H
