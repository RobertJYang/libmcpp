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
 * @file variant.h
 * @brief 定义了 mc 命名空间下的 variant 类，用于表示任意类型的数据
 */
#ifndef MC_VARIANT_H
#define MC_VARIANT_H

#include <mc/traits.h>
#include <mc/variant/io.h>
#include <mc/variant/variant_base.h>

namespace mc {

using variant       = variant_base<>;
using blob          = blob_base<>;
using variants      = typename variant::array_type;
using typed_variant = variant_base<variant_config<std::allocator<char>, true>>;

namespace detail {
// 检测是否存在 to_variant 函数
template <typename T>
auto has_to_variant_function(int)
    -> decltype(to_variant(std::declval<T>(), std::declval<mc::variant&>()), std::true_type{});

template <typename T>
std::false_type has_to_variant_function(...);

template <typename T>
inline constexpr bool has_to_variant_function_v = decltype(has_to_variant_function<T>(0))::value;

// 检测类型是否可构造为 variant，分两步检测避免编译错误
template <typename T>
struct is_variant_constructible {
    // 第一步：检查是否可以构造
    static constexpr bool is_constructible = std::is_constructible_v<mc::variant, T>;

    // 第二步：仅当可构造时，才检查是否有 to_variant 函数
    template <typename U, bool IsConstructible>
    struct check_to_variant {
        static constexpr bool value = false;
    };

    template <typename U>
    struct check_to_variant<U, true> {
        static constexpr bool value = detail::has_to_variant_function_v<U>;
    };

    static constexpr bool value = check_to_variant<T, is_constructible>::value;
};

} // namespace detail

template <typename T>
inline constexpr bool is_variant_constructible_v = detail::is_variant_constructible<T>::value;

// 检查所有类型是否都可以转换为variant
template <typename... Args>
struct all_variant_constructible;

template <>
struct all_variant_constructible<> : std::true_type {};

template <typename T, typename... Rest>
struct all_variant_constructible<T, Rest...> {
    static constexpr bool value =
        is_variant_constructible_v<T> && all_variant_constructible<Rest...>::value;
};

template <typename... Args>
inline constexpr bool all_variant_constructible_v = all_variant_constructible<Args...>::value;

namespace detail {
[[noreturn]] void throw_method_arg_not_match(std::string_view method_name,
                                             std::string_view expect_type,
                                             std::string_view actual_type);

template <typename Arg>
static auto convert_arg(std::string_view name, const mc::variant& var)
    -> std::enable_if_t<!is_variant_v<std::decay_t<Arg>>, Arg> {
    using arg_type = mc::traits::remove_cvref_t<std::decay_t<Arg>>;
    if constexpr (std::is_same_v<arg_type, std::string> ||
                  std::is_same_v<arg_type, std::string_view>) {
        if (var.is_string()) {
            return var.get_string();
        }
    } else if constexpr (std::is_same_v<arg_type, mc::blob>) {
        if (var.is_blob()) {
            return var.get_blob();
        }
    } else if constexpr (std::is_same_v<arg_type, mc::dict>) {
        if (var.is_object()) {
            return var.get_object();
        }
    } else if constexpr (std::is_same_v<arg_type, mc::variants>) {
        if (var.is_array()) {
            return var.get_array();
        }
    }

    if (auto arg = var.try_as<Arg>(); arg) {
        return *arg;
    }

    throw_method_arg_not_match(name, pretty_name<Arg>(), var.get_type_name());
}

template <typename Arg>
static auto convert_arg(std::string_view name, const mc::variant& var)
    -> std::enable_if_t<is_variant_v<std::decay_t<Arg>>, const mc::variant&> {
    return var;
}
} // namespace detail
} // namespace mc

#endif // MC_VARIANT_H