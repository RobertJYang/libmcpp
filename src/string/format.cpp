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

namespace mc {
namespace string {

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

} // namespace string

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

// 定义占位符语法的常量
constexpr std::string_view PLACEHOLDER_START = "${";
constexpr char             PLACEHOLDER_END   = '}';

// 应用格式化规范到字符串（处理宽度和对齐）
static void apply_string_format(std::string& result, std::string_view str, const string::format_spec& fmt) {
    if (!fmt.has_format || fmt.width <= 0 || str.size() >= static_cast<size_t>(fmt.width)) {
        result.append(str);
        return;
    }

    size_t padding   = fmt.width - str.size();
    char   alignment = fmt.alignment;
    if (alignment == '\0') {
        alignment = '<'; // 默认左对齐
    }

    switch (alignment) {
    case '<': // 左对齐
        result.append(str);
        result.append(padding, fmt.fill);
        break;
    case '>': // 右对齐
        result.append(padding, fmt.fill);
        result.append(str);
        break;
    case '^': // 居中对齐
    {
        size_t left_padding  = padding / 2;
        size_t right_padding = padding - left_padding;
        result.append(left_padding, fmt.fill);
        result.append(str);
        result.append(right_padding, fmt.fill);
    } break;
    default:
        result.append(str);
        break;
    }
}

// 根据格式规范格式化variant值并追加到结果中
static void append_formatted_variant_to_string(std::string& result, const variant& value, const string::format_spec& fmt) {
    if (!fmt.has_format) {
        // 没有格式规范，使用默认格式化
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
        return;
    }

    value.visit_with([&](auto&& val) {
        using T = std::decay_t<decltype(val)>;

        if constexpr (std::is_same_v<T, void> || std::is_same_v<T, std::nullptr_t>) {
            apply_string_format(result, "null", fmt);
        } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
            // 整数类型格式化
            std::ostringstream oss;

            switch (fmt.type) {
            case 'd':
            case '\0': // 默认十进制
                oss << std::dec << val;
                break;
            case 'x':
                oss << std::hex << std::nouppercase << val;
                break;
            case 'X':
                oss << std::hex << std::uppercase << val;
                break;
            case 'o':
                oss << std::oct << val;
                break;
            default:
                oss << val;
                break;
            }

            // 应用宽度和对齐
            std::string formatted = oss.str();
            apply_string_format(result, formatted, fmt);

        } else if constexpr (std::is_floating_point_v<T>) {
            // 浮点数类型格式化
            std::ostringstream oss;

            if (fmt.precision >= 0) {
                oss << std::setprecision(fmt.precision);
            }

            switch (fmt.type) {
            case 'f':
            case 'F':
            case '\0': // 默认定点表示法
                oss << std::fixed << val;
                break;
            case 'e':
                oss << std::scientific << std::nouppercase << val;
                break;
            case 'E':
                oss << std::scientific << std::uppercase << val;
                break;
            case 'g':
                oss << std::setprecision(fmt.precision >= 0 ? fmt.precision : 6);
                oss << val;
                break;
            case 'G':
                oss << std::setprecision(fmt.precision >= 0 ? fmt.precision : 6);
                oss << std::uppercase << val;
                break;
            default:
                oss << val;
                break;
            }

            // 应用宽度和对齐
            std::string formatted = oss.str();
            apply_string_format(result, formatted, fmt);

        } else if constexpr (std::is_same_v<T, bool>) {
            // 布尔类型格式化
            std::string bool_str;
            switch (fmt.type) {
            case 'd':
                bool_str = val ? "1" : "0";
                break;
            case '\0':
            default:
                bool_str = val ? "true" : "false";
                break;
            }
            apply_string_format(result, bool_str, fmt);

        } else if constexpr (std::is_same_v<T, std::string>) {
            // 字符串类型格式化
            apply_string_format(result, val, fmt);
        } else {
            // 其他类型使用默认字符串转换
            std::string str = value.to_string();
            apply_string_format(result, str, fmt);
        }
    });
}

// 将variant值转换为字符串并追加到结果中（原始版本，保持向后兼容）
static void append_variant_to_string(std::string& result, const variant& value) {
    string::format_spec default_fmt; // 默认格式（无格式化）
    append_formatted_variant_to_string(result, value, default_fmt);
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

// 处理固定宽度的格式化输出
static void append_with_width(std::string& result, std::string_view formatted, const string::format_spec& fmt) {
    if (fmt.width > 0 && formatted.size() < static_cast<size_t>(fmt.width)) {
        mc::string::fixed_width_append(result, fmt.width, formatted, fmt.alignment != '>');
    } else {
        result.append(formatted);
    }
}

// 根据格式规范格式化variant值并追加到结果中（简化版本）
static void append_formatted_variant_simple(std::string& result, const variant& value, std::string_view format_spec) {
    if (format_spec.empty()) {
        // 没有格式规范，使用默认格式化
        append_variant_to_string(result, value);
        return;
    }

    // 解析格式规范
    string::format_spec parsed_fmt = string::parse_format_spec(format_spec);

    value.visit_with([&](auto&& val) {
        using T = std::decay_t<decltype(val)>;

        if constexpr (std::is_same_v<T, void> || std::is_same_v<T, std::nullptr_t>) {
            append_with_width(result, "null", parsed_fmt);
        } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
            // 整数类型格式化
            char        buffer[64]; // 足够存储任意整数
            const char* begin = buffer;
            char*       end   = buffer;

            // 根据格式类型选择基数
            int  base      = 10;
            bool uppercase = false;
            switch (parsed_fmt.type) {
            case 'x':
                base = 16;
                break;
            case 'X':
                base      = 16;
                uppercase = true;
                break;
            case 'o':
                base = 8;
                break;
            }

            if (base == 10) {
                auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), val);
                end            = ptr;
            } else {
                // 对于非十进制，我们需要自己处理
                using abs_type   = std::make_unsigned_t<T>;
                abs_type abs_val = val < 0 ? static_cast<abs_type>(-val) : static_cast<abs_type>(val);
                if (val < 0) {
                    *end++ = '-';
                    begin  = end;
                }

                // 转换为指定进制
                do {
                    int  digit = abs_val % base;
                    char c;
                    if (digit < 10) {
                        c = '0' + digit;
                    } else {
                        c = (uppercase ? 'A' : 'a') + (digit - 10);
                    }
                    *end++ = c;
                    abs_val /= base;
                } while (abs_val > 0);

                // 反转数字部分
                char* rev_begin = const_cast<char*>(begin);
                std::reverse(rev_begin, end);
            }

            append_with_width(result, std::string_view(buffer, end - buffer), parsed_fmt);
        } else if constexpr (std::is_floating_point_v<T>) {
            // 浮点数类型格式化
            char                 buffer[128]; // 足够存储任意浮点数
            std::to_chars_result tc_result;

            auto precision = parsed_fmt.precision >= 0 ? parsed_fmt.precision : 6;

            switch (parsed_fmt.type) {
            case 'f':
            case 'F':
            case '\0':
                tc_result = std::to_chars(buffer, buffer + sizeof(buffer), val,
                                          std::chars_format::fixed, precision);
                break;
            case 'e':
            case 'E':
                tc_result = std::to_chars(buffer, buffer + sizeof(buffer), val,
                                          std::chars_format::scientific, precision);
                break;
            case 'g':
            case 'G':
                tc_result = std::to_chars(buffer, buffer + sizeof(buffer), val,
                                          std::chars_format::general, precision);
                break;
            default:
                tc_result = std::to_chars(buffer, buffer + sizeof(buffer), val);
                break;
            }

            if (tc_result.ec == std::errc()) {
                append_with_width(result, std::string_view(buffer, tc_result.ptr - buffer), parsed_fmt);
            } else {
                // 如果转换失败，回退到 ostringstream
                std::ostringstream oss;
                if (parsed_fmt.precision >= 0) {
                    oss << std::setprecision(precision);
                }
                switch (parsed_fmt.type) {
                case 'f':
                case 'F':
                case '\0':
                    oss << std::fixed << val;
                    break;
                case 'e':
                case 'E':
                    oss << std::scientific << (parsed_fmt.type == 'E' ? std::uppercase : std::nouppercase) << val;
                    break;
                case 'g':
                case 'G':
                    oss << std::setprecision(precision);
                    oss << (parsed_fmt.type == 'G' ? std::uppercase : std::nouppercase) << val;
                    break;
                default:
                    oss << val;
                    break;
                }
                append_with_width(result, oss.str(), parsed_fmt);
            }
        } else if constexpr (std::is_same_v<T, bool>) {
            // 布尔类型格式化
            append_with_width(result,
                              parsed_fmt.type == 'd' ? (val ? "1" : "0") : (val ? "true" : "false"),
                              parsed_fmt);
        } else if constexpr (std::is_same_v<T, std::string>) {
            // 字符串类型格式化
            append_with_width(result, val, parsed_fmt);
        } else {
            // 其他类型使用默认字符串转换
            append_with_width(result, value.to_string(), parsed_fmt);
        }
    });
}

// 从字典中获取键对应的值并追加到结果字符串
static void resolve_fmt_key(std::string& result, std::string_view key_with_format, const dict& args) {
    // 解析键名和格式规范
    auto [key, format_spec] = parse_key_and_format(key_with_format);

    // 验证 key 格式
    if (!is_valid_fmt_key(key)) {
        mc::string::append(result, PLACEHOLDER_START, key_with_format, PLACEHOLDER_END);
        return;
    }

    auto it = args.find(key);
    if (it != args.end()) {
        // 使用格式规范格式化值
        append_formatted_variant_simple(result, it->value, format_spec);
    } else {
        // 键不存在，保留原始占位符
        mc::string::append(result, PLACEHOLDER_START, key_with_format, PLACEHOLDER_END);
    }
}

// 从字典中获取键对应的值并追加到结果字符串（大小写不敏感版本）
static void resolve_fmt_key_icase(std::string& result, std::string_view key_with_format, const dict& args) {
    // 解析键名和格式规范
    auto [key, format_spec] = parse_key_and_format(key_with_format);

    // 验证 key 格式
    if (!is_valid_fmt_key(key)) {
        mc::string::append(result, PLACEHOLDER_START, key_with_format, PLACEHOLDER_END);
        return;
    }

    // 首先尝试精确匹配
    auto it = args.find(key);
    if (it != args.end()) {
        // 使用格式规范格式化值
        append_formatted_variant_simple(result, it->value, format_spec);
        return;
    }

    // 如果精确匹配失败，进行大小写不敏感的查找
    for (const auto& entry : args) {
        if (mc::string::iequals(entry.key.as_string(), key)) {
            append_formatted_variant_simple(result, entry.value, format_spec);
            return;
        }
    }

    // 键不存在，保留原始占位符
    mc::string::append(result, PLACEHOLDER_START, key_with_format, PLACEHOLDER_END);
}

// 使用参数字典格式化字符串并追加到结果字符串中
void mc::string::format_base(std::string& result, std::string_view format_str, const dict& args, bool icase) {
    if (format_str.empty()) {
        return;
    }

    // 快速检查是否有占位符，如果没有直接追加原始字符串
    if (format_str.find(PLACEHOLDER_START) == std::string_view::npos) {
        result.append(format_str);
        return;
    }

    result.reserve(result.size() + format_str.size() * 2); // 预分配空间

    std::size_t pos = 0;
    while (pos < format_str.size()) {
        // 查找下一个占位符开始位置
        size_t placeholder_start = format_str.find(PLACEHOLDER_START, pos);
        if (placeholder_start == std::string_view::npos) {
            // 没有找到更多占位符，添加剩余文本并返回
            result.append(format_str.substr(pos));
            break;
        }

        // 添加占位符前的普通文本
        if (placeholder_start > pos) {
            result.append(format_str.substr(pos, placeholder_start - pos));
        }

        // 查找占位符的结束位置
        size_t placeholder_end =
            format_str.find(PLACEHOLDER_END, placeholder_start + PLACEHOLDER_START.size());

        if (placeholder_end == std::string_view::npos) {
            // 没有找到结束括号，将剩余内容视为普通文本
            result.append(format_str.substr(pos));
            break;
        }

        // 提取占位符内容（不包含 ${ 和 }）
        std::string_view key =
            format_str.substr(placeholder_start + PLACEHOLDER_START.size(),
                              placeholder_end - (placeholder_start + PLACEHOLDER_START.size()));

        // 解析键并添加到结果
        if (icase) {
            resolve_fmt_key_icase(result, key, args);
        } else {
            resolve_fmt_key(result, key, args);
        }

        // 移动到占位符之后
        pos = placeholder_end + 1;
    }
}

// 使用参数字典格式化字符串并追加到结果字符串中
void mc::string::format(std::string& result, std::string_view format_str, const dict& args) {
    format_base(result, format_str, args, false);
}

// 使用参数字典格式化字符串
std::string mc::string::format(std::string_view format_str, const dict& args) {
    if (format_str.empty()) {
        return std::string();
    }

    // 快速检查是否有占位符，如果没有直接返回原始字符串
    if (format_str.find(PLACEHOLDER_START) == std::string_view::npos) {
        return std::string(format_str);
    }

    std::string result;
    mc::string::format_base(result, format_str, args, false);
    return result;
}

// 使用参数字典格式化字符串（大小写不敏感版本）并追加到结果字符串中
void mc::string::format_icase(std::string& result, std::string_view format_str, const dict& args) {
    format_base(result, format_str, args, true);
}

// 使用参数字典格式化字符串（大小写不敏感版本）
std::string mc::string::format_icase(std::string_view format_str, const dict& args) {
    if (format_str.empty()) {
        return std::string();
    }

    // 快速检查是否有占位符，如果没有直接返回原始字符串
    if (format_str.find(PLACEHOLDER_START) == std::string_view::npos) {
        return std::string(format_str);
    }

    std::string result;
    mc::string::format_base(result, format_str, args, true);
    return result;
}

std::string mc::string::format_v(const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::string result = format_vv(format, args);
    va_end(args);
    return result;
}

std::string mc::string::format_vv(const char* format, va_list args) {
    std::string result;
    if (!format_vv(result, format, args)) {
        return {};
    }

    return result;
}

bool mc::string::format_v(std::string& result, const char* format, ...) {
    va_list args;
    va_start(args, format);
    bool ret = format_vv(result, format, args);
    va_end(args);
    return ret;
}

bool mc::string::format_vv(std::string& result, const char* format, va_list args) {
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

bool mc::string::get_format_args(std::string_view format, mc::dict& arg_names) {
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
        if (key_with_format.empty()) {
            return false;
        }

        // 解析键名（忽略格式规范）
        auto [key, format_spec] = parse_key_and_format(key_with_format);
        args[key]               = true;

        pos = end_pos + 1;
    }

    arg_names = std::move(args);
    return true;
}

} // namespace mc