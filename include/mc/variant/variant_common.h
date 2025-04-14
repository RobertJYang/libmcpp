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
 * @file variant_common.h
 * @brief 定义了 variant 相关的公共声明和前向声明，用于解决循环依赖问题
 */
#ifndef MC_VARIANT_COMMON_H
#define MC_VARIANT_COMMON_H

#include <cstdint>
#include <string>

#define VARIANT_FLOAT_EPSILON 1e-6

namespace mc {
class dict;
class mutable_dict;

// 前置声明
template <typename Config>
class variant_base;

/**
 * @brief 类型特征，用于识别固定长度的整数类型
 */
template <typename T>
inline constexpr bool is_variant_integer_v =
    std::is_same_v<T, bool> || std::is_same_v<T, int8_t> || std::is_same_v<T, uint8_t> ||
    std::is_same_v<T, int16_t> || std::is_same_v<T, uint16_t> || std::is_same_v<T, int32_t> ||
    std::is_same_v<T, uint32_t> || std::is_same_v<T, int64_t> || std::is_same_v<T, uint64_t>;

template <typename T>
inline constexpr bool is_variant_fundamental_v =
    is_variant_integer_v<T> || std::is_floating_point_v<T>;

namespace detail {
template <typename, typename T>
inline constexpr bool check_is_variant_v = false;
template <typename T>
inline constexpr bool check_is_variant_v<std::enable_if_t<std::decay_t<T>::is_variant::value>, T> =
    true;

} // namespace detail

template <typename T>
inline constexpr bool is_variant_v = detail::check_is_variant_v<void, T>;

// 定义 blob_base 类型，用于存储二进制数据
template <typename Allocator = std::allocator<char>>
struct blob_base {
    std::vector<char, Allocator> data;

    blob_base(const Allocator& alloc = Allocator()) : data(alloc) {
    }

    blob_base(const char* data, size_t size, const Allocator& alloc = Allocator())
        : data(data, data + size, alloc) {
    }

    blob_base(std::initializer_list<char> list, const Allocator& alloc = Allocator())
        : data(list, alloc) {
    }

    /**
     * @brief 比较两个 blob_base 对象是否相等
     * @param other 要比较的 blob_base 对象
     * @return 如果两个对象相等则返回 true，否则返回 false
     */
    bool operator==(const blob_base& other) const {
        return data == other.data;
    }

    /**
     * @brief 比较两个 blob_base 对象是否不相等
     * @param other 要比较的 blob_base 对象
     * @return 如果两个对象不相等则返回 true，否则返回 false
     */
    bool operator!=(const blob_base& other) const {
        return !(*this == other);
    }

    bool operator<(const blob_base& other) const {
        auto lhs = std::string_view(data.data(), data.size());
        auto rhs = std::string_view(other.data.data(), other.data.size());
        return lhs < rhs;
    }

    bool operator>(const blob_base& other) const {
        auto lhs = std::string_view(data.data(), data.size());
        auto rhs = std::string_view(other.data.data(), other.data.size());
        return lhs > rhs;
    }

    std::string_view as_string_view() const {
        return std::string_view(data.data(), data.size());
    }
};

template <typename Config>
using variants_base = std::vector<variant_base<Config>, typename Config::variant_alloc_type>;

template <typename Allocator = std::allocator<char>, bool FixedType = false>
struct variant_config {
    using self_type = variant_config<Allocator, FixedType>;

    using alloc_traits    = std::allocator_traits<Allocator>;
    using char_alloc_type = typename alloc_traits::template rebind_alloc<char>;
    using variant_alloc_type =
        typename alloc_traits::template rebind_alloc<variant_base<self_type>>;

    using allocator_type                = Allocator;
    static constexpr bool is_fixed_type = FixedType;
    using string_type = std::basic_string<char, std::char_traits<char>, char_alloc_type>;
    using object_type = dict;
    using array_type  = std::vector<variant_base<self_type>, variant_alloc_type>;
    using blob_type   = blob_base<char_alloc_type>;

    using string_alloc_type = typename alloc_traits::template rebind_alloc<string_type>;
    using object_alloc_type = typename alloc_traits::template rebind_alloc<object_type>;
    using array_alloc_type  = typename alloc_traits::template rebind_alloc<array_type>;
    using blob_alloc_type   = typename alloc_traits::template rebind_alloc<blob_type>;

    using string_ptr_type = typename string_alloc_type::pointer;
    using object_ptr_type = typename object_alloc_type::pointer;
    using array_ptr_type  = typename array_alloc_type::pointer;
    using blob_ptr_type   = typename blob_alloc_type::pointer;
};

/**
 * @brief 数据类型枚举
 */
enum class type_id {
    null_type = 0, ///< 空类型
    int8_type,     ///< 有符号8位整数
    uint8_type,    ///< 无符号8位整数
    int16_type,    ///< 有符号16位整数
    uint16_type,   ///< 无符号16位整数
    int32_type,    ///< 有符号32位整数
    uint32_type,   ///< 无符号32位整数
    int64_type,    ///< 有符号64位整数
    uint64_type,   ///< 无符号64位整数
    double_type,   ///< 双精度浮点数
    bool_type,     ///< 布尔类型
    string_type,   ///< 字符串类型
    array_type,    ///< 数组类型
    object_type,   ///< 对象类型
    blob_type,     ///< 二进制数据类型
    max_type       ///< 最大类型值（用于边界检查）
};

const char*       get_type_name_internal(type_id type);
[[noreturn]] void throw_type_error(const char* expected_type, type_id actual_type);
[[noreturn]] void throw_unknow_type_error(type_id actual_type);
[[noreturn]] void throw_invalid_type_comparison_error(const char* type1, const char* type2,
                                                      const char* op);
size_t            calculate_str_hash(const char* data, size_t length);
template <typename Config>
size_t calculate_array_hash(const variants_base<Config>& array_data);

namespace detail {
// 普通整数类型转 variant_base 的整数类型
template <typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
static type_id fixed_integer_type(T) {
    if constexpr (std::is_signed_v<T>) {
        if constexpr (sizeof(T) == 1) {
            return type_id::int8_type;
        } else if constexpr (sizeof(T) == 2) {
            return type_id::int16_type;
        } else if constexpr (sizeof(T) == 4) {
            return type_id::int32_type;
        }
        return type_id::int64_type;
    } else {
        if constexpr (sizeof(T) == 1) {
            return type_id::uint8_type;
        } else if constexpr (sizeof(T) == 2) {
            return type_id::uint16_type;
        } else if constexpr (sizeof(T) == 4) {
            return type_id::uint32_type;
        }
        return type_id::uint64_type;
    }
}
} // namespace detail

using variant = variant_base<variant_config<>>;

template <typename Config>
std::string to_string(const variant_base<Config>& v) {
    return v.to_string();
}

} // namespace mc

#endif // MC_VARIANT_COMMON_H