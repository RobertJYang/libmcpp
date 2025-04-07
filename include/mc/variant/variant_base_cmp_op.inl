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
 * @file variant_base_cmp_op.inl
 * @brief 实现variant_base比较操作符
 */

#include <cmath> // 用于std::isnan

namespace mc {

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
    return false;
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
    return false;
}

template <typename Config>
template <typename OtherConfig>
bool variant_base<Config>::operator<(const variant_base<OtherConfig>& other) const {
    if (m_type == other.get_type()) {
        return same_type_less(other);
    } else {
        return other_type_less(other);
    }
}

template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
bool variant_base<Config>::operator<(const T& other) const {
    if (is_double()) {
        return m_double < other;
    } else if (is_signed_integer()) {
        return m_int64 < other;
    } else if (is_unsigned_integer()) {
        return m_uint64 < other;
    } else if (is_bool()) {
        return m_bool < other;
    } else if (is_string() || is_blob()) {
        if (auto v = try_as<T>()) {
            return *v < other;
        }
    }
    throw_invalid_type_comparison_error(get_type_name(get_type()), pretty_name<T>(), "<");
    return false;
}

template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
bool variant_base<Config>::operator>(const T& other) const {
    if (is_double()) {
        return m_double > other;
    } else if (is_signed_integer()) {
        return m_int64 > other;
    } else if (is_unsigned_integer()) {
        return m_uint64 > other;
    } else if (is_bool()) {
        return m_bool > other;
    } else if (is_string() || is_blob()) {
        if (auto v = try_as<T>()) {
            return *v > other;
        }
    }
    throw_invalid_type_comparison_error(get_type_name(get_type()), pretty_name<T>(), ">");
    return false;
}

template <typename Config>
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int>>
bool variant_base<Config>::operator==(const T& other) const {
    if (is_double()) {
        return MC_FLOAT_EQUAL(m_double, other, VARIANT_FLOAT_EPSILON);
    } else if (is_signed_integer()) {
        return m_int64 == other;
    } else if (is_unsigned_integer()) {
        return m_uint64 == other;
    } else if (is_bool()) {
        return m_bool == other;
    } else if (is_string() || is_blob()) {
        if (auto v = try_as<T>()) {
            return *v == other;
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
        if (auto v = mc::string::try_to_number<double>(other)) {
            return m_double < *v;
        }
    } else if (is_signed_integer()) {
        if (auto v = mc::string::try_to_number<int64_t>(other)) {
            return m_int64 < *v;
        }
    } else if (is_unsigned_integer()) {
        if (auto v = mc::string::try_to_number<uint64_t>(other)) {
            return m_uint64 < *v;
        }
    } else if (is_bool()) {
        if (auto v = mc::string::try_to_bool(other)) {
            return m_bool < *v;
        }
    }
    throw_invalid_type_comparison_error(get_type_name(get_type()), "string_view", "<");
    return false;
}

template <typename Config>
bool variant_base<Config>::operator>(std::string_view other) const {
    if (is_string()) {
        return *m_string_ptr > other;
    } else if (is_blob()) {
        return m_blob_ptr->as_string_view() > other;
    } else if (is_double()) {
        if (auto v = mc::string::try_to_number<double>(other)) {
            return m_double > *v;
        }
    } else if (is_signed_integer()) {
        if (auto v = mc::string::try_to_number<int64_t>(other)) {
            return m_int64 > *v;
        }
    } else if (is_unsigned_integer()) {
        if (auto v = mc::string::try_to_number<uint64_t>(other)) {
            return m_uint64 > *v;
        }
    } else if (is_bool()) {
        if (auto v = mc::string::try_to_bool(other)) {
            return m_bool > *v;
        }
    }
    throw_invalid_type_comparison_error(get_type_name(get_type()), "string_view", ">");
    return false;
}

template <typename Config>
bool variant_base<Config>::operator==(std::string_view other) const {
    if (is_string()) {
        return *m_string_ptr == other;
    } else if (is_blob()) {
        return m_blob_ptr->as_string_view() == other;
    } else if (is_double()) {
        if (auto v = mc::string::try_to_number<double>(other)) {
            return MC_FLOAT_EQUAL(m_double, *v, VARIANT_FLOAT_EPSILON);
        }
    } else if (is_signed_integer()) {
        if (auto v = mc::string::try_to_number<int64_t>(other)) {
            return m_int64 == *v;
        }
    } else if (is_unsigned_integer()) {
        if (auto v = mc::string::try_to_number<uint64_t>(other)) {
            return m_uint64 == *v;
        }
    } else if (is_bool()) {
        if (auto v = mc::string::try_to_bool(other)) {
            return m_bool == *v;
        }
    }

    return false;
}
} // namespace mc