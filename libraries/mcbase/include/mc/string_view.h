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

#ifndef MC_STRING_VIEW_H
#define MC_STRING_VIEW_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>

namespace mc {

namespace detail {
// `mc::string_view` 内部存储：平凡可拷贝的指针+长度描述，不拥有底层字节。
struct string_view_storage {
    const char*   data = nullptr;
    std::uint64_t size = 0;
};

constexpr std::size_t cstring_length(const char* value) noexcept
{
    if (value == nullptr) {
        return 0U;
    }

    std::size_t length = 0;
    while (value[length] != '\0') {
        ++length;
    }
    return length;
}
} // namespace detail

static_assert(std::is_trivially_copyable_v<detail::string_view_storage>, "string_view_storage 必须保持平凡拷贝");

class string;

// `mc::string_view` 是 borrowed view 包装，不延长底层字符存储的生命周期。
class string_view {
public:
    using size_type = std::size_t;

    static constexpr size_type npos = static_cast<size_type>(-1);

    constexpr string_view() noexcept = default;

    constexpr string_view(const char* data, std::size_t size) noexcept : m_value{data, static_cast<std::uint64_t>(size)}
    {}

    // 字符串字面量优先匹配本模板，以便在 constexpr 上下文中使用（避免走下方 const char* 运行期求长）
    template <std::size_t N>
    constexpr string_view(const char (&literal)[N]) noexcept : string_view(literal, N - 1)
    {}

    constexpr string_view(const char* value) noexcept
        : string_view(value, detail::cstring_length(value))
    {}

    constexpr string_view(std::string_view value) noexcept : string_view(value.data(), value.size())
    {}

    string_view(const std::string& value) noexcept : string_view(value.data(), value.size())
    {}

    string_view(const string& value) noexcept;

    constexpr const char* data() const noexcept
    {
        return m_value.data != nullptr ? m_value.data : "";
    }

    constexpr std::size_t size() const noexcept
    {
        return static_cast<std::size_t>(m_value.size);
    }

    constexpr std::size_t length() const noexcept
    {
        return size();
    }

    constexpr bool empty() const noexcept
    {
        return m_value.size == 0U;
    }

    constexpr const char* begin() const noexcept
    {
        return data();
    }

    constexpr const char* end() const noexcept
    {
        return data() + size();
    }

    constexpr char operator[](std::size_t index) const noexcept
    {
        return data()[index];
    }

    constexpr char front() const noexcept
    {
        return data()[0];
    }

    constexpr char back() const noexcept
    {
        return data()[size() - 1U];
    }

    constexpr bool operator==(string_view rhs) const noexcept
    {
        if (size() != rhs.size()) {
            return false;
        }
        for (std::size_t i = 0; i < size(); ++i) {
            if ((*this)[i] != rhs[i]) {
                return false;
            }
        }
        return true;
    }

    constexpr bool operator!=(string_view rhs) const noexcept
    {
        return !(*this == rhs);
    }

    bool operator==(const char* rhs) const noexcept
    {
        return *this == string_view(rhs);
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
        return *this == string_view(rhs);
    }

    bool operator!=(const std::string& rhs) const noexcept
    {
        return !(*this == rhs);
    }

    bool operator==(const string& rhs) const noexcept;
    bool operator!=(const string& rhs) const noexcept;

    bool operator<(string_view rhs) const noexcept
    {
        return std::string_view(*this) < std::string_view(rhs);
    }

    bool operator>(string_view rhs) const noexcept
    {
        return std::string_view(*this) > std::string_view(rhs);
    }

    bool operator<=(string_view rhs) const noexcept
    {
        return !((*this) > rhs);
    }

    bool operator>=(string_view rhs) const noexcept
    {
        return !((*this) < rhs);
    }

    constexpr operator std::string_view() const noexcept
    {
        return std::string_view(data(), size());
    }

    /** @brief 隐式转换为 std::string，便于与现有代码兼容 */
    operator std::string() const
    {
        return std::string(data(), size());
    }

    int compare(string_view other) const noexcept
    {
        return std::string_view(*this).compare(std::string_view(other));
    }

    string_view substr(size_type pos, size_type n = npos) const
    {
        return string_view(std::string_view(*this).substr(pos, n));
    }

    size_type find(string_view v, size_type pos = 0) const noexcept
    {
        return std::string_view(*this).find(std::string_view(v), pos);
    }

    size_type find(char c, size_type pos = 0) const noexcept
    {
        return std::string_view(*this).find(c, pos);
    }

    size_type find_last_of(string_view v, size_type pos = npos) const noexcept
    {
        return std::string_view(*this).find_last_of(std::string_view(v), pos);
    }

    size_type find_last_of(char c, size_type pos = npos) const noexcept
    {
        return std::string_view(*this).find_last_of(c, pos);
    }

    size_type find_last_of(const char* s, size_type pos = npos) const noexcept
    {
        return std::string_view(*this).find_last_of(s, pos);
    }

    size_type find_first_not_of(char c, size_type pos = 0) const noexcept
    {
        return std::string_view(*this).find_first_not_of(c, pos);
    }

    size_type find_first_not_of(string_view v, size_type pos = 0) const noexcept
    {
        return std::string_view(*this).find_first_not_of(std::string_view(v), pos);
    }

private:
    detail::string_view_storage m_value{};
};

template <typename T, std::enable_if_t<std::is_same_v<std::decay_t<T>, string_view>, int> = 0>
inline constexpr bool operator==(const char* lhs, T rhs) noexcept
{
    return string_view(lhs) == rhs;
}

template <typename T, std::enable_if_t<std::is_same_v<std::decay_t<T>, string_view>, int> = 0>
inline constexpr bool operator!=(const char* lhs, T rhs) noexcept
{
    return !(lhs == rhs);
}

inline std::ostream& operator<<(std::ostream& os, string_view sv)
{
    return os.write(sv.data(), static_cast<std::streamsize>(sv.size()));
}

} // namespace mc

namespace std {

template <>
struct hash<mc::string_view> {
    std::size_t operator()(mc::string_view s) const noexcept
    {
        return hash<string_view>{}(static_cast<string_view>(s));
    }
};

} // namespace std

#endif // MC_STRING_VIEW_H
