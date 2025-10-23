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

#include <mc/dict.h>
#include <mc/traits.h>
#include <mc/variant/io.h>
#include <mc/variant/variant_base.h>

// 必须在 variant_base.h 之后
#include <mc/variant/variant_array.inl>

namespace mc {

using variant       = variant_base<>;
using blob          = blob_base<>;
using variants      = typename variant::array_type;
using typed_variant = variant_base<variant_config<std::allocator<char>, true>>;

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
[[noreturn]] MC_API void throw_method_arg_not_match(std::string_view method_name,
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

#include <mc/variant/container_convert.inl>

#endif // MC_VARIANT_H