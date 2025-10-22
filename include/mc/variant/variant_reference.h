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

namespace mc {

template <typename Config>
class variant_base;

class variant_extension_base;

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
class variant_reference {
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

    // 持有的数据：指针（引用模式）或访问器（extension 模式）
    std::variant<variant_type*, extension_accessor> m_holder;

public:
    // 构造函数：引用模式（用于 array/object）
    explicit variant_reference(variant_type& var)
        : m_holder(&var) {
    }

    // 构造函数：extension 索引访问
    variant_reference(mc::shared_ptr<variant_extension_base> ext, std::size_t index)
        : m_holder(extension_accessor(std::move(ext), index)) {
    }

    // 构造函数：extension 键访问
    variant_reference(mc::shared_ptr<variant_extension_base> ext, std::string key)
        : m_holder(extension_accessor(std::move(ext), std::move(key))) {
    }

    // 拷贝构造和赋值
    variant_reference(const variant_reference&)            = default;
    variant_reference(variant_reference&&)                 = default;
    variant_reference& operator=(const variant_reference&) = delete; // 禁止拷贝赋值
    variant_reference& operator=(variant_reference&&)      = delete;

    /**
     * @brief 获取底层的 variant 引用（可修改）
     */
    variant_type& get() {
        if (auto* ptr = std::get_if<variant_type*>(&m_holder)) {
            return **ptr;
        } else {
            auto& accessor = std::get<extension_accessor>(m_holder);
            // 对于 extension，需要先获取值，缓存，然后返回引用
            if (!accessor.cached_value.has_value()) {
                if (auto* idx = std::get_if<std::size_t>(&accessor.key)) {
                    accessor.cached_value = accessor.extension->get(*idx);
                } else {
                    accessor.cached_value = accessor.extension->get(std::get<std::string>(accessor.key));
                }
            }
            return *accessor.cached_value;
        }
    }

    /**
     * @brief 获取底层的 variant 引用（只读）
     */
    const variant_type& get() const {
        if (auto* ptr = std::get_if<variant_type*>(&m_holder)) {
            return **ptr;
        } else {
            auto& accessor = std::get<extension_accessor>(m_holder);
            if (!accessor.cached_value.has_value()) {
                if (auto* idx = std::get_if<std::size_t>(&accessor.key)) {
                    accessor.cached_value = accessor.extension->get(*idx);
                } else {
                    accessor.cached_value = accessor.extension->get(std::get<std::string>(accessor.key));
                }
            }
            return *accessor.cached_value;
        }
    }

    /**
     * @brief 隐式转换为 variant（用于读取）
     */
    operator const variant_type&() const {
        return get();
    }

    /**
     * @brief 隐式转换为 variant（非 const 版本）
     */
    operator variant_type&() {
        return get();
    }

    /**
     * @brief 赋值操作符（用于修改）
     */
    variant_reference& operator=(const variant_type& value) {
        if (auto* ptr = std::get_if<variant_type*>(&m_holder)) {
            // 引用模式：直接赋值
            **ptr = value;
        } else {
            // extension 模式：调用 set 方法
            auto& accessor = std::get<extension_accessor>(m_holder);
            if (auto* idx = std::get_if<std::size_t>(&accessor.key)) {
                accessor.extension->set(*idx, value);
            } else {
                accessor.extension->set(std::get<std::string>(accessor.key), value);
            }
            // 更新缓存
            accessor.cached_value = value;
        }
        return *this;
    }

    // 支持从其他类型赋值（会转换为 variant）
    template <typename T>
    variant_reference& operator=(const T& value) {
        return *this = variant_type(value);
    }

    /**
     * @brief 支持链式索引访问
     */
    variant_reference operator[](std::size_t pos) {
        return get()[pos];
    }

    /**
     * @brief 支持链式索引访问（只读）
     */
    variant_reference operator[](std::size_t pos) const {
        return get()[pos];
    }

    /**
     * @brief 支持链式键访问
     */
    variant_reference operator[](std::string_view key) {
        return get()[key];
    }

    /**
     * @brief 支持链式键访问（只读）
     */
    variant_reference operator[](std::string_view key) const {
        return get()[key];
    }

    /**
     * @brief 解引用操作符
     */
    variant_type& operator*() {
        return get();
    }

    const variant_type& operator*() const {
        return get();
    }

    /**
     * @brief 成员访问操作符
     */
    variant_type* operator->() {
        return &get();
    }

    const variant_type* operator->() const {
        return &get();
    }

    /**
     * @brief 代理 variant 的 as 方法
     */
    template <typename T>
    T as() const {
        return get().template as<T>();
    }

    // ========== 类型判断方法 ==========
    bool is_null() const {
        return get().is_null();
    }
    bool is_string() const {
        return get().is_string();
    }
    bool is_bool() const {
        return get().is_bool();
    }

    // 整数类型判断
    bool is_int8() const {
        return get().is_int8();
    }
    bool is_uint8() const {
        return get().is_uint8();
    }
    bool is_int16() const {
        return get().is_int16();
    }
    bool is_uint16() const {
        return get().is_uint16();
    }
    bool is_int32() const {
        return get().is_int32();
    }
    bool is_uint32() const {
        return get().is_uint32();
    }
    bool is_int64() const {
        return get().is_int64();
    }
    bool is_uint64() const {
        return get().is_uint64();
    }

    // 其他类型判断
    bool is_double() const {
        return get().is_double();
    }
    bool is_object() const {
        return get().is_object();
    }
    bool is_dict() const {
        return get().is_dict();
    }
    bool is_array() const {
        return get().is_array();
    }
    bool is_blob() const {
        return get().is_blob();
    }
    bool is_extension() const {
        return get().is_extension();
    }

    // 组合类型判断
    bool is_numeric() const {
        return get().is_numeric();
    }
    bool is_integer() const {
        return get().is_integer();
    }
    bool is_signed_integer() const {
        return get().is_signed_integer();
    }
    bool is_unsigned_integer() const {
        return get().is_unsigned_integer();
    }

    // ========== 类型转换方法 ==========

    // 整数类型转换
    int8_t as_int8() const {
        return get().as_int8();
    }
    uint8_t as_uint8() const {
        return get().as_uint8();
    }
    int16_t as_int16() const {
        return get().as_int16();
    }
    uint16_t as_uint16() const {
        return get().as_uint16();
    }
    int32_t as_int32() const {
        return get().as_int32();
    }
    uint32_t as_uint32() const {
        return get().as_uint32();
    }
    int64_t as_int64() const {
        return get().as_int64();
    }
    uint64_t as_uint64() const {
        return get().as_uint64();
    }

    // 其他基础类型转换
    bool as_bool(bool strict = false) const {
        return get().as_bool(strict);
    }
    double as_double() const {
        return get().as_double();
    }
    std::string as_string() const {
        return get().as_string();
    }

    // 容器类型转换
    typename variant_type::array_type as_array() const {
        return get().as_array();
    }
    mc::dict as_dict() const {
        return get().as_dict();
    }
    mc::dict as_mutable_dict() const {
        return get().as_mutable_dict();
    }
    mc::dict as_object() const {
        return get().as_object();
    }

    // blob 和 extension 类型转换
    blob_base<> as_blob() const {
        return get().as_blob();
    }
    typename variant_type::extension_ptr_type as_extension() const {
        return get().as_extension();
    }

    // ========== getter 方法 ==========
    const std::string& get_string() const {
        return get().get_string();
    }
    const typename variant_type::blob_type& get_blob() const {
        return get().get_blob();
    }
    const typename variant_type::array_type& get_array() const {
        return get().get_array();
    }
    const mc::dict& get_object() const {
        return get().get_object();
    }

    // ========== 其他常用方法 ==========
    const char* get_type_name() const {
        return get().get_type_name();
    }

    type_id get_type() const {
        return get().get_type();
    }

    bool contains(std::string_view key) const {
        return get().contains(key);
    }

    std::size_t size() const {
        return get().size();
    }

    size_t hash() const {
        return get().hash();
    }

    variant_type deep_copy() const {
        return get().deep_copy();
    }

    void clear() {
        get().clear();
    }

    void swap(variant_reference& other) noexcept {
        get().swap(other.get());
    }

    // ========== 算术操作符 ==========

    // 与另一个 variant_reference 的算术操作
    template <typename OtherConfig>
    variant_type operator+(const variant_reference<OtherConfig>& other) const {
        return get() + other.get();
    }

    template <typename OtherConfig>
    variant_type operator-(const variant_reference<OtherConfig>& other) const {
        return get() - other.get();
    }

    template <typename OtherConfig>
    variant_type operator*(const variant_reference<OtherConfig>& other) const {
        return get() * other.get();
    }

    template <typename OtherConfig>
    variant_type operator/(const variant_reference<OtherConfig>& other) const {
        return get() / other.get();
    }

    template <typename OtherConfig>
    variant_type operator%(const variant_reference<OtherConfig>& other) const {
        return get() % other.get();
    }

    // 与 variant_base 的算术操作
    template <typename OtherConfig>
    variant_type operator+(const variant_base<OtherConfig>& other) const {
        return get() + other;
    }

    template <typename OtherConfig>
    variant_type operator-(const variant_base<OtherConfig>& other) const {
        return get() - other;
    }

    template <typename OtherConfig>
    variant_type operator*(const variant_base<OtherConfig>& other) const {
        return get() * other;
    }

    template <typename OtherConfig>
    variant_type operator/(const variant_base<OtherConfig>& other) const {
        return get() / other;
    }

    template <typename OtherConfig>
    variant_type operator%(const variant_base<OtherConfig>& other) const {
        return get() % other;
    }

    // 与基础类型的算术操作
    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    variant_type operator+(T other) const {
        return get() + other;
    }

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    variant_type operator-(T other) const {
        return get() - other;
    }

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    variant_type operator*(T other) const {
        return get() * other;
    }

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    variant_type operator/(T other) const {
        return get() / other;
    }

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    variant_type operator%(T other) const {
        return get() % other;
    }

    // 字符串拼接
    variant_type operator+(std::string_view other) const {
        return get() + other;
    }

    // ========== 位操作符 ==========

    // 与另一个 variant_reference 的位操作
    template <typename OtherConfig>
    variant_type operator&(const variant_reference<OtherConfig>& other) const {
        return get() & other.get();
    }

    template <typename OtherConfig>
    variant_type operator|(const variant_reference<OtherConfig>& other) const {
        return get() | other.get();
    }

    template <typename OtherConfig>
    variant_type operator^(const variant_reference<OtherConfig>& other) const {
        return get() ^ other.get();
    }

    template <typename OtherConfig>
    variant_type operator<<(const variant_reference<OtherConfig>& other) const {
        return get() << other.get();
    }

    template <typename OtherConfig>
    variant_type operator>>(const variant_reference<OtherConfig>& other) const {
        return get() >> other.get();
    }

    // 与 variant_base 的位操作
    template <typename OtherConfig>
    variant_type operator&(const variant_base<OtherConfig>& other) const {
        return get() & other;
    }

    template <typename OtherConfig>
    variant_type operator|(const variant_base<OtherConfig>& other) const {
        return get() | other;
    }

    template <typename OtherConfig>
    variant_type operator^(const variant_base<OtherConfig>& other) const {
        return get() ^ other;
    }

    variant_type operator~() const {
        return ~get();
    }

    template <typename OtherConfig>
    variant_type operator<<(const variant_base<OtherConfig>& other) const {
        return get() << other;
    }

    template <typename OtherConfig>
    variant_type operator>>(const variant_base<OtherConfig>& other) const {
        return get() >> other;
    }

    // 与基础类型的位操作
    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    variant_type operator&(T other) const {
        return get() & other;
    }

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    variant_type operator|(T other) const {
        return get() | other;
    }

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    variant_type operator^(T other) const {
        return get() ^ other;
    }

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    variant_type operator<<(T other) const {
        return get() << other;
    }

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    variant_type operator>>(T other) const {
        return get() >> other;
    }

    // ========== 复合赋值操作符 ==========

    // 与另一个 variant_reference 的复合赋值
    template <typename OtherConfig>
    variant_reference& operator+=(const variant_reference<OtherConfig>& other) {
        get() += other.get();
        return *this;
    }

    template <typename OtherConfig>
    variant_reference& operator-=(const variant_reference<OtherConfig>& other) {
        get() -= other.get();
        return *this;
    }

    template <typename OtherConfig>
    variant_reference& operator*=(const variant_reference<OtherConfig>& other) {
        get() *= other.get();
        return *this;
    }

    template <typename OtherConfig>
    variant_reference& operator/=(const variant_reference<OtherConfig>& other) {
        get() /= other.get();
        return *this;
    }

    template <typename OtherConfig>
    variant_reference& operator%=(const variant_reference<OtherConfig>& other) {
        get() %= other.get();
        return *this;
    }

    template <typename OtherConfig>
    variant_reference& operator&=(const variant_reference<OtherConfig>& other) {
        get() &= other.get();
        return *this;
    }

    template <typename OtherConfig>
    variant_reference& operator|=(const variant_reference<OtherConfig>& other) {
        get() |= other.get();
        return *this;
    }

    template <typename OtherConfig>
    variant_reference& operator^=(const variant_reference<OtherConfig>& other) {
        get() ^= other.get();
        return *this;
    }

    template <typename OtherConfig>
    variant_reference& operator<<=(const variant_reference<OtherConfig>& other) {
        get() <<= other.get();
        return *this;
    }

    template <typename OtherConfig>
    variant_reference& operator>>=(const variant_reference<OtherConfig>& other) {
        get() >>= other.get();
        return *this;
    }

    // 与 variant_base 的复合赋值
    template <typename OtherConfig>
    variant_reference& operator+=(const variant_base<OtherConfig>& other) {
        get() += other;
        return *this;
    }

    template <typename OtherConfig>
    variant_reference& operator-=(const variant_base<OtherConfig>& other) {
        get() -= other;
        return *this;
    }

    template <typename OtherConfig>
    variant_reference& operator*=(const variant_base<OtherConfig>& other) {
        get() *= other;
        return *this;
    }

    template <typename OtherConfig>
    variant_reference& operator/=(const variant_base<OtherConfig>& other) {
        get() /= other;
        return *this;
    }

    template <typename OtherConfig>
    variant_reference& operator%=(const variant_base<OtherConfig>& other) {
        get() %= other;
        return *this;
    }

    template <typename OtherConfig>
    variant_reference& operator&=(const variant_base<OtherConfig>& other) {
        get() &= other;
        return *this;
    }

    template <typename OtherConfig>
    variant_reference& operator|=(const variant_base<OtherConfig>& other) {
        get() |= other;
        return *this;
    }

    template <typename OtherConfig>
    variant_reference& operator^=(const variant_base<OtherConfig>& other) {
        get() ^= other;
        return *this;
    }

    template <typename OtherConfig>
    variant_reference& operator<<=(const variant_base<OtherConfig>& other) {
        get() <<= other;
        return *this;
    }

    template <typename OtherConfig>
    variant_reference& operator>>=(const variant_base<OtherConfig>& other) {
        get() >>= other;
        return *this;
    }

    // 与基础类型的复合赋值
    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    variant_reference& operator+=(const T& other) {
        get() += other;
        return *this;
    }

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    variant_reference& operator-=(const T& other) {
        get() -= other;
        return *this;
    }

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    variant_reference& operator*=(const T& other) {
        get() *= other;
        return *this;
    }

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    variant_reference& operator/=(const T& other) {
        get() /= other;
        return *this;
    }

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    variant_reference& operator%=(const T& other) {
        get() %= other;
        return *this;
    }

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    variant_reference& operator&=(const T& other) {
        get() &= other;
        return *this;
    }

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    variant_reference& operator|=(const T& other) {
        get() |= other;
        return *this;
    }

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    variant_reference& operator^=(const T& other) {
        get() ^= other;
        return *this;
    }

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    variant_reference& operator<<=(const T& other) {
        get() <<= other;
        return *this;
    }

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    variant_reference& operator>>=(const T& other) {
        get() >>= other;
        return *this;
    }

    // 字符串拼接复合赋值
    variant_reference& operator+=(std::string_view other) {
        get() += other;
        return *this;
    }

    // ========== 自增自减操作符 ==========
    variant_reference& operator++() {
        ++get();
        return *this;
    }

    variant_reference& operator--() {
        --get();
        return *this;
    }

    variant_type operator++(int) {
        return get()++;
    }

    variant_type operator--(int) {
        return get()--;
    }

    // ========== 一元操作符 ==========
    variant_type operator-() const {
        return -get();
    }

    variant_type operator!() const {
        return !get();
    }

    explicit operator bool() const {
        return static_cast<bool>(get());
    }

    // ========== 比较操作符 ==========

    // 与另一个 variant_reference 比较
    template <typename OtherConfig>
    bool operator==(const variant_reference<OtherConfig>& other) const {
        return get() == other.get();
    }

    template <typename OtherConfig>
    bool operator!=(const variant_reference<OtherConfig>& other) const {
        return get() != other.get();
    }

    template <typename OtherConfig>
    bool operator<(const variant_reference<OtherConfig>& other) const {
        return get() < other.get();
    }

    template <typename OtherConfig>
    bool operator>(const variant_reference<OtherConfig>& other) const {
        return get() > other.get();
    }

    template <typename OtherConfig>
    bool operator<=(const variant_reference<OtherConfig>& other) const {
        return get() <= other.get();
    }

    template <typename OtherConfig>
    bool operator>=(const variant_reference<OtherConfig>& other) const {
        return get() >= other.get();
    }

    // 与 variant_base 比较
    template <typename OtherConfig>
    bool operator==(const variant_base<OtherConfig>& other) const {
        return get() == other;
    }

    template <typename OtherConfig>
    bool operator!=(const variant_base<OtherConfig>& other) const {
        return get() != other;
    }

    template <typename OtherConfig>
    bool operator<(const variant_base<OtherConfig>& other) const {
        return get() < other;
    }

    template <typename OtherConfig>
    bool operator>(const variant_base<OtherConfig>& other) const {
        return get() > other;
    }

    template <typename OtherConfig>
    bool operator<=(const variant_base<OtherConfig>& other) const {
        return get() <= other;
    }

    template <typename OtherConfig>
    bool operator>=(const variant_base<OtherConfig>& other) const {
        return get() >= other;
    }

    // 与基础类型比较
    template <typename T>
    bool operator==(const T& other) const {
        return get() == other;
    }

    template <typename T>
    bool operator!=(const T& other) const {
        return get() != other;
    }

    template <typename T>
    bool operator<(const T& other) const {
        return get() < other;
    }

    template <typename T>
    bool operator>(const T& other) const {
        return get() > other;
    }

    template <typename T>
    bool operator<=(const T& other) const {
        return get() <= other;
    }

    template <typename T>
    bool operator>=(const T& other) const {
        return get() >= other;
    }

    // 字符串比较
    bool operator==(std::string_view other) const {
        return get() == other;
    }

    bool operator!=(std::string_view other) const {
        return get() != other;
    }

    bool operator<(std::string_view other) const {
        return get() < other;
    }

    bool operator>(std::string_view other) const {
        return get() > other;
    }

    bool operator<=(std::string_view other) const {
        return get() <= other;
    }

    bool operator>=(std::string_view other) const {
        return get() >= other;
    }

    // dict 比较
    bool operator==(const mc::dict& other) const {
        return get() == other;
    }

    bool operator!=(const mc::dict& other) const {
        return get() != other;
    }

    // blob 比较
    template <typename OtherAllocator>
    bool operator==(const blob_base<OtherAllocator>& other) const {
        return get() == other;
    }

    template <typename OtherAllocator>
    bool operator!=(const blob_base<OtherAllocator>& other) const {
        return get() != other;
    }

    template <typename OtherAllocator>
    bool operator<(const blob_base<OtherAllocator>& other) const {
        return get() < other;
    }

    template <typename OtherAllocator>
    bool operator>(const blob_base<OtherAllocator>& other) const {
        return get() > other;
    }

    template <typename OtherAllocator>
    bool operator<=(const blob_base<OtherAllocator>& other) const {
        return get() <= other;
    }

    template <typename OtherAllocator>
    bool operator>=(const blob_base<OtherAllocator>& other) const {
        return get() >= other;
    }

    // array 比较
    template <typename OtherConfig>
    bool operator==(const std::vector<variant_base<OtherConfig>>& other) const {
        return get() == other;
    }

    template <typename OtherConfig>
    bool operator!=(const std::vector<variant_base<OtherConfig>>& other) const {
        return get() != other;
    }

    template <typename OtherConfig>
    bool operator<(const std::vector<variant_base<OtherConfig>>& other) const {
        return get() < other;
    }

    template <typename OtherConfig>
    bool operator>(const std::vector<variant_base<OtherConfig>>& other) const {
        return get() > other;
    }

    template <typename OtherConfig>
    bool operator<=(const std::vector<variant_base<OtherConfig>>& other) const {
        return get() <= other;
    }

    template <typename OtherConfig>
    bool operator>=(const std::vector<variant_base<OtherConfig>>& other) const {
        return get() >= other;
    }
};

// 为 variant_reference 提供 from_variant 重载，使其可以在模板中正常工作
template <typename Config, typename T>
void from_variant(const variant_reference<Config>& ref, T& value) {
    from_variant(ref.get(), value);
}

template <typename Config, typename T>
void from_variant(variant_reference<Config>& ref, T& value) {
    from_variant(ref.get(), value);
}

// ========== 全局操作符重载：支持左操作数为基础类型或 variant ==========

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
template <typename Config, typename T>
inline bool operator==(const T& lhs, const variant_reference<Config>& rhs) {
    return lhs == rhs.get();
}

template <typename Config, typename T>
inline bool operator!=(const T& lhs, const variant_reference<Config>& rhs) {
    return lhs != rhs.get();
}

template <typename Config, typename T>
inline bool operator<(const T& lhs, const variant_reference<Config>& rhs) {
    return lhs < rhs.get();
}

template <typename Config, typename T>
inline bool operator>(const T& lhs, const variant_reference<Config>& rhs) {
    return lhs > rhs.get();
}

template <typename Config, typename T>
inline bool operator<=(const T& lhs, const variant_reference<Config>& rhs) {
    return lhs <= rhs.get();
}

template <typename Config, typename T>
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
