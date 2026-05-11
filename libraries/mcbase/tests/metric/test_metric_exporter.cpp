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

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

#include <mc/metric/exporter/file.h>
#include <mc/metric/exporter/prometheus.h>
#include <mc/metric/registry.h>

namespace {

using mc::metric::bucket_sample;
using mc::metric::descriptor;
using mc::metric::detail::make_counter_descriptor;
using mc::metric::detail::make_gauge_descriptor;
using mc::metric::detail::make_histogram_descriptor;
using mc::metric::kind;
using mc::metric::label_set;
using mc::metric::registry;
using mc::metric::registry_options;
using mc::metric::sample;
using mc::metric::snapshot;
using mc::metric::exporter::file_exporter;
using mc::metric::exporter::file_exporter_options;
using mc::metric::exporter::prometheus_options;
using mc::metric::exporter::render_prometheus;

bool _contains(const std::string& haystack, const std::string& needle)
{
    return haystack.find(needle) != std::string::npos;
}

// 找到包含 line 完全匹配的一整行（去除尾换行）
bool _has_line(const std::string& text, const std::string& line)
{
    std::istringstream iss(text);
    std::string        cur;
    while (std::getline(iss, cur)) {
        if (cur == line) return true;
    }
    return false;
}

std::size_t _count_lines_starting_with(const std::string& text, const std::string& prefix)
{
    std::istringstream iss(text);
    std::string        cur;
    std::size_t        n = 0;
    while (std::getline(iss, cur)) {
        if (cur.rfind(prefix, 0) == 0) ++n;
    }
    return n;
}

// ----- render_prometheus 单元测试 -----------------------------------------

TEST(prometheus_render_test, empty_snapshot_yields_empty_output)
{
    snapshot s;
    EXPECT_TRUE(render_prometheus(s).empty());
}

TEST(prometheus_render_test, single_counter_minimal)
{
    sample s;
    s.k       = kind::counter;
    s.name    = "mc_test_requests_total";
    s.help    = "total requests";
    s.i_value = 42;
    snapshot snap{s};

    const auto out = render_prometheus(snap);

    EXPECT_TRUE(_has_line(out, "# HELP mc_test_requests_total total requests"));
    EXPECT_TRUE(_has_line(out, "# TYPE mc_test_requests_total counter"));
    EXPECT_TRUE(_has_line(out, "mc_test_requests_total 42"));
    EXPECT_EQ(out.back(), '\n');
}

TEST(prometheus_render_test, name_sanitization_dots_become_underscores)
{
    sample s;
    s.k       = kind::counter;
    s.name    = "mc.engine.queue.depth"; // 输入风格
    s.i_value = 7;
    snapshot snap{s};
    const auto out = render_prometheus(snap);
    EXPECT_TRUE(_has_line(out, "# TYPE mc_engine_queue_depth counter"));
    EXPECT_TRUE(_has_line(out, "mc_engine_queue_depth 7"));
    EXPECT_FALSE(_contains(out, "mc.engine.queue.depth"));
}

TEST(prometheus_render_test, up_down_counter_maps_to_gauge_type)
{
    sample s;
    s.k       = kind::up_down_counter;
    s.name    = "mc_test_inflight";
    s.i_value = 3;
    const auto out = render_prometheus(snapshot{s});
    EXPECT_TRUE(_has_line(out, "# TYPE mc_test_inflight gauge"));
    EXPECT_TRUE(_has_line(out, "mc_test_inflight 3"));
}

TEST(prometheus_render_test, multiple_label_sets_share_one_help_and_type)
{
    sample a;
    a.k    = kind::counter;
    a.name = "mc_test_requests";
    a.help = "by route";
    a.labels = {{"route", "/a"}};
    a.i_value = 1;

    sample b = a;
    b.labels  = {{"route", "/b"}};
    b.i_value = 2;

    snapshot snap{a, b};
    const auto out = render_prometheus(snap);

    EXPECT_EQ(_count_lines_starting_with(out, "# TYPE mc_test_requests"), 1u);
    EXPECT_EQ(_count_lines_starting_with(out, "# HELP mc_test_requests"), 1u);
    EXPECT_TRUE(_has_line(out, "mc_test_requests{route=\"/a\"} 1"));
    EXPECT_TRUE(_has_line(out, "mc_test_requests{route=\"/b\"} 2"));
}

TEST(prometheus_render_test, label_value_escaping)
{
    sample s;
    s.k    = kind::gauge;
    s.name = "mc_test_escape";
    s.labels = {{"path", "C:\\foo\"bar\nbaz"}};
    s.i_value = 0;
    const auto out = render_prometheus(snapshot{s});
    EXPECT_TRUE(_contains(out, "path=\"C:\\\\foo\\\"bar\\nbaz\""));
}

TEST(prometheus_render_test, histogram_emits_buckets_sum_count_with_inf)
{
    sample s;
    s.k                = kind::histogram;
    s.name             = "mc_test_latency_seconds";
    s.help             = "request latency";
    s.hist_total_count = 7;
    s.hist_sum         = 1.25;
    s.hist_buckets     = {
        {0.01, 1},
        {0.1,  4},
        {1.0,  7},
        {std::numeric_limits<double>::infinity(), 7},
    };

    const auto out = render_prometheus(snapshot{s});
    EXPECT_TRUE(_has_line(out, "# TYPE mc_test_latency_seconds histogram"));
    EXPECT_TRUE(_has_line(out, "mc_test_latency_seconds_bucket{le=\"0.01\"} 1"));
    EXPECT_TRUE(_has_line(out, "mc_test_latency_seconds_bucket{le=\"0.1\"} 4"));
    EXPECT_TRUE(_has_line(out, "mc_test_latency_seconds_bucket{le=\"1\"} 7"));
    EXPECT_TRUE(_has_line(out, "mc_test_latency_seconds_bucket{le=\"+Inf\"} 7"));
    EXPECT_TRUE(_has_line(out, "mc_test_latency_seconds_sum 1.25"));
    EXPECT_TRUE(_has_line(out, "mc_test_latency_seconds_count 7"));
}

TEST(prometheus_render_test, histogram_with_labels_appends_le_at_end)
{
    sample s;
    s.k                = kind::histogram;
    s.name             = "h";
    s.labels           = {{"route", "/x"}};
    s.hist_total_count = 1;
    s.hist_sum         = 0.5;
    s.hist_buckets     = {{1.0, 1}, {std::numeric_limits<double>::infinity(), 1}};
    const auto out = render_prometheus(snapshot{s});
    EXPECT_TRUE(_has_line(out, "h_bucket{route=\"/x\",le=\"1\"} 1"));
    EXPECT_TRUE(_has_line(out, "h_bucket{route=\"/x\",le=\"+Inf\"} 1"));
    EXPECT_TRUE(_has_line(out, "h_sum{route=\"/x\"} 0.5"));
    EXPECT_TRUE(_has_line(out, "h_count{route=\"/x\"} 1"));
}

TEST(prometheus_render_test, stale_marker_emitted_when_enabled)
{
    sample s;
    s.k       = kind::counter;
    s.name    = "mc_test_dead";
    s.i_value = 5;
    s.stale   = true;
    const auto out = render_prometheus(snapshot{s});
    EXPECT_TRUE(_has_line(out, "# stale: mc_test_dead writer dead"));
    EXPECT_TRUE(_has_line(out, "mc_test_dead 5")); // 仍输出 last value
}

TEST(prometheus_render_test, stale_sample_skipped_when_marker_disabled)
{
    sample s;
    s.k       = kind::counter;
    s.name    = "mc_test_dead";
    s.i_value = 5;
    s.stale   = true;

    prometheus_options opts;
    opts.emit_stale_marker = false;
    const auto out = render_prometheus(snapshot{s}, opts);
    EXPECT_FALSE(_contains(out, "mc_test_dead 5"));
}

// ----- 端到端：registry → render -----------------------------------------

TEST(prometheus_render_test, end_to_end_with_real_registry)
{
    auto r = registry::open_heap(registry_options{});
    ASSERT_TRUE(r);

    auto c = r->counter_of(make_counter_descriptor(
        "mc_test_e2e_requests", "1", "总请求数", label_set{{"code", "200"}}));
    c.add(10);

    static constexpr double bounds[] = {0.005, 0.01, 0.05, 0.1};
    auto h = r->histogram_of(make_histogram_descriptor(
        "mc_test_e2e_latency_seconds", "s", "", label_set{}, bounds, 4));
    h.observe(0.003);
    h.observe(0.07);

    const auto out = render_prometheus(r->collect());
    EXPECT_TRUE(_has_line(out, "# TYPE mc_test_e2e_requests counter"));
    EXPECT_TRUE(_has_line(out, "mc_test_e2e_requests{code=\"200\"} 10"));
    EXPECT_TRUE(_has_line(out, "# TYPE mc_test_e2e_latency_seconds histogram"));
    EXPECT_TRUE(_has_line(out, "mc_test_e2e_latency_seconds_count 2"));
}

// ----- file_exporter -----------------------------------------------------

class file_exporter_test : public ::testing::Test {
protected:
    void SetUp() override
    {
        const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::string("/tmp/mc_metric_export_") + std::to_string(::getpid()) + "_" +
                 std::to_string(now_ns) + ".prom";
    }
    void TearDown() override { ::unlink(m_path.c_str()); }

    std::string _read_file(const std::string& path)
    {
        std::ifstream     ifs(path);
        std::stringstream ss;
        ss << ifs.rdbuf();
        return ss.str();
    }

    std::string m_path;
};

TEST_F(file_exporter_test, write_once_creates_file_with_expected_content)
{
    auto r = registry::open_heap(registry_options{});
    ASSERT_TRUE(r);
    r->counter_of(make_counter_descriptor("mc_test_file", "", "", label_set{})).add(123);

    file_exporter_options opts;
    opts.output_path = m_path;
    file_exporter exp(*r, opts);

    const auto bytes = exp.write_once();
    ASSERT_GT(bytes, 0u) << exp.last_error();

    const auto contents = _read_file(m_path);
    EXPECT_EQ(contents.size(), bytes);
    EXPECT_TRUE(_has_line(contents, "# TYPE mc_test_file counter"));
    EXPECT_TRUE(_has_line(contents, "mc_test_file 123"));
}

TEST_F(file_exporter_test, second_write_overwrites_atomically)
{
    auto r = registry::open_heap(registry_options{});
    ASSERT_TRUE(r);
    auto c = r->counter_of(make_counter_descriptor("mc_test_overwrite", "", "", label_set{}));
    c.add(1);

    file_exporter_options opts;
    opts.output_path = m_path;
    file_exporter exp(*r, opts);

    ASSERT_GT(exp.write_once(), 0u);
    EXPECT_TRUE(_has_line(_read_file(m_path), "mc_test_overwrite 1"));

    c.add(99);
    ASSERT_GT(exp.write_once(), 0u);
    EXPECT_TRUE(_has_line(_read_file(m_path), "mc_test_overwrite 100"));

    // 不应有遗留 .tmp 文件
    const auto leftover = m_path + ".tmp." + std::to_string(::getpid());
    struct stat st;
    EXPECT_NE(::stat(leftover.c_str(), &st), 0);
}

TEST_F(file_exporter_test, invalid_path_returns_zero_and_records_error)
{
    auto r = registry::open_heap(registry_options{});
    ASSERT_TRUE(r);
    file_exporter_options opts;
    opts.output_path = "/this/path/should/not/exist/x.prom";
    file_exporter exp(*r, opts);
    EXPECT_EQ(exp.write_once(), 0u);
    EXPECT_FALSE(exp.last_error().empty());
}

} // namespace
