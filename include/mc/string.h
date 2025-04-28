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
#include <cstdio>
#include <cstdlib> // 用于strtod等函数
#include <limits>  // 用于numeric_limits
#include <locale>
#include <mc/pretty_name.h>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// 前向声明
namespace mc {
class dict;

namespace string {

namespace detail {
[[noreturn]] void throw_bad_cast_error(const char* type);
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
 * @brief 将字符串数组连接成一个字符串
 * @param v 字符串数组
 * @param delim 连接符
 * @return 连接后的字符串
 */
std::string join(const std::vector<std::string>& v, std::string_view delim);

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
        result.append(std::to_string(value));
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

std::string format(std::string_view format, const mc::dict& args);
void        format(std::string& result, std::string_view format, const mc::dict& args);

template <typename... Args>
std::string format_v(const std::string& format, Args... args) {
    int size = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
    if (size <= 0) {
        return "";
    }
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1);
}

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
 */
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
bool try_to_number(std::string_view s, T& result) {
    if (s.empty()) {
        return false;
    }

    // 浮点数转换
    if (std::is_floating_point<T>::value) {
        char* end;
        T     value = static_cast<T>(std::strtod(s.data(), &end));
        if (end != s.data() && end == s.data() + s.size()) {
            result = value;
            return true;
        }
        return false;
    }

    // 整数转换
    char* end;

    // 有符号整型
    if (std::is_same<T, int>::value || std::is_same<T, short>::value ||
        std::is_same<T, signed char>::value) {
        long value = std::strtol(s.data(), &end, 10);
        if (end != s.data() && end == s.data() + s.size()) {
            // 验证是否在T类型的范围内
            if (value >= std::numeric_limits<T>::min() && value <= std::numeric_limits<T>::max()) {
                result = static_cast<T>(value);
                return true;
            }
        }
    } else if (std::is_same<T, long>::value) {
        long value = std::strtol(s.data(), &end, 10);
        if (end != s.data() && end == s.data() + s.size()) {
            result = static_cast<T>(value);
            return true;
        }
    } else if (std::is_same<T, long long>::value) {
        long long value = std::strtoll(s.data(), &end, 10);
        if (end != s.data() && end == s.data() + s.size()) {
            result = static_cast<T>(value);
            return true;
        }
    }
    // 无符号整型
    else if (std::is_same<T, unsigned int>::value || std::is_same<T, unsigned short>::value ||
             std::is_same<T, unsigned char>::value) {
        unsigned long value = std::strtoul(s.data(), &end, 10);
        if (end != s.data() && end == s.data() + s.size()) {
            // 验证是否在T类型的范围内
            if (value <= std::numeric_limits<T>::max()) {
                result = static_cast<T>(value);
                return true;
            }
        }
    } else if (std::is_same<T, unsigned long>::value) {
        unsigned long value = std::strtoul(s.data(), &end, 10);
        if (end != s.data() && end == s.data() + s.size()) {
            result = static_cast<T>(value);
            return true;
        }
    } else if (std::is_same<T, unsigned long long>::value) {
        unsigned long long value = std::strtoull(s.data(), &end, 10);
        if (end != s.data() && end == s.data() + s.size()) {
            result = static_cast<T>(value);
            return true;
        }
    }

    return false;
}

/**
 * @brief 尝试将C风格字符串转换为数字
 * @param s 要转换的C风格字符串
 * @param result 转换结果的引用
 * @return 是否转换成功
 */
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
bool try_to_number(const char* s, T& result) {
    return try_to_number<T>(std::string_view(s), result);
}

/**
 * @brief 尝试将标准字符串转换为数字
 * @param s 要转换的标准字符串
 * @param result 转换结果的引用
 * @return 是否转换成功
 */
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
bool try_to_number(const std::string& s, T& result) {
    return try_to_number<T>(std::string_view(s), result);
}

/**
 * @brief 将字符串转换为数字，如果转换失败则抛出异常
 * @param s 要转换的字符串
 * @return 转换结果
 */
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
T to_number(std::string_view s) {
    T result{};
    if (try_to_number<T>(s, result)) {
        return result;
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
T to_number(std::string_view s, T default_value) {
    T result{};
    if (try_to_number<T>(s, result)) {
        return result;
    }

    return default_value;
}

// 定义占位符语法的常量
inline constexpr std::string_view PLACEHOLDER_START = "${";
inline constexpr char             PLACEHOLDER_END   = '}';

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
#define mc_format(fmt, ...)                                                                        \
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
#define mc_format_append(result, fmt, ...)                                                         \
    mc::format(result, fmt, static_cast<const mc::dict&>(mc::mutable_dict() __VA_ARGS__))

} // namespace mc

#endif // MC_STRING_H