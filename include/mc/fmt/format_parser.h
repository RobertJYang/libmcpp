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

#include <mc/common.h>
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

enum class parser_error {
    success,
    invalid_brace_arg,
    invalid_named_arg_name,
    invalid_index_arg,
    invalid_single_brace_arg,
    invalid_dynamic_param,
    invalid_spec_arg,
    name_arg_not_found,
    index_arg_not_found,
};

struct parser_result {
    constexpr parser_result(const char* start) : start_ptr(start)
    {}

    static constexpr parser_result failed(const char*& ptr, parser_error err, size_t skip_chars = 1)
    {
        parser_result result(ptr);
        ptr += skip_chars;
        result.error(err, ptr);
        return result;
    }

    constexpr bool has_error() const
    {
        return err != parser_error::success;
    }

    constexpr void success(const char* next)
    {
        next_ptr = next;
        err      = parser_error::success;
    }

    constexpr void error(parser_error e, const char* current = nullptr)
    {
        if (current != nullptr) {
            text     = string_view(start_ptr, current - start_ptr);
            next_ptr = (current > start_ptr) ? current : (start_ptr + 1);
        } else {
            // 失败跳到下一个字符
            next_ptr = start_ptr + 1;
            text     = string_view(start_ptr, 1);
        }
        err = e;
    }

    const char*      start_ptr{nullptr};
    const char*      next_ptr{nullptr};
    std::string_view text;
    parser_error     err{parser_error::success};
};

// 查找匹配的 '}' 位置，正确处理嵌套的 {}
// 从当前位置开始查找，返回指向匹配 '}' 的下一个位置的指针
template <typename Context>
constexpr const char* to_next_brace(parser_result& result, Context& ctx, const char* ptr, const char* end)
{
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
        result.error(parser_error::invalid_brace_arg);
        return ptr;
    }

    return ptr; // 指向最后一个 '}' 的下一个位置
}

// 通用的占位符内容解析函数
// 解析 { 和 } 之间的内容，提取参数名/索引和格式说明符
template <typename Context>
constexpr parser_result parse_placeholder_content(Context& ctx, const char* ptr, const char* end,
                                                  std::string_view& content, const char*& format_start)
{
    parser_result result(ptr);
    if (ptr >= end || *ptr != '{') {
        result.error(parser_error::invalid_brace_arg);
        return result;
    }

    ++ptr; // 跳过 '{'
    const char* content_start = ptr;

    // 查找内容结束位置（遇到 ':' 或 '}'）
    while (ptr < end && *ptr != ':' && *ptr != '}') {
        ++ptr;
    }

    if (ptr >= end) {
        result.error(parser_error::invalid_brace_arg);
        return result;
    }

    content = std::string_view(content_start, ptr - content_start);

    // 处理格式说明符
    if (*ptr == ':') {
        format_start = ptr + 1;
        ptr          = to_next_brace(result, ctx, ptr + 1, end);
    } else {
        format_start = nullptr;
        ptr          = to_next_brace(result, ctx, ptr, end);
    }

    if (result.has_error()) {
        return result;
    }

    result.success(ptr);
    return result;
}

// 解析 ${name} 或 ${name:format} 语法的占位符
// 返回占位符后的第一个字符的指针和占位符的内容
template <typename Context>
constexpr parser_result parse_named_placeholder(Context& ctx, const char* ptr, const char* end, string_view& name,
                                                const char*& format_start)
{
    const char*   original_ptr = ptr; // 保存原始指针，包含 $
    parser_result result(ptr);
    if (ptr >= end || *ptr != '$') {
        result.error(parser_error::invalid_brace_arg);
        return result;
    }

    ++ptr; // 跳过 '$'

    // 使用通用函数解析 {name:format} 部分
    std::string_view content;
    result = parse_placeholder_content(ctx, ptr, end, content, format_start);
    if (result.has_error()) {
        // 修正错误文本，包含完整的占位符（从$开始）
        if (result.next_ptr > original_ptr) {
            result.text = string_view(original_ptr, result.next_ptr - original_ptr);
        }
        return result;
    }

    // 验证名称是否为有效标识符
    if (!mc::is_identifier(content)) {
        result.error(parser_error::invalid_named_arg_name);
        return result;
    }

    name = content;
    return result;
}

// 检查字符串是否为纯数字
constexpr bool is_numeric_string(std::string_view str)
{
    if (str.empty()) {
        return true; // 空字符串算作数字（自增索引）
    }

    for (char c : str) {
        if (!mc::fmt::detail::isdigit(c)) {
            return false;
        }
    }
    return true;
}

template <typename Context>
constexpr size_t parse_index(Context& ctx, string_view index_str)
{
    size_t index = 0;
    for (char c : index_str) {
        index = index * 10 + (c - '0');
    }
    return index;
}

// 智能解析占位符，支持索引参数和命名参数
// {}, {123}, {name} 等格式
template <typename Context>
constexpr parser_result parse_smart_placeholder(Context& ctx, const char* ptr, const char* end, size_t& index,
                                                std::string_view& name, const char*& format_start)
{
    std::string_view content;
    parser_result    result = parse_placeholder_content(ctx, ptr, end, content, format_start);
    if (result.has_error()) {
        return result;
    }

    // 初始化输出参数
    index = INVALID_INDEX;
    name  = std::string_view();

    // 智能判断内容类型
    if (content.empty()) {
        // 空内容 {} - 自增索引
        index = INVALID_INDEX;
    } else if (is_numeric_string(content)) {
        // 纯数字 {123} - 索引参数
        index = parse_index(ctx, content);
    } else if (mc::is_identifier(content)) {
        // 有效标识符 {name} - 命名参数
        name  = content;
        index = INVALID_INDEX;
    } else {
        // 无效格式
        result.error(parser_error::invalid_index_arg);
        return result;
    }

    return result;
}

// 保持向后兼容的 parse_index_placeholder 函数
template <typename Context>
constexpr parser_result parse_index_placeholder(Context& ctx, const char* ptr, const char* end, size_t& index,
                                                const char*& format_start)
{
    std::string_view name;
    return parse_smart_placeholder(ctx, ptr, end, index, name, format_start);
}

template <typename Context>
constexpr bool resolve_dynamic_param(Context& ctx, size_t index, std::string_view name, int& out)
{
    if (index == INVALID_INDEX && name.empty()) {
        return true; // 没有动态参数
    }

    return ctx.resolve_dynamic_param(index, name, out);
}

template <typename Context>
constexpr bool resolve_dynamic_spec(parser_result& result, Context& ctx, format_spec& spec)
{
    if (!resolve_dynamic_param(ctx, spec.width_index, spec.width_name, spec.width)) {
        result.error(parser_error::invalid_dynamic_param);
        return false;
    }

    if (!resolve_dynamic_param(ctx, spec.precision_index, spec.precision_name, spec.precision)) {
        result.error(parser_error::invalid_dynamic_param);
        return false;
    }

    return true;
}

template <typename Arg>
constexpr const char* parse_format_spec(const char* start, const char* end, format_spec& spec, const Arg* arg)
{
    if (start == nullptr || start >= end) {
        return start;
    }

    string_view format_str(start, end - start);
    return spec.parse(arg, start, end);
}

template <typename Context>
constexpr parser_result parse_named_arg(Context& ctx, const char*& ptr, const char* end, size_t& arg_index)
{
    const char*   original_ptr = ptr; // 保存原始指针，用于错误时重构完整文本
    string_view   name;
    const char*   format_start = nullptr;
    parser_result result       = parse_named_placeholder(ctx, ptr, end, name, format_start);
    if (result.has_error()) {
        return result;
    }

    format_spec                spec;
    typename Context::arg_type arg;
    size_t                     index = INVALID_INDEX;
    if (!ctx.get_named_arg(name, arg, index)) {
        // 设置正确的错误文本，包含完整的占位符
        result.text = string_view(original_ptr, result.next_ptr - original_ptr);
        result.err  = parser_error::name_arg_not_found;
        return result;
    }

    if (format_start != nullptr) {
        if (!parse_format_spec(format_start, result.next_ptr, spec, &arg)) {
            result.error(parser_error::invalid_spec_arg, result.next_ptr);
            return result;
        }
    }

    if (!resolve_dynamic_spec(result, ctx, spec)) {
        return result;
    }

    ctx.format_arg(arg, spec);
    ctx.set_used(index);
    ptr = result.next_ptr;
    ++arg_index;
    return result;
}

template <typename Context>
constexpr parser_result parse_index_arg(Context& ctx, const char*& ptr, const char* end, size_t& arg_index)
{
    const char*      original_ptr = ptr; // 保存原始指针，用于错误时重构完整文本
    size_t           index        = INVALID_INDEX;
    std::string_view name;
    const char*      format_start = nullptr;
    parser_result    result       = parse_smart_placeholder(ctx, ptr, end, index, name, format_start);
    if (result.has_error()) {
        return result;
    }

    format_spec                spec;
    typename Context::arg_type arg;
    size_t                     actual_index = INVALID_INDEX;

    // 根据解析结果获取参数
    if (!name.empty()) {
        // 命名参数
        if (!ctx.get_named_arg(name, arg, actual_index)) {
            // 设置正确的错误文本，包含完整的占位符
            result.text = string_view(original_ptr, result.next_ptr - original_ptr);
            result.err  = parser_error::name_arg_not_found;
            return result;
        }
        ++arg_index; // 命名参数也占用参数计数
    } else {
        // 索引参数
        if (index == INVALID_INDEX) {
            actual_index = arg_index++;
        } else {
            actual_index = index;
            arg_index    = index + 1;
        }

        if (!ctx.get_arg(actual_index, arg)) {
            // 设置正确的错误文本，包含完整的占位符
            result.text = string_view(original_ptr, result.next_ptr - original_ptr);
            result.err  = parser_error::index_arg_not_found;
            return result;
        }
    }

    if (format_start != nullptr) {
        if (!parse_format_spec(format_start, result.next_ptr, spec, &arg)) {
            result.error(parser_error::invalid_spec_arg, result.next_ptr);
            return result;
        }
    }

    if (!resolve_dynamic_spec(result, ctx, spec)) {
        return result;
    }

    ctx.format_arg(arg, spec);
    ctx.set_used(actual_index);
    ptr = result.next_ptr;
    return result;
}

constexpr bool is_escaped(const char* ptr, const char* end, char c)
{
    return ptr + 1 < end && ptr[1] == c;
}

template <typename Context>
constexpr void parse_format_string(string_view fmt_str, Context& ctx)
{
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
            auto result = parser_result::failed(ptr, parser_error::invalid_brace_arg, 1);
            if (ctx.process_result(result)) {
                continue; // 有错误发生，但是上下文处理好了继续
            }
            ctx.raise_error(result);
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

            auto result = parse_named_arg(ctx, ptr, end, arg_index);
            if (result.has_error()) {
                if (!ctx.process_result(result)) {
                    ctx.raise_error(result);
                    return;
                }
                // 有错误发生，但是上下文处理好了继续
                ptr = (result.next_ptr <= ptr) ? ptr + 1 : result.next_ptr; // 保证至少移动一个位置防止死循环
            }
            continue;
        } else if (c == '{') {
            auto result = parse_index_arg(ctx, ptr, end, arg_index);
            if (result.has_error()) {
                if (!ctx.process_result(result)) {
                    ctx.raise_error(result);
                    return;
                }

                // 有错误发生，但是上下文处理好了继续
                ptr = (result.next_ptr <= ptr) ? ptr + 1 : result.next_ptr; // 保证至少移动一个位置防止死循环
                continue;
            }
            continue;
        }

        // 如果不是有效的占位符，按普通字符处理
        ctx.append(c);
        ++ptr;
    }
}

class MC_API format_parser {
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
    static void format_integer(format_context& ctx, T value, const format_spec& spec)
    {
        if constexpr (std::is_signed_v<T>) {
            format_int(ctx, static_cast<int64_t>(value), spec);
        } else {
            format_uint(ctx, static_cast<uint64_t>(value), spec);
        }
    }
};

} // namespace mc::fmt::detail

#endif // MC_FMT_FORMAT_PARSER_H