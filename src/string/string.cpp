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
 * @brief 实现基本字符串处理函数
 */

#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/string.h>
#include <mc/variant.h>

#include <cstring>
#include <functional>
#include <type_traits>

#include <stdarg.h>

namespace mc::string {

namespace detail {
void throw_bad_cast_error(const char* type)
{
    MC_THROW(mc::invalid_arg_exception, "can not cast string to type: ${type}", ("type", type));
}

void throw_overflow_error(const char* type, std::string_view s)
{
    MC_THROW(mc::overflow_exception, "can not cast string to type ${type}, value ${value} overflow",
             ("type", type)("value", s));
}

std::pair<int, std::string_view> detect_number_radix(std::string_view s)
{
    if (s.size() > 1 && s[0] == '0') {
        const char c = s[1];
        if (c == 'x' || c == 'X') {
            return {16, s.substr(2)};
        } else if (c == 'b' || c == 'B') {
            return {2, s.substr(2)};
        } else if (c >= '0' && c <= '7') {
            return {8, s.substr(1)};
        }
    }

    return {10, s};
}

std::string_view prepare_number_string(
    std::string_view s, int radix, char* buffer, std::size_t buffer_size) noexcept
{
    if (s.empty()) {
        return {};
    }

    // 根据进制检查字符串长度是否合法
    size_t max_len;
    switch (radix) {
    case 2:
        max_len = 64;
        break;
    case 8:
        max_len = 22;
        break;
    case 16:
        max_len = 16;
        break;
    default: // 10进制
        max_len = 20;
        break;
    }

    if (s.size() > max_len || (s.size() + 1) > buffer_size) {
        return {};
    }

    std::memcpy(buffer, s.data(), s.size());
    buffer[s.size()] = '\0';
    return std::string_view(buffer, s.size());
}

} // namespace detail

/**
 * @brief 尝试将字符串转换为布尔值
 * @param s 要转换的字符串
 * @param result 转换结果的引用
 * @return 是否转换成功
 */
bool try_to_bool(std::string_view s, bool& result)
{
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

bool to_bool_with_default(std::string_view s, bool default_value)
{
    if (s.empty()) {
        return false;
    }

    bool result;
    if (try_to_bool(s, result)) {
        return result;
    }

    return default_value;
}

bool to_bool(std::string_view s)
{
    if (s.empty()) {
        return false;
    }

    bool result;
    if (try_to_bool(s, result)) {
        return result;
    }

    detail::throw_bad_cast_error("bool");
}

// 忽略大小写比较两个字符串是否相等
bool iequals(std::string_view a, std::string_view b)
{
    if (a.size() != b.size()) {
        return false;
    }

    return std::equal(a.begin(), a.end(), b.begin(), [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) ==
               std::tolower(static_cast<unsigned char>(b));
    });
}

// 忽略大小写比较两个 C 风格字符串是否相等
bool iequals(const char* a, const char* b)
{
    if (!a || !b) {
        return a == b;
    }

    return iequals(std::string_view(a), std::string_view(b));
}

// 将字符串转换为小写
std::string to_lower(std::string_view s)
{
    std::string result(s);
    to_lower_inplace(result);
    return result;
}

// 将字符串转换为大写
std::string to_upper(std::string_view s)
{
    std::string result(s);
    to_upper_inplace(result);
    return result;
}

// 原地将字符串转换为小写
void to_lower_inplace(std::string& s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
}

// 原地将字符串转换为大写
void to_upper_inplace(std::string& s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return std::toupper(c);
    });
}

// 去除字符串两端的空白字符
std::string trim(std::string_view s)
{
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
void trim_inplace(std::string& s)
{
    ltrim_inplace(s);
    rtrim_inplace(s);
}

// 去除字符串左侧的空白字符
std::string ltrim(std::string_view s)
{
    auto start = s.begin();
    while (start != s.end() && std::isspace(static_cast<unsigned char>(*start))) {
        ++start;
    }
    return std::string(start, s.end());
}

// 原地去除字符串左侧的空白字符
void ltrim_inplace(std::string& s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// 去除字符串右侧的空白字符
std::string rtrim(std::string_view s)
{
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
void rtrim_inplace(std::string& s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](unsigned char ch) {
        return !std::isspace(ch);
    })
                .base(),
            s.end());
}

// 按指定分隔符分割字符串
std::vector<std::string> split(std::string_view s, char delim)
{
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
std::vector<std::string> split(std::string_view s, std::string_view delim)
{
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
std::string join(const std::vector<std::string>& v, std::string_view delim)
{
    if (v.empty()) {
        return {};
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
bool starts_with(std::string_view s, std::string_view prefix)
{
    if (prefix.empty()) {
        return true;
    }

    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

// 检查字符串是否以指定后缀结束
bool ends_with(std::string_view s, std::string_view suffix)
{
    return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}

std::string_view longest_common_prefix(std::string_view s1, std::string_view s2)
{
    size_t i = 0;
    while (i < s1.size() && i < s2.size() && s1[i] == s2[i]) {
        ++i;
    }
    return s1.substr(0, i);
}

// 替换字符串中的所有指定子串
std::string replace_all(std::string_view s, std::string_view from, std::string_view to)
{
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
void replace_all_inplace(std::string& s, std::string_view from, std::string_view to)
{
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
bool contains(std::string_view s, std::string_view substring)
{
    return s.find(substring) != std::string_view::npos;
}

// 忽略大小写检查字符串是否包含指定子串
bool icontains(std::string_view s, std::string_view substring)
{
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
std::string_view substr(std::string_view s, int start, int end)
{
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
    return s.substr(adjusted_start, substr_length);
}

/**
 * @brief 获取子字符串，第二个参数指定长度而非结束位置
 */
std::string_view substring(std::string_view s, int start, std::size_t length)
{
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
    return s.substr(adjusted_start, length);
}

/**
 * @brief 使用固定宽度格式化字符串，不足用空格填充，并追加到目标字符串
 */
void fixed_width_append(std::string& result, size_t width, std::string_view s, bool left_align)
{
    // 如果字符串长度已经超过或等于目标宽度，直接追加字符串
    if (s.length() >= width) {
        result.append(s.data(), width);
        return;
    }

    // 计算需要填充的空格数
    size_t padding = width - s.length();

    // 根据对齐方式添加内容
    if (left_align) {
        // 左对齐：先添加字符串，再添加空格
        result.append(s.data(), s.length());
        result.append(padding, ' ');
    } else {
        // 右对齐：先添加空格，再添加字符串
        result.append(padding, ' ');
        result.append(s.data(), s.length());
    }
}

bool is_valid_utf8(std::string_view s)
{
    const unsigned char* bytes  = reinterpret_cast<const unsigned char*>(s.data());
    size_t               length = s.size();

    for (size_t i = 0; i < length;) {
        // 检查单字节字符（0xxxxxxx）
        if (bytes[i] <= 0x7F) {
            i += 1;
            continue;
        }

        // 检查双字节字符（110xxxxx 10xxxxxx）
        else if ((bytes[i] & 0xE0) == 0xC0) {
            // 需要至少2个字节
            if (i + 1 >= length) {
                return false;
            }

            // 检查第二个字节是否符合 10xxxxxx 格式
            if ((bytes[i + 1] & 0xC0) != 0x80) {
                return false;
            }

            // 检查是否为过长编码
            unsigned int code_point = ((bytes[i] & 0x1F) << 6) | (bytes[i + 1] & 0x3F);
            if (code_point < 0x80) {
                return false; // 过长编码
            }

            i += 2;
        }

        // 检查三字节字符（1110xxxx 10xxxxxx 10xxxxxx）
        else if ((bytes[i] & 0xF0) == 0xE0) {
            // 需要至少3个字节
            if (i + 2 >= length) {
                return false;
            }

            // 检查第二、三个字节是否符合 10xxxxxx 格式
            if ((bytes[i + 1] & 0xC0) != 0x80 || (bytes[i + 2] & 0xC0) != 0x80) {
                return false;
            }

            // 检查是否为过长编码，以及是否为代理区间（U+D800-U+DFFF）
            unsigned int code_point =
                ((bytes[i] & 0x0F) << 12) | ((bytes[i + 1] & 0x3F) << 6) | (bytes[i + 2] & 0x3F);
            if (code_point < 0x800 || (code_point >= 0xD800 && code_point <= 0xDFFF)) {
                return false; // 过长编码或代理区间
            }

            i += 3;
        }

        // 检查四字节字符（11110xxx 10xxxxxx 10xxxxxx 10xxxxxx）
        else if ((bytes[i] & 0xF8) == 0xF0) {
            // 需要至少4个字节
            if (i + 3 >= length) {
                return false;
            }

            // 检查第二、三、四个字节是否符合 10xxxxxx 格式
            if ((bytes[i + 1] & 0xC0) != 0x80 || (bytes[i + 2] & 0xC0) != 0x80 ||
                (bytes[i + 3] & 0xC0) != 0x80) {
                return false;
            }

            // 检查是否为过长编码，以及是否超过 Unicode 范围（U+10FFFF）
            unsigned int code_point = ((bytes[i] & 0x07) << 18) | ((bytes[i + 1] & 0x3F) << 12) |
                                      ((bytes[i + 2] & 0x3F) << 6) | (bytes[i + 3] & 0x3F);
            if (code_point < 0x10000 || code_point > 0x10FFFF) {
                return false; // 过长编码或超出范围
            }

            i += 4;
        }

        // 无效的 UTF-8 起始字节
        else {
            return false;
        }
    }

    return true;
}

void to_string(std::string& result, double value)
{
    char   buffer[64];
    double intpart;
    if (modf(value, &intpart) == 0.0) {
        // 如果是整数值，不显示小数点和小数位
        snprintf(buffer, sizeof(buffer), "%.0f", value);
    } else {
        // 先使用默认的6位小数格式
        snprintf(buffer, sizeof(buffer), "%.6f", value);
        // 移除末尾多余的0和小数点
        char* end = buffer + strlen(buffer) - 1;
        while (end > buffer && *end == '0') {
            *end-- = '\0';
        }
        if (end > buffer && *end == '.') {
            *end = '\0';
        }
    }
    result.append(buffer);
}

std::string to_string(double value)
{
    std::string result;
    to_string(result, value);
    return result;
}

std::string to_string(bool value)
{
    std::string result;
    to_string(result, value);
    return result;
}

void to_string(std::string& result, bool value)
{
    result.append(value ? "true" : "false");
}

} // namespace mc::string