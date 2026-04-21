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

#ifndef MC_METRIC_DESCRIPTOR_H
#define MC_METRIC_DESCRIPTOR_H

#include <cstddef>
#include <cstdint>

#include <mc/metric/label_set.h>
#include <mc/metric/metric_id.h>
#include <mc/metric/metric_kind.h>
#include <mc/string_view.h>

namespace mc::metric {

// 直方图 bucket 数量上限
constexpr std::size_t max_histogram_buckets = 32;

// metric 描述信息
struct descriptor {
    kind            k         = kind::none;
    mc::string_view name      = {};
    mc::string_view unit      = {};
    mc::string_view help      = {};
    label_set       labels    = {};

    // 直方图 bucket 边界
    const double*   buckets       = nullptr;
    std::size_t     bucket_count  = 0;

    // 返回 metric 标识
    metric_id_t signature() const noexcept
    {
        return labels.signature(name);
    }
};

// 描述符构造辅助函数
namespace detail {

inline descriptor make_counter_descriptor(mc::string_view name, mc::string_view unit,
                                          mc::string_view help, label_set labels) noexcept
{
    descriptor d;
    d.k      = kind::counter;
    d.name   = name;
    d.unit   = unit;
    d.help   = help;
    d.labels = labels;
    return d;
}

inline descriptor make_up_down_counter_descriptor(mc::string_view name, mc::string_view unit,
                                                  mc::string_view help,
                                                  label_set       labels) noexcept
{
    descriptor d;
    d.k      = kind::up_down_counter;
    d.name   = name;
    d.unit   = unit;
    d.help   = help;
    d.labels = labels;
    return d;
}

inline descriptor make_gauge_descriptor(mc::string_view name, mc::string_view unit,
                                        mc::string_view help, label_set labels) noexcept
{
    descriptor d;
    d.k      = kind::gauge;
    d.name   = name;
    d.unit   = unit;
    d.help   = help;
    d.labels = labels;
    return d;
}

inline descriptor make_histogram_descriptor(mc::string_view name, mc::string_view unit,
                                            mc::string_view help, label_set labels,
                                            const double* buckets,
                                            std::size_t   bucket_count) noexcept
{
    descriptor d;
    d.k            = kind::histogram;
    d.name         = name;
    d.unit         = unit;
    d.help         = help;
    d.labels       = labels;
    d.buckets      = buckets;
    d.bucket_count = bucket_count > max_histogram_buckets ? max_histogram_buckets : bucket_count;
    return d;
}

} // namespace detail

} // namespace mc::metric

#endif // MC_METRIC_DESCRIPTOR_H
