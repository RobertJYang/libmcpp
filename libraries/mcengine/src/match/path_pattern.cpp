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

#include <mc/engine/match/path_pattern.h>

#include <cstddef>

namespace mc::engine::match {

namespace {

constexpr mc::string_view SEG_SINGLE_WILDCARD = "*";
constexpr mc::string_view SEG_MULTI_WILDCARD  = "**";

bool segment_contains_intra_wildcard(mc::string_view seg) noexcept
{
    if (seg == SEG_SINGLE_WILDCARD || seg == SEG_MULTI_WILDCARD) {
        return false;
    }
    return seg.find('*') != mc::string_view::npos;
}

bool segment_is_freeform_capture(mc::string_view seg) noexcept
{
    return seg.size() >= 2 && seg.front() == '{' && seg.back() == '}';
}

bool segment_is_unsupported_capture(mc::string_view seg) noexcept
{
    if (!segment_is_freeform_capture(seg)) {
        return false;
    }
    return seg.find(':') != mc::string_view::npos
           || seg.find('=') != mc::string_view::npos;
}

bool pattern_segment_matches_literal(mc::string_view pat, mc::string_view actual) noexcept
{
    if (pat == SEG_SINGLE_WILDCARD) {
        return true;
    }
    return pat == actual;
}

} // namespace

std::vector<mc::string_view> split_path_segments(mc::string_view path) noexcept
{
    std::vector<mc::string_view> segments;
    if (path.empty()) {
        return segments;
    }

    std::size_t pos = 0;
    while (pos < path.size()) {
        while (pos < path.size() && path[pos] == PATH_SEGMENT_SEP) {
            ++pos;
        }
        if (pos >= path.size()) {
            break;
        }
        const std::size_t start = pos;
        while (pos < path.size() && path[pos] != PATH_SEGMENT_SEP) {
            ++pos;
        }
        segments.push_back(path.substr(start, pos - start));
    }
    return segments;
}

bool path_pattern::is_well_formed(mc::string_view raw) noexcept
{
    if (raw.empty()) {
        return true;
    }

    const auto segs = split_path_segments(raw);
    for (std::size_t i = 0; i < segs.size(); ++i) {
        const auto seg = segs[i];
        if (seg == SEG_MULTI_WILDCARD) {
            if (i + 1 != segs.size()) {
                return false;
            }
            continue;
        }
        if (seg == SEG_SINGLE_WILDCARD) {
            continue;
        }
        if (segment_contains_intra_wildcard(seg)) {
            return false;
        }
        if (segment_is_unsupported_capture(seg)) {
            return false;
        }
    }
    return true;
}

path_pattern::path_pattern(mc::string_view raw)
{
    if (raw.empty() || !is_well_formed(raw)) {
        m_kind = kind::full_wildcard;
        return;
    }

    auto segs = split_path_segments(raw);
    if (segs.empty()) {
        m_kind = kind::full_wildcard;
        return;
    }

    const bool trailing_slash = raw.back() == PATH_SEGMENT_SEP;
    const bool has_doublestar = segs.back() == SEG_MULTI_WILDCARD;
    if (has_doublestar) {
        segs.pop_back();
    }

    m_segments.reserve(segs.size());
    for (auto seg : segs) {
        if (segment_is_freeform_capture(seg)) {
            m_segments.emplace_back(SEG_SINGLE_WILDCARD);
        } else {
            m_segments.emplace_back(seg);
        }
    }

    m_kind = (has_doublestar || trailing_slash) ? kind::prefix_doublestar : kind::exact;
}

bool path_pattern::matches(mc::string_view path) const noexcept
{
    if (m_kind == kind::full_wildcard) {
        return true;
    }

    const auto  target_segs = split_path_segments(path);
    const auto& pat_segs    = m_segments;

    if (m_kind == kind::exact) {
        if (target_segs.size() != pat_segs.size()) {
            return false;
        }
    } else if (target_segs.size() < pat_segs.size()) {
        return false;
    }

    for (std::size_t i = 0; i < pat_segs.size(); ++i) {
        if (!pattern_segment_matches_literal(pat_segs[i], target_segs[i])) {
            return false;
        }
    }
    return true;
}

} // namespace mc::engine::match
