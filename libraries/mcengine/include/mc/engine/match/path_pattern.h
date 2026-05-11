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

#ifndef MC_ENGINE_MATCH_PATH_PATTERN_H
#define MC_ENGINE_MATCH_PATH_PATTERN_H

#include <mc/common.h>
#include <mc/string.h>
#include <mc/string_view.h>

#include <cstdint>
#include <vector>

namespace mc::engine::match {

constexpr char PATH_SEGMENT_SEP = '/';

MC_API std::vector<mc::string_view> split_path_segments(mc::string_view path) noexcept;

// match_rule::path 的 pattern 表达。
// 支持：空串=全通配，exact，末尾斜杠=前缀通配，"*"单段通配，"**"前缀+任意子路径。
// 非法语法容错降级为 full_wildcard。
class MC_API path_pattern {
public:
    enum class kind : std::uint8_t {
        full_wildcard,
        exact,
        prefix_doublestar,
    };

    static bool is_well_formed(mc::string_view raw) noexcept;

    path_pattern() = default;

    explicit path_pattern(mc::string_view raw);

    kind get_kind() const noexcept { return m_kind; }

    bool matches(mc::string_view path) const noexcept;

    const std::vector<mc::string>& segments() const noexcept { return m_segments; }

private:
    kind                    m_kind{kind::full_wildcard};
    std::vector<mc::string> m_segments;
};

} // namespace mc::engine::match

#endif // MC_ENGINE_MATCH_PATH_PATTERN_H
