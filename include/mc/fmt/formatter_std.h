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
#ifndef MC_FMT_FORMATTER_STD_H
#define MC_FMT_FORMATTER_STD_H

#include <mc/fmt/format_parser.h>
#include <mc/fmt/formatter.h>

#include <array>
#include <bitset>
#include <complex>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

// 前向声明
namespace mc {
class dict;
} // namespace mc

namespace mc::fmt {

namespace detail {
// 判断是否为容器类型（排除 string）
template <typename T, typename = void>
struct is_container : std::false_type {};

template <typename T>
struct is_container<T,
                    std::void_t<decltype(std::declval<T>().begin()),
                                decltype(std::declval<T>().end()),
                                typename T::value_type>>
    : std::true_type {};

template <>
struct is_container<std::string> : std::false_type {};
template <>
struct is_container<std::string_view> : std::false_type {};

template <typename T, typename = void>
struct is_map : std::false_type {};

template <typename T>
struct is_map<T, std::void_t<typename T::mapped_type>> : std::true_type {};

// 判断类型是否为 set 类型容器（set, unordered_set, multiset, unordered_multiset）
template <typename T, typename = void>
struct is_set : std::false_type {};

template <typename T>
struct is_set<T,
              std::void_t<typename T::key_type, typename T::value_type>>
    : std::true_type {};

// 递归格式化元素，string 类型加双引号，char 类型加单引号，其他类型直接递归
template <typename Context, typename T>
void format_elem(Context& ctx, const T& value, const format_spec& spec)
{
    if constexpr (is_string_v<T>) {
        ctx.out().push_back('"');
        detail::format_to(ctx, spec, value);
        ctx.out().push_back('"');
    } else if constexpr (is_char_v<T>) {
        ctx.out().push_back('\'');
        detail::format_to(ctx, spec, value);
        ctx.out().push_back('\'');
    } else if constexpr (mc::is_variant_v<T>) {
        if (value.is_string()) {
            ctx.out().push_back('"');
            detail::format_to(ctx, spec, value.as_string());
            ctx.out().push_back('"');
        } else {
            detail::format_to(ctx, spec, value);
        }
    } else {
        detail::format_to(ctx, spec, value);
    }
}
} // namespace detail

using format_spec = detail::format_spec;

template <typename T>
struct formatter<T, std::enable_if_t<detail::is_integer_v<T>>> {
    static constexpr bool is_basic_type = true;

    template <typename Context>
    void format(T value, Context& ctx, const format_spec& spec) const
    {
        detail::format_parser::format_integer(ctx, value, spec);
    }
};

template <>
struct formatter<bool> {
    static constexpr bool is_basic_type = true;

    template <typename Context>
    void format(bool value, Context& ctx, const format_spec& spec) const
    {
        detail::format_parser::format_bool(ctx, value, spec);
    }
};

template <typename T>
struct formatter<T, std::enable_if_t<detail::is_char_v<T>>> {
    static constexpr bool is_basic_type = true;

    template <typename Context>
    void format(T value, Context& ctx, const format_spec& spec) const
    {
        detail::format_parser::format_char(ctx, value, spec);
    }
};

template <typename T>
struct formatter<T, std::enable_if_t<detail::is_string_v<T>>> {
    static constexpr bool is_basic_type = true;

    template <typename Context>
    void format(T value, Context& ctx, const format_spec& spec) const
    {
        detail::format_parser::format_string(ctx, value, spec);
    }
};

template <typename T>
struct formatter<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    static constexpr bool is_basic_type = true;

    template <typename Context>
    void format(T value, Context& ctx, const format_spec& spec) const
    {
        detail::format_parser::format_double(ctx, value, spec);
    }
};

// 通用容器 formatter（排除 string/map）
template <typename T>
struct formatter<T, std::enable_if_t<detail::is_container<T>::value && !detail::is_map<T>::value>> {
    // 格式化输出 [elem1, elem2, ...] 或 {elem1, elem2, ...}
    template <typename Context>
    void format(const T& container, Context& ctx, const format_spec& spec) const
    {
        if constexpr (detail::is_set<T>::value) {
            ctx.append('{');
        } else {
            ctx.append('[');
        }
        bool first = true;
        for (const auto& elem : container) {
            if (!first) {
                ctx.append(',');
            }
            first = false;
            detail::format_elem(ctx, elem, spec);
        }
        if constexpr (detail::is_set<T>::value) {
            ctx.append('}');
        } else {
            ctx.append(']');
        }
    }
};

// map formatter，输出 {key1:value1,key2:value2,...}
template <typename T>
struct formatter<T, std::enable_if_t<detail::is_map<T>::value>> {
    template <typename Context>
    void format(const T& map, Context& ctx, const format_spec& spec) const
    {
        ctx.append('{');
        bool first = true;
        for (const auto& kv : map) {
            if (!first) {
                ctx.append(',');
            }
            first = false;
            detail::format_elem(ctx, kv.first, spec);
            ctx.append(':');
            detail::format_elem(ctx, kv.second, spec);
        }
        ctx.append('}');
    }
};

// pair formatter，输出 (a, b)，string 类型加引号
template <typename T1, typename T2>
struct formatter<std::pair<T1, T2>> {
    template <typename Context>
    void format(const std::pair<T1, T2>& p, Context& ctx, const format_spec& spec) const
    {
        ctx.append('(');
        detail::format_elem(ctx, p.first, spec);
        ctx.append(',');
        detail::format_elem(ctx, p.second, spec);
        ctx.append(')');
    }
};

// tuple formatter 递归实现
template <typename Tuple, std::size_t... I, typename Context>
void format_tuple_impl(const Tuple& t, Context& ctx, const format_spec& spec, std::index_sequence<I...>)
{
    ctx.append('(');
    std::size_t n = 0;
    (void)std::initializer_list<int>{
        (n++ ? (ctx.append(","), 0) : 0,
         detail::format_elem(ctx, std::get<I>(t), spec),
         0)...};
    ctx.append(')');
}

template <typename... Ts>
struct formatter<std::tuple<Ts...>> {
    template <typename Context>
    void format(const std::tuple<Ts...>& t, Context& ctx, const format_spec& spec) const
    {
        format_tuple_impl(t, ctx, spec, std::index_sequence_for<Ts...>{});
    }
};

// optional formatter，string 类型加引号
template <typename T>
struct formatter<std::optional<T>> {
    template <typename Context>
    void format(const std::optional<T>& opt, Context& ctx, const format_spec& spec) const
    {
        if (!opt) {
            ctx.append("none");
        } else {
            ctx.append("optional(");
            detail::format_elem(ctx, *opt, spec);
            ctx.append(')');
        }
    }
};

// variant formatter，string 类型加引号
template <typename... Ts>
struct formatter<std::variant<Ts...>> {
    template <typename Context>
    void format(const std::variant<Ts...>& v, Context& ctx, const format_spec& spec) const
    {
        if (v.valueless_by_exception()) {
            ctx.append("variant(valueless)");
        } else {
            ctx.append("variant(");
            std::visit([&](const auto& value) {
                detail::format_elem(ctx, value, spec);
            }, v);
            ctx.append(')');
        }
    }
};

// bitset formatter
template <size_t N>
struct formatter<std::bitset<N>> {
    template <typename Context>
    void format(const std::bitset<N>& b, Context& ctx, const format_spec& spec) const
    {
        ctx.append(b.to_string());
    }
};

// unique_ptr/shared_ptr/weak_ptr formatter
template <typename T, typename Dt>
struct formatter<std::unique_ptr<T, Dt>> {
    template <typename Context>
    void format(const std::unique_ptr<T, Dt>& p, Context& ctx, const format_spec& spec) const
    {
        if (!p) {
            ctx.append("nullptr");
        } else {
            ctx.append("ptr(");
            detail::format_to(ctx, spec, *p);
            ctx.append(')');
        }
    }
};

template <typename T>
struct formatter<std::shared_ptr<T>> {
    template <typename Context>
    void format(const std::shared_ptr<T>& p, Context& ctx, const format_spec& spec) const
    {
        if (!p) {
            ctx.append("nullptr");
        } else {
            ctx.append("ptr(");
            detail::format_to(ctx, spec, *p);
            ctx.append(')');
        }
    }
};

template <typename T>
struct formatter<std::weak_ptr<T>> {
    template <typename Context>
    void format(const std::weak_ptr<T>& p, Context& ctx, const format_spec& spec) const
    {
        auto sp = p.lock();
        if (!sp) {
            ctx.append("nullptr");
        } else {
            ctx.append("ptr(");
            detail::format_to(ctx, spec, *sp);
            ctx.append(')');
        }
    }
};

// nullptr formatter
template <>
struct formatter<std::nullptr_t> {
    template <typename Context>
    void format(std::nullptr_t, Context& ctx, const format_spec& spec) const
    {
        ctx.append("nullptr");
    }
};

// 指针特化
template <>
struct formatter<const void*> {
    template <typename Context>
    void format(const void* ptr, Context& ctx, const format_spec& spec) const
    {
        detail::format_parser::format_pointer(ctx, ptr, spec);
    }
};

template <>
struct formatter<void*> {
    template <typename Context>
    void format(void* ptr, Context& ctx, const format_spec& spec) const
    {
        detail::format_parser::format_pointer(ctx, ptr, spec);
    }
};

} // namespace mc::fmt

#endif // MC_FMT_FORMATTER_STD_H