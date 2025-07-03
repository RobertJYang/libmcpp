/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
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
 * @file format.cpp
 * @brief 实现字符串格式化功能
 */

#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/string.h>
#include <mc/variant.h>

#include <cstring>
#include <functional>
#include <iomanip>
#include <sstream>
#include <type_traits>

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <stdarg.h>

namespace mc::fmt {

// 定义占位符语法的常量
constexpr std::string_view PLACEHOLDER_START = "${";
constexpr char             PLACEHOLDER_END   = '}';

// 格式规范结构体，用于存储解析后的格式化参数
struct format_spec {
    char alignment  = '\0';  // 对齐方式：'<' 左对齐，'>' 右对齐，'^' 居中
    char fill       = ' ';   // 填充字符，默认为空格
    int  width      = 0;     // 字段宽度
    int  precision  = -1;    // 精度（小数位数）
    char type       = '\0';  // 类型标识符：'d', 'x', 'X', 'o', 'f', 'F', 'e', 'E', 'g', 'G'
    bool has_format = false; // 是否有格式规范

    format_spec() = default;
};

namespace detail {

// 直接写入目标字符串的 streambuf，避免临时缓冲区
class direct_stringbuf : public std::streambuf {
public:
    explicit direct_stringbuf(std::string& target) : m_target(target) {
        setp(nullptr, nullptr);
    }

protected:
    int_type overflow(int_type ch) override {
        if (ch != traits_type::eof()) {
            m_target.push_back(static_cast<char>(ch));
            return ch;
        }
        return traits_type::eof();
    }

    std::streamsize xsputn(const char* s, std::streamsize n) override {
        m_target.append(s, n);
        return n;
    }

private:
    std::string& m_target;
};

// 处理固定宽度的格式化输出
static void append_with_width(std::string& result, std::string_view formatted, const format_spec& fmt) {
    if (fmt.width > 0 && formatted.size() < static_cast<size_t>(fmt.width)) {
        mc::string::fixed_width_append(result, fmt.width, formatted, fmt.alignment != '>');
    } else {
        result.append(formatted);
    }
}

// 处理固定宽度的格式化输出，返回是否需要继续处理
static bool handle_width_format(std::string& result, size_t old_size, const format_spec& fmt) {
    if (fmt.width > 0) {
        std::string_view new_content(result.data() + old_size, result.size() - old_size);
        if (new_content.size() < static_cast<size_t>(fmt.width)) {
            // 先移除新增的内容，使用固定宽度重新添加
            result.resize(old_size);
            append_with_width(result, new_content, fmt);
            return false;
        }
    }
    return true;
}

// 格式化整数
template <typename T>
static void format_integer(std::string& result, T val, const format_spec& fmt) {
    // 创建直接写入的 stream
    direct_stringbuf buf(result);
    std::ostream     out(&buf);

    // 设置进制
    switch (fmt.type) {
    case 'x':
        out << std::hex << std::nouppercase;
        break;
    case 'X':
        out << std::hex << std::uppercase;
        break;
    case 'o':
        out << std::oct;
        break;
    case 'd':
    case '\0':
    default:
        out << std::dec;
        break;
    }

    out << val;
}

// 格式化浮点数
template <typename T>
static void format_floating(std::string& result, T val, const format_spec& fmt) {
    // 创建直接写入的 stream
    direct_stringbuf buf(result);
    std::ostream     out(&buf);

    // 设置精度
    if (fmt.precision >= 0) {
        out << std::setprecision(fmt.precision);
    }

    // 设置格式
    switch (fmt.type) {
    case 'f':
    case 'F':
    case '\0':
        out << std::fixed;
        break;
    case 'e':
        out << std::scientific << std::nouppercase;
        break;
    case 'E':
        out << std::scientific << std::uppercase;
        break;
    case 'g':
        out << std::setprecision(fmt.precision >= 0 ? fmt.precision : 6);
        break;
    case 'G':
        out << std::setprecision(fmt.precision >= 0 ? fmt.precision : 6)
            << std::uppercase;
        break;
    }

    out << val;
}

// 格式化数值类型（整数或浮点数）
template <typename T>
static void format_number(std::string& result, T val, const format_spec& fmt) {
    size_t old_size = result.size();

    if constexpr (std::is_integral_v<T>) {
        format_integer(result, val, fmt);
    } else {
        format_floating(result, val, fmt);
    }

    handle_width_format(result, old_size, fmt);
}

// 快速格式化：直接追加到结果中，不考虑格式化规范
static void append_variant_fast(std::string& result, const variant& value) {
    value.visit_with([&](auto&& val) {
        using T = std::decay_t<decltype(val)>;

        if constexpr (std::is_same_v<T, void> || std::is_same_v<T, std::nullptr_t>) {
            result.append("null");
        } else if constexpr (std::is_arithmetic_v<T> || std::is_same_v<T, bool>) {
            mc::to_string(result, val);
        } else if constexpr (std::is_same_v<T, std::string>) {
            result.append(val);
        } else {
            result.append(value.to_string());
        }
    });
}

// 根据格式规范格式化variant值并追加到结果中
static void append_variant_with_format_spec(std::string& result, const variant& value, const format_spec& fmt) {
    value.visit_with([&](auto&& val) {
        using T = std::decay_t<decltype(val)>;

        if constexpr (std::is_same_v<T, void> || std::is_same_v<T, std::nullptr_t>) {
            append_with_width(result, "null", fmt);
        } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
            format_number(result, val, fmt);
        } else if constexpr (std::is_floating_point_v<T>) {
            format_number(result, val, fmt);
        } else if constexpr (std::is_same_v<T, bool>) {
            append_with_width(result,
                              fmt.type == 'd' ? (val ? "1" : "0") : (val ? "true" : "false"),
                              fmt);
        } else if constexpr (std::is_same_v<T, std::string>) {
            append_with_width(result, val, fmt);
        } else {
            // 其他类型使用默认字符串转换
            append_with_width(result, value.to_string(), fmt);
        }
    });
}

// 解析格式规范字符串，例如 ":.2f", ":10d", ":<10s" 等
static format_spec parse_format_spec(std::string_view spec) {
    format_spec fmt;

    if (spec.empty()) {
        return fmt;
    }

    fmt.has_format = true;
    size_t pos     = 0;

    // 解析对齐和填充字符
    if (pos < spec.size()) {
        char c = spec[pos];
        if (c == '<' || c == '>' || c == '^') {
            fmt.alignment = c;
            ++pos;
        } else if (pos + 1 < spec.size()) {
            char next = spec[pos + 1];
            if (next == '<' || next == '>' || next == '^') {
                fmt.fill      = c;
                fmt.alignment = next;
                pos += 2;
            }
        }
    }

    // 解析宽度
    if (pos < spec.size() && std::isdigit(static_cast<unsigned char>(spec[pos]))) {
        size_t width_start = pos;
        while (pos < spec.size() && std::isdigit(static_cast<unsigned char>(spec[pos]))) {
            ++pos;
        }
        auto width_str = spec.substr(width_start, pos - width_start);
        fmt.width      = std::stoi(std::string(width_str));
    }

    // 解析精度
    if (pos < spec.size() && spec[pos] == '.') {
        ++pos;
        if (pos < spec.size() && std::isdigit(static_cast<unsigned char>(spec[pos]))) {
            size_t precision_start = pos;
            while (pos < spec.size() && std::isdigit(static_cast<unsigned char>(spec[pos]))) {
                ++pos;
            }
            auto precision_str = spec.substr(precision_start, pos - precision_start);
            fmt.precision      = std::stoi(std::string(precision_str));
        }
    }

    // 解析类型
    if (pos < spec.size()) {
        fmt.type = spec[pos];
    }

    return fmt;
}

// 根据格式规范格式化variant值并追加到结果中（字符串格式规范版本）
static void append_variant(std::string& result, const variant& value, std::string_view format_spec) {
    if (format_spec.empty()) {
        append_variant_fast(result, value);
        return;
    }

    // 解析格式规范
    auto parsed_fmt = parse_format_spec(format_spec);
    append_variant_with_format_spec(result, value, parsed_fmt);
}

// 验证键名是否合法（只能包含字母、数字、下划线，且只能以字母和下划线开头）
static bool is_valid_fmt_key(std::string_view key) {
    if (key.empty()) {
        return false;
    }

    // 首字符必须是字母或下划线
    if (!std::isalpha(static_cast<unsigned char>(key[0])) && key[0] != '_') {
        return false;
    }

    // 其余字符必须是字母、数字或下划线
    for (size_t i = 1; i < key.size(); ++i) {
        if (!std::isalnum(static_cast<unsigned char>(key[i])) && key[i] != '_') {
            return false;
        }
    }

    return true;
}

// 解析键名和格式规范，返回{键名, 格式规范}的pair
static std::pair<std::string_view, std::string_view> parse_key_and_format(std::string_view key_with_format) {
    auto colon_pos = key_with_format.find(':');
    if (colon_pos == std::string_view::npos) {
        return {key_with_format, std::string_view()};
    }

    std::string_view key         = key_with_format.substr(0, colon_pos);
    std::string_view format_spec = key_with_format.substr(colon_pos + 1);
    return {key, format_spec};
}

// 从字典中获取键对应的值并追加到结果字符串
static void resolve_fmt_key(std::string& result, std::string_view key_with_format, const dict& args) {
    auto [key, format_spec] = parse_key_and_format(key_with_format);

    if (!is_valid_fmt_key(key)) {
        // 键名无效，保持原样
        result.append(PLACEHOLDER_START);
        result.append(key_with_format);
        result.append(1, PLACEHOLDER_END);
        return;
    }

    auto it = args.find(key);
    if (it == args.end()) {
        // 键不存在，保持原样
        result.append(PLACEHOLDER_START);
        result.append(key_with_format);
        result.append(1, PLACEHOLDER_END);
        return;
    }

    append_variant(result, it->value, format_spec);
}

// 从字典中获取键对应的值并追加到结果字符串（大小写不敏感版本）
static void resolve_fmt_key_icase(std::string& result, std::string_view key_with_format, const dict& args) {
    auto [key, format_spec] = parse_key_and_format(key_with_format);

    if (!is_valid_fmt_key(key)) {
        // 键名无效，保持原样
        result.append(PLACEHOLDER_START);
        result.append(key_with_format);
        result.append(1, PLACEHOLDER_END);
        return;
    }

    // 首先尝试精确匹配
    auto it = args.find(key);
    if (it != args.end()) {
        append_variant(result, it->value, format_spec);
        return;
    }

    // 如果精确匹配失败，进行大小写不敏感的查找
    for (const auto& entry : args) {
        if (mc::string::iequals(entry.key.as_string(), key)) {
            append_variant(result, entry.value, format_spec);
            return;
        }
    }

    // 键不存在，保持原样
    result.append(PLACEHOLDER_START);
    result.append(key_with_format);
    result.append(1, PLACEHOLDER_END);
}

} // namespace detail

// 格式化字符串并追加到结果字符串中
static void format_dict_base(std::string& result, std::string_view format_str, const dict& args, bool icase) {
    if (format_str.empty()) {
        return;
    }

    // 简单预分配：原始文本长度的两倍
    result.reserve(result.size() + format_str.size() * 2);

    size_t pos = 0;
    while (pos < format_str.size()) {
        // 查找下一个占位符
        size_t start = format_str.find(PLACEHOLDER_START, pos);
        if (start == std::string_view::npos) {
            // 没有更多占位符，添加剩余文本
            result.append(format_str.substr(pos));
            break;
        }

        // 添加占位符前的文本
        result.append(format_str.substr(pos, start - pos));

        // 查找占位符结束位置
        size_t end = format_str.find(PLACEHOLDER_END, start + PLACEHOLDER_START.size());
        if (end == std::string_view::npos) {
            // 占位符未闭合，保持原样
            result.append(format_str.substr(start));
            break;
        }

        // 提取键名和格式规范
        std::string_view key = format_str.substr(start + PLACEHOLDER_START.size(),
                                                 end - (start + PLACEHOLDER_START.size()));

        // 解析键并添加到结果
        if (icase) {
            detail::resolve_fmt_key_icase(result, key, args);
        } else {
            detail::resolve_fmt_key(result, key, args);
        }

        pos = end + 1;
    }
}

void format_dict(std::string& result, std::string_view format_str, const dict& args) {
    format_dict_base(result, format_str, args, false);
}

std::string format_dict(std::string_view format_str, const dict& args) {
    std::string result;
    format_dict_base(result, format_str, args, false);
    return result;
}

void format_dict_icase(std::string& result, std::string_view format_str, const dict& args) {
    format_dict_base(result, format_str, args, true);
}

std::string format_dict_icase(std::string_view format_str, const dict& args) {
    std::string result;
    format_dict_base(result, format_str, args, true);
    return result;
}

std::string format_v(const char* format, ...) {
    va_list args;
    va_start(args, format);
    auto result = format_vv(format, args);
    va_end(args);
    return result;
}

std::string format_vv(const char* format, va_list args) {
    std::string result;
    format_vv(result, format, args);
    return result;
}

bool format_v(std::string& result, const char* format, ...) {
    va_list args;
    va_start(args, format);
    bool ret = format_vv(result, format, args);
    va_end(args);
    return ret;
}

bool format_vv(std::string& result, const char* format, va_list args) {
    va_list args_tmp;
    va_copy(args_tmp, args);
    int size = std::vsnprintf(nullptr, 0, format, args_tmp);
    va_end(args_tmp);
    if (size <= 0) {
        result.clear();
        return false;
    }

    result.resize(size + 1);
    int ret = std::vsnprintf(const_cast<char*>(result.data()), size + 1, format, args);
    if (ret <= 0) {
        result.clear();
        return false;
    }

    result.resize(ret);
    return true;
}

bool get_format_args(std::string_view format, mc::dict& arg_names) {
    if (format.empty()) {
        return true;
    }

    std::size_t      pos = 0;
    mc::mutable_dict args;
    while (pos < format.size()) {
        size_t start_pos = format.find(PLACEHOLDER_START, pos);
        if (start_pos == std::string_view::npos) {
            break;
        }

        size_t end_pos = format.find(PLACEHOLDER_END, start_pos + PLACEHOLDER_START.size());
        if (end_pos == std::string_view::npos) {
            return false;
        }

        std::string_view key_with_format = format.substr(start_pos + PLACEHOLDER_START.size(),
                                                         end_pos - (start_pos + PLACEHOLDER_START.size()));

        auto [key, format_spec] = detail::parse_key_and_format(key_with_format);
        args[key]               = true;

        pos = end_pos + 1;
    }

    arg_names = std::move(args);
    return true;
}

} // namespace mc::fmt