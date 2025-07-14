/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_FMT_FORMATTER_MC_H
#define MC_FMT_FORMATTER_MC_H

#include <mc/dict.h>
#include <mc/fmt/format.h>
#include <mc/fmt/formatter.h>
#include <mc/variant.h>

namespace mc::fmt {
namespace detail {

void format_variant(const mc::variant& v, format_context& ctx, const format_spec& spec);
void format_dict(const mc::dict& dict, format_context& ctx, const format_spec& spec);
void format_blob(const mc::blob& blob, format_context& ctx, const format_spec& spec);
void format_extension(const mc::variant_extension_base& ext, format_context& ctx, const format_spec& spec);

} // namespace detail

template <>
struct formatter<mc::variant> {
    template <typename Context>
    void format(const mc::variant& v, Context& ctx, const format_spec& spec) const {
        detail::format_variant(v, ctx, spec);
    }
};

template <>
struct formatter<mc::dict> {
    template <typename Context>
    void format(const mc::dict& dict, Context& ctx, const format_spec& spec) const {
        detail::format_dict(dict, ctx, spec);
    }
};

template <>
struct formatter<mc::mutable_dict> {
    template <typename Context>
    void format(const mc::mutable_dict& dict, Context& ctx, const format_spec& spec) const {
        detail::format_dict(dict, ctx, spec);
    }
};

template <>
struct formatter<mc::blob> {
    template <typename Context>
    void format(const mc::blob& blob, Context& ctx, const format_spec& spec) const {
        detail::format_blob(blob, ctx, spec);
    }
};

template <>
struct formatter<mc::variant_extension_base> {
    template <typename Context>
    void format(const mc::variant_extension_base& ext, Context& ctx, const format_spec& spec) const {
        detail::format_extension(ext, ctx, spec);
    }
};

} // namespace mc::fmt

#endif // MC_FMT_FORMATTER_MC_H