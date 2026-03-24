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

#include <mc/exception.h>
#include <mc/memory.h>
#include <mc/string.h>

#include "securec.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstring>
#include <functional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <stdarg.h>

namespace mc::detail {

// mc::string 内部字节缓冲；不依赖标准库字符串主表示，避免与「仅 mc 类型跨边界」目标冲突。
struct string_bytes {
    std::vector<char> m_buffer;

    string_bytes()
    {
        ensure_initialized();
    }

    explicit string_bytes(mc::string_view v)
    {
        assign(v);
    }

    explicit string_bytes(std::size_t n, char ch)
    {
        m_buffer.assign(n + 1, ch);
        m_buffer[n] = '\0';
    }

    void ensure_initialized()
    {
        if (m_buffer.empty()) {
            m_buffer.push_back('\0');
        }
    }

    void assign(mc::string_view v)
    {
        m_buffer.assign(v.data(), v.data() + static_cast<std::ptrdiff_t>(v.size()));
        m_buffer.push_back('\0');
    }

    bool empty() const noexcept
    {
        return size() == 0U;
    }

    std::size_t size() const noexcept
    {
        return m_buffer.empty() ? 0U : (m_buffer.size() - 1U);
    }

    std::size_t capacity() const noexcept
    {
        return m_buffer.capacity() == 0U ? 0U : (m_buffer.capacity() - 1U);
    }

    mc::string_view view() const noexcept
    {
        return mc::string_view(data(), size());
    }

    const char* data() const noexcept
    {
        return m_buffer.empty() ? "" : m_buffer.data();
    }

    char* data_mutable() noexcept
    {
        ensure_initialized();
        return m_buffer.data();
    }

    bool overlaps(mc::string_view value) const noexcept
    {
        if (value.empty()) {
            return false;
        }
        const char* begin_ptr = data();
        const char* end_ptr   = begin_ptr + static_cast<std::ptrdiff_t>(size());
        const char* src_begin = value.data();
        const char* src_end   = src_begin + static_cast<std::ptrdiff_t>(value.size());
        return src_begin < end_ptr && src_end > begin_ptr;
    }

    void clear() noexcept
    {
        ensure_initialized();
        m_buffer.resize(1);
        m_buffer[0] = '\0';
    }

    void reserve(std::size_t n)
    {
        ensure_initialized();
        if (capacity() >= n) {
            return;
        }
        m_buffer.reserve(n + 1U);
    }

    void shrink_to_fit()
    {
        ensure_initialized();
        m_buffer.shrink_to_fit();
        ensure_initialized();
        m_buffer[size()] = '\0';
    }

    void push_back(char c)
    {
        ensure_initialized();
        m_buffer.insert(m_buffer.end() - 1, c);
    }

    void pop_back()
    {
        if (empty()) {
            return;
        }
        m_buffer.pop_back();
        m_buffer.back() = '\0';
    }

    void resize(std::size_t n, char ch)
    {
        ensure_initialized();
        m_buffer.resize(n + 1U, ch);
        m_buffer[n] = '\0';
    }

    void insert(std::size_t pos, std::size_t count, char ch)
    {
        m_buffer.insert(m_buffer.begin() + static_cast<std::ptrdiff_t>(pos), count, ch);
    }

    char& operator[](std::size_t i) noexcept
    {
        return m_buffer[i];
    }

    const char& operator[](std::size_t i) const noexcept
    {
        return m_buffer[i];
    }

    char& at(std::size_t i)
    {
        if (i >= size()) {
            throw std::out_of_range("string_bytes::at");
        }
        return m_buffer[i];
    }

    char& front()
    {
        return m_buffer.front();
    }

    char& back()
    {
        return m_buffer[size() - 1U];
    }

    using iterator         = std::vector<char>::iterator;
    using const_iterator   = std::vector<char>::const_iterator;
    using reverse_iterator = std::reverse_iterator<iterator>;

    iterator begin() noexcept
    {
        return m_buffer.begin();
    }

    iterator end() noexcept
    {
        return m_buffer.begin() + static_cast<std::ptrdiff_t>(size());
    }

    reverse_iterator rbegin() noexcept
    {
        return reverse_iterator(end());
    }

    reverse_iterator rend() noexcept
    {
        return reverse_iterator(begin());
    }

    void append(const char* str)
    {
        if (str == nullptr) {
            return;
        }
        append(str, std::char_traits<char>::length(str));
    }

    void append(const char* p, std::size_t n)
    {
        if (n == 0U || p == nullptr) {
            return;
        }
        m_buffer.insert(m_buffer.end() - 1, p, p + static_cast<std::ptrdiff_t>(n));
    }

    void append(std::size_t n, char ch)
    {
        m_buffer.insert(m_buffer.end() - 1, n, ch);
    }

    void erase(std::size_t pos, std::size_t count)
    {
        if (count == 0U) {
            return;
        }
        const std::size_t actual = std::min(count, size() - pos);
        m_buffer.erase(m_buffer.begin() + static_cast<std::ptrdiff_t>(pos),
                       m_buffer.begin() + static_cast<std::ptrdiff_t>(pos + actual));
    }

    void erase(iterator first, iterator last)
    {
        m_buffer.erase(first, last);
    }

    void replace(std::size_t pos, std::size_t count, const char* s, std::size_t n)
    {
        const std::size_t actual = std::min(count, size() - pos);
        m_buffer.erase(m_buffer.begin() + static_cast<std::ptrdiff_t>(pos),
                       m_buffer.begin() + static_cast<std::ptrdiff_t>(pos + actual));
        m_buffer.insert(m_buffer.begin() + static_cast<std::ptrdiff_t>(pos), s, s + static_cast<std::ptrdiff_t>(n));
    }

    std::size_t find(mc::string_view needle, std::size_t pos) const
    {
        return view().find(needle, pos);
    }
};

struct string_storage : mc::shared_base {
    string_bytes bytes;

    string_storage() = default;

    explicit string_storage(mc::string_view view) : bytes(view)
    {}

    explicit string_storage(std::size_t n, char ch) : bytes(n, ch)
    {}
};

struct string_mutable_impl {
    static string_bytes& mutable_storage(string& s, std::size_t min_capacity = 0U);
};

string_bytes& string_mutable_impl::mutable_storage(string& s, std::size_t min_capacity)
{
    if (!s.m_storage) {
        s.m_storage = mc::make_shared<string_storage>();
    } else if (s.m_storage->ref_count() > 1U) {
        s.m_storage = mc::make_shared<string_storage>(s.m_storage->bytes.view());
    }

    string_bytes& bytes = s.m_storage->bytes;
    if (min_capacity > bytes.capacity()) {
        bytes.reserve(min_capacity);
    }
    return bytes;
}

} // namespace mc::detail

namespace mc {

string::string() noexcept = default;

string::string(const string& other) noexcept = default;

string::string(string&& other) noexcept = default;

string& string::operator=(const string& other) noexcept = default;

string& string::operator=(string&& other) noexcept = default;

string::~string() = default;

string::string(mc::string_view view)
{
    if (view.empty()) {
        return;
    }
    m_storage = mc::make_shared<detail::string_storage>(view);
}

string::string(const char* first, const char* last)
{
    if (first == nullptr || first >= last) {
        return;
    }
    *this = string(mc::string_view(first, static_cast<std::size_t>(last - first)));
}

string::string(const char* data, std::size_t count) : string(mc::string_view(data, count))
{}

string::string(std::size_t count, char ch)
{
    if (count == 0U) {
        return;
    }
    m_storage = mc::make_shared<detail::string_storage>(count, ch);
}

std::size_t string::size() const noexcept
{
    return m_storage ? m_storage->bytes.size() : 0U;
}

std::size_t string::capacity() const noexcept
{
    return m_storage ? m_storage->bytes.capacity() : 0U;
}

bool string::empty() const noexcept
{
    return size() == 0U;
}

char* string::data() noexcept
{
    return detail::string_mutable_impl::mutable_storage(*this).data_mutable();
}

const char* string::data() const noexcept
{
    return m_storage ? m_storage->bytes.data() : "";
}

mc::string_view string::view() const noexcept
{
    return mc::string_view(data(), size());
}

string string::substr(size_type pos, size_type count) const
{
    return string(view().substr(pos, count));
}

namespace {

mc::string_view stabilize_source_view(const mc::string& target, mc::string_view source, mc::string& stable_copy)
{
    if (source.empty()) {
        return source;
    }

    const mc::string_view current   = target.view();
    const char*           cur_begin = current.data();
    const char*           cur_end   = cur_begin + static_cast<std::ptrdiff_t>(current.size());
    const char*           src_begin = source.data();
    const char*           src_end   = src_begin + static_cast<std::ptrdiff_t>(source.size());

    if (src_begin < cur_end && src_end > cur_begin) {
        stable_copy = mc::string(source);
        return mc::string_view(stable_copy.data(), stable_copy.size());
    }

    return source;
}

mc::string concat_string_views(mc::string_view a, mc::string_view b)
{
    if (a.empty()) {
        return mc::string(b);
    }
    if (b.empty()) {
        return mc::string(a);
    }
    mc::string out;
    out.reserve(a.size() + b.size());
    out.append(a);
    out.append(b);
    return out;
}

} // namespace

string operator+(const string& lhs, const string& rhs)
{
    return concat_string_views(lhs.view(), rhs.view());
}

string operator+(const string& lhs, mc::string_view rhs)
{
    return concat_string_views(lhs.view(), rhs);
}

string operator+(mc::string_view lhs, const string& rhs)
{
    return concat_string_views(lhs, rhs.view());
}

int string::compare(mc::string_view other) const noexcept
{
    return mc::string_view(data(), size()).compare(mc::string_view(other));
}

std::size_t string::hash() const noexcept
{
    return std::hash<mc::string_view>{}(mc::string_view(data(), size()));
}

void string::swap(string& other) noexcept
{
    m_storage.swap(other.m_storage);
}

bool operator==(const string& lhs, const string& rhs) noexcept
{
    return lhs.view() == rhs.view();
}

bool operator!=(const string& lhs, const string& rhs) noexcept
{
    return !(lhs == rhs);
}

bool operator==(const string& lhs, mc::string_view rhs) noexcept
{
    return lhs.view() == rhs;
}

bool operator==(mc::string_view lhs, const string& rhs) noexcept
{
    return lhs == rhs.view();
}

bool operator!=(const string& lhs, mc::string_view rhs) noexcept
{
    return !(lhs.view() == rhs);
}

bool operator!=(mc::string_view lhs, const string& rhs) noexcept
{
    return !(lhs == rhs.view());
}

bool operator<(const string& lhs, const string& rhs) noexcept
{
    return lhs.compare(rhs.view()) < 0;
}

bool operator<=(const string& lhs, const string& rhs) noexcept
{
    return lhs.compare(rhs.view()) <= 0;
}

bool operator>(const string& lhs, const string& rhs) noexcept
{
    return lhs.compare(rhs.view()) > 0;
}

bool operator>=(const string& lhs, const string& rhs) noexcept
{
    return lhs.compare(rhs.view()) >= 0;
}

const char* string::begin() const noexcept
{
    return data();
}

const char* string::end() const noexcept
{
    return data() + size();
}

char* string::begin() noexcept
{
    return data();
}

char* string::end() noexcept
{
    return data() + static_cast<std::ptrdiff_t>(size());
}

void string::clear() noexcept
{
    if (!m_storage) {
        return;
    }
    if (m_storage->ref_count() > 1U) {
        m_storage = mc::make_shared<detail::string_storage>();
        return;
    }
    m_storage->bytes.clear();
}

void string::push_back(char c)
{
    detail::string_mutable_impl::mutable_storage(*this, size() + 1U).push_back(c);
}

void string::pop_back() noexcept
{
    auto& v = detail::string_mutable_impl::mutable_storage(*this);
    if (v.empty()) {
        return;
    }
    v.pop_back();
}

void string::resize(size_type n, char ch)
{
    detail::string_mutable_impl::mutable_storage(*this, n).resize(n, ch);
}

void string::insert(size_type pos, size_type count, char ch)
{
    if (pos > size()) {
        throw std::out_of_range("mc::string::insert");
    }
    detail::string_mutable_impl::mutable_storage(*this, size() + count).insert(pos, count, ch);
}

char& string::operator[](size_type index)
{
    return detail::string_mutable_impl::mutable_storage(*this)[index];
}

const char& string::operator[](size_type index) const
{
    return data()[index];
}

char& string::at(size_type index)
{
    return detail::string_mutable_impl::mutable_storage(*this).at(index);
}

const char& string::at(size_type index) const
{
    if (index >= size()) {
        throw std::out_of_range("mc::string::at");
    }
    return data()[index];
}

char& string::front()
{
    auto& v = detail::string_mutable_impl::mutable_storage(*this);
    if (v.empty()) {
        throw std::out_of_range("mc::string::front");
    }
    return v.front();
}

const char& string::front() const
{
    if (empty()) {
        throw std::out_of_range("mc::string::front");
    }
    return data()[0];
}

char& string::back()
{
    auto& v = detail::string_mutable_impl::mutable_storage(*this);
    if (v.empty()) {
        throw std::out_of_range("mc::string::back");
    }
    return v.back();
}

const char& string::back() const
{
    if (empty()) {
        throw std::out_of_range("mc::string::back");
    }
    return data()[size() - 1U];
}

void string::reserve(std::size_t new_capacity)
{
    if (new_capacity == 0U && empty()) {
        return;
    }
    detail::string_mutable_impl::mutable_storage(*this, new_capacity).reserve(new_capacity);
}

void string::shrink_to_fit()
{
    if (!m_storage) {
        return;
    }
    detail::string_mutable_impl::mutable_storage(*this).shrink_to_fit();
}

void string::append(const char* str)
{
    if (str == nullptr || *str == '\0') {
        return;
    }
    append(mc::string_view(str));
}

void string::append(const char* str, std::size_t count)
{
    if (count == 0U || str == nullptr) {
        return;
    }
    append(mc::string_view(str, count));
}

void string::append(std::size_t count, char ch)
{
    if (count == 0U) {
        return;
    }
    detail::string_mutable_impl::mutable_storage(*this, size() + count).append(count, ch);
}

void string::append(mc::string_view str)
{
    if (str.empty()) {
        return;
    }
    mc::string stable_copy;
    str = stabilize_source_view(*this, str, stable_copy);
    detail::string_mutable_impl::mutable_storage(*this, size() + str.size()).append(str.data(), str.size());
}

void string::append(const string& str)
{
    append(str.view());
}

void string::append(const string& str, size_type pos, size_type n)
{
    mc::string_view full = str.view();
    if (pos > full.size()) {
        throw std::out_of_range("mc::string::append");
    }
    const std::size_t avail = full.size() - pos;
    const std::size_t len   = (n == npos || n > avail) ? avail : n;
    append(full.data() + pos, len);
}

void string::replace(size_type pos, size_type count, mc::string_view str)
{
    if (pos > size()) {
        throw std::out_of_range("mc::string::replace");
    }
    mc::string stable_copy;
    str                          = stabilize_source_view(*this, str, stable_copy);
    const size_type current_size = size();
    const size_type erase_len    = std::min(count, current_size - pos);
    auto&           v = detail::string_mutable_impl::mutable_storage(*this, current_size - erase_len + str.size());
    v.replace(pos, count, str.data(), str.size());
}

void string::erase(size_type pos, size_type n)
{
    auto& v = detail::string_mutable_impl::mutable_storage(*this);
    if (pos > v.size()) {
        throw std::out_of_range("mc::string::erase");
    }
    const std::size_t count = (n == npos || n > (v.size() - pos)) ? (v.size() - pos) : static_cast<std::size_t>(n);
    v.erase(pos, count);
}

char* string::erase(char* first, char* last)
{
    auto& v = detail::string_mutable_impl::mutable_storage(*this);
    char* b = v.data_mutable();
    char* e = b + static_cast<std::ptrdiff_t>(v.size());
    if (first < b || last < first || last > e) {
        throw std::out_of_range("mc::string::erase");
    }
    const std::size_t pos = static_cast<std::size_t>(first - b);
    const std::size_t len = static_cast<std::size_t>(last - first);
    v.erase(pos, len);
    return detail::string_mutable_impl::mutable_storage(*this).data_mutable() + static_cast<std::ptrdiff_t>(pos);
}

string::size_type string::find(mc::string_view v, size_type pos) const noexcept
{
    return view().find(v, pos);
}

string::size_type string::find(char c, size_type pos) const noexcept
{
    return view().find(c, pos);
}

string::size_type string::find_first_not_of(char c, size_type pos) const noexcept
{
    return view().find_first_not_of(c, pos);
}

string& string::operator+=(char c)
{
    push_back(c);
    return *this;
}

string& string::operator+=(mc::string_view v)
{
    append(v);
    return *this;
}

string& string::operator+=(const string& v)
{
    append(v);
    return *this;
}

string& string::operator+=(const char* str)
{
    append(str);
    return *this;
}

void string::to_lower_inplace()
{
    auto& v = detail::string_mutable_impl::mutable_storage(*this);
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
}

void string::to_upper_inplace()
{
    auto& v = detail::string_mutable_impl::mutable_storage(*this);
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
}

void string::ltrim_inplace()
{
    auto&      v  = detail::string_mutable_impl::mutable_storage(*this);
    const auto it = std::find_if(v.begin(), v.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    });
    v.erase(v.begin(), it);
}

void string::rtrim_inplace()
{
    auto&      v  = detail::string_mutable_impl::mutable_storage(*this);
    const auto it = std::find_if(v.rbegin(), v.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base();
    v.erase(it, v.end());
}

void string::trim_inplace()
{
    ltrim_inplace();
    rtrim_inplace();
}

void string::replace_all_inplace(mc::string_view from, mc::string_view to)
{
    if (from.empty()) {
        return;
    }

    auto&       v     = detail::string_mutable_impl::mutable_storage(*this);
    std::size_t start = 0;
    while ((start = v.find(from, start)) != mc::string_view::npos) {
        v.replace(start, from.size(), to.data(), to.size());
        start += to.size();
    }
}

string operator+(const string& lhs, const char* rhs)
{
    if (rhs == nullptr || *rhs == '\0') {
        return lhs;
    }
    return concat_string_views(lhs.view(), mc::string_view(rhs));
}

string operator+(const char* lhs, const string& rhs)
{
    if (lhs == nullptr || *lhs == '\0') {
        return rhs;
    }
    return concat_string_views(mc::string_view(lhs), rhs.view());
}

std::ostream& operator<<(std::ostream& os, const string& s)
{
    return os.write(s.data(), static_cast<std::streamsize>(s.size()));
}

} // namespace mc

namespace mc::strings {

namespace detail {
void throw_bad_cast_error(mc::string_view type)
{
    MC_THROW(mc::invalid_arg_exception, "can not cast string to type: ${type}", ("type", type));
}

void throw_overflow_error(mc::string_view type, mc::string_view s)
{
    MC_THROW(mc::overflow_exception, "can not cast string to type ${type}, value ${value} overflow",
             ("type", type)("value", s));
}

std::pair<int, mc::string_view> detect_number_radix(mc::string_view s)
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

mc::string_view prepare_number_string(mc::string_view s, int radix, char* buffer, std::size_t buffer_size) noexcept
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

    if (s.size() > max_len) {
        errno = ERANGE;
        return {};
    }

    if ((s.size() + 1) > buffer_size) {
        return {};
    }

    errno_t ret = memcpy_s(buffer, buffer_size, s.data(), s.size());
    if (ret != EOK) {
        return {};
    }
    buffer[s.size()] = '\0';
    return mc::string_view(buffer, s.size());
}

} // namespace detail

/**
 * @brief 尝试将字符串转换为布尔值
 * @param s 要转换的字符串
 * @param result 转换结果的引用
 * @return 是否转换成功
 */
bool try_to_bool(mc::string_view s, bool& result)
{
    if (s.empty()) {
        result = false;
        return true;
    }

    if (iequals(s, mc::string_view("true", 4)) || s == mc::string_view("1", 1)) {
        result = true;
        return true;
    } else if (iequals(s, mc::string_view("false", 5)) || s == mc::string_view("0", 1)) {
        result = false;
        return true;
    }

    return false;
}

bool to_bool_with_default(mc::string_view s, bool default_value)
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

bool to_bool(mc::string_view s)
{
    if (s.empty()) {
        return false;
    }

    bool result;
    if (try_to_bool(s, result)) {
        return result;
    }

    detail::throw_bad_cast_error("bool");
    return false;
}

// 忽略大小写比较两个字符串是否相等
bool iequals(mc::string_view a, mc::string_view b)
{
    if (a.size() != b.size()) {
        return false;
    }

    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

// 忽略大小写比较两个 C 风格字符串是否相等
bool iequals(const char* a, const char* b)
{
    if (!a || !b) {
        return a == b;
    }

    return iequals(mc::string_view(a), mc::string_view(b));
}

// 将字符串转换为小写
mc::string to_lower(mc::string_view s)
{
    mc::string result(s);
    result.to_lower_inplace();
    return result;
}

// 将字符串转换为大写
mc::string to_upper(mc::string_view s)
{
    mc::string result(s);
    result.to_upper_inplace();
    return result;
}

// 去除字符串两端的空白字符
mc::string trim(mc::string_view s)
{
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
    std::size_t j = s.size();
    if (i < j) {
        do {
            --j;
        } while (j > i && std::isspace(static_cast<unsigned char>(s[j])));
        ++j;
    }
    return mc::string(s.substr(i, j - i));
}

// 去除字符串左侧的空白字符
mc::string ltrim(mc::string_view s)
{
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
    return mc::string(s.substr(i));
}

// 去除字符串右侧的空白字符
mc::string rtrim(mc::string_view s)
{
    if (s.empty()) {
        return {};
    }
    std::size_t j = s.size();
    do {
        --j;
    } while (j > 0 && std::isspace(static_cast<unsigned char>(s[j])));
    if (!std::isspace(static_cast<unsigned char>(s[j]))) {
        ++j;
    }
    return mc::string(s.substr(0, j));
}

// 按指定分隔符分割字符串
std::vector<mc::string> split(mc::string_view s, char delim)
{
    std::vector<mc::string> result;
    std::size_t             start = 0;
    std::size_t             end   = s.find(delim);

    while (end != mc::string_view::npos) {
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
std::vector<mc::string> split(mc::string_view s, mc::string_view delim)
{
    std::vector<mc::string> result;
    if (delim.empty()) {
        result.emplace_back(s);
        return result;
    }

    std::size_t start = 0;
    std::size_t end   = s.find(delim);

    while (end != mc::string_view::npos) {
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
mc::string join(const std::vector<mc::string>& v, mc::string_view delim)
{
    if (v.empty()) {
        return {};
    }

    mc::string  result;
    std::size_t total_size = 0;

    for (const auto& s : v) {
        total_size += s.size();
    }
    total_size += delim.size() * (v.size() - 1U);

    result.reserve(total_size);

    for (std::size_t i = 0; i < v.size(); ++i) {
        result.append(v[i]);
        if (i < v.size() - 1U) {
            result.append(delim);
        }
    }

    return result;
}

// 检查字符串是否以指定前缀开始
bool starts_with(mc::string_view s, mc::string_view prefix)
{
    if (prefix.empty()) {
        return true;
    }

    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

// 检查字符串是否以指定后缀结束
bool ends_with(mc::string_view s, mc::string_view suffix)
{
    return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}

mc::string_view longest_common_prefix(mc::string_view s1, mc::string_view s2)
{
    std::size_t i = 0;
    while (i < s1.size() && i < s2.size() && s1[i] == s2[i]) {
        ++i;
    }
    return mc::string_view(s1.data(), i);
}

// 替换字符串中的所有指定子串
mc::string replace_all(mc::string_view s, mc::string_view from, mc::string_view to)
{
    if (from.empty()) {
        return mc::string(s);
    }

    mc::string result;
    result.reserve(s.size());

    std::size_t last_pos = 0;
    std::size_t pos      = 0;
    while ((pos = s.find(from, last_pos)) != mc::string_view::npos) {
        result.append(s.substr(last_pos, pos - last_pos));
        result.append(to);
        last_pos = pos + from.size();
    }

    result.append(s.substr(last_pos));
    return result;
}

// 检查字符串是否包含指定子串
bool contains(mc::string_view s, mc::string_view substring)
{
    return s.find(substring) != mc::string_view::npos;
}

// 忽略大小写检查字符串是否包含指定子串
bool icontains(mc::string_view s, mc::string_view substring)
{
    // 避免不必要的字符串拷贝，使用临时缓冲区
    if (s.empty() || substring.empty() || s.size() < substring.size()) {
        return substring.empty();
    }

    // 对于较小的字符串，直接转换为小写进行比较
    if (s.size() <= 1024 && substring.size() <= 1024) {
        const mc::string s_lower         = to_lower(s);
        const mc::string substring_lower = to_lower(substring);
        return contains(s_lower.view(), substring_lower.view());
    }

    // 对于较大的字符串，使用滑动窗口方法避免完整转换
    auto is_equal_ignore_case = [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
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
mc::string_view substr(mc::string_view s, int start, int end)
{
    const std::size_t length = s.size();
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
    std::size_t substr_length = static_cast<std::size_t>(adjusted_end - adjusted_start + 1);

    // 返回子字符串
    return s.substr(static_cast<std::size_t>(adjusted_start), substr_length);
}

/**
 * @brief 获取子字符串，第二个参数指定长度而非结束位置
 */
mc::string_view substring(mc::string_view s, int start, std::size_t length)
{
    const std::size_t str_length = s.size();
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
    const std::size_t start_u = static_cast<std::size_t>(adjusted_start);
    if (length == mc::string_view::npos || start_u + length > str_length) {
        length = str_length - start_u;
    }

    // 返回子字符串
    return s.substr(start_u, length);
}

void fixed_width_append(mc::string& result, size_t width, mc::string_view s, bool left_align)
{
    auto& buf = mc::detail::string_mutable_impl::mutable_storage(result);
    if (s.length() >= width) {
        buf.append(s.data(), width);
        return;
    }
    const std::size_t padding = width - s.length();
    if (left_align) {
        buf.append(s.data(), s.length());
        buf.append(padding, ' ');
    } else {
        buf.append(padding, ' ');
        buf.append(s.data(), s.length());
    }
}

bool is_valid_utf8(mc::string_view s)
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
            unsigned int code_point = ((bytes[i] & 0x0F) << 12) | ((bytes[i + 1] & 0x3F) << 6) | (bytes[i + 2] & 0x3F);
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
            if ((bytes[i + 1] & 0xC0) != 0x80 || (bytes[i + 2] & 0xC0) != 0x80 || (bytes[i + 3] & 0xC0) != 0x80) {
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

void to_string(mc::string& result, double value)
{
    char   buffer[64];
    double intpart;
    if (modf(value, &intpart) == 0.0) {
        snprintf_s(buffer, sizeof(buffer), sizeof(buffer), "%.0f", value);
    } else {
        snprintf_s(buffer, sizeof(buffer), sizeof(buffer), "%.6f", value);
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

mc::string to_string(double value)
{
    mc::string result;
    to_string(result, value);
    return result;
}

mc::string to_string(bool value)
{
    return value ? mc::string("true") : mc::string("false");
}

void to_string(mc::string& result, bool value)
{
    auto& buf = mc::detail::string_mutable_impl::mutable_storage(result);
    buf.append(value ? "true" : "false");
}

} // namespace mc::strings