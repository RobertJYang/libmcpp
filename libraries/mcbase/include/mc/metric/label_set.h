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

#ifndef MC_METRIC_LABEL_SET_H
#define MC_METRIC_LABEL_SET_H

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <utility>

#include <mc/metric/metric_id.h>
#include <mc/string_view.h>

namespace mc::metric {

// 单个标签
struct label {
    mc::string_view key;
    mc::string_view value;
};

// 固定容量标签集合
class label_set {
public:
    static constexpr std::size_t max_labels = 8;

    constexpr label_set() noexcept = default;

    label_set(std::initializer_list<label> init) noexcept
    {
        for (const auto& l : init) {
            if (m_count >= max_labels) {
                m_overflow = true;
                break;
            }
            m_labels[m_count++] = l;
        }
        _sort_by_key();
    }

    constexpr std::size_t size() const noexcept { return m_count; }
    constexpr bool        empty() const noexcept { return m_count == 0; }
    constexpr bool        overflow() const noexcept { return m_overflow; }

    constexpr const label& at(std::size_t i) const noexcept { return m_labels[i]; }

    // 计算 metric 标识
    metric_id_t signature(mc::string_view name) const noexcept
    {
        metric_id_t state = detail::fnv1a(name);
        for (std::size_t i = 0; i < m_count; ++i) {
            state = detail::fnv1a_separator(state);
            state = detail::fnv1a_combine(state, m_labels[i].key);
            state = detail::fnv1a_separator(state);
            state = detail::fnv1a_combine(state, m_labels[i].value);
        }
        return state;
    }

private:
    void _sort_by_key() noexcept
    {
        for (std::size_t i = 1; i < m_count; ++i) {
            label    cur = m_labels[i];
            std::size_t j = i;
            while (j > 0 && _key_less(cur.key, m_labels[j - 1].key)) {
                m_labels[j] = m_labels[j - 1];
                --j;
            }
            m_labels[j] = cur;
        }
    }

    static constexpr bool _key_less(mc::string_view a, mc::string_view b) noexcept
    {
        const std::size_t n = a.size() < b.size() ? a.size() : b.size();
        for (std::size_t i = 0; i < n; ++i) {
            if (a[i] < b[i]) return true;
            if (a[i] > b[i]) return false;
        }
        return a.size() < b.size();
    }

    label       m_labels[max_labels]{};
    std::size_t m_count    = 0;
    bool        m_overflow = false;
};

} // namespace mc::metric

#endif // MC_METRIC_LABEL_SET_H
