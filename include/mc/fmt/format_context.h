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

#ifndef MC_FMT_FORMAT_CONTEXT_H
#define MC_FMT_FORMAT_CONTEXT_H

#include <mc/dict.h>
#include <mc/fmt/format_arg.h>
#include <mc/fmt/format_spec.h>
#include <mc/fmt/formatter_mc.h>

namespace mc::fmt {

namespace detail {
struct MC_API runtime_arg_store : public arg_store<format_arg> {
    using arg_store<format_arg>::add_arg;

    runtime_arg_store() = default;

    template <typename... Args>
    constexpr runtime_arg_store(Args&&... args) {
        (add_arg(std::forward<Args>(args)), ...);
    }

    MC_API void set_dict_args(const mc::dict* args);
    MC_API bool get_arg(size_t index, format_arg& arg) const;
    MC_API bool get_positional(size_t index, format_arg& arg) const;
    MC_API bool get_named(std::string_view name, format_arg& arg, size_t& index) const;
    MC_API bool resolve_dynamic_param(size_t index, std::string_view name, int& out);

    MC_API const mc::variant* get_variant(size_t index) const;
    MC_API const mc::variant* get_variant(std::string_view name) const;

    const dict* named_args{nullptr};
    bool        icase{false};
};

struct parser_result;
} // namespace detail

// 格式化上下文
class MC_API format_context {
public:
    using arg_type = detail::format_arg;

    explicit MC_API format_context(std::string& out, detail::runtime_arg_store& args);
    explicit MC_API format_context(std::string& out);

    MC_API std::string& out();

    MC_API bool get_arg(size_t index, detail::format_arg& arg) const;
    MC_API bool get_named_arg(std::string_view name, detail::format_arg& arg, size_t& index) const;
    MC_API void set_used(size_t index);
    MC_API bool resolve_dynamic_param(size_t index, std::string_view name, int& out);

    MC_API void format_arg(const detail::format_arg& arg, detail::format_spec& spec);
    MC_API void append(char c);
    MC_API void append(std::string_view s);

    bool process_result(const detail::parser_result& result);
    void raise_error(const detail::parser_result& result);

private:
    std::string&               m_out;
    detail::runtime_arg_store& m_args;
};

} // namespace mc::fmt

#endif // MC_FMT_FORMAT_CONTEXT_H