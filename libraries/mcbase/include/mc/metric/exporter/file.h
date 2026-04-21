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

#ifndef MC_METRIC_EXPORTER_FILE_H
#define MC_METRIC_EXPORTER_FILE_H

#include <cstddef>
#include <string>

#include <mc/common.h>
#include <mc/metric/exporter/prometheus.h>
#include <mc/metric/registry.h>

namespace mc::metric::exporter {

struct file_exporter_options {
    // 输出文件路径
    std::string output_path;

    // Prometheus 渲染选项
    prometheus_options prometheus;

    // 新建文件的权限
    unsigned int file_mode = 0644;
};

class MC_API file_exporter {
public:
    // 绑定 registry 的文件导出器
    file_exporter(registry& reg, file_exporter_options opts);

    file_exporter(const file_exporter&)            = delete;
    file_exporter& operator=(const file_exporter&) = delete;

    // 导出一次并写入文件，失败返回 0
    std::size_t write_once();

    // 返回最近一次错误信息
    const std::string& last_error() const noexcept { return m_last_error; }

private:
    registry&             m_registry;
    file_exporter_options m_options;
    std::string           m_last_error;
};

} // namespace mc::metric::exporter

#endif // MC_METRIC_EXPORTER_FILE_H
