/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_STRING_UTILS_H
#define MC_STRING_UTILS_H

#ifndef MC_STRING_H
#include <mc/string.h>
#endif

#include <system_error>

namespace mc::strings {

namespace detail {
template <typename StringLike, typename T>
void append_formatted_number(StringLike& result, T val, const char* format)
{
    constexpr std::size_t BUFFER_SIZE = 64;
    char                  buffer[BUFFER_SIZE];
    int                   len = snprintf_s(buffer, BUFFER_SIZE, BUFFER_SIZE, format, val);
    if (len > 0) {
        result.append(buffer, len);
    }
}

struct to_chars_result {
    char*     ptr;
    std::errc ec;
};

[[noreturn]] MC_API void throw_bad_cast_error(mc::string_view type);
[[noreturn]] MC_API void throw_overflow_error(mc::string_view type, mc::string_view s);

MC_API std::pair<int, mc::string_view> detect_number_radix(mc::string_view s);
MC_API to_chars_result to_chars_double(char* first, char* last, double value) noexcept;

/**
 * @brief 准备一个以尾0结尾的字符串，用于安全地转换为数字
 * @param s 输入字符串
 * @param radix 进制
 * @param buffer 栈上分配的缓冲区
 * @return 可以安全用于 std::strtoX 函数的字符串指针，如果字符串无效则返回 nullptr
 */
MC_API mc::string_view prepare_number_string(mc::string_view s, int radix, char* buffer,
                                             std::size_t buffer_size) noexcept;
} // namespace detail

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
 */
MC_API mc::string_view substr(mc::string_view s, int start, int end = -1);

/**
 * @brief 获取子字符串，第二个参数指定长度而非结束位置
 * @param s 源字符串
 * @param start 起始位置（索引从0开始）
 *        - 正数表示从字符串开头计数的位置（0表示第一个字符）
 *        - 负数表示从字符串末尾计数的位置（-1表示最后一个字符）
 * @param length 要提取的字符数量，默认为 mc::string_view::npos 表示提取到字符串末尾
 * @return 提取的子字符串
 */
MC_API mc::string_view substring(mc::string_view s, int start, std::size_t length = mc::string_view::npos);

template <typename T>
auto to_string(mc::string& result, T value)
    -> std::enable_if_t<std::is_integral_v<T> && std::is_signed_v<T> && !std::is_same_v<std::decay_t<T>, bool>, void>
{
    detail::append_formatted_number(result, static_cast<long long>(value), "%lld");
}

template <typename T>
auto to_string(T value)
    -> std::enable_if_t<std::is_integral_v<T> && std::is_signed_v<T> && !std::is_same_v<std::decay_t<T>, bool>,
                        mc::string>
{
    mc::string result;
    to_string(result, value);
    return result;
}

template <typename T>
auto to_string(mc::string& result, T value)
    -> std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T> && !std::is_same_v<std::decay_t<T>, bool>, void>
{
    detail::append_formatted_number(result, static_cast<unsigned long long>(value), "%llu");
}

template <typename T>
auto to_string(T value)
    -> std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T> && !std::is_same_v<std::decay_t<T>, bool>,
                        mc::string>
{
    mc::string result;
    to_string(result, value);
    return result;
}

template <typename T>
auto to_string(std::string& result, T value)
    -> std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<std::decay_t<T>, bool>, void>
{
    const mc::string formatted = to_string(value);
    result.append(formatted.data(), formatted.size());
}

MC_API mc::string to_string(double value);
MC_API void       to_string(mc::string& result, double value);

MC_API mc::string to_string(bool value);
MC_API void       to_string(mc::string& result, bool value);

inline void to_string(std::string& result, double value)
{
    const mc::string t = to_string(value);
    result.append(t.data(), t.size());
}

inline void to_string(std::string& result, bool value)
{
    result.append(value ? "true" : "false");
}

template <typename T>
void append(mc::string& result, T&& value)
{
    if constexpr (std::is_same_v<std::decay_t<T>, std::string> || std::is_same_v<std::decay_t<T>, std::string_view>) {
        result.append(mc::string_view(value.data(), value.size()));
    } else if constexpr (std::is_same_v<std::decay_t<T>, mc::string_view>) {
        result.append(value);
    } else if constexpr (std::is_same_v<std::decay_t<T>, mc::string>) {
        result.append(value);
    } else if constexpr (std::is_same_v<std::decay_t<T>, std::vector<mc::string>>) {
        for (const auto& s : value) {
            result.append(s);
        }
    } else if constexpr (std::is_same_v<std::decay_t<T>, std::vector<std::string>>) {
        for (const auto& s : value) {
            result.append(mc::string_view(s.data(), s.size()));
        }
    } else if constexpr (std::is_same_v<std::decay_t<T>, const char*>) {
        const char* p = value;
        if (p != nullptr) {
            result.append(p);
        }
    } else if constexpr (std::is_same_v<std::decay_t<T>, char>) {
        result.push_back(value);
    } else if constexpr (std::is_arithmetic_v<std::decay_t<T>>) {
        result.append(to_string(value));
    } else {
        std::ostringstream oss;
        oss << value;
        const auto serialized = oss.str();
        result.append(mc::string_view(serialized.data(), serialized.size()));
    }
}

template <typename T, typename... Args>
void append(mc::string& result, T&& value, Args&&... args)
{
    append(result, std::forward<T>(value));
    append(result, std::forward<Args>(args)...);
}

template <typename T>
void append(std::string& result, T&& value)
{
    mc::string tmp;
    append(tmp, std::forward<T>(value));
    result.append(tmp.data(), tmp.size());
}

template <typename T, typename... Args>
void append(std::string& result, T&& value, Args&&... args)
{
    mc::string tmp;
    append(tmp, std::forward<T>(value), std::forward<Args>(args)...);
    result.append(tmp.data(), tmp.size());
}

MC_API mc::string join(const std::vector<mc::string>& v, mc::string_view delim);

inline mc::string join(const std::vector<std::string>& v, mc::string_view delim)
{
    std::vector<mc::string> tmp;
    tmp.reserve(v.size());
    for (const auto& s : v) {
        tmp.emplace_back(mc::string_view(s.data(), s.size()));
    }
    return join(tmp, delim);
}

template <typename... Args>
mc::string join(mc::string_view delim, Args&&... args)
{
    mc::string result;
    if constexpr (sizeof...(args) == 1) {
        append(result, args...);
    } else if constexpr (sizeof...(args) > 1) {
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

inline mc::string concat(const std::vector<std::string>& v)
{
    mc::string result;
    result.reserve(std::accumulate(v.begin(), v.end(), size_t{0}, [](size_t sum, const auto& s) {
        return sum + s.size();
    }));
    for (const auto& s : v) {
        result.append(mc::string_view(s.data(), s.size()));
    }
    return result;
}

inline mc::string concat(const std::vector<mc::string>& v)
{
    mc::string result;
    for (const auto& s : v) {
        result.append(s);
    }
    return result;
}

template <typename T, typename... Args,
          typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, std::vector<std::string>> &&
                                      !std::is_same_v<std::decay_t<T>, std::vector<mc::string>>>>
mc::string concat(T&& first, Args&&... args)
{
    mc::string result;
    append(result, std::forward<T>(first));
    if constexpr (sizeof...(args) > 0) {
        append(result, std::forward<Args>(args)...);
    }
    return result;
}

MC_API bool iequals(mc::string_view a, mc::string_view b);
MC_API bool iequals(const char* a, const char* b);

MC_API mc::string to_lower(mc::string_view s);
MC_API mc::string to_upper(mc::string_view s);

inline void to_lower_inplace(std::string& s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
}

inline void to_lower_inplace(mc::string& s)
{
    s.to_lower_inplace();
}

inline void to_upper_inplace(std::string& s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
}

inline void to_upper_inplace(mc::string& s)
{
    s.to_upper_inplace();
}

MC_API mc::string trim(mc::string_view s);
MC_API mc::string ltrim(mc::string_view s);

inline void ltrim_inplace(std::string& s)
{
    const auto it = std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    });
    s.erase(s.begin(), it);
}

inline void ltrim_inplace(mc::string& s)
{
    s.ltrim_inplace();
}

MC_API mc::string rtrim(mc::string_view s);

inline void rtrim_inplace(std::string& s)
{
    const auto it = std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base();
    s.erase(it, s.end());
}

inline void rtrim_inplace(mc::string& s)
{
    s.rtrim_inplace();
}

inline void trim_inplace(std::string& s)
{
    ltrim_inplace(s);
    rtrim_inplace(s);
}

inline void trim_inplace(mc::string& s)
{
    s.trim_inplace();
}

MC_API std::vector<mc::string> split(mc::string_view s, char delim);
MC_API std::vector<mc::string> split(mc::string_view s, mc::string_view delim);

MC_API void fixed_width_append(mc::string& result, size_t width, mc::string_view s, bool left_align = true);

inline void fixed_width_append(std::string& result, size_t width, mc::string_view s, bool left_align = true)
{
    if (s.length() >= width) {
        result.append(s.data(), width);
        return;
    }
    const std::size_t padding = width - s.length();
    if (left_align) {
        result.append(s.data(), s.length());
        result.append(padding, ' ');
    } else {
        result.append(padding, ' ');
        result.append(s.data(), s.length());
    }
}

MC_API bool starts_with(mc::string_view s, mc::string_view prefix);
MC_API bool ends_with(mc::string_view s, mc::string_view suffix);
MC_API mc::string_view longest_common_prefix(mc::string_view s1, mc::string_view s2);
MC_API mc::string replace_all(mc::string_view s, mc::string_view from, mc::string_view to);

inline void replace_all_inplace(std::string& s, mc::string_view from, mc::string_view to)
{
    if (from.empty()) {
        return;
    }
    std::size_t            start = 0;
    const std::string_view f(from.data(), from.size());
    const std::string_view t(to.data(), to.size());
    while ((start = s.find(f, start)) != std::string::npos) {
        s.replace(start, from.size(), t);
        start += t.size();
    }
}

inline void replace_all_inplace(mc::string& s, mc::string_view from, mc::string_view to)
{
    s.replace_all_inplace(from, to);
}

MC_API bool contains(mc::string_view s, mc::string_view substring);
MC_API bool icontains(mc::string_view s, mc::string_view substring);

inline bool icontains(const char* haystack, const char* needle)
{
    return icontains(mc::string_view(haystack != nullptr ? haystack : ""),
                     mc::string_view(needle != nullptr ? needle : ""));
}

inline bool icontains(const mc::string& haystack, const char* needle)
{
    return icontains(haystack.view(), mc::string_view(needle != nullptr ? needle : ""));
}

class MC_API split_iterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type        = mc::string_view;
    using difference_type   = std::ptrdiff_t;

    constexpr split_iterator() noexcept
    {}

    constexpr split_iterator(mc::string_view str, mc::string_view delims = " ") noexcept : m_str(str), m_delims(delims)
    {
        find_next();
    }

    constexpr value_type operator*() const noexcept
    {
        return mc::string_view(m_str.data() + m_pos, m_end - m_pos);
    }

    constexpr value_type operator->() const noexcept
    {
        return operator*();
    }

    constexpr split_iterator& operator++() noexcept
    {
        find_next();
        return *this;
    }

    constexpr split_iterator operator++(int) noexcept
    {
        auto tmp = *this;
        ++(*this);
        return tmp;
    }

    constexpr bool operator==(const split_iterator& other) const noexcept
    {
        if (is_end() && other.is_end()) {
            return true;
        } else if (is_end() || other.is_end()) {
            return false;
        }
        return m_str.data() == other.m_str.data() && m_pos == other.m_pos;
    }

    constexpr bool operator!=(const split_iterator& other) const noexcept
    {
        return !(*this == other);
    }

    constexpr bool is_end() const noexcept
    {
        return m_pos >= m_str.size();
    }

    static constexpr split_iterator end() noexcept
    {
        return split_iterator();
    }

    constexpr std::size_t current_pos() const noexcept
    {
        return m_pos;
    }

    constexpr std::size_t end_pos() const noexcept
    {
        return m_end;
    }

    constexpr mc::string_view tail() const noexcept
    {
        if (is_end()) {
            return {};
        }

        auto pos = skip_delims(m_str, m_pos, m_delims);
        return m_str.substr(pos);
    }

    constexpr mc::string_view head() const noexcept
    {
        return m_str.substr(0, m_pos);
    }

private:
    static constexpr std::size_t skip_delims(mc::string_view str, std::size_t pos, mc::string_view delims)
    {
        while (pos < str.size() && delims.find(str[pos]) != mc::string_view::npos) {
            ++pos;
        }
        return pos;
    }

    static constexpr std::size_t find_next_delim(mc::string_view str, std::size_t pos, mc::string_view delims)
    {
        while (pos < str.size() && delims.find(str[pos]) == mc::string_view::npos) {
            ++pos;
        }
        return pos;
    }

    constexpr void find_next() noexcept
    {
        m_pos = m_end;
        m_pos = skip_delims(m_str, m_pos, m_delims);
        m_end = find_next_delim(m_str, m_pos, m_delims);
    }

    mc::string_view m_str;
    mc::string_view m_delims;
    std::size_t     m_pos{0};
    std::size_t     m_end{0};
};

MC_API bool try_to_bool(mc::string_view s, bool& result);
MC_API bool to_bool_with_default(mc::string_view s, bool default_value);
MC_API bool to_bool(mc::string_view s);

/**
 * @brief 尝试将字符串转换为数字
 * @param s 要转换的字符串
 * @param result 转换结果的引用
 * @return 是否转换成功
 * 注意：这个函数底层使用了 std::strtod 和 std::strtol 等的 C 风格版本，必须确保字符串以尾0结尾。
 */
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
bool try_to_number_unsafe(mc::string_view s, T& result, int radix = 0)
{
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

template <typename T, typename S,
          typename = std::enable_if_t<std::is_arithmetic_v<T> && std::is_same_v<std::decay_t<S>, std::string_view>>>
inline bool try_to_number_unsafe(S s, T& result, int radix = 0)
{
    return try_to_number_unsafe<T>(mc::string_view(s.data(), s.size()), result, radix);
}

#ifndef MC_NUMBER_STRING_BUFFER_SIZE
#define MC_NUMBER_STRING_BUFFER_SIZE 128
#endif

template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
bool try_to_number_safe(mc::string_view s, T& result, int radix = 0)
{
    char buffer[MC_NUMBER_STRING_BUFFER_SIZE];
    auto str_data = detail::prepare_number_string(s, radix, buffer, sizeof(buffer));
    if (str_data.empty()) {
        return false;
    }
    return try_to_number_unsafe(str_data, result, radix);
}

template <typename T, typename S,
          typename = std::enable_if_t<std::is_arithmetic_v<T> && std::is_same_v<std::decay_t<S>, std::string_view>>>
inline bool try_to_number_safe(S s, T& result, int radix = 0)
{
    return try_to_number_safe<T>(mc::string_view(s.data(), s.size()), result, radix);
}

template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
bool try_to_number(mc::string_view s, T& result, int radix = 0)
{
    return try_to_number_safe<T>(s, result, radix);
}

template <typename T, typename S,
          typename = std::enable_if_t<std::is_arithmetic_v<T> && std::is_same_v<std::decay_t<S>, std::string_view>>>
inline bool try_to_number(S s, T& result, int radix = 0)
{
    return try_to_number<T>(mc::string_view(s.data(), s.size()), result, radix);
}

template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
bool try_to_number(const char* s, T& result, int radix = 0)
{
    return try_to_number_safe<T>(mc::string_view(s), result, radix);
}

template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
T to_number(mc::string_view s, int radix = 0)
{
    T result{};
    if (try_to_number<T>(s, result, radix)) {
        return result;
    }

    if (errno == ERANGE) {
        detail::throw_overflow_error(mc::pretty_name<T>(), s);
    }

    detail::throw_bad_cast_error(mc::pretty_name<T>());
}

template <typename T, typename S,
          typename = std::enable_if_t<std::is_arithmetic_v<T> && std::is_same_v<std::decay_t<S>, std::string_view>>>
inline T to_number(S s, int radix = 0)
{
    return to_number<T>(mc::string_view(s.data(), s.size()), radix);
}

template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
T to_number(const char* s, int radix = 0)
{
    return to_number<T>(mc::string_view(s), radix);
}

template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
T to_number_with_default(mc::string_view s, T default_value, int radix = 0)
{
    T result{};
    if (try_to_number<T>(s, result, radix)) {
        return result;
    }

    return default_value;
}

template <typename T, typename S,
          typename = std::enable_if_t<std::is_arithmetic_v<T> && std::is_same_v<std::decay_t<S>, std::string_view>>>
inline T to_number_with_default(S s, T default_value, int radix = 0)
{
    return to_number_with_default<T>(mc::string_view(s.data(), s.size()), default_value, radix);
}

template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
T to_number_with_default(const char* s, T default_value, int radix = 0)
{
    return to_number_with_default<T>(mc::string_view(s), default_value, radix);
}

MC_API bool is_valid_utf8(mc::string_view s);

template <typename S, typename = std::enable_if_t<std::is_same_v<std::decay_t<S>, std::string_view>>>
inline bool is_valid_utf8(S s)
{
    return is_valid_utf8(mc::string_view(s.data(), s.size()));
}

} // namespace mc::strings

namespace mc {

template <typename T>
inline string string::to_string(T value)
{
    return string(mc::strings::to_string(value));
}

inline string string::to_lower() const
{
    return string(mc::strings::to_lower(view()));
}

inline string string::to_upper() const
{
    return string(mc::strings::to_upper(view()));
}

inline string string::trim() const
{
    return string(mc::strings::trim(view()));
}

inline string string::ltrim() const
{
    return string(mc::strings::ltrim(view()));
}

inline string string::rtrim() const
{
    return string(mc::strings::rtrim(view()));
}

inline bool string::starts_with(mc::string_view prefix) const
{
    return mc::strings::starts_with(view(), prefix);
}

inline bool string::starts_with(char prefix) const
{
    return view().starts_with(prefix);
}

inline bool string::ends_with(mc::string_view suffix) const
{
    return mc::strings::ends_with(view(), suffix);
}

inline bool string::ends_with(char suffix) const
{
    return view().ends_with(suffix);
}

inline bool string::contains(mc::string_view needle) const
{
    return mc::strings::contains(view(), needle);
}

inline bool string::contains(char needle) const
{
    return view().contains(needle);
}

inline bool string::icontains(mc::string_view needle) const
{
    return mc::strings::icontains(view(), needle);
}

inline string string::replace_all(mc::string_view from, mc::string_view to) const
{
    return string(mc::strings::replace_all(view(), from, to));
}

inline bool string::iequals(mc::string_view a, mc::string_view b)
{
    return mc::strings::iequals(a, b);
}

inline bool string::iequals(const char* a, const char* b)
{
    return mc::strings::iequals(a, b);
}

inline bool string::try_to_bool(mc::string_view s, bool& result)
{
    return mc::strings::try_to_bool(s, result);
}

inline bool string::to_bool(mc::string_view s)
{
    return mc::strings::to_bool(s);
}

inline bool string::to_bool_with_default(mc::string_view s, bool default_value)
{
    return mc::strings::to_bool_with_default(s, default_value);
}

template <typename T>
inline bool string::try_to_number(mc::string_view s, T& result, int radix)
{
    return mc::strings::try_to_number<T>(s, result, radix);
}

template <typename T>
inline T string::to_number(mc::string_view s, int radix)
{
    return mc::strings::to_number<T>(s, radix);
}

template <typename T>
inline T string::to_number_with_default(mc::string_view s, T default_value, int radix)
{
    return mc::strings::to_number_with_default<T>(s, default_value, radix);
}

inline bool string::is_valid_utf8(mc::string_view s)
{
    return mc::strings::is_valid_utf8(s);
}

using mc::strings::to_bool;
using mc::strings::to_bool_with_default;
using mc::strings::to_number;
using mc::strings::to_number_with_default;
using mc::strings::to_string;
using mc::strings::try_to_number;

} // namespace mc

#endif // MC_STRING_UTILS_H
