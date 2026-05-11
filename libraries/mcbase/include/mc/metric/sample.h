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

#ifndef MC_METRIC_SAMPLE_H
#define MC_METRIC_SAMPLE_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <mc/metric/metric_id.h>
#include <mc/metric/metric_kind.h>

namespace mc::metric {

// 直方图 bucket 数据
struct bucket_sample {
    double        upper_bound = 0.0;
    std::uint64_t cumulative_count = 0;
};

// 单个 metric 的快照
struct sample {
    metric_id_t                                  id   = 0;
    kind                                         k    = kind::none;
    std::string                                  name;
    std::string                                  unit;
    std::string                                  help;
    std::vector<std::pair<std::string, std::string>> labels;

    // 标量值
    std::int64_t  i_value = 0;
    double        d_value = 0.0;

    // 直方图值
    std::uint64_t              hist_total_count = 0;
    double                     hist_sum         = 0.0;
    std::vector<bucket_sample> hist_buckets;

    // 写入者信息
    std::uint16_t owner_endpoint = 0;
    bool          stale          = false;
};

using snapshot = std::vector<sample>;

} // namespace mc::metric

#endif // MC_METRIC_SAMPLE_H
