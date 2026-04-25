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

/**
 * @file metric.h
 * @brief mcbase metric 子模块聚合头与声明式埋点宏
 *
 * 该头提供以下能力：
 *   1. 统一引入 metric 公开类型 (counter/gauge/histogram/registry/...)
 *   2. 定义零开销的"声明式"埋点宏 MC_METRIC_COUNTER / MC_METRIC_GAUGE /
 *      MC_METRIC_HISTOGRAM；每个调用点首次执行时通过 default_registry 解析出底层
 *      slot 指针并缓存，后续调用只是一次原子操作
 *
 * 用法示例：
 * @code
 *   void on_request() {
 *       MC_METRIC_COUNTER("mc.engine.requests").add(1);
 *
 *       MC_METRIC_HISTOGRAM_BUCKETS(latency_buckets, 0.001, 0.005, 0.01, 0.05, 0.1, 0.5, 1.0);
 *       MC_METRIC_HISTOGRAM("mc.engine.latency_seconds", latency_buckets).observe(0.012);
 *   }
 * @endcode
 */
#ifndef MC_METRIC_H
#define MC_METRIC_H

#include <mc/metric/counter.h>
#include <mc/metric/descriptor.h>
#include <mc/metric/gauge.h>
#include <mc/metric/histogram.h>
#include <mc/metric/label_set.h>
#include <mc/metric/metric_id.h>
#include <mc/metric/metric_kind.h>
#include <mc/metric/registry.h>
#include <mc/metric/sample.h>

namespace mc::metric::detail {

// 每次 reset_default_registry_for_test 递增，cached_* 检测到版本变化时重新绑定。
MC_API std::uint64_t default_registry_version() noexcept;
MC_API void          bump_default_registry_version() noexcept;

// 每个调用点一份缓存指针 + 绑定时的 registry 版本号。
// 当 registry 被 reset 后版本号不匹配，自动重新获取。

// 未绑定时的 sentinel 值，保证不等于任何合法版本号（从 0 开始递增）
constexpr uint64_t k_unbound_version = ~uint64_t(0);

template <typename DescF>
inline counter& cached_counter(DescF descf) noexcept
{
    static counter  cached;
    static uint64_t bound_version = k_unbound_version;
    auto            cur_version   = default_registry_version();
    if (MC_UNLIKELY(bound_version != cur_version)) {
        cached        = default_registry().counter_of(descf());
        bound_version = cur_version;
    }
    return cached;
}

template <typename DescF>
inline counter& cached_up_down_counter(DescF descf) noexcept
{
    static counter  cached;
    static uint64_t bound_version = k_unbound_version;
    auto            cur_version   = default_registry_version();
    if (MC_UNLIKELY(bound_version != cur_version)) {
        cached        = default_registry().up_down_counter_of(descf());
        bound_version = cur_version;
    }
    return cached;
}

template <typename DescF>
inline gauge& cached_gauge(DescF descf) noexcept
{
    static gauge    cached;
    static uint64_t bound_version = k_unbound_version;
    auto            cur_version   = default_registry_version();
    if (MC_UNLIKELY(bound_version != cur_version)) {
        cached        = default_registry().gauge_of(descf());
        bound_version = cur_version;
    }
    return cached;
}

template <typename DescF>
inline histogram& cached_histogram(DescF descf) noexcept
{
    static histogram cached;
    static uint64_t  bound_version = k_unbound_version;
    auto             cur_version   = default_registry_version();
    if (MC_UNLIKELY(bound_version != cur_version)) {
        cached        = default_registry().histogram_of(descf());
        bound_version = cur_version;
    }
    return cached;
}

} // namespace mc::metric::detail

// ---------------------------------------------------------------------------
// 埋点宏：在 default_registry 上声明并取得句柄；每个宏调用点零次堆分配
// ---------------------------------------------------------------------------

// counter：单调递增；可选传入 {{"k","v"}, ...} 形式的 label_set 初始化列表
#define MC_METRIC_COUNTER(NAME, ...)                                                              \
    (::mc::metric::detail::cached_counter([]() -> const ::mc::metric::descriptor& {              \
        static const ::mc::metric::descriptor _mc_metric_desc =                                  \
            ::mc::metric::detail::make_counter_descriptor(NAME, "", "",                          \
                                                          ::mc::metric::label_set{__VA_ARGS__}); \
        return _mc_metric_desc;                                                                  \
    }))

#define MC_METRIC_UP_DOWN_COUNTER(NAME, ...)                                                       \
    (::mc::metric::detail::cached_up_down_counter([]() -> const ::mc::metric::descriptor& {       \
        static const ::mc::metric::descriptor _mc_metric_desc =                                   \
            ::mc::metric::detail::make_up_down_counter_descriptor(                                \
                NAME, "", "", ::mc::metric::label_set{__VA_ARGS__});                              \
        return _mc_metric_desc;                                                                   \
    }))

#define MC_METRIC_GAUGE(NAME, ...)                                                                \
    (::mc::metric::detail::cached_gauge([]() -> const ::mc::metric::descriptor& {                \
        static const ::mc::metric::descriptor _mc_metric_desc =                                  \
            ::mc::metric::detail::make_gauge_descriptor(NAME, "", "",                            \
                                                        ::mc::metric::label_set{__VA_ARGS__});  \
        return _mc_metric_desc;                                                                  \
    }))

// 直方图：必须提供静态生命期的 bucket 边界数组（推荐用 MC_METRIC_HISTOGRAM_BUCKETS 声明）
#define MC_METRIC_HISTOGRAM_BUCKETS(NAME, ...)                                                    \
    static constexpr double NAME[] = {__VA_ARGS__}

#define MC_METRIC_HISTOGRAM(NAME, BUCKETS, ...)                                                   \
    (::mc::metric::detail::cached_histogram([]() -> const ::mc::metric::descriptor& {            \
        static const ::mc::metric::descriptor _mc_metric_desc =                                  \
            ::mc::metric::detail::make_histogram_descriptor(                                     \
                NAME, "", "", ::mc::metric::label_set{__VA_ARGS__}, (BUCKETS),                   \
                sizeof(BUCKETS) / sizeof((BUCKETS)[0]));                                         \
        return _mc_metric_desc;                                                                  \
    }))

#endif // MC_METRIC_H
