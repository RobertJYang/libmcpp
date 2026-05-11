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

#ifndef MC_METRIC_METRIC_KIND_H
#define MC_METRIC_METRIC_KIND_H

#include <cstdint>

#include <mc/string_view.h>

namespace mc::metric {

// metric 类型
enum class kind : std::uint8_t {
    none             = 0,
    counter          = 1,
    up_down_counter  = 2,
    gauge            = 3,
    histogram        = 4,
};

inline constexpr mc::string_view kind_name(kind k) noexcept
{
    switch (k) {
        case kind::none:            return mc::string_view("none");
        case kind::counter:         return mc::string_view("counter");
        case kind::up_down_counter: return mc::string_view("up_down_counter");
        case kind::gauge:           return mc::string_view("gauge");
        case kind::histogram:       return mc::string_view("histogram");
    }
    return mc::string_view("unknown");
}

} // namespace mc::metric

#endif // MC_METRIC_METRIC_KIND_H
