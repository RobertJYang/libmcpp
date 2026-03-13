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

#include <mc/exception.h>
#include <mc/fmt/format_context.h>
#include <mc/fmt/format_parser.h>

namespace mc::fmt {
static detail::runtime_arg_store s_empty_args;

namespace detail {

void runtime_arg_store::set_dict_args(const mc::dict* args)
{
    named_args = args;
}

bool runtime_arg_store::get_arg(size_t index, format_arg& arg) const
{
    if (auto* v = get_variant(index); v != nullptr) {
        arg = format_arg(*v);
        return true;
    }

    return arg_store<format_arg>::get_arg(index, arg);
}

bool runtime_arg_store::get_positional(size_t index, format_arg& arg) const
{
    return runtime_arg_store::get_arg(index, arg);
}

bool runtime_arg_store::get_named(std::string_view name, format_arg& arg, size_t& index) const
{
    if (named_args != nullptr) {
        if (auto* v = get_variant(name); v != nullptr) {
            arg = format_arg(*v);
            return true;
        }
    }
    return arg_store<format_arg>::get_named(name, arg, index);
}

bool runtime_arg_store::resolve_dynamic_param(size_t index, std::string_view name, int& out)
{
    if (named_args == nullptr) {
        return arg_store<format_arg>::resolve_dynamic_param(index, name, out);
    }

    const mc::variant* v = index != INVALID_INDEX ? get_variant(index) : get_variant(name);
    if (v == nullptr) {
        return false;
    }

    if (v->is_integer()) {
        out = static_cast<int>(v->as<int>());
        return true;
    }

    if (auto val = v->template try_as<int>()) {
        out = *val;
        return true;
    }

    return false;
}

const mc::variant* runtime_arg_store::get_variant(size_t index) const
{
    if (named_args == nullptr || index > named_args->size()) {
        return nullptr;
    }

    auto it = named_args->find(index);
    if (it != named_args->end()) {
        return &it->value;
    }

    return nullptr;
}

static bool iequals(const mc::variant& key, std::string_view name)
{
    if (key.is_string()) {
        return mc::string::iequals(key.get_string(), name);
    }

    return mc::string::iequals(key.as_string(), name);
}

const mc::variant* runtime_arg_store::get_variant(std::string_view name) const
{
    if (named_args == nullptr) {
        return nullptr;
    }

    auto it = named_args->find(name);
    if (it != named_args->end()) {
        return &it->value;
    }

    if (icase) {
        // 如果精确匹配失败，进行大小写不敏感的查找
        for (const auto& entry : *named_args) {
            if (iequals(entry.key, name)) {
                return &entry.value;
            }
        }
    }

    return nullptr;
}

} // namespace detail

format_context::format_context(std::string& out, detail::runtime_arg_store& args)
    : m_out(out), m_args(args)
{
}

format_context::format_context(std::string& out)
    : m_out(out), m_args(s_empty_args)
{
}

std::string& format_context::out()
{
    return m_out;
}

bool format_context::get_arg(size_t index, detail::format_arg& arg) const
{
    if (m_args.get_positional(index, arg)) {
        return true;
    }

    return false;
}

bool format_context::get_named_arg(std::string_view name, detail::format_arg& arg, size_t& index) const
{
    if (m_args.get_named(name, arg, index)) {
        return true;
    }

    return false;
}

void format_context::set_used(size_t index)
{
    m_args.set_used(index);
}

bool format_context::resolve_dynamic_param(size_t index, std::string_view name, int& out)
{
    return m_args.resolve_dynamic_param(index, name, out);
}

void format_context::format_arg(const detail::format_arg& arg, detail::format_spec& spec)
{
    detail::format_parser::format_arg(*this, arg, spec);
}

void format_context::append(char c)
{
    m_out.push_back(c);
}

void format_context::append(std::string_view s)
{
    m_out.append(s);
}

bool format_context::process_result(const detail::parser_result& result)
{
    if (!result.has_error()) {
        return true;
    }

    // 运行时吃掉所有错误，将原始文本输出
    append(result.text);
    return true;
}

void format_context::raise_error(const detail::parser_result& result)
{
    if (!result.has_error()) {
        return;
    }

    switch (result.err) {
        case detail::parser_error::invalid_brace_arg:
            MC_THROW(mc::format_error, "Corresponding '}}' not found");
            break;
        case detail::parser_error::invalid_named_arg_name:
            MC_THROW(mc::format_error, "Named parameter name cannot be empty");
            break;
        case detail::parser_error::invalid_index_arg:
            MC_THROW(mc::format_error, "Positional parameter index must be a number");
            break;
        case detail::parser_error::invalid_single_brace_arg:
            MC_THROW(mc::format_error, "Standalone '}}' in format string");
            break;
        case detail::parser_error::name_arg_not_found:
            MC_THROW(mc::format_error, "Named parameter not found: ${name}",
                    ("name", result.text));
            break;
        case detail::parser_error::index_arg_not_found:
            MC_THROW(mc::format_error, "Positional parameter not found: ${index}",
                    ("index", result.text));
            break;
        case detail::parser_error::invalid_spec_arg:
            MC_THROW(mc::format_error, "Format string or parameter error: ${text}",
                    ("text", result.text));
            break;
        case detail::parser_error::invalid_dynamic_param:
            MC_THROW(mc::format_error, "Dynamic parameter type error: ${text}",
                    ("text", result.text));
            break;
        default:
            break;
    }
}

} // namespace mc::fmt