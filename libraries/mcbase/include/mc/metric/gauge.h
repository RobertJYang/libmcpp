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

#ifndef MC_METRIC_GAUGE_H
#define MC_METRIC_GAUGE_H

#include <atomic>
#include <cstdint>
#include <cstring>

#include <mc/common.h>

namespace mc::metric {

// gauge 句柄
class MC_API gauge {
public:
    gauge() noexcept = default;

    explicit gauge(std::atomic<std::int64_t>* slot) noexcept : m_slot(slot) {}

    bool is_valid() const noexcept { return m_slot != nullptr; }

    MC_ALWAYS_INLINE void set(std::int64_t v) noexcept
    {
        if (MC_UNLIKELY(m_slot == nullptr)) {
            return;
        }
        m_slot->store(v, std::memory_order_relaxed);
    }

    MC_ALWAYS_INLINE void add(std::int64_t v) noexcept
    {
        if (MC_UNLIKELY(m_slot == nullptr)) {
            return;
        }
        m_slot->fetch_add(v, std::memory_order_relaxed);
    }

    MC_ALWAYS_INLINE void set_double(double v) noexcept
    {
        if (MC_UNLIKELY(m_slot == nullptr)) {
            return;
        }
        std::int64_t bits = 0;
        std::memcpy(&bits, &v, sizeof(bits));
        m_slot->store(bits, std::memory_order_relaxed);
    }

    std::int64_t value() const noexcept
    {
        return m_slot != nullptr ? m_slot->load(std::memory_order_relaxed) : 0;
    }

    double value_double() const noexcept
    {
        std::int64_t bits = value();
        double       d    = 0.0;
        std::memcpy(&d, &bits, sizeof(d));
        return d;
    }

private:
    std::atomic<std::int64_t>* m_slot = nullptr;
};

} // namespace mc::metric

#endif // MC_METRIC_GAUGE_H
