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
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <mc/common.h>
#include <mc/pretty_name.h>

namespace mc::string {

namespace detail {
template <typename T>
void append_formatted_number(std::string& result, T val, const char* format) {
    constexpr std::size_t BUFFER_SIZE = 64;
    char                  buffer[BUFFER_SIZE];
    int                   len = std::snprintf(buffer, BUFFER_SIZE, format, val);
    if (len > 0) {
        result.append(buffer, len);
    }
}

[[noreturn]] MC_API void throw_bad_cast_error(const char* type);
[[noreturn]] MC_API void throw_overflow_error(const char* type, std::string_view s);

MC_API std::pair<int, std::string_view> detect_number_radix(std::string_view s);

/**
 * @brief 准备一个以尾0结尾的字符串，用于安全地转换为数字
 * @param s 输入字符串
 * @param radix 进制
 * @param buffer 栈上分配的缓冲区
 * @return 可以安全用于 std::strtoX 函数的字符串指针，如果字符串无效则返回 nullptr
 */
MC_API std::string_view prepare_number_string(
    std::string_view s, int radix, char* buffer, std::size_t buffer_size) noexcept;
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

MC_API std::string to_string(double value);
MC_API void        to_string(std::string& result, double value);

MC_API std::string to_string(bool value);
MC_API void        to_string(std::string& result, bool value);

/**
 * @brief 忽略大小写比较两个字符串是否相等
 * @param a 第一个字符串
 * @param b 第二个字符串
 * @return 如果两个字符串忽略大小写后相等则返回 true，否则返回 false
 */
MC_API bool iequals(std::string_view a, std::string_view b);

/**
 * @brief 忽略大小写比较两个 C 风格字符串是否相等
 * @param a 第一个 C 风格字符串
 * @param b 第二个 C 风格字符串
 * @return 如果两个 C 风格字符串忽略大小写后相等则返回 true，否则返回 false
 */
MC_API bool iequals(const char* a, const char* b);

/**
 * @brief 将字符串转换为小写
 * @param s 要转换的字符串
 * @return 转换后的小写字符串
 */
MC_API std::string to_lower(std::string_view s);

/**
 * @brief 将字符串转换为大写
 * @param s 要转换的字符串
 * @return 转换后的大写字符串
 */
MC_API std::string to_upper(std::string_view s);

/**
 * @brief 原地将字符串转换为小写
 * @param s 要转换的字符串
 */
MC_API void to_lower_inplace(std::string& s);

/**
 * @brief 原地将字符串转换为大写
 * @param s 要转换的字符串
 */
MC_API void to_upper_inplace(std::string& s);

/**
 * @brief 去除字符串两端的空白字符
 * @param s 要处理的字符串
 * @return 处理后的字符串
 */
MC_API std::string trim(std::string_view s);

/**
 * @brief 原地去除字符串两端的空白字符
 * @param s 要处理的字符串
 */
MC_API void trim_inplace(std::string& s);

/**
 * @brief 去除字符串左侧的空白字符
 * @param s 要处理的字符串
 * @return 处理后的字符串
 */
MC_API std::string ltrim(std::string_view s);

/**
 * @brief 原地去除字符串左侧的空白字符
 * @param s 要处理的字符串
 */
MC_API void ltrim_inplace(std::string& s);

/**
 * @brief 去除字符串右侧的空白字符
 * @param s 要处理的字符串
 * @return 处理后的字符串
 */
MC_API std::string rtrim(std::string_view s);

/**
 * @brief 原地去除字符串右侧的空白字符
 * @param s 要处理的字符串
 */
MC_API void rtrim_inplace(std::string& s);

/**
 * @brief 按指定分隔符分割字符串
 * @param s 要分割的字符串
 * @param delim 分隔符
 * @return 分割后的字符串数组
 */
MC_API std::vector<std::string> split(std::string_view s, char delim);

/**
 * @brief 按指定分隔符分割字符串
 * @param s 要分割的字符串
 * @param delim 分隔符字符串
 * @return 分割后的字符串数组
 */
MC_API std::vector<std::string> split(std::string_view s, std::string_view delim);

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
MC_API std::string_view substr(std::string_view s, int start, int end = -1);

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
MC_API std::string_view substring(std::string_view s, int start, std::size_t length = std::string::npos);

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
        result.append(to_string(value));
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
MC_API std::string join(const std::vector<std::string>& v, std::string_view delim);

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
MC_API void fixed_width_append(std::string& result, size_t width, std::string_view s,
                               bool left_align = true);

/**
 * @brief 检查字符串是否以指定前缀开始
 * @param s 要检查的字符串
 * @param prefix 前缀
 * @return 如果字符串以指定前缀开始则返回 true，否则返回 false
 */
MC_API bool starts_with(std::string_view s, std::string_view prefix);

/**
 * @brief 检查字符串是否以指定后缀结束
 * @param s 要检查的字符串
 * @param suffix 后缀
 * @return 如果字符串以指定后缀结束则返回 true，否则返回 false
 */
MC_API bool ends_with(std::string_view s, std::string_view suffix);

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
MC_API std::string_view longest_common_prefix(std::string_view s1, std::string_view s2);

/**
 * @brief 替换字符串中的所有指定子串
 * @param s 要处理的字符串
 * @param from 要替换的子串
 * @param to 替换成的子串
 * @return 处理后的字符串
 */
MC_API std::string replace_all(std::string_view s, std::string_view from, std::string_view to);

/**
 * @brief 原地替换字符串中的所有指定子串
 * @param s 要处理的字符串
 * @param from 要替换的子串
 * @param to 替换成的子串
 */
MC_API void replace_all_inplace(std::string& s, std::string_view from, std::string_view to);

/**
 * @brief 检查字符串是否包含指定子串
 * @param s 要检查的字符串
 * @param substring 子串
 * @return 如果字符串包含指定子串则返回 true，否则返回 false
 */
MC_API bool contains(std::string_view s, std::string_view substring);

/**
 * @brief 忽略大小写检查字符串是否包含指定子串
 * @param s 要检查的字符串
 * @param substring 子串
 * @return 如果字符串忽略大小写后包含指定子串则返回 true，否则返回 false
 */
MC_API bool icontains(std::string_view s, std::string_view substring);

MC_API bool try_to_bool(std::string_view s, bool& result);

/**
 * @brief 将字符串转换为布尔值，如果转换失败则返回默认值
 * @param s 要转换的字符串
 * @param default_value 转换失败时返回的默认值
 * @return 转换结果或默认值
 */
MC_API bool to_bool_with_default(std::string_view s, bool default_value);

/**
 * @brief 将字符串转换为布尔值，如果转换失败则抛出异常
 * @param s 要转换的字符串
 * @return 转换结果
 */
MC_API bool to_bool(std::string_view s);

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
bool try_to_number_unsafe(std::string_view s, T& result, int radix = 0) {
    errno = 0;
    if (s.empty()) {
        return false;
    }

    if (radix == 0) {
        auto v = detail::detect_number_radix(s);
        radix  = v.first;
        s      = v.second;
    } else if (radix != 2 && radix != 8 && radix != 10 && radix != 16) {
        radix = 10;
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

#ifndef MC_NUMBER_STRING_BUFFER_SIZE
#define MC_NUMBER_STRING_BUFFER_SIZE 128
#endif

/**
 * @brief 尝试安全的将字符串转换为数字，使用栈上分配的缓冲区
 * @param s 要转换的字符串
 * @param result 转换结果的引用
 * @param radix 进制
 * @return 是否转换成功
 */
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
bool try_to_number_safe(std::string_view s, T& result, int radix = 0) {
    char buffer[MC_NUMBER_STRING_BUFFER_SIZE];
    auto str_data = detail::prepare_number_string(s, radix, buffer, sizeof(buffer));
    return try_to_number_unsafe(str_data, result, radix);
}

/**
 * @brief 尝试将C风格字符串转换为数字
 * @param s 要转换的C风格字符串
 * @param result 转换结果的引用
 * @return 是否转换成功
 */
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
bool try_to_number(const char* s, T& result, int radix = 0) {
    return try_to_number_safe<T>(std::string_view(s), result, radix);
}

/**
 * @brief 尝试将字符串转换为数字
 * @param s 要转换的字符串
 * @param result 转换结果的引用
 * @param radix 进制
 * @return 是否转换成功
 */
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
bool try_to_number(std::string_view s, T& result, int radix = 0) {
    return try_to_number_safe<T>(s, result, radix);
}

/**
 * @brief 尝试将标准字符串转换为数字
 * @param s 要转换的标准字符串
 * @param result 转换结果的引用
 * @return 是否转换成功
 */
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
bool try_to_number(const std::string& s, T& result, int radix = 0) {
    return try_to_number_unsafe<T>(s, result, radix);
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
 * @brief 将标准字符串转换为数字
 * @param s 要转换的标准字符串
 * @param radix 进制
 * @return 转换结果
 */
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
T to_number(const std::string& s, int radix = 0) {
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
 * @brief 将C风格字符串转换为数字
 * @param s 要转换的C风格字符串
 * @param radix 进制
 * @return 转换结果
 */
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
T to_number(const char* s, int radix = 0) {
    return to_number<T>(std::string_view(s), radix);
}

/**
 * @brief 将字符串转换为数字，如果转换失败则返回默认值
 * @param s 要转换的字符串
 * @param default_value 转换失败时返回的默认值
 * @return 转换结果或默认值
 */
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
T to_number_with_default(std::string_view s, T default_value, int radix = 0) {
    T result{};
    if (try_to_number<T>(s, result, radix)) {
        return result;
    }

    return default_value;
}

/**
 * @brief 将标准字符串转换为数字，如果转换失败则返回默认值
 * @param s 要转换的标准字符串
 * @param default_value 转换失败时返回的默认值
 * @param radix 进制
 * @return 转换结果或默认值
 */
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
T to_number_with_default(const std::string& s, T default_value, int radix = 0) {
    T result{};
    if (try_to_number<T>(s, result, radix)) {
        return result;
    }

    return default_value;
}

/**
 * @brief 将C风格字符串转换为数字，如果转换失败则返回默认值
 * @param s 要转换的C风格字符串
 * @param default_value 转换失败时返回的默认值
 * @param radix 进制
 * @return 转换结果或默认值
 */
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
T to_number_with_default(const char* s, T default_value, int radix = 0) {
    return to_number_with_default<T>(std::string_view(s), default_value, radix);
}

MC_API bool is_valid_utf8(std::string_view s);

/**
 * @brief 字符串分割迭代器
 * 用于高效地遍历分隔符分割的字符串片段
 */
class MC_API split_iterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type        = std::string_view;
    using difference_type   = std::ptrdiff_t;

    /**
     * @brief 默认构造函数，构造一个结束迭代器
     */
    constexpr split_iterator() noexcept {
    }

    /**
     * @brief 构造函数
     * @param str 要分割的字符串
     * @param delims 分隔符集合，其中的任意字符都视为分隔符
     */
    constexpr split_iterator(std::string_view str, std::string_view delims = " ") noexcept
        : m_str(str), m_delims(delims) {
        find_next();
    }

    constexpr value_type operator*() const noexcept {
        return std::string_view(m_str.data() + m_pos, m_end - m_pos);
    }

    constexpr value_type operator->() const noexcept {
        return operator*();
    }

    constexpr split_iterator& operator++() noexcept {
        find_next();
        return *this;
    }

    constexpr split_iterator operator++(int) noexcept {
        auto tmp = *this;
        ++(*this);
        return tmp;
    }

    constexpr bool operator==(const split_iterator& other) const noexcept {
        if (is_end() && other.is_end()) {
            return true;
        } else if (is_end() || other.is_end()) {
            return false;
        }
        return m_str.data() == other.m_str.data() && m_pos == other.m_pos;
    }

    constexpr bool operator!=(const split_iterator& other) const noexcept {
        return !(*this == other);
    }

    constexpr bool is_end() const noexcept {
        return m_pos >= m_str.size();
    }

    static constexpr split_iterator end() noexcept {
        return split_iterator();
    }

    /**
     * @brief 获取当前位置在原字符串中的起始偏移
     * @return 当前片段在原字符串中的起始位置
     */
    constexpr std::size_t current_pos() const noexcept {
        return m_pos;
    }

    /**
     * @brief 获取当前位置在原字符串中的结束偏移
     * @return 当前片段在原字符串中的结束位置
     */
    constexpr std::size_t end_pos() const noexcept {
        return m_end;
    }

    /**
     * @brief 获取当前位置在原字符串中的剩余部分
     * @return 当前片段在原字符串中的剩余部分，会跳过分隔符
     */
    constexpr std::string_view tail() const noexcept {
        if (is_end()) {
            return {};
        }

        auto pos = skip_delims(m_str, m_pos, m_delims);
        return m_str.substr(pos);
    }

    /**
     * @brief 获取当前位置在原字符串中的前置部分
     * @return 当前片段在原字符串中的前置部分
     */
    constexpr std::string_view head() const noexcept {
        return m_str.substr(0, m_pos);
    }

private:
    static constexpr std::size_t skip_delims(std::string_view str, std::size_t pos, std::string_view delims) {
        while (pos < str.size() && delims.find(str[pos]) != std::string_view::npos) {
            ++pos;
        }
        return pos;
    }

    static constexpr std::size_t find_next_delim(std::string_view str, std::size_t pos, std::string_view delims) {
        while (pos < str.size() && delims.find(str[pos]) == std::string_view::npos) {
            ++pos;
        }
        return pos;
    }

    constexpr void find_next() noexcept {
        m_pos = m_end;
        m_pos = skip_delims(m_str, m_pos, m_delims);
        m_end = find_next_delim(m_str, m_pos, m_delims);
    }

    std::string_view m_str;    ///< 要分割的字符串
    std::string_view m_delims; ///< 分隔符集合
    std::size_t      m_pos{0}; ///< 当前片段的起始位置
    std::size_t      m_end{0}; ///< 当前片段的结束位置
};
} // namespace mc::string

namespace mc {
using mc::string::to_bool;
using mc::string::to_bool_with_default;
using mc::string::to_number;
using mc::string::to_number_with_default;
using mc::string::to_string;
using mc::string::try_to_number;
} // namespace mc

#endif // MC_STRING_H