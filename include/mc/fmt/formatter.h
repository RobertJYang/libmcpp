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

#ifndef MC_FMT_FORMATTER_H
#define MC_FMT_FORMATTER_H

#include <type_traits>

namespace mc::fmt {

class format_context;

template <typename T, typename Enable = void>
struct formatter {};

namespace detail {
template <typename T, typename = void>
struct has_formatter : std::false_type {};

template <typename T>
struct has_formatter<T,
                     std::void_t<decltype(&mc::fmt::formatter<T>::template format<format_context>)>>
    : std::true_type {};

template <typename T, typename = void>
struct has_parse : std::false_type {};

template <typename T>
struct has_parse<T, std::void_t<decltype(&mc::fmt::formatter<T>::template parse<false>)>>
    : std::true_type {};

template <typename T, typename = void>
struct has_get_value : std::false_type {};

template <typename T>
struct has_get_value<T, std::void_t<decltype(&mc::fmt::formatter<T>::get_value)>>
    : std::true_type {};

template <typename T, typename = void>
struct is_basic_formatter : std::false_type {};

template <typename T>
struct is_basic_formatter<T, std::enable_if_t<mc::fmt::formatter<T>::is_basic_type>>
    : std::true_type {};

template <typename T>
constexpr bool is_basic_formatter_v = is_basic_formatter<T>::value;

template <typename T>
struct is_char
    : std::integral_constant<
          bool,
          std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, char> ||
              std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, signed char>> {};

template <typename T>
struct is_integer
    : std::integral_constant<
          bool,
          std::is_integral_v<std::remove_cv_t<std::remove_reference_t<T>>> &&
              !std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, bool> &&
              !is_char<T>::value> {};

template <typename T>
constexpr bool has_formatter_v = has_formatter<T>::value;

template <typename T>
constexpr bool has_parse_v = has_parse<T>::value;

template <typename T>
constexpr bool is_char_v = is_char<T>::value;

template <typename T>
constexpr bool is_integer_v = is_integer<T>::value;

template <typename T>
constexpr bool is_string_v =
    std::is_same_v<std::decay_t<T>, std::string> ||
    std::is_same_v<std::decay_t<T>, std::string_view> ||
    std::is_same_v<std::decay_t<T>, const char*> ||
    std::is_same_v<std::decay_t<T>, char*>;

constexpr bool isdigit(char c) {
    return c >= '0' && c <= '9';
}

constexpr bool to_integer(std::string_view s, size_t& out) {
    size_t value = 0;
    for (char c : s) {
        if (!isdigit(c)) {
            return false;
        }

        value = value * 10 + (c - '0');
    }

    out = value;
    return true;
}

} // namespace detail
} // namespace mc::fmt

#endif // MC_FMT_FORMATTER_H