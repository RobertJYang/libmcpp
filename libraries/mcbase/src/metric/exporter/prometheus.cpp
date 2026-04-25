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

#include <mc/metric/exporter/prometheus.h>
#include <mc/string_utils.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_map>
#include <vector>

namespace mc::metric::exporter {
namespace {

// ----- 名称 / 字符串处理 ---------------------------------------------------

// Prometheus metric name 合法字符：[a-zA-Z_:][a-zA-Z0-9_:]*
bool _is_name_head(char c) noexcept
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == ':';
}

bool _is_name_tail(char c) noexcept
{
    return _is_name_head(c) || (c >= '0' && c <= '9');
}

std::string _sanitize_name(const std::string& in)
{
    std::string out;
    out.reserve(in.size());
    bool head = true;
    for (char c : in) {
        if (head) {
            out.push_back(_is_name_head(c) ? c : '_');
            head = false;
        } else {
            out.push_back(_is_name_tail(c) ? c : '_');
        }
    }
    if (out.empty()) out = "_";
    return out;
}

// label 名只允许 [a-zA-Z_][a-zA-Z0-9_]*；与 name 相比少了 ':'
std::string _sanitize_label_name(const std::string& in)
{
    std::string out;
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        const char c = in[i];
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
                        (i > 0 && c >= '0' && c <= '9');
        out.push_back(ok ? c : '_');
    }
    if (out.empty()) out = "_";
    return out;
}

// label value 转义：\\ → \\\\, " → \", \n → \n
void _append_escaped_label_value(std::string& out, const std::string& v)
{
    out.reserve(out.size() + v.size() + 2);
    for (char c : v) {
        switch (c) {
            case '\\': out.append("\\\\"); break;
            case '"':  out.append("\\\""); break;
            case '\n': out.append("\\n");  break;
            default:   out.push_back(c);   break;
        }
    }
}

// HELP 注释行转义：\\ → \\\\, \n → \n
void _append_escaped_help(std::string& out, const std::string& v)
{
    out.reserve(out.size() + v.size());
    for (char c : v) {
        switch (c) {
            case '\\': out.append("\\\\"); break;
            case '\n': out.append("\\n");  break;
            default:   out.push_back(c);   break;
        }
    }
}

// ----- 数值格式化 ---------------------------------------------------------

// Prometheus 接受 integer / float / "+Inf" / "-Inf" / "NaN"
//
// 用 std::to_chars 输出 *最短可往返* 表示：0.1 → "0.1"，π → "3.141592653589793"。
// 既能 round-trip 又对人友好，比 %.17g 强（后者对 0.1 给出
// "0.10000000000000001"）
void _append_double(std::string& out, double v)
{
    if (std::isnan(v)) {
        out.append("NaN");
        return;
    }
    if (std::isinf(v)) {
        out.append(v > 0 ? "+Inf" : "-Inf");
        return;
    }
    char       buf[32];
    const auto r = mc::strings::detail::to_chars_double(buf, buf + sizeof(buf), v);
    out.append(buf, r.ptr);
}

void _append_int64(std::string& out, std::int64_t v)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
    out.append(buf);
}

void _append_uint64(std::string& out, std::uint64_t v)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(v));
    out.append(buf);
}

// ----- label 块拼接 -------------------------------------------------------

// 写 {k1="v1",k2="v2"}；labels 为空时不写任何字符
void _append_labels(std::string& out,
                    const std::vector<std::pair<std::string, std::string>>& labels)
{
    if (labels.empty()) return;
    out.push_back('{');
    for (std::size_t i = 0; i < labels.size(); ++i) {
        if (i > 0) out.push_back(',');
        out.append(_sanitize_label_name(labels[i].first));
        out.append("=\"");
        _append_escaped_label_value(out, labels[i].second);
        out.push_back('"');
    }
    out.push_back('}');
}

// 写 {k1="v1",...,extra_k="extra_v"}（用于 histogram 的 le）。
// extra 永远附在末尾
void _append_labels_with_extra(std::string& out,
                               const std::vector<std::pair<std::string, std::string>>& labels,
                               const char* extra_key, const std::string& extra_val)
{
    out.push_back('{');
    for (std::size_t i = 0; i < labels.size(); ++i) {
        out.append(_sanitize_label_name(labels[i].first));
        out.append("=\"");
        _append_escaped_label_value(out, labels[i].second);
        out.append("\",");
    }
    out.append(extra_key);
    out.append("=\"");
    _append_escaped_label_value(out, extra_val);
    out.append("\"}");
}

// ----- Prometheus TYPE 映射 -----------------------------------------------

const char* _prom_type(kind k) noexcept
{
    switch (k) {
        case kind::counter:         return "counter";
        case kind::up_down_counter: return "gauge";   // Prometheus 无 up/down
        case kind::gauge:           return "gauge";
        case kind::histogram:       return "histogram";
        default:                    return "untyped";
    }
}

// ----- 单条 sample 渲染 ---------------------------------------------------

void _render_scalar(std::string& out, const std::string& name, const sample& s)
{
    // metric_slot 内部统一用 int64 存储（counter / up_down / gauge 共用一字段），
    // 因此这里只读 i_value；用户若用 gauge.set_double 写入，需要自行在采集侧
    // 通过 bit_cast 或专用 sample.d_value 解释。否则会被解释为整数比特流，
    // 这是有意为之 —— Prometheus 文本格式没有"位级 double"概念
    out.append(name);
    _append_labels(out, s.labels);
    out.push_back(' ');
    _append_int64(out, s.i_value);
    out.push_back('\n');
}

void _render_histogram(std::string& out, const std::string& name, const sample& s)
{
    // _bucket × N（cumulative）+ _sum + _count
    char bound_buf[32];
    for (const auto& b : s.hist_buckets) {
        std::string le_str;
        if (std::isinf(b.upper_bound)) {
            le_str = "+Inf";
        } else {
            const auto r = mc::strings::detail::to_chars_double(bound_buf, bound_buf + sizeof(bound_buf),
                                                                b.upper_bound);
            le_str.assign(bound_buf, r.ptr);
        }
        out.append(name);
        out.append("_bucket");
        _append_labels_with_extra(out, s.labels, "le", le_str);
        out.push_back(' ');
        _append_uint64(out, b.cumulative_count);
        out.push_back('\n');
    }
    out.append(name);
    out.append("_sum");
    _append_labels(out, s.labels);
    out.push_back(' ');
    _append_double(out, s.hist_sum);
    out.push_back('\n');

    out.append(name);
    out.append("_count");
    _append_labels(out, s.labels);
    out.push_back(' ');
    _append_uint64(out, s.hist_total_count);
    out.push_back('\n');
}

} // namespace

// ----- 主入口 -------------------------------------------------------------

void render_prometheus(const snapshot& snap, std::string& out, const prometheus_options& opts)
{
    if (snap.empty()) return;

    // 把 sample 按 sanitize 后的 name 分组，让同名（不同 label 集）的 metric
    // 共享一行 # HELP / # TYPE。组内顺序保持输入顺序。
    struct group_key {
        std::string  name;
        const sample* first = nullptr;
    };
    std::vector<group_key>                                groups;
    std::unordered_map<std::string, std::vector<const sample*>> by_name;

    for (const auto& s : snap) {
        std::string nm = opts.sanitize_names ? _sanitize_name(s.name) : s.name;
        auto        it = by_name.find(nm);
        if (it == by_name.end()) {
            groups.push_back({nm, &s});
            by_name.emplace(std::move(nm), std::vector<const sample*>{&s});
        } else {
            it->second.push_back(&s);
        }
    }

    out.reserve(out.size() + snap.size() * 96);

    for (const auto& g : groups) {
        const auto& samples = by_name[g.name];
        const auto& head    = *g.first;

        if (!head.help.empty()) {
            out.append("# HELP ");
            out.append(g.name);
            out.push_back(' ');
            _append_escaped_help(out, head.help);
            out.push_back('\n');
        }
        out.append("# TYPE ");
        out.append(g.name);
        out.push_back(' ');
        out.append(_prom_type(head.k));
        out.push_back('\n');

        if (opts.emit_unit_comment && !head.unit.empty()) {
            out.append("# UNIT ");
            out.append(g.name);
            out.push_back(' ');
            out.append(head.unit);
            out.push_back('\n');
        }

        for (const sample* sp : samples) {
            if (sp->stale) {
                if (!opts.emit_stale_marker) continue;
                out.append("# stale: ");
                out.append(g.name);
                out.append(" writer dead\n");
            }
            if (sp->k == kind::histogram) {
                _render_histogram(out, g.name, *sp);
            } else {
                _render_scalar(out, g.name, *sp);
            }
        }
    }
}

std::string render_prometheus(const snapshot& snap, const prometheus_options& opts)
{
    std::string out;
    out.reserve(snap.size() * 96);
    render_prometheus(snap, out, opts);
    return out;
}

} // namespace mc::metric::exporter
