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

#ifndef MC_METRIC_METRIC_ID_H
#define MC_METRIC_METRIC_ID_H

#include <cstdint>

#include <mc/string_view.h>

namespace mc::metric {

// metric 标识类型
using metric_id_t = std::uint64_t;

namespace detail {

constexpr metric_id_t fnv_offset_basis = 0xcbf29ce484222325ULL;
constexpr metric_id_t fnv_prime        = 0x100000001b3ULL;

constexpr metric_id_t fnv1a_combine(metric_id_t state, mc::string_view input) noexcept
{
    metric_id_t h = state;
    for (std::size_t i = 0; i < input.size(); ++i) {
        h ^= static_cast<unsigned char>(input[i]);
        h *= fnv_prime;
    }
    return h;
}

constexpr metric_id_t fnv1a(mc::string_view input) noexcept
{
    return fnv1a_combine(fnv_offset_basis, input);
}

constexpr metric_id_t fnv1a_separator(metric_id_t state) noexcept
{
    return fnv1a_combine(state, mc::string_view("\x1f", 1));
}

} // namespace detail

// 根据名称计算 metric 标识
constexpr metric_id_t hash_name(mc::string_view name) noexcept
{
    return detail::fnv1a(name);
}

} // namespace mc::metric

#endif // MC_METRIC_METRIC_ID_H
