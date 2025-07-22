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
#ifndef MC_FMT_FORMAT_PARSER_H
#define MC_FMT_FORMAT_PARSER_H

#include <mc/fmt/format_arg.h>
#include <mc/fmt/format_spec.h>

#include <string>
#include <string_view>

namespace mc::fmt {
class format_context;
} // namespace mc::fmt

namespace mc::fmt::detail {
template <typename Arg>
struct arg_store;

struct custom_t;

// 查找匹配的 '}' 位置，正确处理嵌套的 {}
// 从当前位置开始查找，返回指向匹配 '}' 的下一个位置的指针
template <typename Context>
constexpr const char* to_next_brace(Context& ctx, const char* ptr, const char* end) {
    int brace_count = 1; // 我们已经在一个 { 内部

    while (ptr < end && brace_count > 0) {
        if (*ptr == '{') {
            brace_count++;
        } else if (*ptr == '}') {
            brace_count--;
        }
        ++ptr;
    }

    if (brace_count > 0) {
        ctx.invalid_brace_arg();
    }

    return ptr; // 指向最后一个 '}' 的下一个位置
}

// 解析 ${name} 或 ${name:format} 语法的占位符
template <typename Context>
constexpr const char* parse_named_placeholder(Context& ctx, const char* ptr, const char* end,
                                              string_view& name, const char*& format_start) {
    if (ptr >= end || *ptr != '$') {
        return nullptr;
    }

    ++ptr; // 跳过 '$'
    if (ptr >= end || *ptr != '{') {
        return nullptr;
    }

    ++ptr; // 跳过 '{'
    const char* name_start = ptr;

    // 查找名称结束位置
    while (ptr < end && *ptr != ':' && *ptr != '}') {
        ++ptr;
    }

    if (ptr >= end) {
        ctx.invalid_brace_arg();
        return nullptr;
    }

    name = string_view(name_start, ptr - name_start);

    // 命名参数的名称不能为空
    if (name.empty()) {
        ctx.invalid_named_arg_name();
        return nullptr;
    }

    if (*ptr == ':') {
        format_start = ptr + 1;
        // 使用通用函数查找匹配的 '}'
        ptr = to_next_brace(ctx, ptr + 1, end);
    } else {
        format_start = nullptr;
        ptr          = to_next_brace(ctx, ptr, end);
    }

    return ptr; // 已经指向 '}' 的下一个位置
}

template <typename Context>
constexpr size_t parse_index(Context& ctx, string_view index_str) {
    size_t index = 0;
    for (char c : index_str) {
        if (!mc::fmt::detail::isdigit(c)) {
            ctx.invalid_index_arg();
            break;
        }
        index = index * 10 + (c - '0');
    }
    return index;
}

template <typename Context>
constexpr const char* parse_index_placeholder(Context& ctx, const char* ptr, const char* end,
                                              size_t& index, const char*& format_start) {
    if (ptr >= end || *ptr != '{') {
        return nullptr;
    }

    ++ptr; // 跳过 '{'
    const char* index_start = ptr;

    // 查找索引结束位置
    while (ptr < end && *ptr != ':' && *ptr != '}') {
        ++ptr;
    }

    if (ptr >= end) {
        ctx.invalid_brace_arg();
        return nullptr;
    }

    // 解析索引，空表示使用自动递增索引
    if (ptr != index_start) {
        index = parse_index(ctx, string_view(index_start, ptr - index_start));
    } else {
        index = INVALID_INDEX;
    }

    if (*ptr == ':') {
        format_start = ptr + 1;
        ptr          = to_next_brace(ctx, ptr + 1, end);
    } else {
        format_start = nullptr;
        ptr          = to_next_brace(ctx, ptr, end);
    }

    return ptr; // 已经指向 '}' 的下一个位置
}

template <typename Context>
constexpr bool resolve_dynamic_param(Context& ctx, size_t index, std::string_view name, int& out) {
    if (index == INVALID_INDEX && name.empty()) {
        return true; // 没有动态参数
    }

    return ctx.resolve_dynamic_param(index, name, out);
}

template <typename Context>
constexpr void resolve_dynamic_spec(Context& ctx, format_spec& spec) {
    if (!resolve_dynamic_param(ctx, spec.width_index, spec.width_name, spec.width)) {
        ctx.dynamic_width_param_type_error();
    }

    if (!resolve_dynamic_param(ctx, spec.precision_index, spec.precision_name, spec.precision)) {
        ctx.dynamic_precision_param_type_error();
    }
}

template <typename Arg>
constexpr const char* parse_format_spec(const char* start, const char* end, format_spec& spec, const Arg* arg) {
    if (start == nullptr || start >= end) {
        return start;
    }

    string_view format_str(start, end - start);
    return spec.parse(arg, start, end);
}

template <typename Context>
constexpr bool parse_named_arg(Context& ctx, const char*& ptr, const char* end, size_t& arg_index) {
    string_view name;
    const char* format_start = nullptr;
    const char* next_ptr     = parse_named_placeholder(ctx, ptr, end, name, format_start);
    if (next_ptr == nullptr) {
        return false;
    }

    format_spec                spec;
    typename Context::arg_type arg;
    size_t                     index = INVALID_INDEX;
    if (!ctx.get_named_arg(name, arg, index)) {
        ctx.invalid_named_arg(name);
        return false;
    }

    if (format_start != nullptr) {
        if (!parse_format_spec(format_start, next_ptr, spec, &arg)) {
            return false;
        }
    }

    resolve_dynamic_spec(ctx, spec);
    ctx.format_arg(arg, spec);
    ctx.set_used(index);
    ptr = next_ptr;
    ++arg_index;
    return true;
}

template <typename Context>
constexpr bool parse_index_arg(Context& ctx, const char*& ptr, const char* end, size_t& arg_index) {
    size_t      index        = INVALID_INDEX;
    const char* format_start = nullptr;
    const char* next_ptr     = parse_index_placeholder(ctx, ptr, end, index, format_start);
    if (next_ptr == nullptr) {
        return false;
    }

    if (index == INVALID_INDEX) {
        index = arg_index++;
    } else {
        arg_index = index + 1;
    }

    format_spec                spec;
    typename Context::arg_type arg;
    if (!ctx.get_arg(index, arg)) {
        ctx.invalid_index_arg(index);
        return false;
    }

    if (format_start != nullptr) {
        if (!parse_format_spec(format_start, next_ptr, spec, &arg)) {
            return false;
        }
    }

    resolve_dynamic_spec(ctx, spec);
    ctx.format_arg(arg, spec);
    ctx.set_used(index);
    ptr = next_ptr;
    return true;
}

constexpr bool is_escaped(const char* ptr, const char* end, char c) {
    return ptr + 1 < end && ptr[1] == c;
}

template <typename Context>
constexpr void parse_format_string(string_view fmt_str, Context& ctx) {
    const char* ptr       = fmt_str.data();
    const char* end       = ptr + fmt_str.size();
    size_t      arg_index = 0;
    while (ptr < end) {
        auto c = *ptr;

        // 处理普通字符
        if (c != '{' && c != '$' && c != '}') {
            ctx.append(c);
            ++ptr;
            continue;
        }

        // 处理 '}' 转义
        if (c == '}') {
            if (is_escaped(ptr, end, '}')) {
                ctx.append('}');
                ptr += 2;
                continue;
            }
            ctx.invalid_single_brace_arg();
            return;
        }

        // 处理 '{' 转义
        if (c == '{' && is_escaped(ptr, end, '{')) {
            ctx.append('{');
            ptr += 2;
            continue;
        }

        if (c == '$') {
            // 如果 $ 后面不是 {，则按普通字符处理
            if (!is_escaped(ptr, end, '{')) {
                ctx.append('$');
                ptr += 1;
                continue;
            }

            if (!parse_named_arg(ctx, ptr, end, arg_index)) {
                return;
            }
            continue;
        } else if (c == '{') {
            if (!parse_index_arg(ctx, ptr, end, arg_index)) {
                return;
            }
            continue;
        }

        // 如果不是有效的占位符，按普通字符处理
        ctx.append(c);
        ++ptr;
    }
}

class format_parser {
public:
    static void parse(std::string_view fmt_str, format_context& ctx);
    static void format_arg(format_context& ctx, const format_arg& arg, format_spec& spec);
    static void format_double(format_context& ctx, float value, const format_spec& spec);
    static void format_double(format_context& ctx, double value, const format_spec& spec);
    static void format_double(format_context& ctx, long double value, const format_spec& spec);
    static void format_int(format_context& ctx, int64_t value, const format_spec& spec);
    static void format_uint(format_context& ctx, uint64_t value, const format_spec& spec);
    static void format_string(format_context& ctx, std::string_view str, const format_spec& spec);
    static void format_pointer(format_context& ctx, const void* ptr, const format_spec& spec);
    static void format_char(format_context& ctx, char c, const format_spec& spec);
    static void format_bool(format_context& ctx, bool value, const format_spec& spec);
    static void format_custom(format_context& ctx, const custom_t& custom, const format_spec& spec);

    template <typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
    static void format_integer(format_context& ctx, T value, const format_spec& spec) {
        if constexpr (std::is_signed_v<T>) {
            format_int(ctx, static_cast<int64_t>(value), spec);
        } else {
            format_uint(ctx, static_cast<uint64_t>(value), spec);
        }
    }
};

} // namespace mc::fmt::detail

#endif // MC_FMT_FORMAT_PARSER_H