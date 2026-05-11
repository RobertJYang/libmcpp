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

#ifndef MC_SHM_STRING_VIEW_H
#define MC_SHM_STRING_VIEW_H

#include <cstddef>
#include <string_view>

namespace mc::shm {

// mc::shm::string_view
//
// shm 内字符串的 non-owning 视图。与 std::string_view 语义一致，作为独立类型
// 放在 mc::shm 命名空间下，与 mc::shm::string 成对存在：
//   - string：owning，负责分配 / 释放 shm buffer
//   - string_view：non-owning，仅观察一段已存在的 shm 数据
//
// 实现策略：内部组合 std::string_view，并提供到 std::string_view 的隐式转换。
// 所有比较操作通过转为 std::string_view 后走标准库实现，避免与 std::string_view
// 的同名 operator 产生重载二义性。

class string_view {
public:
    constexpr string_view() noexcept = default;
    constexpr string_view(const char* data, std::size_t size) noexcept : m_sv(data, size) {}
    constexpr string_view(std::string_view sv) noexcept : m_sv(sv) {}

    [[nodiscard]] constexpr const char* data() const noexcept { return m_sv.data(); }
    [[nodiscard]] constexpr std::size_t size() const noexcept { return m_sv.size(); }
    [[nodiscard]] constexpr bool        empty() const noexcept { return m_sv.empty(); }

    // 隐式转为 std::string_view：所有比较 / I/O 均复用标准库
    [[nodiscard]] constexpr operator std::string_view() const noexcept { return m_sv; }

    [[nodiscard]] constexpr std::string_view std_view() const noexcept { return m_sv; }

private:
    std::string_view m_sv;
};

} // namespace mc::shm

#endif // MC_SHM_STRING_VIEW_H
