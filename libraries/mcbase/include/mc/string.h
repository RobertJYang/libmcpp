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
 * @brief 定义了 mc 字符串公开入口与字符串工具函数
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
#include <utility>
#include <vector>

#include "securec.h"
#include <mc/common.h>
#include <mc/memory.h>
#include <mc/pretty_name.h>
#include <mc/quark_fwd.h>
#include <mc/string_view.h>

namespace mc {

class quark;

namespace detail {
struct string_mutable_impl;
} // namespace detail

class MC_API string {
public:
    using size_type                 = std::size_t;
    static constexpr size_type npos = static_cast<size_type>(-1);

    string() noexcept;
    string(mc::string_view view);
    string(const string& other) noexcept;
    string(string&& other) noexcept;
    ~string();

    string& operator=(const string& other) noexcept;
    string& operator=(string&& other) noexcept;

    inline string(const std::string& str) : string(mc::string_view(str))
    {}

    inline string(const char* value) : string(mc::string_view(value))
    {}

    string(quark q) noexcept;

    static string from_quark(quark q) noexcept;

    /** @brief 是否处于 quark 模式 */
    bool  is_quark() const noexcept;
    quark to_quark() const noexcept;

    std::size_t size() const noexcept;
    std::size_t capacity() const noexcept;
    bool        empty() const noexcept;
    char*       data() noexcept;
    const char* data() const noexcept;
    const char* c_str() const noexcept
    {
        return data();
    }
    std::size_t length() const noexcept
    {
        return size();
    }
    mc::string_view view() const noexcept;
    int             compare(mc::string_view other) const noexcept;
    int             compare(size_type pos, size_type count, mc::string_view other) const;
    int compare(size_type pos1, size_type count1, mc::string_view other, size_type pos2, size_type count2 = npos) const;
    int compare(size_type pos, size_type count, const char* str) const;
    int compare(size_type pos, size_type count, const char* str, size_type count2) const;
    std::size_t hash() const noexcept;
    void        swap(string& other) noexcept;

    inline int compare(size_type pos, size_type count, const string& str) const
    {
        return compare(pos, count, str.view());
    }

    inline int compare(size_type pos1, size_type count1, const string& str, size_type pos2,
                       size_type count2 = npos) const
    {
        return compare(pos1, count1, str.view(), pos2, count2);
    }

    inline int compare(size_type pos, size_type count, const std::string& str) const
    {
        return compare(pos, count, mc::string_view(str.data(), str.size()));
    }

    inline int compare(size_type pos1, size_type count1, const std::string& str, size_type pos2,
                       size_type count2 = npos) const
    {
        return compare(pos1, count1, mc::string_view(str.data(), str.size()), pos2, count2);
    }

    inline int compare(size_type pos, size_type count, std::string_view str) const
    {
        return compare(pos, count, mc::string_view(str.data(), str.size()));
    }

    inline int compare(size_type pos1, size_type count1, std::string_view str, size_type pos2,
                       size_type count2 = npos) const
    {
        return compare(pos1, count1, mc::string_view(str.data(), str.size()), pos2, count2);
    }

    /** @brief 隐式转换为 std::string */
    operator std::string() const noexcept
    {
        mc::string_view v = view();
        return std::string(v.data(), v.size());
    }

    operator std::string_view() const noexcept
    {
        mc::string_view v = view();
        return std::string_view(v.data(), v.size());
    }

    static string join(const std::vector<mc::string>& parts, mc::string_view delim)
    {
        string result;
        bool   first = true;
        for (const auto& part : parts) {
            if (!first) {
                result.append(delim);
            }
            result.append(part);
            first = false;
        }
        return result;
    }

    static string join(const std::vector<std::string>& parts, mc::string_view delim)
    {
        string result;
        bool   first = true;
        for (const auto& part : parts) {
            if (!first) {
                result.append(delim);
            }
            result.append(mc::string_view(part.data(), part.size()));
            first = false;
        }
        return result;
    }

    template <typename... Args>
    static string concat(Args&&... args)
    {
        string result;
        (result.append(std::forward<Args>(args)), ...);
        return result;
    }

    static string concat(const std::vector<mc::string>& parts)
    {
        string result;
        for (const auto& part : parts) {
            result.append(part);
        }
        return result;
    }

    static string concat(std::vector<mc::string>& parts)
    {
        return concat(static_cast<const std::vector<mc::string>&>(parts));
    }

    static string concat(const std::vector<std::string>& parts)
    {
        string result;
        for (const auto& part : parts) {
            result.append(part);
        }
        return result;
    }

    static string concat(std::vector<std::string>& parts)
    {
        return concat(static_cast<const std::vector<std::string>&>(parts));
    }

    static mc::string_view substring(mc::string_view s, int start, size_type count = npos)
    {
        const auto size = s.size();
        if (size == 0) {
            return {};
        }

        long long pos = start >= 0 ? static_cast<long long>(start) : static_cast<long long>(size) + start;
        if (pos < 0 || static_cast<size_type>(pos) >= size) {
            return {};
        }

        const auto begin = static_cast<size_type>(pos);
        const auto limit = count == npos || begin + count > size ? size - begin : count;
        return mc::string_view(s.data() + begin, limit);
    }

    static string to_lower(mc::string_view s)
    {
        string result(s);
        for (size_type i = 0; i < result.size(); ++i) {
            result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
        }
        return result;
    }

    static string to_upper(mc::string_view s)
    {
        string result(s);
        for (size_type i = 0; i < result.size(); ++i) {
            result[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[i])));
        }
        return result;
    }

    static string trim(mc::string_view s)
    {
        size_type begin = 0;
        size_type end   = s.size();
        while (begin < end && std::isspace(static_cast<unsigned char>(s[begin]))) {
            ++begin;
        }
        while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
            --end;
        }
        return string(mc::string_view(s.data() + begin, end - begin));
    }

    static void ltrim_inplace(string& s)
    {
        s.ltrim_inplace();
    }

    static void rtrim_inplace(string& s)
    {
        s.rtrim_inplace();
    }

    static void trim_inplace(string& s)
    {
        s.trim_inplace();
    }

    static void ltrim_inplace(std::string& s)
    {
        size_type begin = 0;
        while (begin < s.size() && std::isspace(static_cast<unsigned char>(s[begin]))) {
            ++begin;
        }
        s.erase(0, begin);
    }

    static void rtrim_inplace(std::string& s)
    {
        size_type end = s.size();
        while (end > 0 && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
            --end;
        }
        s.erase(end);
    }

    static void trim_inplace(std::string& s)
    {
        size_type begin = 0;
        size_type end   = s.size();
        while (begin < end && std::isspace(static_cast<unsigned char>(s[begin]))) {
            ++begin;
        }
        while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
            --end;
        }
        s = s.substr(begin, end - begin);
    }

    static bool starts_with(mc::string_view s, mc::string_view prefix) noexcept
    {
        if (prefix.size() > s.size()) {
            return false;
        }
        for (size_type i = 0; i < prefix.size(); ++i) {
            if (s[i] != prefix[i]) {
                return false;
            }
        }
        return true;
    }

    static bool contains(mc::string_view s, mc::string_view substring) noexcept
    {
        if (substring.empty()) {
            return true;
        }
        if (substring.size() > s.size()) {
            return false;
        }
        return s.find(substring) != mc::string_view::npos;
    }

    static string replace_all(mc::string_view s, mc::string_view from, mc::string_view to)
    {
        return string(s).replace_all(from, to);
    }

    bool operator==(mc::string_view rhs) const noexcept
    {
        return view() == rhs;
    }

    bool operator!=(mc::string_view rhs) const noexcept
    {
        return !(*this == rhs);
    }

    bool operator==(const char* rhs) const noexcept
    {
        if (rhs == nullptr) {
            return empty();
        }
        return view() == mc::string_view(rhs);
    }

    bool operator!=(const char* rhs) const noexcept
    {
        return !(*this == rhs);
    }

    bool operator==(std::string_view rhs) const noexcept
    {
        return std::string_view(*this) == rhs;
    }

    bool operator!=(std::string_view rhs) const noexcept
    {
        return !(*this == rhs);
    }

    bool operator==(const std::string& rhs) const noexcept
    {
        return std::string_view(*this) == std::string_view(rhs);
    }

    bool operator!=(const std::string& rhs) const noexcept
    {
        return !(*this == rhs);
    }

    string to_lower() const;
    string to_upper() const;
    string trim() const;
    string ltrim() const;
    string rtrim() const;
    string substr(size_type pos = 0, size_type count = npos) const;
    bool   starts_with(mc::string_view prefix) const;
    bool   starts_with(char prefix) const;
    bool   ends_with(mc::string_view suffix) const;
    bool   ends_with(char suffix) const;
    bool   contains(mc::string_view substring) const;
    bool   contains(char substring) const;
    bool   icontains(mc::string_view substring) const;
    string replace_all(mc::string_view from, mc::string_view to) const;

    void to_lower_inplace();
    void to_upper_inplace();
    void trim_inplace();
    void ltrim_inplace();
    void rtrim_inplace();
    void replace_all_inplace(mc::string_view from, mc::string_view to);

    static bool iequals(mc::string_view a, mc::string_view b);
    static bool iequals(const char* a, const char* b);

    template <typename T>
    static string to_string(T value);

    static bool try_to_bool(mc::string_view s, bool& result);
    static bool to_bool(mc::string_view s);
    static bool to_bool_with_default(mc::string_view s, bool default_value);

    template <typename T>
    static bool try_to_number(mc::string_view s, T& result, int radix = 0);

    template <typename T>
    static T to_number(mc::string_view s, int radix = 0);

    template <typename T>
    static T to_number_with_default(mc::string_view s, T default_value, int radix = 0);

    static bool is_valid_utf8(mc::string_view s);

    string(const char* first, const char* last);
    string(const char* data, std::size_t count);
    string(std::size_t count, char ch);

    const char* begin() const noexcept;
    const char* end() const noexcept;
    char*       begin() noexcept;
    char*       end() noexcept;

    void clear() noexcept;
    void push_back(char c);
    void pop_back() noexcept;
    void reserve(std::size_t new_capacity);
    void shrink_to_fit();
    void resize(size_type n, char ch = '\0');
    void insert(size_type pos, size_type count, char ch);
    void insert(size_type pos, mc::string_view str);
    void insert(size_type pos, const char* str, size_type count);

    inline void insert(size_type pos, const string& str)
    {
        insert(pos, str.view());
    }

    inline void insert(size_type pos, const std::string& str)
    {
        insert(pos, mc::string_view(str.data(), str.size()));
    }

    inline void insert(size_type pos, std::string_view str)
    {
        insert(pos, mc::string_view(str.data(), str.size()));
    }

    inline void insert(size_type pos, const char* str)
    {
        insert(pos, mc::string_view(str));
    }

    char&       operator[](size_type index);
    const char& operator[](size_type index) const;
    char&       at(size_type index);
    const char& at(size_type index) const;
    char&       front();
    const char& front() const;
    char&       back();
    const char& back() const;

    void append(const char* str);
    void append(const char* str, std::size_t count);
    void append(std::size_t count, char ch);
    void append(mc::string_view str);
    void append(mc::string_view str, size_type pos, size_type n = npos);
    void append(const string& str);
    void append(const string& str, size_type pos, size_type n = npos);

    /** @brief 追加 std::string */
    inline void append(const std::string& str)
    {
        append(mc::string_view(str.data(), str.size()));
    }

    inline void append(const std::string& str, size_type pos, size_type n = npos)
    {
        append(mc::string_view(str.data(), str.size()), pos, n);
    }

    inline void append(std::string_view str, size_type pos, size_type n = npos)
    {
        append(mc::string_view(str.data(), str.size()), pos, n);
    }

    void replace(size_type pos, size_type count, mc::string_view str);
    void replace(size_type pos, size_type count, mc::string_view str, size_type pos2, size_type count2 = npos);
    void replace(size_type pos, size_type count, const char* str, size_type count2);

    inline void replace(size_type pos, size_type count, const string& str)
    {
        replace(pos, count, str.view());
    }

    inline void replace(size_type pos, size_type count, const string& str, size_type pos2, size_type count2 = npos)
    {
        replace(pos, count, str.view(), pos2, count2);
    }

    inline void replace(size_type pos, size_type count, const std::string& str)
    {
        replace(pos, count, mc::string_view(str.data(), str.size()));
    }

    inline void replace(size_type pos, size_type count, const std::string& str, size_type pos2, size_type count2 = npos)
    {
        replace(pos, count, mc::string_view(str.data(), str.size()), pos2, count2);
    }

    inline void replace(size_type pos, size_type count, std::string_view str)
    {
        replace(pos, count, mc::string_view(str.data(), str.size()));
    }

    inline void replace(size_type pos, size_type count, std::string_view str, size_type pos2, size_type count2 = npos)
    {
        replace(pos, count, mc::string_view(str.data(), str.size()), pos2, count2);
    }

    inline void replace(size_type pos, size_type count, const char* str)
    {
        replace(pos, count, mc::string_view(str));
    }
    void      erase(size_type pos = 0, size_type n = npos);
    char*     erase(char* first, char* last);
    size_type find(mc::string_view v, size_type pos = 0) const noexcept;
    size_type find(const char* str, size_type pos, size_type count) const noexcept;
    size_type find(char c, size_type pos = 0) const noexcept;
    size_type rfind(mc::string_view v, size_type pos = npos) const noexcept;
    size_type rfind(char c, size_type pos = npos) const noexcept;
    size_type rfind(const char* str, size_type pos = npos) const noexcept;
    size_type rfind(const char* str, size_type pos, size_type count) const noexcept;

    inline size_type rfind(const string& str, size_type pos = npos) const noexcept
    {
        return rfind(str.view(), pos);
    }

    inline size_type rfind(const std::string& str, size_type pos = npos) const noexcept
    {
        return rfind(mc::string_view(str.data(), str.size()), pos);
    }

    inline size_type rfind(std::string_view str, size_type pos = npos) const noexcept
    {
        return rfind(mc::string_view(str.data(), str.size()), pos);
    }

    size_type find_first_of(mc::string_view v, size_type pos = 0) const noexcept;
    size_type find_first_of(char c, size_type pos = 0) const noexcept;
    size_type find_first_of(const char* str, size_type pos = 0) const noexcept;
    size_type find_first_of(const char* str, size_type pos, size_type count) const noexcept;

    inline size_type find_first_of(const string& str, size_type pos = 0) const noexcept
    {
        return find_first_of(str.view(), pos);
    }

    inline size_type find_first_of(const std::string& str, size_type pos = 0) const noexcept
    {
        return find_first_of(mc::string_view(str.data(), str.size()), pos);
    }

    inline size_type find_first_of(std::string_view str, size_type pos = 0) const noexcept
    {
        return find_first_of(mc::string_view(str.data(), str.size()), pos);
    }

    size_type find_last_of(mc::string_view v, size_type pos = npos) const noexcept;
    size_type find_last_of(char c, size_type pos = npos) const noexcept;
    size_type find_last_of(const char* str, size_type pos = npos) const noexcept;
    size_type find_last_of(const char* str, size_type pos, size_type count) const noexcept;

    inline size_type find_last_of(const string& str, size_type pos = npos) const noexcept
    {
        return find_last_of(str.view(), pos);
    }

    inline size_type find_last_of(const std::string& str, size_type pos = npos) const noexcept
    {
        return find_last_of(mc::string_view(str.data(), str.size()), pos);
    }

    inline size_type find_last_of(std::string_view str, size_type pos = npos) const noexcept
    {
        return find_last_of(mc::string_view(str.data(), str.size()), pos);
    }

    size_type find_first_not_of(mc::string_view v, size_type pos = 0) const noexcept;
    size_type find_first_not_of(char c, size_type pos = 0) const noexcept;
    size_type find_first_not_of(const char* str, size_type pos = 0) const noexcept;
    size_type find_first_not_of(const char* str, size_type pos, size_type count) const noexcept;

    inline size_type find_first_not_of(const string& str, size_type pos = 0) const noexcept
    {
        return find_first_not_of(str.view(), pos);
    }

    inline size_type find_first_not_of(const std::string& str, size_type pos = 0) const noexcept
    {
        return find_first_not_of(mc::string_view(str.data(), str.size()), pos);
    }

    inline size_type find_first_not_of(std::string_view str, size_type pos = 0) const noexcept
    {
        return find_first_not_of(mc::string_view(str.data(), str.size()), pos);
    }

    size_type find_last_not_of(mc::string_view v, size_type pos = npos) const noexcept;
    size_type find_last_not_of(char c, size_type pos = npos) const noexcept;
    size_type find_last_not_of(const char* str, size_type pos = npos) const noexcept;
    size_type find_last_not_of(const char* str, size_type pos, size_type count) const noexcept;

    inline size_type find_last_not_of(const string& str, size_type pos = npos) const noexcept
    {
        return find_last_not_of(str.view(), pos);
    }

    inline size_type find_last_not_of(const std::string& str, size_type pos = npos) const noexcept
    {
        return find_last_not_of(mc::string_view(str.data(), str.size()), pos);
    }

    inline size_type find_last_not_of(std::string_view str, size_type pos = npos) const noexcept
    {
        return find_last_not_of(mc::string_view(str.data(), str.size()), pos);
    }

    string& operator+=(char c);
    string& operator+=(mc::string_view v);
    string& operator+=(const string& v);
    string& operator+=(const char* str);

    /** @brief 追加 std::string */
    inline string& operator+=(const std::string& str)
    {
        *this += mc::string_view(str.data(), str.size());
        return *this;
    }

private:
    friend struct detail::string_mutable_impl;

    struct impl;
    std::unique_ptr<impl> m_impl;
};

inline std::string to_std_string(const string& s)
{
    mc::string_view v = s.view();
    return std::string(v.data(), v.size());
}

MC_API bool operator==(const string& lhs, const string& rhs) noexcept;
MC_API bool operator!=(const string& lhs, const string& rhs) noexcept;
MC_API bool operator<(const string& lhs, const string& rhs) noexcept;
MC_API bool operator<=(const string& lhs, const string& rhs) noexcept;
MC_API bool operator>(const string& lhs, const string& rhs) noexcept;
MC_API bool operator>=(const string& lhs, const string& rhs) noexcept;

MC_API string operator+(const string& lhs, const string& rhs);
MC_API string operator+(const string& lhs, mc::string_view rhs);
MC_API string operator+(mc::string_view lhs, const string& rhs);
inline string operator+(mc::string_view lhs, mc::string_view rhs)
{
    return string(lhs) + rhs;
}
inline string operator+(const char* lhs, mc::string_view rhs)
{
    return string(lhs) + rhs;
}

inline string operator+(mc::string_view lhs, const char* rhs)
{
    return string(lhs) + mc::string_view(rhs);
}

MC_API string operator+(const string& lhs, const char* rhs);
MC_API string operator+(const char* lhs, const string& rhs);

inline string operator+(const std::string& lhs, mc::string_view rhs)
{
    return mc::string_view(lhs.data(), lhs.size()) + rhs;
}

inline string operator+(const string& lhs, const std::string& rhs)
{
    return lhs + mc::string_view(rhs.data(), rhs.size());
}

inline string operator+(const std::string& lhs, const string& rhs)
{
    return mc::string_view(lhs.data(), lhs.size()) + rhs;
}

inline string operator+(mc::string_view lhs, const std::string& rhs)
{
    return lhs + mc::string_view(rhs.data(), rhs.size());
}

inline string operator+(std::string_view lhs, mc::string_view rhs)
{
    return mc::string_view(lhs.data(), lhs.size()) + rhs;
}

inline string operator+(std::string_view lhs, const string& rhs)
{
    return mc::string_view(lhs.data(), lhs.size()) + rhs;
}

inline string operator+(std::string_view lhs, const std::string& rhs)
{
    return mc::string_view(lhs.data(), lhs.size()) + mc::string_view(rhs.data(), rhs.size());
}

template <typename T, std::enable_if_t<std::is_same_v<std::decay_t<T>, string>, int> = 0>
inline bool operator==(const char* lhs, const T& rhs) noexcept
{
    return mc::string_view(lhs) == rhs.view();
}

template <typename T, std::enable_if_t<std::is_same_v<std::decay_t<T>, string>, int> = 0>
inline bool operator!=(const char* lhs, const T& rhs) noexcept
{
    return !(lhs == rhs);
}

inline std::ostream& operator<<(std::ostream& os, const string& s)
{
    return os.write(s.data(), static_cast<std::streamsize>(s.size()));
}

inline void swap(string& lhs, string& rhs) noexcept
{
    lhs.swap(rhs);
}

inline string_view::string_view(const string& value) noexcept : string_view(value.view())
{}

inline bool string_view::operator==(const string& rhs) const noexcept
{
    return *this == rhs.view();
}

inline bool string_view::operator!=(const string& rhs) const noexcept
{
    return !(*this == rhs);
}

} // namespace mc

namespace std {

template <>
struct hash<mc::string> {
    size_t operator()(const mc::string& value) const noexcept
    {
        return value.hash();
    }
};

} // namespace std

#include <mc/string_utils.h>

#endif // MC_STRING_H