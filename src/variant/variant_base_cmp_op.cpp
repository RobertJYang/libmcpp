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
 * @file variant_base_cmp_op.cpp
 * @brief 实现variant_base复杂比较操作符（超过3行的函数）
 */

#include <cmath>
#include <mc/variant/variant_base.h>

namespace mc {

// ======== 与 variant_base<OtherConfig> 的复杂比较操作符 ========

template <typename Config>
template <typename OtherConfig>
bool variant_base<Config>::operator<=(const variant_base<OtherConfig>& other) const {
    if (is_double()) {
        if (std::isnan(m_double)) {
            return false;
        }
    } else if (other.is_double()) {
        if (std::isnan(other.as_double())) {
            return false;
        }
    }
    return !(*this > other);
}

template <typename Config>
template <typename OtherConfig>
bool variant_base<Config>::operator>=(const variant_base<OtherConfig>& other) const {
    if (is_double()) {
        if (std::isnan(m_double)) {
            return false;
        }
    } else if (other.is_double()) {
        if (std::isnan(other.as_double())) {
            return false;
        }
    }
    return !(*this < other);
}

// ======== 私有辅助函数 ========

template <typename Config>
template <typename OtherConfig>
bool variant_base<Config>::same_type_equal(const variant_base<OtherConfig>& other) const {
    switch (m_type) {
    case type_id::null_type:
        return true;
    case type_id::double_type: {
        return MC_FLOAT_EQUAL(m_double, other.m_double, VARIANT_FLOAT_EPSILON);
    }
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        return m_int64 == other.m_int64;
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        return m_uint64 == other.m_uint64;
    case type_id::bool_type:
        return m_bool == other.m_bool;
    case type_id::string_type:
        return *m_string_ptr == *other.m_string_ptr;
    case type_id::array_type:
        if (m_array.size() != other.m_array.size()) {
            return false;
        }
        return std::equal(m_array.begin(), m_array.end(), other.m_array.begin());
    case type_id::object_type:
        return m_object == other.m_object;
    case type_id::blob_type:
        return m_blob_ptr->as_string_view() == other.m_blob_ptr->as_string_view();
    case type_id::extension_type: {
        if (m_extension == other.m_extension) {
            return true;
        } else if (!m_extension || !other.m_extension) {
            return false;
        } else {
            return m_extension->equals(*other.m_extension);
        }
    }
    default:
        break;
    }

    return false;
}

template <typename Config>
template <typename OtherConfig>
bool variant_base<Config>::other_type_equal(const variant_base<OtherConfig>& other) const {
    // 任何一个是数字，优先转换为数字比较
    // 任何一个是double，优先转换为double比较
    if (is_numeric() && (other.is_numeric() || other.is_string() || other.is_blob())) {
        if (is_double()) {
            if (auto v = other.template try_as<double>()) {
                return MC_FLOAT_EQUAL(m_double, v.value(), VARIANT_FLOAT_EPSILON);
            }
        } else if (other.is_double()) {
            if (auto v = try_as<double>()) {
                return MC_FLOAT_EQUAL(v.value(), other.m_double, VARIANT_FLOAT_EPSILON);
            }
        } else if (is_signed_integer()) {
            if (auto v = other.template try_as<int64_t>()) {
                return m_int64 == v.value();
            }
        } else if (is_unsigned_integer()) {
            if (auto v = other.template try_as<uint64_t>()) {
                return m_uint64 == v.value();
            }
        } else if (is_bool()) {
            if (auto v = other.template try_as<bool>()) {
                return m_bool == v.value();
            }
        }
    }

    if (other.is_numeric() && (is_numeric() || is_string() || is_blob())) {
        if (other.is_double()) {
            if (auto v = try_as<double>()) {
                return MC_FLOAT_EQUAL(v.value(), other.m_double, VARIANT_FLOAT_EPSILON);
            }
        } else if (is_double()) {
            if (auto v = other.template try_as<double>()) {
                return MC_FLOAT_EQUAL(m_double, v.value(), VARIANT_FLOAT_EPSILON);
            }
        } else if (other.is_signed_integer()) {
            if (auto v = try_as<int64_t>()) {
                return v.value() == other.as_int64();
            }
        } else if (other.is_unsigned_integer()) {
            if (auto v = try_as<uint64_t>()) {
                return v.value() == other.as_uint64();
            }
        } else if (other.is_bool()) {
            if (auto v = try_as<bool>()) {
                return v.value() == other.as_bool();
            }
        }
    }

    return false;
}

template <typename Config>
template <typename OtherConfig>
bool variant_base<Config>::same_type_less(const variant_base<OtherConfig>& other) const {
    switch (m_type) {
    case type_id::null_type:
        return false; // null等于null
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        return m_int64 < other.m_int64;
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        return m_uint64 < other.m_uint64;
    case type_id::double_type:
        return m_double < other.m_double;
    case type_id::bool_type:
        return m_bool < other.m_bool;
    case type_id::string_type:
        return *m_string_ptr < *other.m_string_ptr;
    case type_id::blob_type:
        return *m_blob_ptr < *other.m_blob_ptr;
    default:
        break;
    }

    throw_invalid_type_comparison_error(get_type_name(), other.get_type_name(), "<");
}

template <typename Config>
template <typename OtherConfig>
bool variant_base<Config>::other_type_less(const variant_base<OtherConfig>& other) const {
    // 任何一个是数字，优先转换为数字比较
    // 任何一个是double，优先转换为double比较
    if (is_numeric() && (other.is_numeric() || other.is_string() || other.is_blob())) {
        if (is_double()) {
            if (auto v = other.template try_as<double>()) {
                return m_double < v.value();
            }
        } else if (other.is_double()) {
            if (auto v = try_as<double>()) {
                return v.value() < other.as_double();
            }
        } else if (is_signed_integer()) {
            if (auto v = other.template try_as<int64_t>()) {
                return m_int64 < v.value();
            }
        } else if (is_unsigned_integer()) {
            if (auto v = other.template try_as<uint64_t>()) {
                return m_uint64 < v.value();
            }
        } else if (is_bool()) {
            if (auto v = other.template try_as<bool>()) {
                return m_bool < v.value();
            }
        }
    }

    if (other.is_numeric() && (is_numeric() || is_string() || is_blob())) {
        if (other.is_double()) {
            if (auto v = try_as<double>()) {
                return v.value() < other.as_double();
            }
        } else if (is_double()) {
            if (auto v = other.template try_as<double>()) {
                return m_double < v.value();
            }
        } else if (other.is_signed_integer()) {
            if (auto v = try_as<int64_t>()) {
                return v.value() < other.as_int64();
            }
        } else if (other.is_unsigned_integer()) {
            if (auto v = try_as<uint64_t>()) {
                return v.value() < other.as_uint64();
            }
        } else if (other.is_bool()) {
            if (auto v = try_as<bool>()) {
                return v.value() < other.as_bool();
            }
        }
    }

    if (is_string()) {
        if (other.is_blob()) {
            return *m_string_ptr < other.m_blob_ptr->as_string_view();
        }
    } else if (is_blob()) {
        if (other.is_string()) {
            return m_blob_ptr->as_string_view() < *other.m_string_ptr;
        }
    }

    throw_invalid_type_comparison_error(get_type_name(), other.get_type_name(), "<");
}

// ======== 与 std::string_view 的比较操作符 ========

template <typename Config>
bool variant_base<Config>::operator==(std::string_view other) const {
    if (is_string()) {
        return *m_string_ptr == other;
    } else if (is_blob()) {
        return m_blob_ptr->as_string_view() == other;
    } else if (is_double()) {
        double v;
        if (mc::string::try_to_number<double>(other, v)) {
            return MC_FLOAT_EQUAL(m_double, v, VARIANT_FLOAT_EPSILON);
        }
    } else if (is_signed_integer()) {
        int64_t v;
        if (mc::string::try_to_number<int64_t>(other, v)) {
            return m_int64 == v;
        }
    } else if (is_unsigned_integer()) {
        uint64_t v;
        if (mc::string::try_to_number<uint64_t>(other, v)) {
            return m_uint64 == v;
        }
    } else if (is_bool()) {
        bool v;
        if (mc::string::try_to_bool(other, v)) {
            return m_bool == v;
        }
    }

    return false;
}

template <typename Config>
bool variant_base<Config>::operator<(std::string_view other) const {
    if (is_string()) {
        return *m_string_ptr < other;
    } else if (is_blob()) {
        return m_blob_ptr->as_string_view() < other;
    } else if (is_double()) {
        double v;
        if (mc::string::try_to_number<double>(other, v)) {
            return m_double < v;
        }
    } else if (is_signed_integer()) {
        int64_t v;
        if (mc::string::try_to_number<int64_t>(other, v)) {
            return m_int64 < v;
        }
    } else if (is_unsigned_integer()) {
        uint64_t v;
        if (mc::string::try_to_number<uint64_t>(other, v)) {
            return m_uint64 < v;
        }
    } else if (is_bool()) {
        bool v;
        if (mc::string::try_to_bool(other, v)) {
            return m_bool < v;
        }
    }
    throw_invalid_type_comparison_error(get_type_name(get_type()), "string_view", "<");
}

template <typename Config>
bool variant_base<Config>::operator>(std::string_view other) const {
    if (is_string()) {
        return *m_string_ptr > other;
    } else if (is_blob()) {
        return m_blob_ptr->as_string_view() > other;
    } else if (is_double()) {
        double v;
        if (mc::string::try_to_number<double>(other, v)) {
            return m_double > v;
        }
    } else if (is_signed_integer()) {
        int64_t v;
        if (mc::string::try_to_number<int64_t>(other, v)) {
            return m_int64 > v;
        }
    } else if (is_unsigned_integer()) {
        uint64_t v;
        if (mc::string::try_to_number<uint64_t>(other, v)) {
            return m_uint64 > v;
        }
    } else if (is_bool()) {
        bool v;
        if (mc::string::try_to_bool(other, v)) {
            return m_bool > v;
        }
    }
    throw_invalid_type_comparison_error(get_type_name(get_type()), "string_view", ">");
}

} // namespace mc

// ======== 显式实例化 ========

namespace mc {

template MC_API bool variant::operator<= <variant_config<>>(const variant& other) const;
template MC_API bool variant::operator>= <variant_config<>>(const variant& other) const;

template MC_API bool variant::same_type_equal<variant_config<>>(const variant& other) const;
template MC_API bool variant::other_type_equal<variant_config<>>(const variant& other) const;
template MC_API bool variant::same_type_less<variant_config<>>(const variant& other) const;
template MC_API bool variant::other_type_less<variant_config<>>(const variant& other) const;

template MC_API bool variant::operator==(std::string_view other) const;
template MC_API bool variant::operator<(std::string_view other) const;
template MC_API bool variant::operator>(std::string_view other) const;

} // namespace mc