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

#ifndef MC_METRIC_HISTOGRAM_H
#define MC_METRIC_HISTOGRAM_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <mc/common.h>

namespace mc::metric {

// histogram 内部数据
struct histogram_data {
    std::uint32_t              bucket_count;
    std::uint32_t              _pad;
    std::atomic<std::uint64_t> count;
    std::atomic<std::uint64_t> sum_bits;
};

class MC_API histogram {
public:
    histogram() noexcept = default;

    explicit histogram(histogram_data* data) noexcept : m_data(data) {}

    bool is_valid() const noexcept { return m_data != nullptr; }

    void observe(double value) noexcept
    {
        if (MC_UNLIKELY(m_data == nullptr)) {
            return;
        }
        const std::uint32_t bc      = m_data->bucket_count;
        const double*       bounds  = _bounds_ptr();
        std::atomic<std::uint64_t>* counts = _counts_ptr();

        std::uint32_t idx = bc;
        for (std::uint32_t i = 0; i < bc; ++i) {
            if (value <= bounds[i]) {
                idx = i;
                break;
            }
        }

        counts[idx].fetch_add(1, std::memory_order_relaxed);
        m_data->count.fetch_add(1, std::memory_order_relaxed);
        _sum_add(value);
    }

    // 返回样本总数
    std::uint64_t total_count() const noexcept
    {
        return m_data != nullptr ? m_data->count.load(std::memory_order_relaxed) : 0;
    }

    double sum() const noexcept
    {
        if (m_data == nullptr) return 0.0;
        std::uint64_t bits = m_data->sum_bits.load(std::memory_order_relaxed);
        double        v    = 0.0;
        std::memcpy(&v, &bits, sizeof(v));
        return v;
    }

    std::uint32_t bucket_count() const noexcept
    {
        return m_data != nullptr ? m_data->bucket_count : 0;
    }

    double bucket_bound(std::uint32_t i) const noexcept
    {
        if (m_data == nullptr || i >= m_data->bucket_count) return 0.0;
        return _bounds_ptr()[i];
    }

    std::uint64_t bucket_count_at(std::uint32_t i) const noexcept
    {
        if (m_data == nullptr) return 0;
        if (i > m_data->bucket_count) return 0;
        return _counts_ptr()[i].load(std::memory_order_relaxed);
    }

private:
    void _sum_add(double v) noexcept
    {
        std::uint64_t old_bits = m_data->sum_bits.load(std::memory_order_relaxed);
        std::uint64_t new_bits = 0;
        do {
            double cur = 0.0;
            std::memcpy(&cur, &old_bits, sizeof(cur));
            cur += v;
            std::memcpy(&new_bits, &cur, sizeof(new_bits));
        } while (!m_data->sum_bits.compare_exchange_weak(old_bits, new_bits,
                                                         std::memory_order_relaxed,
                                                         std::memory_order_relaxed));
    }

    double* _bounds_ptr() const noexcept
    {
        return reinterpret_cast<double*>(reinterpret_cast<std::uint8_t*>(m_data) +
                                         sizeof(histogram_data));
    }

    std::atomic<std::uint64_t>* _counts_ptr() const noexcept
    {
        return reinterpret_cast<std::atomic<std::uint64_t>*>(
            reinterpret_cast<std::uint8_t*>(m_data) + sizeof(histogram_data) +
            sizeof(double) * m_data->bucket_count);
    }

    histogram_data* m_data = nullptr;
};

} // namespace mc::metric

#endif // MC_METRIC_HISTOGRAM_H
