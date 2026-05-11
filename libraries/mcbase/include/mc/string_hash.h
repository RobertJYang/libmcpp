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
 * @file string_hash.h
 * @brief libmcpp 统一字符串 hash 入口
 */

#ifndef MC_STRING_HASH_H
#define MC_STRING_HASH_H

#include <cstddef>
#include <cstdint>

namespace mc {

/** @brief 32 位 FNV-1a */
constexpr std::uint32_t string_hash(const char* data, std::size_t size) noexcept
{
    constexpr std::uint32_t fnv_offset_basis = 0x811C9DC5U;
    constexpr std::uint32_t fnv_prime        = 0x01000193U;

    std::uint32_t value = fnv_offset_basis;
    for (std::size_t i = 0U; i < size; ++i) {
        value ^= static_cast<std::uint8_t>(data[i]);
        value *= fnv_prime;
    }
    return value;
}

} // namespace mc

#endif // MC_STRING_HASH_H
