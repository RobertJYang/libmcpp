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

#include <mc/fmt/format_arg.h>
#include <mc/fmt/format_spec.h>

namespace mc::fmt {

// 格式化上下文
class format_context {
public:
    using char_type = char;

    explicit format_context(std::string& out, const detail::arg_store& args);
    explicit format_context(std::string& out);

    std::string& out();

    const detail::format_arg* get_arg(size_t index) const;
    const detail::format_arg* get_named_arg(std::string_view name) const;

    void format_arg(const detail::format_arg& arg, detail::format_spec& spec);
    void append(char c);
    void append(std::string_view s);

    void dynamic_width_param_type_error();
    void dynamic_precision_param_type_error();
    void invalid_brace_arg();
    void invalid_named_arg_name();
    void invalid_index_arg();
    void invalid_single_brace_arg();
    void invalid_named_arg(std::string_view name);
    void invalid_index_arg(size_t index);

private:
    std::string&             m_out;
    const detail::arg_store& m_args;
};

} // namespace mc::fmt

#endif // MC_FMT_FORMAT_CONTEXT_H