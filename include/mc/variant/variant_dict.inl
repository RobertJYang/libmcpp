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
 * @file variant_dict.inl
 * @brief 定义了依赖 dict 的 variant 实现部分
 */

#ifndef MC_VARIANT_DICT_INL
#define MC_VARIANT_DICT_INL

namespace mc {

template <typename Config>
dict variant_base<Config>::as_dict() const {
    return as_object();
}

template <typename Config>
mutable_dict variant_base<Config>::as_mutable_dict() const {
    return as_object();
}

template <typename Config>
dict variant_base<Config>::as_object() const {
    if (!is_object()) {
        throw_type_error("object", m_type);
    }
    return m_object;
}

template <typename Config>
variant_base<Config>::variant_base(const variant_base& other)
    : m_type(other.m_type), m_alloc(other.m_alloc) {
    switch (other.m_type) {
    case type_id::null_type:
        break;
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        m_int64 = other.m_int64;
        break;
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        m_uint64 = other.m_uint64;
        break;
    case type_id::double_type:
        m_double = other.m_double;
        break;
    case type_id::bool_type:
        m_bool = other.m_bool;
        break;
    case type_id::string_type:
        m_string_ptr = mc::allocate_ptr<string_type>(m_alloc, other.m_string_ptr->data(),
                                                     other.m_string_ptr->size(), m_alloc);
        break;
    case type_id::array_type:
        m_array_ptr = mc::allocate_ptr<array_type>(m_alloc, *other.m_array_ptr, m_alloc);
        break;
    case type_id::object_type:
        new (&m_object) object_type(other.m_object);
        break;
    case type_id::blob_type:
        m_blob_ptr = mc::allocate_ptr<blob_type>(m_alloc, other.m_blob_ptr->data.data(),
                                                 other.m_blob_ptr->data.size(), m_alloc);
        break;
    case type_id::extension_type:
        new (&m_extension) extension_ptr_type(other.m_extension);
        break;
    default:
        break;
    }
}

template <typename Config>
template <typename OtherConfig>
variant_base<Config>::variant_base(const variant_base<OtherConfig>& other,
                                   const allocator_type&            alloc) {
    m_type  = other.m_type;
    m_alloc = alloc;
    switch (other.m_type) {
    case type_id::null_type:
        break;
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        m_int64 = other.m_int64;
        break;
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        m_uint64 = other.m_uint64;
        break;
    case type_id::double_type:
        m_double = other.m_double;
        break;
    case type_id::bool_type:
        m_bool = other.m_bool;
        break;
    case type_id::string_type:
        m_string_ptr = mc::allocate_ptr<string_type>(m_alloc, other.m_string_ptr->data(),
                                                     other.m_string_ptr->size(), m_alloc);
        break;
    case type_id::array_type: {
        m_array_ptr = mc::allocate_ptr<array_type>(m_alloc, m_alloc);
        for (auto& item : *other.m_array_ptr) {
            m_array_ptr->emplace_back(item, m_alloc);
        }
        break;
    }
    case type_id::object_type:
        new (&m_object) object_type(other.m_object);
        break;
    case type_id::blob_type:
        m_blob_ptr = mc::allocate_ptr<blob_type>(m_alloc, other.m_blob_ptr->data.data(),
                                                 other.m_blob_ptr->data.size(), m_alloc);
        break;
    case type_id::extension_type:
        new (&m_extension) extension_ptr_type(other.m_extension);
        break;
    default:
        break;
    }
}

template <typename Config>
void variant_base<Config>::clear() {
    switch (m_type) {
    case type_id::string_type:
        mc::destroy_ptr(m_alloc, m_string_ptr);
        break;
    case type_id::array_type:
        mc::destroy_ptr(m_alloc, m_array_ptr);
        break;
    case type_id::object_type:
        m_object.~object_type();
        break;
    case type_id::blob_type:
        mc::destroy_ptr(m_alloc, m_blob_ptr);
        break;
    case type_id::extension_type:
        m_extension.~extension_ptr_type();
        break;
    default:
        break;
    }
    m_type   = type_id::null_type;
    m_uint64 = 0;
}

template <typename Config>
size_t variant_base<Config>::size() const {
    switch (m_type) {
    case type_id::array_type:
        return m_array_ptr->size();
    case type_id::object_type:
        return m_object.size();
    case type_id::string_type:
        return m_string_ptr->size();
    case type_id::blob_type:
        return m_blob_ptr->data.size();
    default:
        return 0;
    }
}

template <typename Config>
const variant_base<Config>& variant_base<Config>::operator[](std::string_view key) const {
    if (m_type != type_id::object_type) {
        throw_type_error("object", m_type);
    }

    return (m_object)[key];
}

template <typename Config>
const variant_base<Config>&
variant_base<Config>::get(std::string_view key, const variant_base<Config>& default_value) const {
    if (m_type != type_id::object_type) {
        return default_value;
    }

    return m_object.get(key, default_value);
}

template <typename Config>
bool variant_base<Config>::contains(std::string_view key) const {
    if (m_type != type_id::object_type) {
        return false;
    }

    return m_object.contains(key);
}

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
        if (m_array_ptr->size() != other.m_array_ptr->size()) {
            return false;
        }
        return std::equal(m_array_ptr->begin(), m_array_ptr->end(), other.m_array_ptr->begin());
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
bool variant_base<Config>::operator==(const variant_base<OtherConfig>& other) const {
    if (m_type == other.get_type()) {
        return same_type_equal(other);
    } else {
        return other_type_equal(other);
    }
}

template <typename Config>
bool variant_base<Config>::operator==(const dict& other) const {
    if (m_type != type_id::object_type) {
        return false;
    }
    return m_object == other;
}

template <typename Config>
void to_variant(const dict& var, variant_base<Config>& vo) {
    variant_base<Config>(var).swap(vo);
}

template <typename Config>
void from_variant(const variant_base<Config>& var, dict& vo) {
    vo = var.as_dict();
}

template <typename Config>
void to_variant(const mutable_dict& var, variant_base<Config>& vo) {
    variant_base<Config>(static_cast<const dict&>(var)).swap(vo);
}

template <typename Config>
void from_variant(const variant_base<Config>& var, mutable_dict& vo) {
    vo = var.as_mutable_dict();
}

} // namespace mc

#endif // MC_VARIANT_DICT_INL