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
 * @file main.cpp
 * @brief metric 模块端到端示例
 *
 * 演示流程：
 *   1. 父进程开 shm metric registry，把它装为默认 registry
 *   2. fork 出一个子进程 (reader)：周期性独立打开同一块 shm，把所有 metric
 *      用 Prometheus exposition 格式打印到 stdout
 *   3. 父进程模拟一个简单 HTTP 服务的工作负载：
 *        counter   mc_example_requests_total{route, code}
 *        up_down   mc_example_inflight_requests
 *        gauge     mc_example_cpu_pct
 *        histogram mc_example_request_seconds
 *      用宏埋点，业务代码看不到底层 shm
 *   4. 父进程跑约 5 秒后退出，reader 收到 SIGTERM 退出
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <thread>

#include <sys/wait.h>
#include <unistd.h>

#include <mc/metric.h>
#include <mc/metric/exporter/file.h>
#include <mc/metric/exporter/prometheus.h>
#include <mc/metric/registry.h>
#include <mc/shm/detail/shared_memory_backend.h>
#include <mc/shm/shm_runtime.h>

namespace {

constexpr const char* k_region_name      = "mc_metric_example_region";
constexpr const char* k_registry_name    = "example";
constexpr int         k_writer_run_secs  = 5;
constexpr int         k_reader_period_ms = 1000;

std::atomic<bool> g_stop{false};

void on_signal(int /*sig*/) noexcept
{
    g_stop.store(true);
}

mc::shm::runtime_options runtime_opts()
{
    mc::shm::runtime_options o;
    o.region_name = k_region_name;
    o.region_size = 4 * 1024 * 1024;
    return o;
}

mc::metric::registry_options registry_opts()
{
    mc::metric::registry_options o;
    o.capacity         = 256;
    o.arena_bytes      = 32 * 1024;
    o.hist_arena_bytes = 16 * 1024;
    o.name             = k_registry_name;
    return o;
}

// ---------------------------------------------------------------------------
// reader：周期把 snapshot 渲染成 Prometheus 文本，stdout + 落盘
// ---------------------------------------------------------------------------

int run_reader()
{
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    mc::shm::shm_runtime rt(runtime_opts());
    if (!rt.is_valid()) {
        std::fprintf(stderr, "[reader] failed to open shm runtime\n");
        return 1;
    }
    auto reg = mc::metric::registry::open_shm(rt, registry_opts());
    if (!reg) {
        std::fprintf(stderr, "[reader] failed to open metric registry\n");
        return 2;
    }

    mc::metric::exporter::file_exporter_options fopts;
    fopts.output_path = std::string("/tmp/mc_metric_example_") +
                        std::to_string(::getppid()) + ".prom";
    mc::metric::exporter::file_exporter file_exp(*reg, fopts);

    while (!g_stop.load()) {
        const auto snap = reg->collect();
        const auto text = mc::metric::exporter::render_prometheus(snap);
        std::fprintf(stdout, "\n=== reader pid=%d snapshot (%zu metrics) ===\n",
                     static_cast<int>(::getpid()), snap.size());
        std::fwrite(text.data(), 1, text.size(), stdout);
        std::fflush(stdout);

        const auto wrote = file_exp.write_once();
        if (wrote == 0 && !file_exp.last_error().empty()) {
            std::fprintf(stderr, "[reader] file_exporter error: %s\n",
                         file_exp.last_error().c_str());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(k_reader_period_ms));
    }
    std::fprintf(stdout, "[reader pid=%d] exit; metric file at %s\n",
                 static_cast<int>(::getpid()), fopts.output_path.c_str());
    return 0;
}

// ---------------------------------------------------------------------------
// writer：模拟业务流量；所有埋点都用宏，不直接接触 registry
// ---------------------------------------------------------------------------

// 动态标签 helper：用 registry 直接 lookup（每次调一次哈希+CAS）。
// 该模式适合 cardinality 不可预知的场景（route/code 来自外部输入）。
mc::metric::counter dynamic_request_counter(mc::string_view route, mc::string_view code)
{
    const auto desc = mc::metric::detail::make_counter_descriptor(
        "mc_example_requests_total", "", "",
        mc::metric::label_set{{"route", route}, {"code", code}});
    return mc::metric::default_registry().counter_of(desc);
}

void simulate_request(std::mt19937& rng)
{
    static std::uniform_int_distribution<int>     route_dist(0, 2);
    static std::uniform_real_distribution<double> latency_dist(0.0005, 0.5);
    static std::bernoulli_distribution            error_dist(0.1);

    const char* route = nullptr;
    switch (route_dist(rng)) {
        case 0:  route = "/api/v1/users";  break;
        case 1:  route = "/api/v1/orders"; break;
        default: route = "/api/v1/health"; break;
    }
    const bool        is_error = error_dist(rng);
    const char* const code     = is_error ? "500" : "200";
    const double      latency  = latency_dist(rng);

    // 模式 A：纯静态埋点 —— 编译期确定 label，宏内 magic-static 缓存
    MC_METRIC_UP_DOWN_COUNTER("mc_example_inflight_requests").add_signed(1);

    // 模式 B：动态 label —— 走 registry 直查，每次一次哈希探测
    dynamic_request_counter(route, code).add(1);

    // 模式 A：histogram 同样使用宏
    MC_METRIC_HISTOGRAM_BUCKETS(buckets, 0.001, 0.005, 0.01, 0.05, 0.1, 0.5, 1.0);
    MC_METRIC_HISTOGRAM("mc_example_request_seconds", buckets).observe(latency);

    MC_METRIC_UP_DOWN_COUNTER("mc_example_inflight_requests").add_signed(-1);
}

int run_writer()
{
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    mc::shm::detail::shared_memory_backend::remove(k_region_name);

    mc::shm::shm_runtime rt(runtime_opts());
    if (!rt.is_valid()) {
        std::fprintf(stderr, "[writer] failed to open shm runtime\n");
        return 1;
    }
    auto reg = mc::metric::registry::open_shm(rt, registry_opts());
    if (!reg) {
        std::fprintf(stderr, "[writer] failed to open metric registry\n");
        return 2;
    }
    mc::metric::install_default_registry(std::move(reg));

    const auto child = ::fork();
    if (child < 0) {
        std::perror("fork");
        return 3;
    }
    if (child == 0) {
        return run_reader();
    }

    std::fprintf(stdout, "[writer pid=%d] started; reader pid=%d; running %d seconds\n",
                 static_cast<int>(::getpid()), static_cast<int>(child), k_writer_run_secs);

    std::mt19937 rng(0xC0FFEE);
    const auto   deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(k_writer_run_secs);
    int sample_tick = 0;
    while (!g_stop.load() && std::chrono::steady_clock::now() < deadline) {
        for (int i = 0; i < 200; ++i) {
            simulate_request(rng);
        }
        // 模拟一个 gauge：当前 CPU 占用百分比
        MC_METRIC_GAUGE("mc_example_cpu_pct").set(20 + (sample_tick++ % 50));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ::kill(child, SIGTERM);
    int status = 0;
    ::waitpid(child, &status, 0);

    mc::shm::detail::shared_memory_backend::remove(k_region_name);
    std::fprintf(stdout, "[writer pid=%d] done\n", static_cast<int>(::getpid()));
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc > 1 && std::strcmp(argv[1], "--reader") == 0) {
        return run_reader();
    }
    return run_writer();
}
