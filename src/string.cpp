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
 * @file string.cpp
 * @brief 实现 string.h 中声明的字符串处理函数
 */
#include <cstring>
#include <functional>
#include <mc/dict.h>
#include <mc/string.h>
#include <mc/variant.h>
#include <type_traits>

namespace mc {
namespace string {
// 忽略大小写比较两个字符串是否相等
bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }

    return std::equal(a.begin(), a.end(), b.begin(), [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) ==
               std::tolower(static_cast<unsigned char>(b));
    });
}

// 忽略大小写比较两个 C 风格字符串是否相等
bool iequals(const char* a, const char* b) {
    if (!a || !b) {
        return a == b;
    }

    return iequals(std::string_view(a), std::string_view(b));
}

// 将字符串转换为小写
std::string to_lower(std::string_view s) {
    std::string result(s);
    to_lower_inplace(result);
    return result;
}

// 将字符串转换为大写
std::string to_upper(std::string_view s) {
    std::string result(s);
    to_upper_inplace(result);
    return result;
}

// 原地将字符串转换为小写
void to_lower_inplace(std::string& s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
}

// 原地将字符串转换为大写
void to_upper_inplace(std::string& s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return std::toupper(c);
    });
}

// 去除字符串两端的空白字符
std::string trim(std::string_view s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(static_cast<unsigned char>(*start))) {
        ++start;
    }

    auto end = s.end();
    if (start != s.end()) {
        end = s.end();
        do {
            --end;
        } while (std::isspace(static_cast<unsigned char>(*end)) && end != start);
        ++end;
    }

    return std::string(start, end);
}

// 原地去除字符串两端的空白字符
void trim_inplace(std::string& s) {
    ltrim_inplace(s);
    rtrim_inplace(s);
}

// 去除字符串左侧的空白字符
std::string ltrim(std::string_view s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(static_cast<unsigned char>(*start))) {
        ++start;
    }
    return std::string(start, s.end());
}

// 原地去除字符串左侧的空白字符
void ltrim_inplace(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
}

// 去除字符串右侧的空白字符
std::string rtrim(std::string_view s) {
    auto end = s.end();
    if (s.begin() != s.end()) {
        end = s.end();
        do {
            --end;
        } while (end != s.begin() && std::isspace(static_cast<unsigned char>(*end)));
        if (!std::isspace(static_cast<unsigned char>(*end))) {
            ++end;
        }
    }
    return std::string(s.begin(), end);
}

// 原地去除字符串右侧的空白字符
void rtrim_inplace(std::string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](unsigned char ch) {
                             return !std::isspace(ch);
                         })
                .base(),
            s.end());
}

// 按指定分隔符分割字符串
std::vector<std::string> split(std::string_view s, char delim) {
    std::vector<std::string>    result;
    std::string_view::size_type start = 0;
    std::string_view::size_type end   = s.find(delim);

    while (end != std::string_view::npos) {
        result.emplace_back(s.substr(start, end - start));
        start = end + 1;
        end   = s.find(delim, start);
    }

    if (start < s.size()) {
        result.emplace_back(s.substr(start));
    }

    return result;
}

// 按指定分隔符分割字符串
std::vector<std::string> split(std::string_view s, std::string_view delim) {
    std::vector<std::string> result;
    if (delim.empty()) {
        result.emplace_back(s);
        return result;
    }

    std::string_view::size_type start = 0;
    std::string_view::size_type end   = s.find(delim);

    while (end != std::string_view::npos) {
        result.emplace_back(s.substr(start, end - start));
        start = end + delim.size();
        end   = s.find(delim, start);
    }

    if (start < s.size()) {
        result.emplace_back(s.substr(start));
    }

    return result;
}

// 将字符串数组连接成一个字符串
std::string join(const std::vector<std::string>& v, std::string_view delim) {
    if (v.empty()) {
        return "";
    }

    std::string result;
    size_t      total_size = 0;

    // 预先计算结果字符串的大小以减少内存分配
    for (const auto& s : v) {
        total_size += s.size();
    }
    total_size += delim.size() * (v.size() - 1);

    result.reserve(total_size);

    for (size_t i = 0; i < v.size(); ++i) {
        result += v[i];
        if (i < v.size() - 1) {
            result.append(delim.data(), delim.size());
        }
    }

    return result;
}

// 检查字符串是否以指定前缀开始
bool starts_with(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

// 检查字符串是否以指定后缀结束
bool ends_with(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}

// 替换字符串中的所有指定子串
std::string replace_all(std::string_view s, std::string_view from, std::string_view to) {
    if (from.empty()) {
        return std::string(s);
    }

    std::string result;
    result.reserve(s.size());

    size_t last_pos = 0;
    size_t pos      = 0;
    while ((pos = s.find(from, last_pos)) != std::string_view::npos) {
        result.append(s.data() + last_pos, pos - last_pos);
        result.append(to.data(), to.size());
        last_pos = pos + from.size();
    }

    result.append(s.data() + last_pos, s.size() - last_pos);
    return result;
}

// 原地替换字符串中的所有指定子串
void replace_all_inplace(std::string& s, std::string_view from, std::string_view to) {
    if (from.empty()) {
        return;
    }

    size_t start_pos = 0;
    while ((start_pos = s.find(from, start_pos)) != std::string::npos) {
        s.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

// 检查字符串是否包含指定子串
bool contains(std::string_view s, std::string_view substring) {
    return s.find(substring) != std::string_view::npos;
}

// 忽略大小写检查字符串是否包含指定子串
bool icontains(std::string_view s, std::string_view substring) {
    // 避免不必要的字符串拷贝，使用临时缓冲区
    if (s.empty() || substring.empty() || s.size() < substring.size()) {
        return substring.empty();
    }

    // 对于较小的字符串，直接转换为小写进行比较
    if (s.size() <= 1024 && substring.size() <= 1024) {
        std::string s_lower         = to_lower(s);
        std::string substring_lower = to_lower(substring);
        return contains(s_lower, substring_lower);
    }

    // 对于较大的字符串，使用滑动窗口方法避免完整转换
    auto is_equal_ignore_case = [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) ==
               std::tolower(static_cast<unsigned char>(b));
    };

    for (size_t i = 0; i <= s.size() - substring.size(); ++i) {
        bool found = true;
        for (size_t j = 0; j < substring.size(); ++j) {
            if (!is_equal_ignore_case(s[i + j], substring[j])) {
                found = false;
                break;
            }
        }
        if (found) {
            return true;
        }
    }

    return false;
}

/**
 * @brief 获取子字符串，支持负数索引
 */
std::string substr(std::string_view s, int start, int end) {
    const std::size_t length = s.length();
    
    // 空字符串直接返回
    if (length == 0) {
        return "";
    }
    
    // 调整起始位置（处理负数索引）
    int adjusted_start = start;
    if (adjusted_start < 0) {
        // 负数表示从末尾计数
        adjusted_start = static_cast<int>(length) + adjusted_start;
    }
    
    // 调整结束位置（处理负数索引）
    int adjusted_end = end;
    if (adjusted_end < 0) {
        // 负数表示从末尾计数
        adjusted_end = static_cast<int>(length) + adjusted_end;
    }
    
    // 边界检查
    if (adjusted_start < 0) {
        adjusted_start = 0;
    }
    if (adjusted_start >= static_cast<int>(length)) {
        return "";
    }
    if (adjusted_end >= static_cast<int>(length)) {
        adjusted_end = static_cast<int>(length) - 1;
    }
    if (adjusted_end < adjusted_start) {
        return "";
    }
    
    // 计算子字符串长度
    std::size_t substr_length = adjusted_end - adjusted_start + 1;
    
    // 返回子字符串
    return std::string(s.substr(adjusted_start, substr_length));
}

/**
 * @brief 获取子字符串，第二个参数指定长度而非结束位置
 */
std::string substring(std::string_view s, int start, std::size_t length) {
    const std::size_t str_length = s.length();
    
    // 空字符串直接返回
    if (str_length == 0) {
        return "";
    }
    
    // 调整起始位置（处理负数索引）
    int adjusted_start = start;
    if (adjusted_start < 0) {
        // 负数表示从末尾计数
        adjusted_start = static_cast<int>(str_length) + adjusted_start;
    }
    
    // 边界检查
    if (adjusted_start < 0) {
        // 如果调整后的起始位置仍为负数，表示超出了字符串的开头
        return "";
    }
    if (adjusted_start >= static_cast<int>(str_length)) {
        return "";
    }
    
    // 调整长度（如果超出字符串范围）
    if (length == std::string::npos || 
        adjusted_start + static_cast<std::size_t>(length) > str_length) {
        length = str_length - adjusted_start;
    }
    
    // 返回子字符串
    return std::string(s.substr(adjusted_start, length));
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

// 辅助函数：直接将格式化后的数值追加到结果字符串
template <typename T>
static void append_formatted_number(std::string& result, T val, const char* format) {
    // 对于数值类型，64字节的缓冲区通常足够了
    constexpr std::size_t BUFFER_SIZE = 64;
    char                  buffer[BUFFER_SIZE];
    int                   len = std::snprintf(buffer, BUFFER_SIZE, format, val);
    if (len > 0) {
        result.append(buffer, len);
    }
}

// 将variant值转换为字符串并追加到结果中
static void append_variant_to_string(std::string& result, const variant& value) {
    value.visit_with([&result](auto&& val) {
        using T = std::decay_t<decltype(val)>;

        if constexpr (std::is_same_v<T, void>) {
            result.append("null");
        } else if constexpr (std::is_same_v<T, int64_t>) {
            append_formatted_number(result, static_cast<long long>(val), "%lld");
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            append_formatted_number(result, static_cast<unsigned long long>(val), "%llu");
        } else if constexpr (std::is_same_v<T, double>) {
            append_formatted_number(result, val, "%g");
        } else if constexpr (std::is_same_v<T, bool>) {
            result.append(val ? "true" : "false");
        } else if constexpr (std::is_same_v<T, std::string>) {
            result.append(val);
        } else {
            result.append("[complex type]");
        }
    });
}

// 定义占位符语法的常量
static const std::string_view PLACEHOLDER_START = "${";
static const char             PLACEHOLDER_END   = '}';

// 从字典中获取键对应的值并追加到结果字符串
static void resolve_fmt_key(std::string& result, std::string_view key, const dict& args) {
    // 验证 key 格式
    if (!is_valid_fmt_key(key)) {
        mc::string::append(result, "${", key, '}');
        return;
    }

    auto it = args.find(key);
    if (it != args.end()) {
        // 直接转换值为字符串，不处理嵌套占位符
        append_variant_to_string(result, it->value);
    } else {
        // 键不存在，保留原始占位符
        mc::string::append(result, "${", key, '}');
    }
}

// 使用参数字典格式化字符串并追加到结果字符串中
void mc::string::format(std::string& result, std::string_view format_str, const dict& args) {
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
        resolve_fmt_key(result, key, args);

        // 移动到占位符之后
        pos = placeholder_end + 1;
    }
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
    mc::string::format(result, format_str, args);
    return result;
}

} // namespace mc