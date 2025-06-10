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
 * @file string.h
 * @brief 定义了 mc 命名空间下的字符串处理函数
 */
#ifndef MC_STRING_H
#define MC_STRING_H

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <locale>
#include <mc/pretty_name.h>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// 前向声明
namespace mc {
class dict;

namespace detail {
// 辅助函数：直接将格式化后的数值追加到结果字符串
template <typename T>
void append_formatted_number(std::string& result, T val, const char* format) {
    constexpr std::size_t BUFFER_SIZE = 64;
    char                  buffer[BUFFER_SIZE];
    int                   len = std::snprintf(buffer, BUFFER_SIZE, format, val);
    if (len > 0) {
        result.append(buffer, len);
    }
}
} // namespace detail

template <typename T>
auto to_string(std::string& result, T value) -> std::enable_if_t<std::is_integral_v<T> && std::is_signed_v<T>, void> {
    detail::append_formatted_number(result, static_cast<long long>(value), "%lld");
}

template <typename T>
auto to_string(T value) -> std::enable_if_t<std::is_integral_v<T> && std::is_signed_v<T>, std::string> {
    std::string result;
    to_string(result, value);
    return result;
}

template <typename T>
auto to_string(std::string& result, T value) -> std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T>, void> {
    detail::append_formatted_number(result, static_cast<unsigned long long>(value), "%llu");
}

template <typename T>
auto to_string(T value) -> std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T>, std::string> {
    std::string result;
    to_string(result, value);
    return result;
}

std::string to_string(double value);
void        to_string(std::string& result, double value);

std::string to_string(bool value);
void        to_string(std::string& result, bool value);

namespace string {

namespace detail {
[[noreturn]] void throw_bad_cast_error(const char* type);
[[noreturn]] void throw_overflow_error(const char* type, std::string_view s);

std::pair<int, std::string_view> detect_number_radix(std::string_view s);
} // namespace detail

/**
 * @brief 忽略大小写比较两个字符串是否相等
 * @param a 第一个字符串
 * @param b 第二个字符串
 * @return 如果两个字符串忽略大小写后相等则返回 true，否则返回 false
 */
bool iequals(std::string_view a, std::string_view b);

/**
 * @brief 忽略大小写比较两个 C 风格字符串是否相等
 * @param a 第一个 C 风格字符串
 * @param b 第二个 C 风格字符串
 * @return 如果两个 C 风格字符串忽略大小写后相等则返回 true，否则返回 false
 */
bool iequals(const char* a, const char* b);

/**
 * @brief 将字符串转换为小写
 * @param s 要转换的字符串
 * @return 转换后的小写字符串
 */
std::string to_lower(std::string_view s);

/**
 * @brief 将字符串转换为大写
 * @param s 要转换的字符串
 * @return 转换后的大写字符串
 */
std::string to_upper(std::string_view s);

/**
 * @brief 原地将字符串转换为小写
 * @param s 要转换的字符串
 */
void to_lower_inplace(std::string& s);

/**
 * @brief 原地将字符串转换为大写
 * @param s 要转换的字符串
 */
void to_upper_inplace(std::string& s);

/**
 * @brief 去除字符串两端的空白字符
 * @param s 要处理的字符串
 * @return 处理后的字符串
 */
std::string trim(std::string_view s);

/**
 * @brief 原地去除字符串两端的空白字符
 * @param s 要处理的字符串
 */
void trim_inplace(std::string& s);

/**
 * @brief 去除字符串左侧的空白字符
 * @param s 要处理的字符串
 * @return 处理后的字符串
 */
std::string ltrim(std::string_view s);

/**
 * @brief 原地去除字符串左侧的空白字符
 * @param s 要处理的字符串
 */
void ltrim_inplace(std::string& s);

/**
 * @brief 去除字符串右侧的空白字符
 * @param s 要处理的字符串
 * @return 处理后的字符串
 */
std::string rtrim(std::string_view s);

/**
 * @brief 原地去除字符串右侧的空白字符
 * @param s 要处理的字符串
 */
void rtrim_inplace(std::string& s);

/**
 * @brief 按指定分隔符分割字符串
 * @param s 要分割的字符串
 * @param delim 分隔符
 * @return 分割后的字符串数组
 */
std::vector<std::string> split(std::string_view s, char delim);

/**
 * @brief 按指定分隔符分割字符串
 * @param s 要分割的字符串
 * @param delim 分隔符字符串
 * @return 分割后的字符串数组
 */
std::vector<std::string> split(std::string_view s, std::string_view delim);

/**
 * @brief 获取子字符串，支持负数索引
 * @param s 源字符串
 * @param start 起始位置（索引从0开始）
 *        - 正数表示从字符串开头计数的位置（0表示第一个字符）
 *        - 负数表示从字符串末尾计数的位置（-1表示最后一个字符）
 * @param end 结束位置（索引从0开始，默认为-1，表示到字符串末尾）
 *        - 正数表示从字符串开头计数的位置
 *        - 负数表示从字符串末尾计数的位置（-1表示最后一个字符）
 * @return 提取的子字符串
 *
 * 示例:
 * @code
 * // 获取完整字符串
 * std::string_view s1 = mc::string::substr("hello", 0, -1);  // "hello"
 *
 * // 获取前三个字符
 * std::string_view s2 = mc::string::substr("hello", 0, 2);   // "hel"
 *
 * // 获取最后三个字符
 * std::string_view s3 = mc::string::substr("hello", -3);     // "llo"
 *
 * // 获取从第二个到倒数第二个字符
 * std::string_view s4 = mc::string::substr("hello", 1, -2);  // "ell"
 * @endcode
 */
std::string_view substr(std::string_view s, int start, int end = -1);

/**
 * @brief 获取子字符串，第二个参数指定长度而非结束位置
 * @param s 源字符串
 * @param start 起始位置（索引从0开始）
 *        - 正数表示从字符串开头计数的位置（0表示第一个字符）
 *        - 负数表示从字符串末尾计数的位置（-1表示最后一个字符）
 * @param length 要提取的字符数量，默认为 std::string::npos 表示提取到字符串末尾
 * @return 提取的子字符串
 *
 * 示例:
 * @code
 * // 获取完整字符串
 * std::string_view s1 = mc::string::substring("hello", 0);  // "hello"
 *
 * // 获取前三个字符
 * std::string_view s2 = mc::string::substring("hello", 0, 3);  // "hel"
 *
 * // 获取最后三个字符
 * std::string_view s3 = mc::string::substring("hello", -3);  // "llo"
 *
 * // 获取从第二个字符开始的三个字符
 * std::string_view s4 = mc::string::substring("hello", 1, 3);  // "ell"
 * @endcode
 */
std::string_view substring(std::string_view s, int start, std::size_t length = std::string::npos);

/**
 * @brief 将一个或多个值追加到字符串中
 * @param result 要追加到的字符串
 * @param value 要追加的值
 * @param args 要追加的值
 */
template <typename T>
void append(std::string& result, T&& value) {
    if constexpr (std::is_same_v<std::decay_t<T>, std::string> ||
                  std::is_same_v<std::decay_t<T>, std::string_view>) {
        // 对于字符串和字符串视图，直接追加
        result.append(value);
    } else if constexpr (std::is_same_v<std::decay_t<T>, const char*>) {
        // 对于C风格字符串，直接追加
        result.append(value);
    } else if constexpr (std::is_same_v<std::decay_t<T>, char>) {
        // 对于单个字符，使用append(1, char)
        result.append(1, value);
    } else if constexpr (std::is_arithmetic_v<std::decay_t<T>>) {
        // 对于数值类型，转换为字符串后追加
        result.append(mc::to_string(value));
    } else {
        // 对于其他类型，尝试使用std::ostringstream
        std::ostringstream oss;
        oss << value;
        result.append(oss.str());
    }
}
template <typename T, typename... Args>
void append(std::string& result, T&& value, Args&&... args) {
    append(result, std::forward<T>(value));
    append(result, std::forward<Args>(args)...);
}

/**
 * @brief 将字符串数组连接成一个字符串
 * @param v 字符串数组
 * @param delim 连接符
 * @return 连接后的字符串
 */
std::string join(const std::vector<std::string>& v, std::string_view delim);

template <typename... Args>
std::string join(std::string_view delim, Args&&... args) {
    std::string result;
    if constexpr (sizeof...(args) == 0) {
        // 无参数时返回空字符串
    } else if constexpr (sizeof...(args) == 1) {
        // 单个参数时直接转换并追加
        append(result, args...);
    } else if constexpr (sizeof...(args) > 1) {
        // 多个参数时，先添加第一个参数
        auto add_with_delim = [&result, delim](const auto& arg) {
            if (!result.empty()) {
                result.append(delim);
            }
            append(result, arg);
        };
        (add_with_delim(args), ...);
    }
    return result;
}

/**
 * @brief 将字符串数组连接成一个字符串
 * @param v 字符串数组
 * @return 连接后的字符串
 */
inline std::string concat(const std::vector<std::string>& v) {
    std::string result;
    result.reserve(std::accumulate(v.begin(), v.end(), size_t{0},
                                   [](size_t sum, const std::string& s) {
        return sum + s.size();
    }));
    for (const auto& s : v) {
        result.append(s);
    }
    return result;
}

/**
 * @brief 将多个参数连接成一个字符串
 * @param args 要连接的参数
 * @return 连接后的字符串
 */
template <typename T, typename... Args,
          typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, std::vector<std::string>>>>
std::string concat(T&& first, Args&&... args) {
    std::string result;
    append(result, std::forward<T>(first));
    if constexpr (sizeof...(args) > 0) {
        append(result, std::forward<Args>(args)...);
    }
    return result;
}

/**
 * @brief 使用固定宽度格式化字符串，不足用空格填充，并追加到目标字符串
 * @param result 要追加结果的目标字符串
 * @param width 目标宽度
 * @param s 要格式化的字符串
 * @param left_align 是否左对齐，默认为true
 */
void fixed_width_append(std::string& result, size_t width, std::string_view s,
                        bool left_align = true);

/**
 * @brief 检查字符串是否以指定前缀开始
 * @param s 要检查的字符串
 * @param prefix 前缀
 * @return 如果字符串以指定前缀开始则返回 true，否则返回 false
 */
bool starts_with(std::string_view s, std::string_view prefix);

/**
 * @brief 检查字符串是否以指定后缀结束
 * @param s 要检查的字符串
 * @param suffix 后缀
 * @return 如果字符串以指定后缀结束则返回 true，否则返回 false
 */
bool ends_with(std::string_view s, std::string_view suffix);

/**
 * @brief 查找两个字符串的最长公共前缀
 * @param s1 第一个字符串
 * @param s2 第二个字符串
 * @return 两个字符串的最长公共前缀
 *
 * 示例:
 * @code
 * std::string_view prefix = mc::string::longest_common_prefix("hello", "help");  // "hel"
 * std::string_view empty = mc::string::longest_common_prefix("hello", "world");  // ""
 * @endcode
 */
std::string_view longest_common_prefix(std::string_view s1, std::string_view s2);

/**
 * @brief 替换字符串中的所有指定子串
 * @param s 要处理的字符串
 * @param from 要替换的子串
 * @param to 替换成的子串
 * @return 处理后的字符串
 */
std::string replace_all(std::string_view s, std::string_view from, std::string_view to);

/**
 * @brief 原地替换字符串中的所有指定子串
 * @param s 要处理的字符串
 * @param from 要替换的子串
 * @param to 替换成的子串
 */
void replace_all_inplace(std::string& s, std::string_view from, std::string_view to);

/**
 * @brief 检查字符串是否包含指定子串
 * @param s 要检查的字符串
 * @param substring 子串
 * @return 如果字符串包含指定子串则返回 true，否则返回 false
 */
bool contains(std::string_view s, std::string_view substring);

/**
 * @brief 忽略大小写检查字符串是否包含指定子串
 * @param s 要检查的字符串
 * @param substring 子串
 * @return 如果字符串忽略大小写后包含指定子串则返回 true，否则返回 false
 */
bool icontains(std::string_view s, std::string_view substring);

void        format_base(std::string& result, std::string_view format_str, const mc::dict& args, bool icase);
std::string format(std::string_view format, const mc::dict& args);
void        format(std::string& result, std::string_view format, const mc::dict& args);

std::string format_icase(std::string_view format, const mc::dict& args);
void        format_icase(std::string& result, std::string_view format, const mc::dict& args);

/**
 * @brief 使用可变参数格式化字符串
 * @param format 格式化字符串
 * @param ... 可变参数
 * @return 格式化后的字符串
 */
std::string format_v(const char* format, ...);

/**
 * @brief 使用可变参数格式化字符串
 * @param format 格式化字符串
 * @param args 可变参数
 * @return 格式化后的字符串
 */
std::string format_vv(const char* format, va_list args);

/**
 * @brief 使用可变参数格式化字符串，并追加到目标字符串
 * @param result 接收格式化结果的目标字符串
 * @param format 格式化字符串
 * @param ... 可变参数
 */
bool format_v(std::string& result, const char* format, ...);

/**
 * @brief 使用可变参数格式化字符串，并追加到目标字符串
 * @param result 接收格式化结果的目标字符串
 * @param format 格式化字符串
 * @param args 可变参数
 */
bool format_vv(std::string& result, const char* format, va_list args);

/**
 * @brief 尝试将字符串转换为布尔值
 * @param s 要转换的字符串
 * @param result 转换结果的引用
 * @return 是否转换成功
 */
inline bool try_to_bool(std::string_view s, bool& result) {
    if (s.empty()) {
        result = false;
        return true;
    }

    if (iequals(s, std::string_view("true", 4)) || s == std::string_view("1", 1)) {
        result = true;
        return true;
    } else if (iequals(s, std::string_view("false", 5)) || s == std::string_view("0", 1)) {
        result = false;
        return true;
    }

    return false;
}

/**
 * @brief 将字符串转换为布尔值，如果转换失败则返回默认值
 * @param s 要转换的字符串
 * @param default_value 转换失败时返回的默认值
 * @return 转换结果或默认值
 */
inline bool to_bool(std::string_view s, bool default_value) {
    if (s.empty()) {
        return false;
    }

    bool result;
    if (try_to_bool(s, result)) {
        return result;
    }

    return default_value;
}

/**
 * @brief 将字符串转换为布尔值，如果转换失败则抛出异常
 * @param s 要转换的字符串
 * @return 转换结果
 */
inline bool to_bool(std::string_view s) {
    if (s.empty()) {
        return false;
    }

    bool result;
    if (try_to_bool(s, result)) {
        return result;
    }

    detail::throw_bad_cast_error("bool");
}

/**
 * @brief 尝试将字符串转换为数字
 * @param s 要转换的字符串
 * @param result 转换结果的引用
 * @return 是否转换成功
 * 注意：这个函数底层使用了 std::strtod 和 std::strtol 等的 C
 * 风格版本，必须确保字符串能以尾0结尾，否则存在安全风险，因为 string_view
 * 不保证以尾0结尾，建议后续用 std::from_chars 替代
 */
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
bool try_to_number(std::string_view s, T& result, int radix = 0) {
    errno = 0;
    if (s.empty()) {
        return false;
    }

    if (radix == 0) {
        auto v = detail::detect_number_radix(s);
        radix  = v.first;
        s      = v.second;
    }

    char* end;
    if constexpr (std::is_floating_point_v<T>) {
        result = std::strtod(s.data(), &end);
    } else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
        if constexpr (sizeof(T) == sizeof(long long)) {
            result = std::strtoll(s.data(), &end, radix);
        } else {
            auto v = std::strtol(s.data(), &end, radix);
            if (v < std::numeric_limits<T>::min() || v > std::numeric_limits<T>::max()) {
                return false;
            }
            result = static_cast<T>(v);
        }
    } else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
        if constexpr (sizeof(T) == sizeof(unsigned long long)) {
            result = std::strtoull(s.data(), &end, radix);
        } else {
            auto v = std::strtoul(s.data(), &end, radix);
            if (v > std::numeric_limits<T>::max()) {
                return false;
            }
            result = static_cast<T>(v);
        }
    }

    if (errno == ERANGE) {
        return false;
    }

    return end != s.data() && end == s.data() + s.size();
}

/**
 * @brief 尝试将C风格字符串转换为数字
 * @param s 要转换的C风格字符串
 * @param result 转换结果的引用
 * @return 是否转换成功
 */
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
bool try_to_number(const char* s, T& result, int radix = 0) {
    return try_to_number<T>(std::string_view(s), result, radix);
}

/**
 * @brief 尝试将标准字符串转换为数字
 * @param s 要转换的标准字符串
 * @param result 转换结果的引用
 * @return 是否转换成功
 */
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
bool try_to_number(const std::string& s, T& result, int radix = 0) {
    return try_to_number<T>(std::string_view(s), result, radix);
}

/**
 * @brief 将字符串转换为数字，如果转换失败则抛出异常
 * @param s 要转换的字符串
 * @return 转换结果
 */
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
T to_number(std::string_view s, int radix = 0) {
    T result{};
    if (try_to_number<T>(s, result, radix)) {
        return result;
    }

    if (errno == ERANGE) {
        detail::throw_overflow_error(mc::pretty_name<T>(), s);
    }

    detail::throw_bad_cast_error(mc::pretty_name<T>());
}

/**
 * @brief 将字符串转换为数字，如果转换失败则返回默认值
 * @param s 要转换的字符串
 * @param default_value 转换失败时返回的默认值
 * @return 转换结果或默认值
 */
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
T to_number(std::string_view s, T default_value, int radix = 0) {
    T result{};
    if (try_to_number<T>(s, result, radix)) {
        return result;
    }

    if (errno == ERANGE) {
        detail::throw_overflow_error(mc::pretty_name<T>(), s);
    }

    return default_value;
}

bool get_format_args(std::string_view format, mc::dict& arg_names);

bool is_valid_utf8(std::string_view s);

} // namespace string

/**
 * @brief 使用参数字典格式化字符串
 * @param format 格式化字符串，使用 ${key} 作为占位符
 * @param args 包含替换值的字典
 * @return 格式化后的字符串
 *
 * 示例:
 * @code
 * std::string result = mc::format("${host}:${port}",
 *                                 mc::dict("host", hostname)("port", port));
 * @endcode
 */
inline std::string format(std::string_view format, const mc::dict& args) {
    return mc::string::format(format, args);
}

/**
 * @brief 使用参数字典格式化字符串并写入到给定的结果字符串中
 * @param result 接收格式化结果的字符串引用
 * @param format 格式化字符串，使用 ${key} 作为占位符
 * @param args 包含替换值的字典
 *
 * 示例:
 * @code
 * std::string result;
 * mc::format(result, "${host}:${port}", mc::dict("host", hostname)("port", port));
 * @endcode
 */
inline void format(std::string& result, std::string_view format, const mc::dict& args) {
    mc::string::format(result, format, args);
}

/**
 * @brief 使用参数字典格式化字符串（大小写不敏感版本）
 * @param format 格式化字符串，使用 ${key} 作为占位符
 * @param args 包含替换值的字典
 * @return 格式化后的字符串
 *
 * 在查找替换值时忽略键名的大小写。首先尝试精确匹配，如果失败则进行大小写不敏感的查找。
 *
 * 示例:
 * @code
 * std::string result = mc::format_icase("${HOST}:${port}",
 *                                       mc::dict("host", hostname)("PORT", port));
 * @endcode
 */
inline std::string format_icase(std::string_view format, const mc::dict& args) {
    return mc::string::format_icase(format, args);
}

/**
 * @brief 使用参数字典格式化字符串并写入到给定的结果字符串中（大小写不敏感版本）
 * @param result 接收格式化结果的字符串引用
 * @param format 格式化字符串，使用 ${key} 作为占位符
 * @param args 包含替换值的字典
 *
 * 在查找替换值时忽略键名的大小写。首先尝试精确匹配，如果失败则进行大小写不敏感的查找。
 *
 * 示例:
 * @code
 * std::string result;
 * mc::format_icase(result, "${HOST}:${port}", mc::dict("host", hostname)("PORT", port));
 * @endcode
 */
inline void format_icase(std::string& result, std::string_view format, const mc::dict& args) {
    mc::string::format_icase(result, format, args);
}

/**
 * @brief 字符串格式化
 * @param format_v 格式化字符串，类似于 printf 的格式
 * @param args 格式化参数
 * @return 格式化后的字符串
 */
template <typename... Args>
std::string format_v(const std::string& format, Args... args) {
    return mc::string::format_v(format, args...);
}

/**
 * @brief 解析格式化字符串，提取占位符
 * @param format 格式化字符串
 * @param arg_names 存储占位符名称的字典
 * @return 如果解析成功返回 true，否则返回 false
 */
inline bool get_format_args(std::string_view format, mc::dict& arg_names) {
    return mc::string::get_format_args(format, arg_names);
}

/**
 * @brief 简化format函数调用的宏
 * @param fmt 格式字符串，使用 ${key} 作为占位符
 * @param ... 键值对序列，形式为 (key1, value1)(key2, value2)
 * @return 格式化后的字符串
 *
 * 示例:
 * @code
 * // 不带参数的调用
 * std::string s1 = mc_format("无参数字符串");
 *
 * // 带参数的调用
 * std::string s2 = mc_format("${host}:${port}", ("host", "example.com")("port", 8080));
 * @endcode
 */
#define mc_format(fmt, ...) \
    mc::format(fmt, static_cast<const mc::dict&>(mc::mutable_dict() __VA_ARGS__))

/**
 * @brief 简化format函数调用的宏，将结果追加到现有字符串
 * @param result 接收格式化结果的字符串引用
 * @param fmt 格式字符串，使用 ${key} 作为占位符
 * @param ... 键值对序列，形式为 (key1, value1)(key2, value2)
 *
 * 示例:
 * @code
 * std::string result = "前缀：";
 * mc_format_append(result, "${host}:${port}", ("host", "example.com")("port", 8080));
 * @endcode
 */
#define mc_format_append(result, fmt, ...) \
    mc::format(result, fmt, static_cast<const mc::dict&>(mc::mutable_dict() __VA_ARGS__))

} // namespace mc

#endif // MC_STRING_H