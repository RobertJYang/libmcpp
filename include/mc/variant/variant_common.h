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
#include <variant>

#include <mc/common.h>
#include <mc/memory.h>
#include <mc/pretty_name.h>

// 前向声明
namespace mc {
template <typename T, typename Allocator>
class array;
class variants;
class variant_reference;
class dict;
class variant_base;
class variant_extension_base;
} // namespace mc

#ifndef VARIANT_FLOAT_EPSILON
#define VARIANT_FLOAT_EPSILON MC_FLOAT_EPSILON
#endif

namespace mc {

/**
 * @brief 类型特征，用于识别固定长度的整数类型
 */
template <typename T>
inline constexpr bool is_variant_integer_v =
    std::is_same_v<std::decay_t<T>, bool> ||
    std::is_integral_v<std::decay_t<T>>;

template <typename T>
inline constexpr bool is_variant_string_v =
    std::is_same_v<T, std::string_view> ||
    std::is_same_v<T, std::string> ||
    std::is_same_v<T, char*> || std::is_same_v<T, const char*>;

template <typename T>
inline constexpr bool is_variant_fundamental_v =
    is_variant_integer_v<std::decay_t<T>> ||
    std::is_floating_point_v<std::decay_t<T>> ||
    is_variant_string_v<std::decay_t<T>>;

namespace detail {
template <typename, typename T>
inline constexpr bool check_is_variant_v = false;
template <typename T>
inline constexpr bool check_is_variant_v<std::enable_if_t<std::decay_t<T>::is_variant::value>, T> =
    true;

} // namespace detail

template <typename T>
inline constexpr bool is_variant_v = detail::check_is_variant_v<void, T>;

// variant_reference 类型特征
template <typename T>
struct is_variant_reference : std::false_type {};

template <>
struct is_variant_reference<variant_reference> : std::true_type {};

template <typename T>
constexpr bool is_variant_reference_v = is_variant_reference<T>::value;

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

    void operator+=(const blob_base& other) {
        data.reserve(data.size() + other.data.size());
        data.insert(data.end(), other.data.begin(), other.data.end());
    }

    void operator+=(std::string_view other) {
        data.reserve(data.size() + other.size());
        data.insert(data.end(), other.begin(), other.end());
    }

    std::string_view as_string_view() const {
        return std::string_view(data.data(), data.size());
    }
};
using blob = blob_base<>;

template <typename Allocator = std::allocator<char>>
struct variant_config {
    using alloc_traits    = std::allocator_traits<Allocator>;
    using char_alloc_type = typename alloc_traits::template rebind_alloc<char>;
    using variant_alloc_type =
        typename alloc_traits::template rebind_alloc<variant_base>;

    using allocator_type = Allocator;
    using string_type    = std::basic_string<char, std::char_traits<char>, char_alloc_type>;
    using object_type    = dict;
    using array_type     = variants;
    using blob_type      = blob_base<char_alloc_type>;
    using extension_type = variant_extension_base;

    using string_alloc_type = typename alloc_traits::template rebind_alloc<string_type>;
    using array_alloc_type  = typename alloc_traits::template rebind_alloc<array_type>;
    using blob_alloc_type   = typename alloc_traits::template rebind_alloc<blob_type>;

    using string_ptr_type    = typename string_alloc_type::pointer;
    using array_ptr_type     = array_type;
    using blob_ptr_type      = typename blob_alloc_type::pointer;
    using extension_ptr_type = typename mc::shared_ptr<extension_type>;
};

/**
 * @brief 数据类型枚举
 */
enum class type_id {
    null_type = 0,  ///< 空类型
    int8_type,      ///< 有符号8位整数
    uint8_type,     ///< 无符号8位整数
    int16_type,     ///< 有符号16位整数
    uint16_type,    ///< 无符号16位整数
    int32_type,     ///< 有符号32位整数
    uint32_type,    ///< 无符号32位整数
    int64_type,     ///< 有符号64位整数
    uint64_type,    ///< 无符号64位整数
    double_type,    ///< 双精度浮点数
    bool_type,      ///< 布尔类型
    string_type,    ///< 字符串类型
    array_type,     ///< 数组类型
    object_type,    ///< 对象类型
    blob_type,      ///< 二进制数据类型
    extension_type, ///< 扩展类型
    max_type        ///< 最大类型值（用于边界检查）
};

[[noreturn]] MC_API void throw_type_error(const char* expected_type, type_id actual_type);
MC_API const char*       get_type_name_internal(type_id type);
[[noreturn]] MC_API void throw_unknow_type_error(type_id actual_type);
[[noreturn]] MC_API void throw_invalid_type_comparison_error(const char* type1, const char* type2,
                                                             const char* op);
[[noreturn]] MC_API void throw_invalid_type_operation_error(const char* type1, const char* type2,
                                                            const char* op);
[[noreturn]] MC_API void throw_divide_by_zero_exception(const char* msg);
[[noreturn]] MC_API void throw_out_of_range_error(const char* msg);
[[noreturn]] MC_API void throw_out_of_range_error(size_t index, size_t size);
[[noreturn]] MC_API void throw_bad_cast_error(const char* msg);
[[noreturn]] MC_API void throw_runtime_error(const char* msg);
[[noreturn]] MC_API void throw_not_supported_error(const char* operation);
[[noreturn]] MC_API void throw_extension_null_error();
[[noreturn]] MC_API void throw_container_overflow_error(const char* container_type);
MC_API size_t            calculate_str_hash(std::string_view data);
MC_API size_t            calculate_array_hash(const variants& array_data);

namespace detail {

// 普通整数类型转 variant_base 的整数类型
template <typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
static type_id fixed_integer_type() {
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

template <typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
static auto fixed_integer(T val) {
    if constexpr (std::is_signed_v<T>) {
        if constexpr (sizeof(T) == 1) {
            return static_cast<int8_t>(val);
        } else if constexpr (sizeof(T) == 2) {
            return static_cast<int16_t>(val);
        } else if constexpr (sizeof(T) == 4) {
            return static_cast<int32_t>(val);
        } else {
            return static_cast<int64_t>(val);
        }
    } else {
        if constexpr (sizeof(T) == 1) {
            return static_cast<uint8_t>(val);
        } else if constexpr (sizeof(T) == 2) {
            return static_cast<uint16_t>(val);
        } else if constexpr (sizeof(T) == 4) {
            return static_cast<uint32_t>(val);
        } else {
            return static_cast<uint64_t>(val);
        }
    }
}

using numeric_value_t = std::variant<
    bool,
    int8_t, uint8_t,
    int16_t, uint16_t,
    int32_t, uint32_t,
    int64_t, uint64_t,
    float, double>;

template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
numeric_value_t make_numeric_value(T val) {
    if constexpr (std::is_same_v<T, bool>) {
        return numeric_value_t(val);
    } else if constexpr (std::is_integral_v<T>) {
        return numeric_value_t(fixed_integer(val));
    } else if constexpr (std::is_floating_point_v<T>) {
        return numeric_value_t(static_cast<double>(val));
    }
    throw_invalid_type_operation_error(mc::pretty_name<T>(), "numeric_t", "make_numeric");
}

struct numeric_t {
    numeric_t() = default;

    // explicit构造函数，防止隐式转换
    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    explicit numeric_t(T value) : data(make_numeric_value(value)) {
    }

    explicit numeric_t(numeric_value_t value) : data(std::move(value)) {
    }

    numeric_value_t data;
};

template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
numeric_t make_numeric(T val) {
    return numeric_t(make_numeric_value(val));
}

template <typename T>
T get_numeric(const numeric_t& val) {
    return std::visit([](auto&& v) -> T {
        return static_cast<T>(v);
    }, val.data);
}
} // namespace detail

using variant = variant_base;

MC_API std::string to_string(const variant_base& v);

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
    static constexpr bool is_constructible = mc::is_variant_fundamental_v<T> ||
                                             std::is_same_v<std::decay_t<T>, mc::dict> ||
                                             std::is_same_v<std::decay_t<T>, mc::blob> ||
                                             std::is_same_v<std::decay_t<T>, mc::variants>;

    // 第二步：仅当不可构造时，才检查是否有 to_variant 函数
    template <typename U, bool IsConstructible>
    struct check_to_variant {
        static constexpr bool value = true;
    };

    template <typename U>
    struct check_to_variant<U, false> {
        static constexpr bool value = detail::has_to_variant_function_v<U>;
    };

    static constexpr bool value = check_to_variant<T, is_constructible>::value;
};

} // namespace detail

template <typename T>
inline constexpr bool is_variant_constructible_v = detail::is_variant_constructible<T>::value;

inline std::ostream& operator<<(std::ostream& os, const type_id& type) {
    return os << static_cast<int>(type);
}
template <typename Allocator>
inline std::ostream& operator<<(std::ostream& os, const blob_base<Allocator>& blob) {
    return os << "blob[" << blob.data.size() << "]";
}
} // namespace mc

#endif // MC_VARIANT_COMMON_H