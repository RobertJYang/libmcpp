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

#ifndef MC_METRIC_EXPORTER_PROMETHEUS_H
#define MC_METRIC_EXPORTER_PROMETHEUS_H

#include <string>

#include <mc/common.h>
#include <mc/metric/sample.h>

namespace mc::metric::exporter {

struct prometheus_options {
    // 是否规范化 metric 名称
    bool sanitize_names = true;

    // 是否输出 unit 注释
    bool emit_unit_comment = false;

    // 是否输出 stale 标记
    bool emit_stale_marker = true;
};

// 渲染到输出字符串末尾
MC_API void render_prometheus(const snapshot& snap, std::string& out,
                              const prometheus_options& opts = {});

// 渲染并返回字符串
MC_API std::string render_prometheus(const snapshot& snap,
                                     const prometheus_options& opts = {});

} // namespace mc::metric::exporter

#endif // MC_METRIC_EXPORTER_PROMETHEUS_H
