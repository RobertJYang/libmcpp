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

#ifndef MC_METRIC_REGISTRY_H
#define MC_METRIC_REGISTRY_H

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

#include <mc/common.h>
#include <mc/metric/counter.h>
#include <mc/metric/descriptor.h>
#include <mc/metric/gauge.h>
#include <mc/metric/histogram.h>
#include <mc/metric/sample.h>
#include <mc/string_view.h>

namespace mc::shm {
class shm_runtime;
}

namespace mc::metric {

// registry 创建选项
struct registry_options {
    // 槽数量
    std::size_t capacity = 4096;
    // 描述符存储空间大小
    std::size_t arena_bytes = 64 * 1024;
    // 直方图存储空间大小
    std::size_t hist_arena_bytes = 32 * 1024;
    // registry 名称
    mc::string_view name = "default";
    // 所属 endpoint id
    std::uint16_t owner_endpoint = 0;
    // 所属实例 id
    std::uint32_t writer_instance = 0;
};

// metric 注册与采集接口
class MC_API registry {
public:
    virtual ~registry() = default;

    // 获取或创建 counter
    virtual counter   counter_of(const descriptor& d)   = 0;
    virtual counter   up_down_counter_of(const descriptor& d) = 0;
    virtual gauge     gauge_of(const descriptor& d)     = 0;
    virtual histogram histogram_of(const descriptor& d) = 0;

    // 已注册 metric 数量
    virtual std::size_t registered_count() const noexcept = 0;

    // 被丢弃的 metric 数量
    virtual std::size_t overflow_count() const noexcept = 0;

    // 收集当前快照
    virtual snapshot collect() const = 0;

    // 标记失效写入者
    virtual std::size_t prune_stale_writers() = 0;

    // 创建共享内存 registry
    static std::unique_ptr<registry> open_shm(mc::shm::shm_runtime& rt, const registry_options& opts);

    // 创建堆上 registry
    static std::unique_ptr<registry> open_heap(const registry_options& opts);
};

// 全局默认 registry
MC_API registry& default_registry();
MC_API void      install_default_registry(std::shared_ptr<registry> r);
MC_API void      reset_default_registry_for_test() noexcept;

} // namespace mc::metric

#endif // MC_METRIC_REGISTRY_H
