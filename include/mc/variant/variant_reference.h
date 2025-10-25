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
 * @file variant_reference.h
 * @brief 定义了 variant 的代理引用类型，用于支持统一的 operator[] 访问
 */
#ifndef MC_VARIANT_REFERENCE_H
#define MC_VARIANT_REFERENCE_H

#include <functional>
#include <memory>
#include <variant>

#include <mc/variant/variant_common.h>
#include <mc/variant/variants.h>

namespace mc {

template <typename Config>
class variant_base;

class variant_extension_base;

// 前向声明
class variants;

/**
 * @brief variant 的代理引用类型
 *
 * 这个类型用于统一处理 variant 的 operator[] 返回值：
 * - 对于 array/object 类型：持有内部元素的指针，零开销
 * - 对于 extension 类型：通过 get/set 方法访问，持有临时值
 *
 * 支持链式访问：var[0]["key"][1]
 */
template <typename Config>
class MC_API variant_reference {
public:
    using variant_type = variant_base<Config>;

private:
    // 存储访问信息的结构
    struct extension_accessor {
        mc::shared_ptr<variant_extension_base> extension;
        std::variant<std::size_t, std::string> key;
        mutable std::optional<variant_type>    cached_value; // 缓存获取的值

        extension_accessor(mc::shared_ptr<variant_extension_base> ext, std::size_t idx)
            : extension(std::move(ext)), key(idx) {
        }

        extension_accessor(mc::shared_ptr<variant_extension_base> ext, std::string k)
            : extension(std::move(ext)), key(std::move(k)) {
        }
    };

    // 存储 variants 访问信息的结构
    struct variants_accessor {
        variants                            container;
        std::size_t                         index;
        mutable std::optional<variant_type> cached_value; // 缓存获取的值

        variants_accessor(variants cont, std::size_t idx)
            : container(std::move(cont)), index(idx) {
        }
    };

    // 存储临时值的结构（用于右值构造函数）
    struct value_holder {
        variant_type value;

        value_holder(variant_type&& val)
            : value(std::move(val)) {
        }
    };

    std::variant<variant_type*, extension_accessor, variants_accessor, value_holder> m_holder;

public:
    variant_reference() = default;

    // 构造函数：引用模式（用于 array/object）
    explicit variant_reference(variant_type& var);

    // 构造函数：值模式（用于临时对象）
    explicit variant_reference(variant_type&& var);

    // 构造函数：extension 索引访问
    variant_reference(mc::shared_ptr<variant_extension_base> ext, std::size_t index);

    // 构造函数：extension 键访问
    variant_reference(mc::shared_ptr<variant_extension_base> ext, std::string key);

    // 构造函数：variants 索引访问
    variant_reference(variants cont, std::size_t index);

    // 拷贝构造和赋值
    variant_reference(const variant_reference&)            = default;
    variant_reference(variant_reference&&)                 = default;
    variant_reference& operator=(const variant_reference&) = delete; // 禁止拷贝赋值
    variant_reference& operator=(variant_reference&& other) noexcept;

    void swap(variant_reference& other) noexcept;

    template <typename OtherConfig>
    friend void swap(variant_reference<OtherConfig> lhs, variant_reference<OtherConfig> rhs) noexcept;

    /**
     * @brief 获取底层的 variant 引用（可修改）
     */
    variant_type& get();

    /**
     * @brief 获取底层的 variant 引用（只读）
     */
    const variant_type& get() const;

    /**
     * @brief 隐式转换为 variant（用于读取）
     */
    operator const variant_type&() const;

    /**
     * @brief 隐式转换为 variant（非 const 版本）
     */
    operator variant_type&();

    /**
     * @brief 赋值操作符（用于修改）
     */
    variant_reference& operator=(const variant_type& value);

    // 支持从其他类型赋值（会转换为 variant）
    // 使用 SFINAE 排除 T 为 variant_reference 的情况，避免歧义
    template <typename T, typename = std::enable_if_t<!is_variant_reference_v<std::decay_t<T>>>>
    variant_reference& operator=(const T& value);

    /**
     * @brief 支持链式索引访问
     */
    variant_reference operator[](std::size_t pos);

    /**
     * @brief 支持链式索引访问（只读）
     */
    variant_reference operator[](std::size_t pos) const;

    /**
     * @brief 支持链式键访问
     */
    variant_reference operator[](std::string_view key);

    /**
     * @brief 支持链式键访问（只读）
     */
    variant_reference operator[](std::string_view key) const;

    /**
     * @brief 解引用操作符
     */
    variant_type& operator*();

    const variant_type& operator*() const;

    /**
     * @brief 成员访问操作符
     */
    variant_type* operator->();

    const variant_type* operator->() const;

    /**
     * @brief 代理 variant 的 as 方法
     */
    template <typename T>
    T as() const {
        return get().template as<T>();
    }

    // ========== 类型判断方法 ==========
    bool is_null() const;
    bool is_string() const;
    bool is_bool() const;

    // 整数类型判断
    bool is_int8() const;
    bool is_uint8() const;
    bool is_int16() const;
    bool is_uint16() const;
    bool is_int32() const;
    bool is_uint32() const;
    bool is_int64() const;
    bool is_uint64() const;

    // 其他类型判断
    bool is_double() const;
    bool is_object() const;
    bool is_dict() const;
    bool is_array() const;
    bool is_blob() const;
    bool is_extension() const;

    // 组合类型判断
    bool is_numeric() const;
    bool is_integer() const;
    bool is_signed_integer() const;
    bool is_unsigned_integer() const;

    // ========== 类型转换方法 ==========

    // 整数类型转换
    int8_t   as_int8() const;
    uint8_t  as_uint8() const;
    int16_t  as_int16() const;
    uint16_t as_uint16() const;
    int32_t  as_int32() const;
    uint32_t as_uint32() const;
    int64_t  as_int64() const;
    uint64_t as_uint64() const;

    // 其他基础类型转换
    bool        as_bool(bool strict = false) const;
    double      as_double() const;
    std::string as_string() const;

    // 容器类型转换
    typename variant_type::array_type as_array() const;
    mc::dict                          as_dict() const;
    mc::dict                          as_mutable_dict() const;
    mc::dict                          as_object() const;

    // blob 和 extension 类型转换
    blob_base<>                               as_blob() const;
    typename variant_type::extension_ptr_type as_extension() const;

    // ========== getter 方法 ==========
    const std::string&                       get_string() const;
    const typename variant_type::blob_type&  get_blob() const;
    const typename variant_type::array_type& get_array() const;
    const mc::dict&                          get_object() const;

    // ========== 其他常用方法 ==========
    const char* get_type_name() const;

    type_id get_type() const;

    bool contains(std::string_view key) const;

    std::size_t size() const;

    size_t hash() const;

    variant_type deep_copy() const;

    void clear();

    // ========== 算术操作符 ==========

    // 与另一个 variant_reference 的算术操作
    template <typename OtherConfig>
    variant_type operator+(const variant_reference<OtherConfig>& other) const;

    template <typename OtherConfig>
    variant_type operator-(const variant_reference<OtherConfig>& other) const;

    template <typename OtherConfig>
    variant_type operator*(const variant_reference<OtherConfig>& other) const;

    template <typename OtherConfig>
    variant_type operator/(const variant_reference<OtherConfig>& other) const;

    template <typename OtherConfig>
    variant_type operator%(const variant_reference<OtherConfig>& other) const;

    // 与 variant_base 的算术操作
    template <typename OtherConfig>
    variant_type operator+(const variant_base<OtherConfig>& other) const;

    template <typename OtherConfig>
    variant_type operator-(const variant_base<OtherConfig>& other) const;

    template <typename OtherConfig>
    variant_type operator*(const variant_base<OtherConfig>& other) const;

    template <typename OtherConfig>
    variant_type operator/(const variant_base<OtherConfig>& other) const;

    template <typename OtherConfig>
    variant_type operator%(const variant_base<OtherConfig>& other) const;

    // 与基础类型的算术操作
    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    variant_type operator+(T other) const;

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    variant_type operator-(T other) const;

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    variant_type operator*(T other) const;

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    variant_type operator/(T other) const;

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    variant_type operator%(T other) const;

    // 字符串拼接
    variant_type operator+(std::string_view other) const;

    // ========== 位操作符 ==========

    // 与另一个 variant_reference 的位操作
    template <typename OtherConfig>
    variant_type operator&(const variant_reference<OtherConfig>& other) const;

    template <typename OtherConfig>
    variant_type operator|(const variant_reference<OtherConfig>& other) const;

    template <typename OtherConfig>
    variant_type operator^(const variant_reference<OtherConfig>& other) const;

    template <typename OtherConfig>
    variant_type operator<<(const variant_reference<OtherConfig>& other) const;

    template <typename OtherConfig>
    variant_type operator>>(const variant_reference<OtherConfig>& other) const;

    // 与 variant_base 的位操作
    template <typename OtherConfig>
    variant_type operator&(const variant_base<OtherConfig>& other) const;

    template <typename OtherConfig>
    variant_type operator|(const variant_base<OtherConfig>& other) const;

    template <typename OtherConfig>
    variant_type operator^(const variant_base<OtherConfig>& other) const;

    variant_type operator~() const;

    template <typename OtherConfig>
    variant_type operator<<(const variant_base<OtherConfig>& other) const;

    template <typename OtherConfig>
    variant_type operator>>(const variant_base<OtherConfig>& other) const;

    // 与基础类型的位操作
    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    variant_type operator&(T other) const;

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    variant_type operator|(T other) const;

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    variant_type operator^(T other) const;

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    variant_type operator<<(T other) const;

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    variant_type operator>>(T other) const;

    // ========== 复合赋值操作符 ==========

    // 与另一个 variant_reference 的复合赋值
    template <typename OtherConfig>
    variant_reference& operator+=(const variant_reference<OtherConfig>& other);

    template <typename OtherConfig>
    variant_reference& operator-=(const variant_reference<OtherConfig>& other);

    template <typename OtherConfig>
    variant_reference& operator*=(const variant_reference<OtherConfig>& other);

    template <typename OtherConfig>
    variant_reference& operator/=(const variant_reference<OtherConfig>& other);

    template <typename OtherConfig>
    variant_reference& operator%=(const variant_reference<OtherConfig>& other);

    template <typename OtherConfig>
    variant_reference& operator&=(const variant_reference<OtherConfig>& other);

    template <typename OtherConfig>
    variant_reference& operator|=(const variant_reference<OtherConfig>& other);

    template <typename OtherConfig>
    variant_reference& operator^=(const variant_reference<OtherConfig>& other);

    template <typename OtherConfig>
    variant_reference& operator<<=(const variant_reference<OtherConfig>& other);

    template <typename OtherConfig>
    variant_reference& operator>>=(const variant_reference<OtherConfig>& other);

    // 与 variant_base 的复合赋值
    template <typename OtherConfig>
    variant_reference& operator+=(const variant_base<OtherConfig>& other);

    template <typename OtherConfig>
    variant_reference& operator-=(const variant_base<OtherConfig>& other);

    template <typename OtherConfig>
    variant_reference& operator*=(const variant_base<OtherConfig>& other);

    template <typename OtherConfig>
    variant_reference& operator/=(const variant_base<OtherConfig>& other);

    template <typename OtherConfig>
    variant_reference& operator%=(const variant_base<OtherConfig>& other);

    template <typename OtherConfig>
    variant_reference& operator&=(const variant_base<OtherConfig>& other);

    template <typename OtherConfig>
    variant_reference& operator|=(const variant_base<OtherConfig>& other);

    template <typename OtherConfig>
    variant_reference& operator^=(const variant_base<OtherConfig>& other);

    template <typename OtherConfig>
    variant_reference& operator<<=(const variant_base<OtherConfig>& other);

    template <typename OtherConfig>
    variant_reference& operator>>=(const variant_base<OtherConfig>& other);

    // 与基础类型的复合赋值
    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    variant_reference& operator+=(const T& other);

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    variant_reference& operator-=(const T& other);

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    variant_reference& operator*=(const T& other);

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    variant_reference& operator/=(const T& other);

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    variant_reference& operator%=(const T& other);

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    variant_reference& operator&=(const T& other);

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    variant_reference& operator|=(const T& other);

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    variant_reference& operator^=(const T& other);

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    variant_reference& operator<<=(const T& other);

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    variant_reference& operator>>=(const T& other);

    // 字符串拼接复合赋值
    variant_reference& operator+=(std::string_view other);

    // ========== 自增自减操作符 ==========
    variant_reference& operator++();

    variant_reference& operator--();

    variant_type operator++(int);

    variant_type operator--(int);

    // ========== 一元操作符 ==========
    variant_type operator-() const;

    variant_type operator!() const;

    explicit operator bool() const;

    // ========== 比较操作符 ==========

    // 与另一个 variant_reference 比较
    template <typename OtherConfig>
    bool operator==(const variant_reference<OtherConfig>& other) const;

    template <typename OtherConfig>
    bool operator!=(const variant_reference<OtherConfig>& other) const;

    template <typename OtherConfig>
    bool operator<(const variant_reference<OtherConfig>& other) const;

    template <typename OtherConfig>
    bool operator>(const variant_reference<OtherConfig>& other) const;

    template <typename OtherConfig>
    bool operator<=(const variant_reference<OtherConfig>& other) const;

    template <typename OtherConfig>
    bool operator>=(const variant_reference<OtherConfig>& other) const;

    // 与 variant_base 比较
    template <typename OtherConfig>
    bool operator==(const variant_base<OtherConfig>& other) const;

    template <typename OtherConfig>
    bool operator!=(const variant_base<OtherConfig>& other) const;

    template <typename OtherConfig>
    bool operator<(const variant_base<OtherConfig>& other) const;

    template <typename OtherConfig>
    bool operator>(const variant_base<OtherConfig>& other) const;

    template <typename OtherConfig>
    bool operator<=(const variant_base<OtherConfig>& other) const;

    template <typename OtherConfig>
    bool operator>=(const variant_base<OtherConfig>& other) const;

    // 与基础类型比较
    // 使用 SFINAE 排除 T 为 variant_reference 的情况，避免与全局操作符版本产生歧义
    template <typename T, typename = std::enable_if_t<!is_variant_reference_v<std::decay_t<T>>>>
    bool operator==(const T& other) const;

    template <typename T, typename = std::enable_if_t<!is_variant_reference_v<std::decay_t<T>>>>
    bool operator!=(const T& other) const;

    template <typename T, typename = std::enable_if_t<!is_variant_reference_v<std::decay_t<T>>>>
    bool operator<(const T& other) const;

    template <typename T, typename = std::enable_if_t<!is_variant_reference_v<std::decay_t<T>>>>
    bool operator>(const T& other) const;

    template <typename T, typename = std::enable_if_t<!is_variant_reference_v<std::decay_t<T>>>>
    bool operator<=(const T& other) const;

    template <typename T, typename = std::enable_if_t<!is_variant_reference_v<std::decay_t<T>>>>
    bool operator>=(const T& other) const;

    // 字符串比较
    bool operator==(std::string_view other) const;

    bool operator!=(std::string_view other) const;

    bool operator<(std::string_view other) const;

    bool operator>(std::string_view other) const;

    bool operator<=(std::string_view other) const;

    bool operator>=(std::string_view other) const;

    // dict 比较
    bool operator==(const mc::dict& other) const;

    bool operator!=(const mc::dict& other) const;

    // blob 比较
    template <typename OtherAllocator>
    bool operator==(const blob_base<OtherAllocator>& other) const;

    template <typename OtherAllocator>
    bool operator!=(const blob_base<OtherAllocator>& other) const;

    template <typename OtherAllocator>
    bool operator<(const blob_base<OtherAllocator>& other) const;

    template <typename OtherAllocator>
    bool operator>(const blob_base<OtherAllocator>& other) const;

    template <typename OtherAllocator>
    bool operator<=(const blob_base<OtherAllocator>& other) const;

    template <typename OtherAllocator>
    bool operator>=(const blob_base<OtherAllocator>& other) const;

    // array 比较
    template <typename OtherConfig>
    bool operator==(const std::vector<variant_base<OtherConfig>>& other) const;

    template <typename OtherConfig>
    bool operator!=(const std::vector<variant_base<OtherConfig>>& other) const;

    template <typename OtherConfig>
    bool operator<(const std::vector<variant_base<OtherConfig>>& other) const;

    template <typename OtherConfig>
    bool operator>(const std::vector<variant_base<OtherConfig>>& other) const;

    template <typename OtherConfig>
    bool operator<=(const std::vector<variant_base<OtherConfig>>& other) const;

    template <typename OtherConfig>
    bool operator>=(const std::vector<variant_base<OtherConfig>>& other) const;
};

// 赋值操作符实现
template <typename Config>
template <typename T, typename>
inline variant_reference<Config>& variant_reference<Config>::operator=(const T& value) {
    return *this = variant_type(value);
}

// 算术操作符实现
template <typename Config>
template <typename OtherConfig>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator+(const variant_reference<OtherConfig>& other) const {
    return get() + other.get();
}

template <typename Config>
template <typename OtherConfig>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator-(const variant_reference<OtherConfig>& other) const {
    return get() - other.get();
}

template <typename Config>
template <typename OtherConfig>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator*(const variant_reference<OtherConfig>& other) const {
    return get() * other.get();
}

template <typename Config>
template <typename OtherConfig>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator/(const variant_reference<OtherConfig>& other) const {
    return get() / other.get();
}

template <typename Config>
template <typename OtherConfig>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator%(const variant_reference<OtherConfig>& other) const {
    return get() % other.get();
}

template <typename Config>
template <typename OtherConfig>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator+(const variant_base<OtherConfig>& other) const {
    return get() + other;
}

template <typename Config>
template <typename OtherConfig>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator-(const variant_base<OtherConfig>& other) const {
    return get() - other;
}

template <typename Config>
template <typename OtherConfig>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator*(const variant_base<OtherConfig>& other) const {
    return get() * other;
}

template <typename Config>
template <typename OtherConfig>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator/(const variant_base<OtherConfig>& other) const {
    return get() / other;
}

template <typename Config>
template <typename OtherConfig>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator%(const variant_base<OtherConfig>& other) const {
    return get() % other;
}

template <typename Config>
template <typename T, typename>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator+(T other) const {
    return get() + other;
}

template <typename Config>
template <typename T, typename>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator-(T other) const {
    return get() - other;
}

template <typename Config>
template <typename T, typename>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator*(T other) const {
    return get() * other;
}

template <typename Config>
template <typename T, typename>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator/(T other) const {
    return get() / other;
}

template <typename Config>
template <typename T, typename>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator%(T other) const {
    return get() % other;
}

template <typename Config>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator+(std::string_view other) const {
    return get() + other;
}

// 位操作符实现
template <typename Config>
template <typename OtherConfig>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator&(const variant_reference<OtherConfig>& other) const {
    return get() & other.get();
}

template <typename Config>
template <typename OtherConfig>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator|(const variant_reference<OtherConfig>& other) const {
    return get() | other.get();
}

template <typename Config>
template <typename OtherConfig>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator^(const variant_reference<OtherConfig>& other) const {
    return get() ^ other.get();
}

template <typename Config>
template <typename OtherConfig>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator<<(const variant_reference<OtherConfig>& other) const {
    return get() << other.get();
}

template <typename Config>
template <typename OtherConfig>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator>>(const variant_reference<OtherConfig>& other) const {
    return get() >> other.get();
}

template <typename Config>
template <typename OtherConfig>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator&(const variant_base<OtherConfig>& other) const {
    return get() & other;
}

template <typename Config>
template <typename OtherConfig>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator|(const variant_base<OtherConfig>& other) const {
    return get() | other;
}

template <typename Config>
template <typename OtherConfig>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator^(const variant_base<OtherConfig>& other) const {
    return get() ^ other;
}

template <typename Config>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator~() const {
    return ~get();
}

template <typename Config>
template <typename OtherConfig>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator<<(const variant_base<OtherConfig>& other) const {
    return get() << other;
}

template <typename Config>
template <typename OtherConfig>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator>>(const variant_base<OtherConfig>& other) const {
    return get() >> other;
}

template <typename Config>
template <typename T, typename>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator&(T other) const {
    return get() & other;
}

template <typename Config>
template <typename T, typename>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator|(T other) const {
    return get() | other;
}

template <typename Config>
template <typename T, typename>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator^(T other) const {
    return get() ^ other;
}

template <typename Config>
template <typename T, typename>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator<<(T other) const {
    return get() << other;
}

template <typename Config>
template <typename T, typename>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator>>(T other) const {
    return get() >> other;
}

// 复合赋值操作符实现
template <typename Config>
template <typename OtherConfig>
inline variant_reference<Config>& variant_reference<Config>::operator+=(const variant_reference<OtherConfig>& other) {
    *this = get() + other.get();
    return *this;
}

template <typename Config>
template <typename OtherConfig>
inline variant_reference<Config>& variant_reference<Config>::operator-=(const variant_reference<OtherConfig>& other) {
    *this = get() - other.get();
    return *this;
}

template <typename Config>
template <typename OtherConfig>
inline variant_reference<Config>& variant_reference<Config>::operator*=(const variant_reference<OtherConfig>& other) {
    *this = get() * other.get();
    return *this;
}

template <typename Config>
template <typename OtherConfig>
inline variant_reference<Config>& variant_reference<Config>::operator/=(const variant_reference<OtherConfig>& other) {
    *this = get() / other.get();
    return *this;
}

template <typename Config>
template <typename OtherConfig>
inline variant_reference<Config>& variant_reference<Config>::operator%=(const variant_reference<OtherConfig>& other) {
    *this = get() % other.get();
    return *this;
}

template <typename Config>
template <typename OtherConfig>
inline variant_reference<Config>& variant_reference<Config>::operator&=(const variant_reference<OtherConfig>& other) {
    *this = get() & other.get();
    return *this;
}

template <typename Config>
template <typename OtherConfig>
inline variant_reference<Config>& variant_reference<Config>::operator|=(const variant_reference<OtherConfig>& other) {
    *this = get() | other.get();
    return *this;
}

template <typename Config>
template <typename OtherConfig>
inline variant_reference<Config>& variant_reference<Config>::operator^=(const variant_reference<OtherConfig>& other) {
    *this = get() ^ other.get();
    return *this;
}

template <typename Config>
template <typename OtherConfig>
inline variant_reference<Config>& variant_reference<Config>::operator<<=(const variant_reference<OtherConfig>& other) {
    *this = get() << other.get();
    return *this;
}

template <typename Config>
template <typename OtherConfig>
inline variant_reference<Config>& variant_reference<Config>::operator>>=(const variant_reference<OtherConfig>& other) {
    *this = get() >> other.get();
    return *this;
}

template <typename Config>
template <typename OtherConfig>
inline variant_reference<Config>& variant_reference<Config>::operator+=(const variant_base<OtherConfig>& other) {
    *this = get() + other;
    return *this;
}

template <typename Config>
template <typename OtherConfig>
inline variant_reference<Config>& variant_reference<Config>::operator-=(const variant_base<OtherConfig>& other) {
    *this = get() - other;
    return *this;
}

template <typename Config>
template <typename OtherConfig>
inline variant_reference<Config>& variant_reference<Config>::operator*=(const variant_base<OtherConfig>& other) {
    *this = get() * other;
    return *this;
}

template <typename Config>
template <typename OtherConfig>
inline variant_reference<Config>& variant_reference<Config>::operator/=(const variant_base<OtherConfig>& other) {
    *this = get() / other;
    return *this;
}

template <typename Config>
template <typename OtherConfig>
inline variant_reference<Config>& variant_reference<Config>::operator%=(const variant_base<OtherConfig>& other) {
    *this = get() % other;
    return *this;
}

template <typename Config>
template <typename OtherConfig>
inline variant_reference<Config>& variant_reference<Config>::operator&=(const variant_base<OtherConfig>& other) {
    *this = get() & other;
    return *this;
}

template <typename Config>
template <typename OtherConfig>
inline variant_reference<Config>& variant_reference<Config>::operator|=(const variant_base<OtherConfig>& other) {
    *this = get() | other;
    return *this;
}

template <typename Config>
template <typename OtherConfig>
inline variant_reference<Config>& variant_reference<Config>::operator^=(const variant_base<OtherConfig>& other) {
    *this = get() ^ other;
    return *this;
}

template <typename Config>
template <typename OtherConfig>
inline variant_reference<Config>& variant_reference<Config>::operator<<=(const variant_base<OtherConfig>& other) {
    *this = get() << other;
    return *this;
}

template <typename Config>
template <typename OtherConfig>
inline variant_reference<Config>& variant_reference<Config>::operator>>=(const variant_base<OtherConfig>& other) {
    *this = get() >> other;
    return *this;
}

template <typename Config>
template <typename T, typename>
inline variant_reference<Config>& variant_reference<Config>::operator+=(const T& other) {
    *this = get() + other;
    return *this;
}

template <typename Config>
template <typename T, typename>
inline variant_reference<Config>& variant_reference<Config>::operator-=(const T& other) {
    *this = get() - other;
    return *this;
}

template <typename Config>
template <typename T, typename>
inline variant_reference<Config>& variant_reference<Config>::operator*=(const T& other) {
    *this = get() * other;
    return *this;
}

template <typename Config>
template <typename T, typename>
inline variant_reference<Config>& variant_reference<Config>::operator/=(const T& other) {
    *this = get() / other;
    return *this;
}

template <typename Config>
template <typename T, typename>
inline variant_reference<Config>& variant_reference<Config>::operator%=(const T& other) {
    *this = get() % other;
    return *this;
}

template <typename Config>
template <typename T, typename>
inline variant_reference<Config>& variant_reference<Config>::operator&=(const T& other) {
    *this = get() & other;
    return *this;
}

template <typename Config>
template <typename T, typename>
inline variant_reference<Config>& variant_reference<Config>::operator|=(const T& other) {
    *this = get() | other;
    return *this;
}

template <typename Config>
template <typename T, typename>
inline variant_reference<Config>& variant_reference<Config>::operator^=(const T& other) {
    *this = get() ^ other;
    return *this;
}

template <typename Config>
template <typename T, typename>
inline variant_reference<Config>& variant_reference<Config>::operator<<=(const T& other) {
    *this = get() << other;
    return *this;
}

template <typename Config>
template <typename T, typename>
inline variant_reference<Config>& variant_reference<Config>::operator>>=(const T& other) {
    *this = get() >> other;
    return *this;
}

template <typename Config>
inline variant_reference<Config>& variant_reference<Config>::operator+=(std::string_view other) {
    *this = get() + other;
    return *this;
}

// 自增自减操作符实现
template <typename Config>
inline variant_reference<Config>& variant_reference<Config>::operator++() {
    *this = get() + 1;
    return *this;
}

template <typename Config>
inline variant_reference<Config>& variant_reference<Config>::operator--() {
    *this = get() - 1;
    return *this;
}

template <typename Config>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator++(int) {
    auto old_value = get();
    *this          = old_value + 1;
    return old_value;
}

template <typename Config>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator--(int) {
    auto old_value = get();
    *this          = old_value - 1;
    return old_value;
}

// 一元操作符实现
template <typename Config>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator-() const {
    return -get();
}

template <typename Config>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::operator!() const {
    return !get();
}

template <typename Config>
inline variant_reference<Config>::operator bool() const {
    return static_cast<bool>(get());
}

// 简单的类型检查函数
template <typename Config>
inline bool variant_reference<Config>::is_null() const {
    return get().is_null();
}

template <typename Config>
inline bool variant_reference<Config>::is_string() const {
    return get().is_string();
}

template <typename Config>
inline bool variant_reference<Config>::is_bool() const {
    return get().is_bool();
}

template <typename Config>
inline bool variant_reference<Config>::is_int8() const {
    return get().is_int8();
}

template <typename Config>
inline bool variant_reference<Config>::is_uint8() const {
    return get().is_uint8();
}

template <typename Config>
inline bool variant_reference<Config>::is_int16() const {
    return get().is_int16();
}

template <typename Config>
inline bool variant_reference<Config>::is_uint16() const {
    return get().is_uint16();
}

template <typename Config>
inline bool variant_reference<Config>::is_int32() const {
    return get().is_int32();
}

template <typename Config>
inline bool variant_reference<Config>::is_uint32() const {
    return get().is_uint32();
}

template <typename Config>
inline bool variant_reference<Config>::is_int64() const {
    return get().is_int64();
}

template <typename Config>
inline bool variant_reference<Config>::is_uint64() const {
    return get().is_uint64();
}

template <typename Config>
inline bool variant_reference<Config>::is_double() const {
    return get().is_double();
}

template <typename Config>
inline bool variant_reference<Config>::is_object() const {
    return get().is_object();
}

template <typename Config>
inline bool variant_reference<Config>::is_dict() const {
    return get().is_dict();
}

template <typename Config>
inline bool variant_reference<Config>::is_array() const {
    return get().is_array();
}

template <typename Config>
inline bool variant_reference<Config>::is_blob() const {
    return get().is_blob();
}

template <typename Config>
inline bool variant_reference<Config>::is_extension() const {
    return get().is_extension();
}

template <typename Config>
inline bool variant_reference<Config>::is_numeric() const {
    return get().is_numeric();
}

template <typename Config>
inline bool variant_reference<Config>::is_integer() const {
    return get().is_integer();
}

template <typename Config>
inline bool variant_reference<Config>::is_signed_integer() const {
    return get().is_signed_integer();
}

template <typename Config>
inline bool variant_reference<Config>::is_unsigned_integer() const {
    return get().is_unsigned_integer();
}

// 简单的类型转换函数
template <typename Config>
inline int8_t variant_reference<Config>::as_int8() const {
    return get().as_int8();
}

template <typename Config>
inline uint8_t variant_reference<Config>::as_uint8() const {
    return get().as_uint8();
}

template <typename Config>
inline int16_t variant_reference<Config>::as_int16() const {
    return get().as_int16();
}

template <typename Config>
inline uint16_t variant_reference<Config>::as_uint16() const {
    return get().as_uint16();
}

template <typename Config>
inline int32_t variant_reference<Config>::as_int32() const {
    return get().as_int32();
}

template <typename Config>
inline uint32_t variant_reference<Config>::as_uint32() const {
    return get().as_uint32();
}

template <typename Config>
inline int64_t variant_reference<Config>::as_int64() const {
    return get().as_int64();
}

template <typename Config>
inline uint64_t variant_reference<Config>::as_uint64() const {
    return get().as_uint64();
}

template <typename Config>
inline bool variant_reference<Config>::as_bool(bool strict) const {
    return get().as_bool(strict);
}

template <typename Config>
inline double variant_reference<Config>::as_double() const {
    return get().as_double();
}

template <typename Config>
inline std::string variant_reference<Config>::as_string() const {
    return get().as_string();
}

template <typename Config>
inline typename variant_reference<Config>::variant_type::array_type variant_reference<Config>::as_array() const {
    return get().as_array();
}

template <typename Config>
inline mc::dict variant_reference<Config>::as_dict() const {
    return get().as_dict();
}

template <typename Config>
inline mc::dict variant_reference<Config>::as_mutable_dict() const {
    return get().as_mutable_dict();
}

template <typename Config>
inline mc::dict variant_reference<Config>::as_object() const {
    return get().as_object();
}

template <typename Config>
inline blob_base<> variant_reference<Config>::as_blob() const {
    return get().as_blob();
}

template <typename Config>
inline typename variant_reference<Config>::variant_type::extension_ptr_type variant_reference<Config>::as_extension() const {
    return get().as_extension();
}

// 简单的访问函数
template <typename Config>
inline const std::string& variant_reference<Config>::get_string() const {
    return get().get_string();
}

template <typename Config>
inline const typename variant_reference<Config>::variant_type::blob_type& variant_reference<Config>::get_blob() const {
    return get().get_blob();
}

template <typename Config>
inline const typename variant_reference<Config>::variant_type::array_type& variant_reference<Config>::get_array() const {
    return get().get_array();
}

template <typename Config>
inline const mc::dict& variant_reference<Config>::get_object() const {
    return get().get_object();
}

template <typename Config>
inline const char* variant_reference<Config>::get_type_name() const {
    return get().get_type_name();
}

template <typename Config>
inline type_id variant_reference<Config>::get_type() const {
    return get().get_type();
}

template <typename Config>
inline bool variant_reference<Config>::contains(std::string_view key) const {
    return get().contains(key);
}

template <typename Config>
inline std::size_t variant_reference<Config>::size() const {
    return get().size();
}

template <typename Config>
inline size_t variant_reference<Config>::hash() const {
    return get().hash();
}

template <typename Config>
inline typename variant_reference<Config>::variant_type variant_reference<Config>::deep_copy() const {
    return get().deep_copy();
}

template <typename Config>
inline void variant_reference<Config>::clear() {
    get().clear();
}

// 简单的比较操作符
template <typename Config>
template <typename OtherConfig>
inline bool variant_reference<Config>::operator==(const variant_reference<OtherConfig>& other) const {
    return get() == other.get();
}

template <typename Config>
template <typename OtherConfig>
inline bool variant_reference<Config>::operator!=(const variant_reference<OtherConfig>& other) const {
    return get() != other.get();
}

template <typename Config>
template <typename OtherConfig>
inline bool variant_reference<Config>::operator<(const variant_reference<OtherConfig>& other) const {
    return get() < other.get();
}

template <typename Config>
template <typename OtherConfig>
inline bool variant_reference<Config>::operator>(const variant_reference<OtherConfig>& other) const {
    return get() > other.get();
}

template <typename Config>
template <typename OtherConfig>
inline bool variant_reference<Config>::operator<=(const variant_reference<OtherConfig>& other) const {
    return get() <= other.get();
}

template <typename Config>
template <typename OtherConfig>
inline bool variant_reference<Config>::operator>=(const variant_reference<OtherConfig>& other) const {
    return get() >= other.get();
}

template <typename Config>
template <typename OtherConfig>
inline bool variant_reference<Config>::operator==(const variant_base<OtherConfig>& other) const {
    return get() == other;
}

template <typename Config>
template <typename OtherConfig>
inline bool variant_reference<Config>::operator!=(const variant_base<OtherConfig>& other) const {
    return get() != other;
}

template <typename Config>
template <typename OtherConfig>
inline bool variant_reference<Config>::operator<(const variant_base<OtherConfig>& other) const {
    return get() < other;
}

template <typename Config>
template <typename OtherConfig>
inline bool variant_reference<Config>::operator>(const variant_base<OtherConfig>& other) const {
    return get() > other;
}

template <typename Config>
template <typename OtherConfig>
inline bool variant_reference<Config>::operator<=(const variant_base<OtherConfig>& other) const {
    return get() <= other;
}

template <typename Config>
template <typename OtherConfig>
inline bool variant_reference<Config>::operator>=(const variant_base<OtherConfig>& other) const {
    return get() >= other;
}

template <typename Config>
template <typename T, typename>
inline bool variant_reference<Config>::operator==(const T& other) const {
    return get() == other;
}

template <typename Config>
template <typename T, typename>
inline bool variant_reference<Config>::operator!=(const T& other) const {
    return get() != other;
}

template <typename Config>
template <typename T, typename>
inline bool variant_reference<Config>::operator<(const T& other) const {
    return get() < other;
}

template <typename Config>
template <typename T, typename>
inline bool variant_reference<Config>::operator>(const T& other) const {
    return get() > other;
}

template <typename Config>
template <typename T, typename>
inline bool variant_reference<Config>::operator<=(const T& other) const {
    return get() <= other;
}

template <typename Config>
template <typename T, typename>
inline bool variant_reference<Config>::operator>=(const T& other) const {
    return get() >= other;
}

template <typename Config>
inline bool variant_reference<Config>::operator==(std::string_view other) const {
    return get() == other;
}

template <typename Config>
inline bool variant_reference<Config>::operator!=(std::string_view other) const {
    return get() != other;
}

template <typename Config>
inline bool variant_reference<Config>::operator<(std::string_view other) const {
    return get() < other;
}

template <typename Config>
inline bool variant_reference<Config>::operator>(std::string_view other) const {
    return get() > other;
}

template <typename Config>
inline bool variant_reference<Config>::operator<=(std::string_view other) const {
    return get() <= other;
}

template <typename Config>
inline bool variant_reference<Config>::operator>=(std::string_view other) const {
    return get() >= other;
}

template <typename Config>
inline bool variant_reference<Config>::operator==(const mc::dict& other) const {
    return get() == other;
}

template <typename Config>
inline bool variant_reference<Config>::operator!=(const mc::dict& other) const {
    return get() != other;
}

template <typename Config>
template <typename OtherAllocator>
inline bool variant_reference<Config>::operator==(const blob_base<OtherAllocator>& other) const {
    return get() == other;
}

template <typename Config>
template <typename OtherAllocator>
inline bool variant_reference<Config>::operator!=(const blob_base<OtherAllocator>& other) const {
    return get() != other;
}

template <typename Config>
template <typename OtherAllocator>
inline bool variant_reference<Config>::operator<(const blob_base<OtherAllocator>& other) const {
    return get() < other;
}

template <typename Config>
template <typename OtherAllocator>
inline bool variant_reference<Config>::operator>(const blob_base<OtherAllocator>& other) const {
    return get() > other;
}

template <typename Config>
template <typename OtherAllocator>
inline bool variant_reference<Config>::operator<=(const blob_base<OtherAllocator>& other) const {
    return get() <= other;
}

template <typename Config>
template <typename OtherAllocator>
inline bool variant_reference<Config>::operator>=(const blob_base<OtherAllocator>& other) const {
    return get() >= other;
}

template <typename Config>
template <typename OtherConfig>
inline bool variant_reference<Config>::operator==(const std::vector<variant_base<OtherConfig>>& other) const {
    return get() == other;
}

template <typename Config>
template <typename OtherConfig>
inline bool variant_reference<Config>::operator!=(const std::vector<variant_base<OtherConfig>>& other) const {
    return get() != other;
}

template <typename Config>
template <typename OtherConfig>
inline bool variant_reference<Config>::operator<(const std::vector<variant_base<OtherConfig>>& other) const {
    return get() < other;
}

template <typename Config>
template <typename OtherConfig>
inline bool variant_reference<Config>::operator>(const std::vector<variant_base<OtherConfig>>& other) const {
    return get() > other;
}

template <typename Config>
template <typename OtherConfig>
inline bool variant_reference<Config>::operator<=(const std::vector<variant_base<OtherConfig>>& other) const {
    return get() <= other;
}

template <typename Config>
template <typename OtherConfig>
inline bool variant_reference<Config>::operator>=(const std::vector<variant_base<OtherConfig>>& other) const {
    return get() >= other;
}

template <typename Config>
void to_variant(const variant_reference<Config>& ref, variant_base<Config>& vo) {
    vo = ref.get();
}

template <typename Config>
void to_variant(variant_reference<Config>& ref, variant_base<Config>& vo) {
    vo = ref.get();
}

template <typename Config, typename T>
inline void from_variant(const variant_reference<Config>& ref, T& value) {
    from_variant(ref.get(), value);
}

template <typename Config, typename T>
inline void from_variant(variant_reference<Config>& ref, T& value) {
    from_variant(ref.get(), value);
}

// 算术操作符：基础类型 op variant_reference
template <typename Config, typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base<Config> operator+(T lhs, const variant_reference<Config>& rhs) {
    return lhs + rhs.get();
}

template <typename Config, typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base<Config> operator-(T lhs, const variant_reference<Config>& rhs) {
    return lhs - rhs.get();
}

template <typename Config, typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base<Config> operator*(T lhs, const variant_reference<Config>& rhs) {
    return lhs * rhs.get();
}

template <typename Config, typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base<Config> operator/(T lhs, const variant_reference<Config>& rhs) {
    return lhs / rhs.get();
}

template <typename Config, typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base<Config> operator%(T lhs, const variant_reference<Config>& rhs) {
    return lhs % rhs.get();
}

// 算术操作符：variant_base op variant_reference
template <typename Config, typename OtherConfig>
inline variant_base<Config> operator+(const variant_base<Config>& lhs, const variant_reference<OtherConfig>& rhs) {
    return lhs + rhs.get();
}

template <typename Config, typename OtherConfig>
inline variant_base<Config> operator-(const variant_base<Config>& lhs, const variant_reference<OtherConfig>& rhs) {
    return lhs - rhs.get();
}

template <typename Config, typename OtherConfig>
inline variant_base<Config> operator*(const variant_base<Config>& lhs, const variant_reference<OtherConfig>& rhs) {
    return lhs * rhs.get();
}

template <typename Config, typename OtherConfig>
inline variant_base<Config> operator/(const variant_base<Config>& lhs, const variant_reference<OtherConfig>& rhs) {
    return lhs / rhs.get();
}

template <typename Config, typename OtherConfig>
inline variant_base<Config> operator%(const variant_base<Config>& lhs, const variant_reference<OtherConfig>& rhs) {
    return lhs % rhs.get();
}

// 位操作符：基础类型 op variant_reference
template <typename Config, typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
inline variant_base<Config> operator&(T lhs, const variant_reference<Config>& rhs) {
    return lhs & rhs.get();
}

template <typename Config, typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
inline variant_base<Config> operator|(T lhs, const variant_reference<Config>& rhs) {
    return lhs | rhs.get();
}

template <typename Config, typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
inline variant_base<Config> operator^(T lhs, const variant_reference<Config>& rhs) {
    return lhs ^ rhs.get();
}

template <typename Config, typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
inline variant_base<Config> operator<<(T lhs, const variant_reference<Config>& rhs) {
    return lhs << rhs.get();
}

template <typename Config, typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
inline variant_base<Config> operator>>(T lhs, const variant_reference<Config>& rhs) {
    return lhs >> rhs.get();
}

// 位操作符：variant_base op variant_reference
template <typename Config, typename OtherConfig>
inline variant_base<Config> operator&(const variant_base<Config>& lhs, const variant_reference<OtherConfig>& rhs) {
    return lhs & rhs.get();
}

template <typename Config, typename OtherConfig>
inline variant_base<Config> operator|(const variant_base<Config>& lhs, const variant_reference<OtherConfig>& rhs) {
    return lhs | rhs.get();
}

template <typename Config, typename OtherConfig>
inline variant_base<Config> operator^(const variant_base<Config>& lhs, const variant_reference<OtherConfig>& rhs) {
    return lhs ^ rhs.get();
}

template <typename Config, typename OtherConfig>
inline variant_base<Config> operator<<(const variant_base<Config>& lhs, const variant_reference<OtherConfig>& rhs) {
    return lhs << rhs.get();
}

template <typename Config, typename OtherConfig>
inline variant_base<Config> operator>>(const variant_base<Config>& lhs, const variant_reference<OtherConfig>& rhs) {
    return lhs >> rhs.get();
}

// 比较操作符：基础类型 op variant_reference
// 使用 SFINAE 排除 T 为 variant_reference 的情况，避免与成员函数版本产生歧义
template <typename Config, typename T, typename = std::enable_if_t<!is_variant_reference_v<std::decay_t<T>>>>
inline bool operator==(const T& lhs, const variant_reference<Config>& rhs) {
    return lhs == rhs.get();
}

template <typename Config, typename T, typename = std::enable_if_t<!is_variant_reference_v<std::decay_t<T>>>>
inline bool operator!=(const T& lhs, const variant_reference<Config>& rhs) {
    return lhs != rhs.get();
}

template <typename Config, typename T, typename = std::enable_if_t<!is_variant_reference_v<std::decay_t<T>>>>
inline bool operator<(const T& lhs, const variant_reference<Config>& rhs) {
    return lhs < rhs.get();
}

template <typename Config, typename T, typename = std::enable_if_t<!is_variant_reference_v<std::decay_t<T>>>>
inline bool operator>(const T& lhs, const variant_reference<Config>& rhs) {
    return lhs > rhs.get();
}

template <typename Config, typename T, typename = std::enable_if_t<!is_variant_reference_v<std::decay_t<T>>>>
inline bool operator<=(const T& lhs, const variant_reference<Config>& rhs) {
    return lhs <= rhs.get();
}

template <typename Config, typename T, typename = std::enable_if_t<!is_variant_reference_v<std::decay_t<T>>>>
inline bool operator>=(const T& lhs, const variant_reference<Config>& rhs) {
    return lhs >= rhs.get();
}

// 比较操作符：variant_base op variant_reference
template <typename Config, typename OtherConfig>
inline bool operator==(const variant_base<Config>& lhs, const variant_reference<OtherConfig>& rhs) {
    return lhs == rhs.get();
}

template <typename Config, typename OtherConfig>
inline bool operator!=(const variant_base<Config>& lhs, const variant_reference<OtherConfig>& rhs) {
    return lhs != rhs.get();
}

template <typename Config, typename OtherConfig>
inline bool operator<(const variant_base<Config>& lhs, const variant_reference<OtherConfig>& rhs) {
    return lhs < rhs.get();
}

template <typename Config, typename OtherConfig>
inline bool operator>(const variant_base<Config>& lhs, const variant_reference<OtherConfig>& rhs) {
    return lhs > rhs.get();
}

template <typename Config, typename OtherConfig>
inline bool operator<=(const variant_base<Config>& lhs, const variant_reference<OtherConfig>& rhs) {
    return lhs <= rhs.get();
}

template <typename Config, typename OtherConfig>
inline bool operator>=(const variant_base<Config>& lhs, const variant_reference<OtherConfig>& rhs) {
    return lhs >= rhs.get();
}

// 字符串拼接：string_view op variant_reference
template <typename Config>
inline variant_base<Config> operator+(std::string_view lhs, const variant_reference<Config>& rhs) {
    return lhs + rhs.get();
}

template <typename Config>
inline bool operator==(std::string_view lhs, const variant_reference<Config>& rhs) {
    return lhs == rhs.get();
}

template <typename Config>
inline bool operator!=(std::string_view lhs, const variant_reference<Config>& rhs) {
    return lhs != rhs.get();
}

template <typename Config>
inline bool operator<(std::string_view lhs, const variant_reference<Config>& rhs) {
    return lhs < rhs.get();
}

template <typename Config>
inline bool operator>(std::string_view lhs, const variant_reference<Config>& rhs) {
    return lhs > rhs.get();
}

template <typename Config>
inline bool operator<=(std::string_view lhs, const variant_reference<Config>& rhs) {
    return lhs <= rhs.get();
}

template <typename Config>
inline bool operator>=(std::string_view lhs, const variant_reference<Config>& rhs) {
    return lhs >= rhs.get();
}

} // namespace mc

#endif // MC_VARIANT_REFERENCE_H
