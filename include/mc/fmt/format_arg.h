/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_FMT_FORMAT_ARG_H
#define MC_FMT_FORMAT_ARG_H

#include <mc/exception.h>
#include <mc/fmt/formatter.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace mc::fmt {
class format_context;
}
namespace mc::fmt::detail {

// 命名参数
template <typename T>
class named_arg {
public:
    constexpr explicit named_arg(std::string_view name, const T& value) : m_name(name), m_value(value) {
    }

    constexpr std::string_view name() const {
        return m_name;
    }

    constexpr const T& value() const {
        return m_value;
    }

private:
    std::string_view m_name;
    const T&         m_value;
};

// 创建命名参数的辅助函数
template <typename T>
constexpr auto arg(std::string_view name, const T& value) -> named_arg<T> {
    return named_arg<T>{name, value};
}

template <typename T>
constexpr auto arg(const T& value) -> const T& {
    return std::forward<const T&>(value);
}

using string_view = std::string_view;
using monostate   = std::monostate;
struct format_spec;

struct custom_t {
    constexpr custom_t()
        : obj(nullptr),
          format_fn(nullptr),
          parse_fn(nullptr) {
    }

    const void* obj;
    void (*format_fn)(const void*, format_context&, const format_spec& spec);
    bool (*parse_fn)(std::string_view, format_spec&);
};

// format_arg 类型擦除存储，支持基础类型和自定义类型
struct format_arg {
    using storage_type = std::variant<
        monostate, bool, char, int64_t, uint64_t, double, float, long double,
        std::string_view, const char*, const void*, custom_t>;

    storage_type value;
    bool         used = false;

    constexpr format_arg() : value(monostate{}) {
    }

    constexpr explicit format_arg(bool v) : value(v) {
    }

    constexpr explicit format_arg(char v) : value(v) {
    }

    template <typename T, std::enable_if_t<std::is_integral_v<T> &&
                                               std::is_signed_v<T>,
                                           int> = 0>
    constexpr explicit format_arg(T v) : value(static_cast<int64_t>(v)) {
    }

    template <typename T, std::enable_if_t<std::is_integral_v<T> &&
                                               std::is_unsigned_v<T>,
                                           int> = 0>
    constexpr explicit format_arg(T v) : value(static_cast<uint64_t>(v)) {
    }

    constexpr explicit format_arg(double v) : value(v) {
    }

    constexpr explicit format_arg(float v) : value(v) {
    }

    constexpr explicit format_arg(long double v) : value(v) {
    }

    constexpr explicit format_arg(std::string_view v) : value(v) {
    }

    explicit format_arg(const std::string& v) : value(std::string_view{v}) {
    }

    constexpr explicit format_arg(const char* v) : value(v) {
    }

    constexpr explicit format_arg(const void* v) : value(v) {
    }

    template <typename T, std::enable_if_t<
                              has_formatter_v<T> &&
                                  !is_basic_formatter_v<T>,
                              int> = 0>
    explicit format_arg(const T& v) : value(make_custom_value(v)) {
    }

    template <typename T, std::enable_if_t<
                              has_formatter_v<T> &&
                                  !is_basic_formatter_v<T>,
                              int> = 0>
    constexpr explicit format_arg(const T* v)
        : value(compile_make_custom_value(v)) {
    }

    constexpr void make_unused() {
        used = true;
    }

    constexpr bool is_used() const {
        return used;
    }

private:
    template <typename T>
    custom_t make_custom_value(const T& v) const {
        custom_t custom_val;
        custom_val.obj       = &v;
        custom_val.format_fn = [](const void* p, format_context& ctx, const format_spec& spec) {
            formatter<T>{}.template format<format_context>(*static_cast<const T*>(p), ctx, spec);
        };
        if constexpr (has_parse_v<T>) {
            custom_val.parse_fn = [](std::string_view fmt_str, format_spec& spec) -> bool {
                return formatter<T>{}.template parse<false>(fmt_str, spec);
            };
        } else {
            custom_val.parse_fn = nullptr;
        }
        return custom_val;
    }

    template <typename T>
    constexpr custom_t compile_make_custom_value(const T*) const {
        custom_t custom_val;
        custom_val.obj       = nullptr;
        custom_val.format_fn = nullptr;
        if constexpr (has_parse_v<T>) {
            custom_val.parse_fn = [](std::string_view fmt_str, format_spec& spec) -> bool {
                return formatter<T>{}.template parse<true>(fmt_str, spec);
            };
        } else {
            custom_val.parse_fn = nullptr;
        }
        return custom_val;
    }

public:
    template <typename Visitor>
    constexpr void visit(Visitor&& vis) const {
        std::visit(vis, value);
    }

    // 类型判断方法
    constexpr bool is_null() const {
        return std::holds_alternative<monostate>(value);
    }
    constexpr bool is_signed_integer() const {
        return std::holds_alternative<int64_t>(value);
    }
    constexpr bool is_unsigned_integer() const {
        return std::holds_alternative<uint64_t>(value);
    }
    constexpr bool is_float() const {
        return std::holds_alternative<float>(value);
    }
    constexpr bool is_double() const {
        return std::holds_alternative<double>(value);
    }
    constexpr bool is_long_double() const {
        return std::holds_alternative<long double>(value);
    }
    constexpr bool is_bool() const {
        return std::holds_alternative<bool>(value);
    }
    constexpr bool is_char() const {
        return std::holds_alternative<char>(value);
    }
    constexpr bool is_string() const {
        return std::holds_alternative<std::string_view>(value);
    }
    constexpr bool is_cstring() const {
        return std::holds_alternative<const char*>(value);
    }
    constexpr bool is_pointer() const {
        return std::holds_alternative<const void*>(value);
    }
    constexpr bool is_custom() const {
        return std::holds_alternative<custom_t>(value);
    }

    // 取值方法
    int64_t as_signed() const {
        return std::get<int64_t>(value);
    }
    uint64_t as_unsigned() const {
        return std::get<uint64_t>(value);
    }
    float as_float() const {
        return std::get<float>(value);
    }
    double as_double() const {
        return std::get<double>(value);
    }
    long double as_long_double() const {
        return std::get<long double>(value);
    }
    bool as_bool() const {
        return std::get<bool>(value);
    }
    char as_char() const {
        return std::get<char>(value);
    }
    std::string_view as_string() const {
        return std::get<std::string_view>(value);
    }
    const char* as_cstring() const {
        return std::get<const char*>(value);
    }
    const void* as_pointer() const {
        return std::get<const void*>(value);
    }

    constexpr const custom_t& as_custom() const {
        return std::get<custom_t>(value);
    }

    explicit operator bool() const {
        return !is_null();
    }

    constexpr bool parse_custom(const char*& ptr, const char* end, format_spec& spec) const {
        std::string_view format_str(ptr, end - ptr - 1);
        return as_custom().parse_fn(format_str, spec);
    }

    constexpr bool parser(const char*& ptr, const char* end, format_spec& spec) const {
        if (!is_custom() || !as_custom().parse_fn) {
            return false;
        }

        if (!parse_custom(ptr, end, spec)) {
            ptr = nullptr;
        }

        return true;
    }
};

#ifndef MC_FMT_MAX_ARGS
#define MC_FMT_MAX_ARGS 16
#endif

constexpr size_t max_args = MC_FMT_MAX_ARGS;

struct arg_entry {
    std::string_view name;
    format_arg       arg;
};

// 类型特征，用于检测是否是命名参数
template <typename T>
struct is_named_arg : std::false_type {};

template <typename T>
inline constexpr bool is_named_arg_v = is_named_arg<T>::value;

struct arg_store {
    std::array<arg_entry, max_args> entries;
    size_t                          m_size = 0;

    constexpr arg_store() = default;

    template <typename... Args>
    constexpr arg_store(Args&&... args) {
        (add_arg(std::forward<Args>(args)), ...);
    }

    // 添加位置参数
    template <typename T, std::enable_if_t<!is_named_arg_v<T>, int> = 0>
    constexpr void add_arg(const T& value) {
        entries[m_size].name = std::string_view{};
        entries[m_size].arg  = format_arg(value);
        ++m_size;
    }

    // 添加命名参数的特化
    template <typename T>
    constexpr void add_arg(const named_arg<T>& arg) {
        entries[m_size].name = arg.name();
        entries[m_size].arg  = format_arg(arg.value());
        ++m_size;
    }

    constexpr void add_arg(const std::monostate&) {
    }

    constexpr void add_arg(std::string_view name, format_arg arg) {
        entries[m_size].name = name;
        entries[m_size].arg  = arg;
        ++m_size;
    }

    constexpr size_t size() const {
        return m_size;
    }

    constexpr const format_arg* get_arg(size_t index) const {
        return &entries[index].arg;
    }

    // 按位置获取参数
    constexpr const format_arg* get_positional(size_t index) const {
        size_t positional_count = 0;
        for (size_t i = 0; i < m_size; ++i) {
            if (positional_count == index) {
                return &entries[i].arg;
            }
            ++positional_count;
        }
        return nullptr;
    }

    // 按名称获取参数
    constexpr const format_arg* get_named(std::string_view name) const {
        for (size_t i = 0; i < m_size; ++i) {
            if (entries[i].name == name) {
                return &entries[i].arg;
            }
        }
        return nullptr;
    }
};

template <typename T>
struct is_named_arg<named_arg<T>> : std::true_type {};

template <typename T>
constexpr bool has_formatter_v<named_arg<T>> = has_formatter_v<T>;

// 前向声明编译期参数结构
template <typename T>
struct compile_named_arg;

template <typename T>
struct compile_positional_arg;

// 为编译期参数结构添加类型特征
template <typename T>
struct is_named_arg<mc::fmt::detail::compile_named_arg<T>> : std::true_type {};

template <typename T>
constexpr bool has_formatter_v<mc::fmt::detail::compile_named_arg<T>> = has_formatter_v<T>;

template <typename T>
constexpr bool has_formatter_v<mc::fmt::detail::compile_positional_arg<T>> = has_formatter_v<T>;

} // namespace mc::fmt::detail

#endif // MC_FMT_FORMAT_ARG_H
