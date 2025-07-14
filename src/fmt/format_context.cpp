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

#include <mc/fmt/format_context.h>
#include <mc/fmt/format_parser.h>

namespace mc::fmt {
static detail::arg_store m_empty_args;

format_context::format_context(std::string& out, const detail::arg_store& args) : m_out(out), m_args(args) {
}

format_context::format_context(std::string& out) : m_out(out), m_args(m_empty_args) {
}

std::string& format_context::out() {
    return m_out;
}

const detail::format_arg* format_context::get_arg(size_t index) const {
    if (auto arg = m_args.get_positional(index)) {
        return arg;
    }

    MC_THROW(mc::invalid_arg_exception, "找不到位置参数: ${index}",
             ("index", index));
}

const detail::format_arg* format_context::get_named_arg(std::string_view name) const {
    if (auto arg = m_args.get_named(name)) {
        return arg;
    }

    MC_THROW(mc::invalid_arg_exception, "找不到命名参数: ${name}",
             ("name", name));
}

void format_context::dynamic_width_param_type_error() {
    MC_THROW(mc::format_error, "动态宽度参数类型错误，必须为整数");
}

void format_context::dynamic_precision_param_type_error() {
    MC_THROW(mc::format_error, "动态精度参数类型错误，必须为整数");
}

void format_context::invalid_brace_arg() {
    MC_THROW(mc::format_error, "未找到对应的 '}'");
}

void format_context::invalid_named_arg_name() {
    MC_THROW(mc::format_error, "命名参数名称不能为空");
}

void format_context::invalid_index_arg() {
    MC_THROW(mc::format_error, "位置参数索引必须是数字");
}

void format_context::invalid_single_brace_arg() {
    MC_THROW(mc::format_error, "单独的 '}' 在格式化字符串中");
}

void format_context::invalid_named_arg(std::string_view name) {
    MC_THROW(mc::format_error, "找不到命名参数: ${name}",
             ("name", name));
}

void format_context::invalid_index_arg(size_t index) {
    MC_THROW(mc::format_error, "找不到位置参数: ${index}",
             ("index", index));
}

void format_context::format_arg(const detail::format_arg& arg, detail::format_spec& spec) {
    detail::format_parser::format_arg(*this, arg, spec);
}

void format_context::append(char c) {
    m_out.push_back(c);
}

void format_context::append(std::string_view s) {
    m_out.append(s);
}

} // namespace mc::fmt