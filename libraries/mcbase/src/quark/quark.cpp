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
 * @file quark.cpp
 * @brief mc::quark 公开 API 实现
 */

#include <mc/quark.h>

#include "include/quark_provider.h"
#include "include/quark_record.h"

namespace mc {

namespace {

constexpr const char* k_empty_cstr   = "";
constexpr std::size_t k_empty_hash   = static_cast<std::size_t>(mc::string_hash(nullptr, 0U));
constexpr std::size_t k_invalid_hash = 0U;  // invalid 沿用 0 标记，不归到 fnv 域

// invalid / empty 都映射到空串，常量短路免 provider
inline bool _is_sentinel(quark::id_type id) noexcept
{
    return id <= quark::empty_id;
}

} // namespace

quark quark::try_from(mc::string_view value) noexcept
{
    return quark{quark_detail::active_provider().lookup(value)};
}

mc::string_view quark::view() const noexcept
{
    if (_is_sentinel(m_id)) {
        return mc::string_view(k_empty_cstr, 0U);
    }
    const auto* record = quark_detail::active_provider().resolve(m_id);
    if (record == nullptr) {
        return mc::string_view(k_empty_cstr, 0U);
    }
    return record->view();
}

const char* quark::c_str() const noexcept
{
    if (_is_sentinel(m_id)) {
        return k_empty_cstr;
    }
    const auto* record = quark_detail::active_provider().resolve(m_id);
    return record != nullptr ? record->data_ptr() : k_empty_cstr;
}

std::size_t quark::size() const noexcept
{
    if (_is_sentinel(m_id)) {
        return 0U;
    }
    const auto* record = quark_detail::active_provider().resolve(m_id);
    return record != nullptr ? static_cast<std::size_t>(record->size) : 0U;
}

std::size_t quark::hash() const noexcept
{
    if (m_id == invalid_id) {
        return k_invalid_hash;
    }
    if (m_id == empty_id) {
        return k_empty_hash;  // 真实空串 hash，与 dict 桶 / mc::string_hash 一致
    }
    const auto* record = quark_detail::active_provider().resolve(m_id);
    return record != nullptr ? static_cast<std::size_t>(record->hash) : k_invalid_hash;
}

quark::descriptor_t quark::descriptor() const noexcept
{
    if (m_id == invalid_id) {
        return descriptor_t{mc::string_view(k_empty_cstr, 0U), k_invalid_hash};
    }
    if (m_id == empty_id) {
        return descriptor_t{mc::string_view(k_empty_cstr, 0U), k_empty_hash};
    }
    const auto* record = quark_detail::active_provider().resolve(m_id);
    if (record == nullptr) {
        return descriptor_t{mc::string_view(k_empty_cstr, 0U), k_invalid_hash};
    }
    return descriptor_t{record->view(), static_cast<std::size_t>(record->hash)};
}

quark operator""_q(const char* literal, std::size_t length)
{
    return quark{detail::intern_trusted_literal(mc::string_view(literal, length))};
}

} // namespace mc
