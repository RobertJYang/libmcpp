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
 * @file quark_record.h
 * @brief quark 注册表内部记录与比较器
 */
#ifndef MC_SRC_QUARK_RECORD_H
#define MC_SRC_QUARK_RECORD_H

#include <cstddef>
#include <cstdint>

#include <mc/intrusive/hook.h>
#include <mc/intrusive/offset_ptr.h>
#include <mc/intrusive/unordered_set.h>
#include <mc/quark.h>
#include <mc/string_hash.h>
#include <mc/string_view.h>

namespace mc::quark_detail {

/** @brief 单个 quark 条目 */
struct quark_record : mc::intrusive::unordered_set_base_hook<> {
    quark::id_type                          id{quark::invalid_id};
    std::uint32_t                           size{0U};
    std::uint32_t                           hash{0U};
    std::uint32_t                           reserved{0U};
    mc::intrusive::offset_ptr<const char>   data{};

    const char* data_ptr() const noexcept
    {
        return data.get();
    }

    mc::string_view view() const noexcept
    {
        return mc::string_view(data_ptr(), size);
    }
};

constexpr std::uint32_t fnv1a32(const char* data, std::size_t size) noexcept
{
    return mc::string_hash(data, size);
}

inline std::uint32_t fnv1a32(mc::string_view view) noexcept
{
    return mc::string_hash(view.data(), view.size());
}

/** @brief 异质 hasher */
struct view_hasher {
    std::size_t operator()(const quark_record& record) const noexcept
    {
        return record.hash;
    }

    std::size_t operator()(mc::string_view view) const noexcept
    {
        return fnv1a32(view);
    }
};

/** @brief 异质 equal */
struct view_equal {
    bool operator()(const quark_record& lhs, const quark_record& rhs) const noexcept
    {
        if (lhs.size != rhs.size) {
            return false;
        }
        return lhs.view() == rhs.view();
    }

    bool operator()(mc::string_view lhs, const quark_record& rhs) const noexcept
    {
        if (lhs.size() != rhs.size) {
            return false;
        }
        return lhs == rhs.view();
    }

    bool operator()(const quark_record& lhs, mc::string_view rhs) const noexcept
    {
        return view_equal{}(rhs, lhs);
    }
};

} // namespace mc::quark_detail

#endif // MC_SRC_QUARK_RECORD_H
