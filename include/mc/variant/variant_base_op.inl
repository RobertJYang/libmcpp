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

/**
 * @file variant_base_op.inl
 * @brief 实现variant_base算术和位运算操作符
 */

#ifndef MC_VARIANT_VARIANT_BASE_OP_INL
#define MC_VARIANT_VARIANT_BASE_OP_INL

namespace mc {

// ======== 算术运算 ========

template <typename Config>
template <typename OtherConfig>
variant_base<Config> variant_base<Config>::operator+(const variant_base<OtherConfig>& other) const {
    // 任意一个是字符串，使用字符串拼接
    if (is_string()) {
        if (other.is_string()) {
            return {get_string() + other.get_string()};
        } else if (other.is_blob()) {
            std::string tmp;
            tmp.reserve(size() + other.size());
            tmp += get_string();
            tmp += other.get_blob().as_string_view();
            return {std::move(tmp)};
        }
        return {get_string() + other.as_string()};
    } else if (other.is_string()) {
        return {as_string() + other.get_string()};
    }

    // blob 只允许与 blob 或 string 相加
    if (is_blob()) {
        if (other.is_blob()) {
            auto blob = *m_blob_ptr;
            blob += other.get_blob();
            return {std::move(blob)};
        } else if (other.is_string()) {
            auto blob = *m_blob_ptr;
            blob += other.get_string();
            return {std::move(blob)};
        }
        throw_invalid_type_operation_error(get_type_name(), other.get_type_name(), "+");
    }

    // 如果两个都是数组则使用数组拼接，否则将另一个元素添加到数组末尾
    if (is_array()) {
        auto result = *m_array_ptr;
        if (other.is_array()) {
            result.insert(result.end(), other.m_array_ptr->begin(), other.m_array_ptr->end());
        } else {
            result.push_back(other);
        }
        return {std::move(result)};
    }

    // 如果两个都是对象则使用对象拼接，否则抛出异常
    if (is_object()) {
        if (!other.is_object()) {
            throw_invalid_type_operation_error(get_type_name(), other.get_type_name(), "+");
        }
        return {*m_object_ptr + *other.m_object_ptr};
    }

    // 尝试转换成数值相加，如果失败则抛出异常
    try {
        if (is_double() || other.is_double()) {
            return {as_double() + other.as_double()};
        }

        if (is_unsigned_integer() && other.is_unsigned_integer()) {
            return {as_uint64() + other.as_uint64()};
        }

        return {as_int64() + other.as_int64()};
    } catch (const std::exception&) {
        throw_invalid_type_operation_error(get_type_name(), other.get_type_name(), "+");
    }
}

template <typename Config>
template <typename OtherConfig>
variant_base<Config> variant_base<Config>::operator-(const variant_base<OtherConfig>& other) const {
    try {
        if (is_double() || other.is_double()) {
            return {as_double() - other.as_double()};
        }

        if (is_unsigned_integer() && other.is_unsigned_integer()) {
            // 处理下溢
            if (as_uint64() < other.as_uint64()) {
                return {static_cast<int64_t>(as_uint64()) -
                        static_cast<int64_t>(other.as_uint64())};
            }
            return {as_uint64() - other.as_uint64()};
        }

        return {as_int64() - other.as_int64()};
    } catch (const std::exception&) {
        throw_invalid_type_operation_error(get_type_name(), other.get_type_name(), "-");
    }
}

template <typename Config>
template <typename OtherConfig>
variant_base<Config> variant_base<Config>::operator*(const variant_base<OtherConfig>& other) const {
    try {
        if (is_double() || other.is_double()) {
            return {as_double() * other.as_double()};
        }

        if (is_unsigned_integer() && other.is_unsigned_integer()) {
            return {as_uint64() * other.as_uint64()};
        }

        return {as_int64() * other.as_int64()};
    } catch (const std::exception&) {
        throw_invalid_type_operation_error(get_type_name(), other.get_type_name(), "*");
    }
}

template <typename T, typename Variant1, typename Variant2>
Variant1 variant_divide(const Variant1& self, const Variant2& other) {
    auto s_value = self.template try_as<T>();
    if (!s_value) {
        throw_invalid_type_operation_error(self.get_type_name(), other.get_type_name(), "/");
    }

    auto o_value = other.template try_as<T>();
    if (!o_value) {
        throw_invalid_type_operation_error(self.get_type_name(), other.get_type_name(), "/");
    }

    if (*o_value == 0) {
        throw_divide_by_zero_exception("除零错误");
    }

    return {*s_value / *o_value};
}

template <typename Config>
template <typename OtherConfig>
variant_base<Config> variant_base<Config>::operator/(const variant_base<OtherConfig>& other) const {
    if (is_double() || other.is_double() || is_bool()) {
        return variant_divide<double>(*this, other);
    }

    if (is_unsigned_integer() && other.is_unsigned_integer()) {
        return variant_divide<uint64_t>(*this, other);
    }

    return variant_divide<int64_t>(*this, other);
}

template <typename T, typename Variant1, typename Variant2>
Variant1 variant_mod(const Variant1& self, const Variant2& other) {
    auto s_value = self.template try_as<T>();
    if (!s_value) {
        throw_invalid_type_operation_error(self.get_type_name(), other.get_type_name(), "%");
    }

    auto o_value = other.template try_as<T>();
    if (!o_value) {
        throw_invalid_type_operation_error(self.get_type_name(), other.get_type_name(), "%");
    }

    if (*o_value == 0) {
        throw_divide_by_zero_exception("除零错误");
    }

    if constexpr (std::is_floating_point_v<T>) {
        return {static_cast<int64_t>(*s_value) % static_cast<int64_t>(*o_value)};
    } else {
        return {*s_value % *o_value};
    }
}

template <typename Config>
template <typename OtherConfig>
variant_base<Config> variant_base<Config>::operator%(const variant_base<OtherConfig>& other) const {
    if (is_double() || other.is_double()) {
        return variant_mod<double>(*this, other);
    }

    if (is_unsigned_integer() && other.is_unsigned_integer()) {
        return variant_mod<uint64_t>(*this, other);
    }

    return variant_mod<int64_t>(*this, other);
}

// ======== 位运算 ========

template <typename Config>
template <typename OtherConfig>
variant_base<Config> variant_base<Config>::operator&(const variant_base<OtherConfig>& other) const {
    try {
        if (is_unsigned_integer() && other.is_unsigned_integer()) {
            return {as_uint64() & other.as_uint64()};
        }

        return {as_int64() & other.as_int64()};
    } catch (const std::exception&) {
        throw_invalid_type_operation_error(get_type_name(), other.get_type_name(), "&");
    }
}

template <typename Config>
template <typename OtherConfig>
variant_base<Config> variant_base<Config>::operator|(const variant_base<OtherConfig>& other) const {
    try {
        if (is_unsigned_integer() && other.is_unsigned_integer()) {
            return {as_uint64() | other.as_uint64()};
        }

        return {as_int64() | other.as_int64()};
    } catch (const std::exception&) {
        throw_invalid_type_operation_error(get_type_name(), other.get_type_name(), "|");
    }
}

template <typename Config>
template <typename OtherConfig>
variant_base<Config> variant_base<Config>::operator^(const variant_base<OtherConfig>& other) const {
    try {
        if (is_unsigned_integer() && other.is_unsigned_integer()) {
            return {as_uint64() ^ other.as_uint64()};
        }

        return {as_int64() ^ other.as_int64()};
    } catch (const std::exception&) {
        throw_invalid_type_operation_error(get_type_name(), other.get_type_name(), "^");
    }
}

template <typename Config>
variant_base<Config> variant_base<Config>::operator~() const {
    try {
        if (is_unsigned_integer()) {
            return {~as_uint64()};
        }

        return {~as_int64()};
    } catch (const std::exception&) {
        throw_invalid_type_operation_error(get_type_name(), "", "~");
    }
}

template <typename Config>
template <typename OtherConfig>
variant_base<Config>
variant_base<Config>::operator<<(const variant_base<OtherConfig>& other) const {
    try {
        uint64_t shift_amount = other.as_uint64();
        if (shift_amount >= sizeof(uint64_t) * 8) {
            return {0};
        }
        if (is_unsigned_integer()) {
            return {as_uint64() << shift_amount};
        }
        return {as_int64() << shift_amount};
    } catch (const std::exception&) {
        throw_invalid_type_operation_error(get_type_name(), other.get_type_name(), "<<");
    }
}

template <typename Config>
template <typename OtherConfig>
variant_base<Config>
variant_base<Config>::operator>>(const variant_base<OtherConfig>& other) const {
    try {
        uint64_t shift_amount = other.as_uint64();
        if (shift_amount >= sizeof(uint64_t) * 8) {
            if (is_signed_integer() && as_int64() < 0) {
                return {-1}; // 对负数，保持符号位
            }
            return {0};
        }
        if (is_unsigned_integer()) {
            return {as_uint64() >> shift_amount};
        }
        return {as_int64() >> shift_amount};
    } catch (const std::exception&) {
        throw_invalid_type_operation_error(get_type_name(), other.get_type_name(), ">>");
    }
}

// ======== 算术操作与基本类型 ========

template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
variant_base<Config> variant_base<Config>::operator+(T other) const {
    if (is_string()) {
        return variant_base<Config>(get_string() + std::to_string(other));
    } else if (is_array()) {
        return *this + variant(other);
    } else if (is_object()) {
        throw_invalid_type_operation_error(get_type_name(), pretty_name<T>(), "+");
    } else if (is_double() || std::is_floating_point_v<T>) {
        return variant_base<Config>(as_double() + static_cast<double>(other));
    } else if (is_unsigned_integer() && (std::is_unsigned_v<T> || other >= 0)) {
        return variant_base<Config>(as_uint64() + static_cast<uint64_t>(other));
    } else {
        return variant_base<Config>(as_int64() + static_cast<int64_t>(other));
    }
}

template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
variant_base<Config> variant_base<Config>::operator-(T other) const {
    try {
        if (is_double() || std::is_floating_point_v<T>) {
            return variant_base<Config>(as_double() - static_cast<double>(other));
        } else if (is_unsigned_integer() && (std::is_unsigned_v<T> || other >= 0)) {
            if (as_uint64() < static_cast<uint64_t>(other)) {
                return variant_base<Config>(static_cast<int64_t>(as_uint64()) -
                                            static_cast<int64_t>(other));
            }
            return variant_base<Config>(as_uint64() - static_cast<uint64_t>(other));
        }

        return variant_base<Config>(as_int64() - static_cast<int64_t>(other));
    } catch (const std::exception&) {
        throw_invalid_type_operation_error(get_type_name(), pretty_name<T>(), "-");
    }
}

template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
variant_base<Config> variant_base<Config>::operator*(T other) const {
    try {
        if (is_double() || std::is_floating_point_v<T>) {
            return variant_base<Config>(as_double() * static_cast<double>(other));
        } else if (is_unsigned_integer() && (std::is_unsigned_v<T> || other >= 0)) {
            return variant_base<Config>(as_uint64() * static_cast<uint64_t>(other));
        } else {
            return variant_base<Config>(as_int64() * static_cast<int64_t>(other));
        }
    } catch (const std::exception&) {
        throw_invalid_type_operation_error(get_type_name(), pretty_name<T>(), "*");
    }
}

template <typename T>
bool is_zero(T value) {
    if constexpr (std::is_floating_point_v<T>) {
        return MC_FLOAT_EQUAL(static_cast<double>(value), 0.0, VARIANT_FLOAT_EPSILON);
    } else {
        return value == 0;
    }
}

template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
variant_base<Config> variant_base<Config>::operator/(T other) const {
    if (is_zero(other)) {
        throw_divide_by_zero_exception("除零错误");
    }

    try {
        if (is_double() || std::is_floating_point_v<T> || is_bool() || std::is_same_v<T, bool>) {
            return variant_base<Config>(as_double() / static_cast<double>(other));
        } else if (is_unsigned_integer() && (std::is_unsigned_v<T> || other >= 0)) {
            return variant_base<Config>(as_uint64() / static_cast<uint64_t>(other));
        }

        return variant_base<Config>(as_int64() / static_cast<int64_t>(other));
    } catch (const std::exception&) {
        throw_invalid_type_operation_error(get_type_name(), pretty_name<T>(), "/");
    }
}

template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
variant_base<Config> variant_base<Config>::operator%(T other) const {
    if (is_zero(other)) {
        throw_divide_by_zero_exception("取模运算除零错误");
    }

    try {
        if (is_unsigned_integer() && (std::is_unsigned_v<T> || other >= 0)) {
            return variant_base<Config>(as_uint64() % static_cast<uint64_t>(other));
        } else {
            return variant_base<Config>(as_int64() % static_cast<int64_t>(other));
        }
    } catch (const std::exception&) {
        throw_invalid_type_operation_error(get_type_name(), pretty_name<T>(), "%");
    }
}

// ======== 位运算与基本类型 ========

template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
variant_base<Config> variant_base<Config>::operator&(T other) const {
    try {
        if (is_unsigned_integer() && std::is_unsigned_v<T>) {
            return variant_base<Config>(as_uint64() & static_cast<uint64_t>(other));
        }
        return variant_base<Config>(as_int64() & static_cast<int64_t>(other));
    } catch (const std::exception&) {
        throw_invalid_type_operation_error(get_type_name(), pretty_name<T>(), "&");
    }
}

template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
variant_base<Config> variant_base<Config>::operator|(T other) const {
    try {
        if (is_unsigned_integer() && std::is_unsigned_v<T>) {
            return variant_base<Config>(as_uint64() | static_cast<uint64_t>(other));
        }
        return variant_base<Config>(as_int64() | static_cast<int64_t>(other));
    } catch (const std::exception&) {
        throw_invalid_type_operation_error(get_type_name(), pretty_name<T>(), "|");
    }
}

template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
variant_base<Config> variant_base<Config>::operator^(T other) const {
    try {
        if (is_unsigned_integer() && std::is_unsigned_v<T>) {
            return variant_base<Config>(as_uint64() ^ static_cast<uint64_t>(other));
        }
        return variant_base<Config>(as_int64() ^ static_cast<int64_t>(other));
    } catch (const std::exception&) {
        throw_invalid_type_operation_error(get_type_name(), pretty_name<T>(), "^");
    }
}

template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
variant_base<Config> variant_base<Config>::operator<<(T other) const {
    try {
        uint64_t shift_amount = static_cast<uint64_t>(other);
        if (shift_amount >= sizeof(uint64_t) * 8) {
            return {0};
        }
        if (is_unsigned_integer()) {
            return {as_uint64() << shift_amount};
        }
        return {as_int64() << shift_amount};
    } catch (const std::exception&) {
        throw_invalid_type_operation_error(get_type_name(), pretty_name<T>(), "<<");
    }
}

template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
variant_base<Config> variant_base<Config>::operator>>(T other) const {
    try {
        uint64_t shift_amount = static_cast<uint64_t>(other);
        if (shift_amount >= sizeof(uint64_t) * 8) {
            if (is_signed_integer() && as_int64() < 0) {
                return {-1}; // 对负数，保持符号位
            }
            return {0};
        }

        if (is_unsigned_integer()) {
            return {as_uint64() >> shift_amount};
        }
        return {as_int64() >> shift_amount};
    } catch (const std::exception&) {
        throw_invalid_type_operation_error(get_type_name(), pretty_name<T>(), ">>");
    }
}

// ======== 字符串操作 ========

template <typename Config>
variant_base<Config> variant_base<Config>::operator+(std::string_view other) const {
    if (is_string()) {
        std::string tmp;
        tmp.reserve(size() + other.size());
        tmp += get_string();
        tmp += other;
        return variant_base<Config>(std::move(tmp));
    } else if (is_blob()) {
        blob_type tmp;
        tmp.data.reserve(size() + other.size());
        auto blob = get_blob().as_string_view();
        tmp.data.insert(tmp.data.end(), blob.begin(), blob.end());
        tmp.data.insert(tmp.data.end(), other.begin(), other.end());
        return variant_base<Config>(std::move(tmp));
    } else {
        return *this + variant(other);
    }
}

// ======== 复合赋值操作 ========

template <typename Config>
template <typename OtherConfig>
variant_base<Config>& variant_base<Config>::operator+=(const variant_base<OtherConfig>& other) {
    set_value(*this + other);
    return *this;
}

template <typename Config>
template <typename OtherConfig>
variant_base<Config>& variant_base<Config>::operator-=(const variant_base<OtherConfig>& other) {
    set_value(*this - other);
    return *this;
}

template <typename Config>
template <typename OtherConfig>
variant_base<Config>& variant_base<Config>::operator*=(const variant_base<OtherConfig>& other) {
    set_value(*this * other);
    return *this;
}

template <typename Config>
template <typename OtherConfig>
variant_base<Config>& variant_base<Config>::operator/=(const variant_base<OtherConfig>& other) {
    set_value(*this / other);
    return *this;
}

template <typename Config>
template <typename OtherConfig>
variant_base<Config>& variant_base<Config>::operator%=(const variant_base<OtherConfig>& other) {
    set_value(*this % other);
    return *this;
}

template <typename Config>
template <typename OtherConfig>
variant_base<Config>& variant_base<Config>::operator&=(const variant_base<OtherConfig>& other) {
    set_value(*this & other);
    return *this;
}

template <typename Config>
template <typename OtherConfig>
variant_base<Config>& variant_base<Config>::operator|=(const variant_base<OtherConfig>& other) {
    set_value(*this | other);
    return *this;
}

template <typename Config>
template <typename OtherConfig>
variant_base<Config>& variant_base<Config>::operator^=(const variant_base<OtherConfig>& other) {
    set_value(*this ^ other);
    return *this;
}

template <typename Config>
template <typename OtherConfig>
variant_base<Config>& variant_base<Config>::operator<<=(const variant_base<OtherConfig>& other) {
    set_value(*this << other);
    return *this;
}

template <typename Config>
template <typename OtherConfig>
variant_base<Config>& variant_base<Config>::operator>>=(const variant_base<OtherConfig>& other) {
    set_value(*this >> other);
    return *this;
}

// 与基本类型的复合赋值运算
template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
variant_base<Config>& variant_base<Config>::operator+=(const T& other) {
    set_value(*this + other);
    return *this;
}

template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
variant_base<Config>& variant_base<Config>::operator-=(const T& other) {
    set_value(*this - other);
    return *this;
}

template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
variant_base<Config>& variant_base<Config>::operator*=(const T& other) {
    set_value(*this * other);
    return *this;
}

template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
variant_base<Config>& variant_base<Config>::operator/=(const T& other) {
    set_value(*this / other);
    return *this;
}

template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
variant_base<Config>& variant_base<Config>::operator%=(const T& other) {
    set_value(*this % other);
    return *this;
}

template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
variant_base<Config>& variant_base<Config>::operator&=(const T& other) {
    set_value(*this & other);
    return *this;
}

template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
variant_base<Config>& variant_base<Config>::operator|=(const T& other) {
    set_value(*this | other);
    return *this;
}

template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
variant_base<Config>& variant_base<Config>::operator^=(const T& other) {
    set_value(*this ^ other);
    return *this;
}

template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
variant_base<Config>& variant_base<Config>::operator<<=(const T& other) {
    set_value(*this << other);
    return *this;
}

template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
variant_base<Config>& variant_base<Config>::operator>>=(const T& other) {
    set_value(*this >> other);
    return *this;
}

// 字符串的复合赋值运算
template <typename Config>
variant_base<Config>& variant_base<Config>::operator+=(std::string_view other) {
    set_value(*this + other);
    return *this;
}

// ======== 算术运算与基本类型的友元运算符 ========

template <typename Config, typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base<Config> operator+(T lhs, const variant_base<Config>& rhs) {
    if (rhs.is_string()) {
        return {std::to_string(lhs) + rhs.get_string()};
    } else if (rhs.is_numeric()) {
        return rhs + lhs;
    }

    throw_invalid_type_operation_error(rhs.get_type_name(), pretty_name<T>(), "+");
}

template <typename Config, typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base<Config> operator-(T lhs, const variant_base<Config>& rhs) {
    return variant_base<Config>(lhs) - rhs;
}

template <typename Config, typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base<Config> operator*(T lhs, const variant_base<Config>& rhs) {
    return rhs * lhs;
}

template <typename Config, typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base<Config> operator/(T lhs, const variant_base<Config>& rhs) {
    return variant_base<Config>(lhs) / rhs;
}

template <typename Config, typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base<Config> operator%(T lhs, const variant_base<Config>& rhs) {
    return variant_base<Config>(lhs) % rhs;
}

// ======== 位运算与基本类型的友元运算符 ========

template <typename Config, typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base<Config> operator&(T lhs, const variant_base<Config>& rhs) {
    return rhs & lhs;
}

template <typename Config, typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base<Config> operator|(T lhs, const variant_base<Config>& rhs) {
    return rhs | lhs;
}

template <typename Config, typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base<Config> operator^(T lhs, const variant_base<Config>& rhs) {
    return rhs ^ lhs;
}

template <typename Config, typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base<Config> operator<<(T lhs, const variant_base<Config>& rhs) {
    return variant_base<Config>(lhs) << rhs;
}

template <typename Config, typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
inline variant_base<Config> operator>>(T lhs, const variant_base<Config>& rhs) {
    return variant_base<Config>(lhs) >> rhs;
}

// ======== 字符串操作的友元运算符 ========

template <typename Config>
inline variant_base<Config> operator+(const char* lhs, const variant_base<Config>& rhs) {
    return std::string_view(lhs) + rhs;
}

template <typename Config>
inline variant_base<Config> operator+(const std::string& lhs, const variant_base<Config>& rhs) {
    return std::string_view(lhs) + rhs;
}

template <typename Config>
inline variant_base<Config> operator+(std::string_view lhs, const variant_base<Config>& rhs) {
    std::string tmp;
    if (rhs.is_string()) {
        tmp.reserve(lhs.size() + rhs.get_string().size());
        tmp += lhs;
        tmp += rhs.get_string();
    } else if (rhs.is_blob()) {
        tmp.reserve(lhs.size() + rhs.get_blob().data.size());
        tmp += lhs;
        tmp += rhs.get_blob().as_string_view();
    } else {
        auto s = rhs.as_string();
        tmp.reserve(lhs.size() + s.size());
        tmp += lhs;
        tmp += s;
    }
    return variant_base<Config>(std::move(tmp));
}

// ======== 前置自增运算符 ========
template <typename Config>
variant_base<Config>& variant_base<Config>::operator++() {
    if (is_double()) {
        m_double += 1.0;
    } else if (is_unsigned_integer()) {
        m_uint64 += 1;
    } else if (is_signed_integer()) {
        m_int64 += 1;
    } else if (is_bool()) {
        m_bool = true; // bool类型自增后为true
    } else {
        throw_invalid_type_operation_error(get_type_name(), "", "++");
    }
    return *this;
}

// ======== 前置自减运算符 ========
template <typename Config>
variant_base<Config>& variant_base<Config>::operator--() {
    if (is_double()) {
        m_double -= 1.0;
    } else if (is_unsigned_integer()) {
        m_uint64 -= 1;
    } else if (is_signed_integer()) {
        m_int64 -= 1;
    } else if (is_bool()) {
        m_bool = false; // bool类型自减后为false
    } else {
        throw_invalid_type_operation_error(get_type_name(), "", "--");
    }
    return *this;
}

// ======== 后置自增运算符 ========
template <typename Config>
variant_base<Config> variant_base<Config>::operator++(int) {
    variant_base<Config> temp(*this);
    ++(*this);
    return temp;
}

// ======== 后置自减运算符 ========
template <typename Config>
variant_base<Config> variant_base<Config>::operator--(int) {
    variant_base<Config> temp(*this);
    --(*this);
    return temp;
}

// ======== 一元取反运算符 ========
template <typename Config>
variant_base<Config> variant_base<Config>::operator-() const {
    try {
        if (is_double()) {
            return {-m_double};
        } else if (is_unsigned_integer()) {
            // 无符号数取负，需要转换为有符号数
            return {-static_cast<int64_t>(m_uint64)};
        } else if (is_signed_integer()) {
            return {-m_int64};
        } else if (is_bool()) {
            return {m_bool ? -1 : 0};
        } else {
            return {-as_int64()};
        }
    } catch (const std::exception&) {
        throw_invalid_type_operation_error(get_type_name(), "", "-");
    }
}

// ======== 一元逻辑非运算符 ========
template <typename Config>
variant_base<Config> variant_base<Config>::operator!() const {
    return {!as_bool()};
}

} // namespace mc

#endif // MC_VARIANT_VARIANT_BASE_OP_INL