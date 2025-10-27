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
 * @file variant_base.cpp
 * @brief variant_base 类的显式模板实例化
 */

#include <mc/dict/entry.h>
#include <mc/traits.h>
#include <mc/variant/variant_base.h>
#include <mc/variant/variant_reference.h>

namespace mc {

template <typename Config>
variant_base<Config>::variant_base(const variant_base& other)
    : m_type(other.m_type), m_is_fixed(other.m_is_fixed), m_alloc(other.m_alloc) {
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
        new (&m_array) array_type(other.m_array, m_alloc);
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
mc::dict variant_base<Config>::as_dict() const {
    return as_object();
}

template <typename Config>
mc::dict variant_base<Config>::as_object() const {
    if (!is_object()) {
        throw_type_error("object", m_type);
    }
    return m_object;
}

template <typename Config>
void variant_base<Config>::clear() {
    switch (m_type) {
    case type_id::string_type:
        mc::destroy_ptr(m_alloc, m_string_ptr);
        break;
    case type_id::array_type:
        m_array.~array_type();
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
        return m_array.size();
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
variant_reference<Config> variant_base<Config>::operator[](std::string_view key) {
    if (m_type == type_id::object_type) {
        return variant_reference<Config>(m_object[key]);
    } else if (m_type == type_id::extension_type) {
        if (!m_extension) {
            throw_extension_null_error();
        }
        // 优化：如果 extension 支持零开销引用访问，直接返回指针
        if (m_extension->supports_reference_access()) {
            if (auto* ptr = m_extension->get_ptr(key)) {
                return variant_reference<Config>(*ptr);
            }
        }
        // 否则使用值访问模式（有拷贝开销）
        return variant_reference<Config>(m_extension, std::string(key));
    } else {
        throw_type_error("object", m_type);
    }
}

template <typename Config>
variant_reference<Config> variant_base<Config>::operator[](std::string_view key) const {
    if (m_type == type_id::object_type) {
        return variant_reference<Config>(const_cast<variant_base<Config>&>((m_object)[key]));
    } else if (m_type == type_id::extension_type) {
        if (!m_extension) {
            throw_extension_null_error();
        }
        // 优化：如果 extension 支持零开销引用访问，直接返回指针
        if (m_extension->supports_reference_access()) {
            if (auto* ptr = m_extension->get_ptr(key)) {
                return variant_reference<Config>(const_cast<variant_base<Config>&>(*ptr));
            }
        }
        // 否则使用值访问模式（有拷贝开销）
        return variant_reference<Config>(m_extension, std::string(key));
    } else {
        throw_type_error("object", m_type);
    }
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
bool variant_base<Config>::operator==(const dict& other) const {
    if (!is_object()) {
        return false;
    }
    return m_object == other;
}

// ======== 第四批：赋值和设置函数 ========

template <typename Config>
template <typename OtherConfig>
variant_base<OtherConfig>& variant_base<Config>::set_value(const variant_base<OtherConfig>& other) {
    switch (m_type) {
    case type_id::int8_type: {
        m_int64 = other.as_int8();
        break;
    }
    case type_id::uint8_type: {
        m_uint64 = other.as_uint8();
        break;
    }
    case type_id::int16_type: {
        m_int64 = other.as_int16();
        break;
    }
    case type_id::uint16_type: {
        m_uint64 = other.as_uint16();
        break;
    }
    case type_id::int32_type: {
        m_int64 = other.as_int32();
        break;
    }
    case type_id::uint32_type: {
        m_uint64 = other.as_uint32();
        break;
    }
    case type_id::int64_type: {
        m_int64 = other.as_int64();
        break;
    }
    case type_id::uint64_type: {
        m_uint64 = other.as_uint64();
        break;
    }
    case type_id::double_type: {
        m_double = other.as_double();
        break;
    }
    case type_id::bool_type: {
        m_bool = other.as_bool();
        break;
    }
    case type_id::string_type: {
        *m_string_ptr = other.as_string();
        break;
    }
    case type_id::array_type: {
        new (&m_array) array_type(other.as_array());
        break;
    }
    case type_id::object_type: {
        new (&m_object) object_type(other.as_object());
        break;
    }
    case type_id::blob_type: {
        *m_blob_ptr = other.as_blob();
        break;
    }
    case type_id::extension_type: {
        new (&m_extension) extension_ptr_type(other.as_extension());
        break;
    }
    default:
        throw_unknow_type_error(m_type);
        break;
    }

    return *this;
}

template <typename Config>
template <typename OtherConfig>
variant_base<OtherConfig>& variant_base<Config>::set_value(variant_base<OtherConfig>&& other) {
    if (m_type == other.m_type) {
        // 如果类型相同，直接移动内容
        swap(other);
        return *this;
    } else {
        // 调用普通的常量引用版本
        return set_value(other);
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
        new (&m_array) array_type(m_alloc);
        for (const auto& item : other.m_array) {
            m_array.push_back(item);
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

// ======== 成员函数实现 ========

// 构造函数
template <typename Config>
variant_base<Config>::variant_base(type_id type) : m_uint64(0), m_type(type), m_is_fixed(false) {
    switch (type) {
    case type_id::string_type:
        m_string_ptr = mc::allocate_ptr<string_type>(m_alloc);
        break;
    case type_id::array_type:
        new (&m_array) array_type();
        break;
    case type_id::object_type:
        new (&m_object) object_type();
        break;
    case type_id::blob_type:
        m_blob_ptr = mc::allocate_ptr<blob_type>(m_alloc);
        break;
    case type_id::extension_type:
        new (&m_extension) extension_ptr_type();
        break;
    default:
        break;
    }
}

// 移动构造函数
template <typename Config>
variant_base<Config>::variant_base(variant_base&& other) noexcept {
    m_type     = other.m_type;
    m_is_fixed = other.m_is_fixed;
    m_alloc    = std::move(other.m_alloc);
    switch (m_type) {
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
        m_string_ptr = other.m_string_ptr;
        break;
    case type_id::array_type:
        new (&m_array) array_type(other.m_array);
        break;
    case type_id::object_type:
        new (&m_object) object_type(std::move(other.m_object));
        break;
    case type_id::blob_type:
        m_blob_ptr = other.m_blob_ptr;
        break;
    case type_id::extension_type:
        new (&m_extension) extension_ptr_type(std::move(other.m_extension));
        break;
    default:
        break;
    }

    other.m_type     = type_id::null_type;
    other.m_is_fixed = false;
    other.m_uint64   = 0;
}

// 访问器函数
template <typename Config>
void variant_base<Config>::visit(const visitor& v) const {
    switch (m_type) {
    case type_id::null_type:
        v.handle();
        break;
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        v.handle(m_int64);
        break;
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        v.handle(m_uint64);
        break;
    case type_id::double_type:
        v.handle(m_double);
        break;
    case type_id::bool_type:
        v.handle(m_bool);
        break;
    case type_id::string_type:
        v.handle(*m_string_ptr);
        break;
    case type_id::object_type:
        v.handle(m_object);
        break;
    case type_id::array_type:
        v.handle(m_array);
        break;
    case type_id::blob_type:
        v.handle(*m_blob_ptr);
        break;
    case type_id::extension_type:
        if (m_extension) {
            v.handle(*m_extension);
        }
        break;
    default:
        throw_unknow_type_error(m_type);
    }
}

// 类型检查函数
template <typename Config>
bool variant_base<Config>::is_numeric() const {
    switch (m_type) {
    case type_id::bool_type: // 与 std::is_arithmetic_v 一样，bool 类型也属于数值类型
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
    case type_id::double_type:
        return true;
    default:
        return false;
    }
}

template <typename Config>
bool variant_base<Config>::is_integer() const {
    switch (m_type) {
    case type_id::bool_type: // 与 std::is_integer_v 一样，bool 类型也属于整数类型
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        return true;
    default:
        return false;
    }
}

template <typename Config>
bool variant_base<Config>::is_signed_integer() const {
    return m_type == type_id::int8_type || m_type == type_id::int16_type ||
           m_type == type_id::int32_type || m_type == type_id::int64_type;
}

template <typename Config>
bool variant_base<Config>::is_unsigned_integer() const {
    return m_type == type_id::uint8_type || m_type == type_id::uint16_type ||
           m_type == type_id::uint32_type || m_type == type_id::uint64_type;
}

// 类型转换函数
template <typename Config>
int64_t variant_base<Config>::as_int64() const {
    switch (m_type) {
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        return m_int64;
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        return static_cast<int64_t>(m_uint64);
    case type_id::double_type:
        return static_cast<int64_t>(m_double);
    case type_id::bool_type:
        return m_bool ? 1 : 0;
    case type_id::string_type:
        return mc::string::to_number<int64_t>(*m_string_ptr);
    case type_id::blob_type:
        return mc::string::to_number<int64_t>(m_blob_ptr->as_string_view());
    case type_id::extension_type:
        return m_extension ? m_extension->as_int64() : 0;
    default:
        throw_type_error("int64", m_type);
    }
}

template <typename Config>
uint64_t variant_base<Config>::as_uint64() const {
    switch (m_type) {
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        return m_uint64;
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        return static_cast<uint64_t>(m_int64);
    case type_id::double_type:
        return static_cast<uint64_t>(m_double);
    case type_id::bool_type:
        return m_bool ? 1 : 0;
    case type_id::string_type:
        return mc::string::to_number<uint64_t>(*m_string_ptr);
    case type_id::blob_type:
        return mc::string::to_number<uint64_t>(m_blob_ptr->as_string_view());
    case type_id::extension_type:
        return m_extension ? m_extension->as_uint64() : 0;
    default:
        throw_type_error("uint64", m_type);
        return 0;
    }
}

template <typename Config>
bool variant_base<Config>::as_bool(bool strict) const {
    switch (m_type) {
    case type_id::bool_type:
        return m_bool;
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        return m_int64 != 0;
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        return m_uint64 != 0;
    case type_id::double_type:
        return m_double != 0;
    case type_id::string_type: {
        return mc::string::to_bool_with_default(*m_string_ptr, !strict);
    }
    case type_id::blob_type: {
        return mc::string::to_bool_with_default(m_blob_ptr->as_string_view(), !strict);
    }
    case type_id::extension_type:
        return m_extension ? m_extension->as_bool() : false;
    case type_id::null_type:
        return false;
    case type_id::array_type:
        return !m_array.empty();
    case type_id::object_type:
        return !m_object.empty();
    default:
        return false;
    }
}

template <typename Config>
double variant_base<Config>::as_double() const {
    switch (m_type) {
    case type_id::double_type:
        return m_double;
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        return static_cast<double>(m_int64);
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        return static_cast<double>(m_uint64);
    case type_id::bool_type:
        return m_bool ? 1.0 : 0.0;
    case type_id::string_type: {
        return mc::string::to_number<double>(*m_string_ptr);
    }
    case type_id::blob_type: {
        return mc::string::to_number<double>(m_blob_ptr->as_string_view());
    }
    case type_id::extension_type:
        return m_extension ? m_extension->as_double() : 0.0;
    default:
        throw_type_error("double", m_type);
        return 0.0;
    }
}

template <typename Config>
blob_base<> variant_base<Config>::as_blob() const {
    switch (m_type) {
    case type_id::blob_type:
        return *m_blob_ptr;
    case type_id::string_type: {
        blob_base<> b;
        b.data.assign(m_string_ptr->begin(), m_string_ptr->end());
        return b;
    }
    case type_id::extension_type: {
        if (!m_extension) {
            return {};
        }

        blob_base<> b;
        auto        s = m_extension->as_string();
        b.data.assign(s.begin(), s.end());
        return b;
    }
    default:
        throw_type_error("blob_base", m_type);
    }
}

template <typename Config>
std::string variant_base<Config>::as_string() const {
    switch (m_type) {
    case type_id::string_type:
        return *m_string_ptr;
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        return std::to_string(m_int64);
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        return std::to_string(m_uint64);
    case type_id::double_type: {
        return mc::to_string(m_double);
    }
    case type_id::bool_type:
        return m_bool ? "true" : "false";
    case type_id::null_type:
        return "null";
    case type_id::blob_type: {
        return std::string(m_blob_ptr->as_string_view());
    }
    case type_id::extension_type:
        return m_extension ? m_extension->as_string() : std::string();
    default:
        return to_string();
    }
}

// 数组访问操作符
template <typename Config>
variant_reference<Config> variant_base<Config>::operator[](std::size_t pos) {
    if (m_type == type_id::array_type) {
        auto& arr = m_array;
        if (pos >= arr.size()) {
            throw_out_of_range_error(pos, arr.size());
        }
        return arr[pos];
    } else if (m_type == type_id::extension_type) {
        if (!m_extension) {
            throw_extension_null_error();
        }
        // 优化：如果 extension 支持零开销引用访问，直接返回指针
        if (m_extension->supports_reference_access()) {
            if (auto* ptr = m_extension->get_ptr(pos)) {
                return variant_reference<Config>(*ptr);
            }
        }
        // 否则使用值访问模式（有拷贝开销）
        return variant_reference<Config>(m_extension, pos);
    } else {
        throw_type_error("array", m_type);
    }
}

template <typename Config>
variant_reference<Config> variant_base<Config>::operator[](std::size_t pos) const {
    if (m_type == type_id::array_type) {
        const auto& arr = m_array;
        if (pos >= arr.size()) {
            throw_out_of_range_error(pos, arr.size());
        }
        return variant_reference<Config>(arr[pos]);
    } else if (m_type == type_id::extension_type) {
        if (!m_extension) {
            throw_extension_null_error();
        }
        // 优化：如果 extension 支持零开销引用访问，直接返回指针
        if (m_extension->supports_reference_access()) {
            if (auto* ptr = m_extension->get_ptr(pos)) {
                return variant_reference<Config>(const_cast<variant_base<Config>&>(*ptr));
            }
        }
        // 否则使用值访问模式（有拷贝开销）
        return variant_reference<Config>(m_extension, pos);
    } else {
        throw_type_error("array", m_type);
    }
}

// 赋值操作符
template <typename Config>
variant_base<Config>& variant_base<Config>::operator=(const variant_base& other) {
    if (this == &other) {
        return *this;
    }

    if (!is_fixed_type()) {
        variant_base(other).swap(*this);
    } else {
        if (other.m_type == m_type || m_type == type_id::null_type) {
            variant_base(other).swap(*this);
        } else {
            set_value(other);
        }
    }
    return *this;
}

template <typename Config>
variant_base<Config>& variant_base<Config>::operator=(variant_base&& other) {
    if (this == &other) {
        return *this;
    }

    if (!is_fixed_type()) {
        variant_base(std::move(other)).swap(*this);
    } else {
        if (other.m_type == m_type || m_type == type_id::null_type) {
            variant_base(std::move(other)).swap(*this);
        } else {
            // 类型不一样那就一定存在转换，相比move会有一定开销
            set_value(other);
            other.clear();
        }
    }
    return *this;
}

template <typename Config>
variant_base<Config>& variant_base<Config>::operator=(const char* s) {
    if (s) {
        return operator=(std::string_view(s));
    }

    switch (m_type) {
    case type_id::null_type:
        break;
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        m_int64 = 0;
        break;
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        m_uint64 = 0;
        break;
    case type_id::double_type:
        m_double = 0;
        break;
    case type_id::bool_type:
        m_bool = 0;
        break;
    case type_id::string_type:
    case type_id::array_type:
    case type_id::object_type:
    case type_id::blob_type:
    case type_id::extension_type:
    default: {
        if (!is_fixed_type()) {
            variant_base().swap(*this);
        } else if (m_type == type_id::string_type) {
            // 对于固定类型来说，给字符串赋值 0(nullptr) 会清空字符串
            m_string_ptr->clear();
        } else {
            throw_type_error(get_type_name(), type_id::string_type);
        }
        break;
    }
    }
    return *this;
}

template <typename Config>
variant_base<Config>& variant_base<Config>::operator=(std::string_view s) {
    if (m_type == type_id::string_type) {
        m_string_ptr->assign(s.begin(), s.end());
    } else if (m_type == type_id::blob_type) {
        // 字符串可以赋值给二进制类型
        m_blob_ptr->data.assign(s.begin(), s.end());
    } else {
        *this = variant_base(s);
    }
    return *this;
}

template <typename Config>
template <typename Allocator>
variant_base<Config>& variant_base<Config>::operator=(const std::basic_string<char, std::char_traits<char>, Allocator>& s) {
    return operator=(std::string_view(s));
}

template <typename Config>
template <typename Allocator>
variant_base<Config>& variant_base<Config>::operator=(const blob_base<Allocator>& b) {
    if (m_type == type_id::blob_type) {
        m_blob_ptr->data.assign(b.data.begin(), b.data.end());
    } else {
        *this = variant_base(b);
    }
    return *this;
}

// 拷贝函数
template <typename Config>
variant_base<Config> variant_base<Config>::copy() const {
    return visit_with([this](const auto& value) -> variant_base {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            return variant_base();
        } else if constexpr (std::is_arithmetic_v<T>) {
            return variant_base(value);
        } else if constexpr (std::is_same_v<T, string_type>) {
            return variant_base(value, this->m_alloc);
        } else if constexpr (std::is_same_v<T, array_type>) {
            return variant_base(value.copy());
        } else if constexpr (std::is_same_v<T, object_type>) {
            return variant_base(value.copy());
        } else if constexpr (std::is_same_v<T, blob_type>) {
            variant_base result(type_id::blob_type);
            result.m_alloc    = this->m_alloc;
            result.m_blob_ptr = mc::allocate_ptr<blob_type>(this->m_alloc, value.data.data(), value.data.size(), this->m_alloc);
            return result;
        } else if constexpr (std::is_same_v<T, extension_type>) {
            variant_base result(type_id::extension_type);
            result.m_alloc     = this->m_alloc;
            result.m_extension = value.copy();
            return result;
        } else {
            return variant_base();
        }
    });
}

// 深拷贝函数
template <typename Config>
variant_base<Config> variant_base<Config>::deep_copy(mc::detail::copy_context* ctx) const {
    if (!ctx) {
        mc::detail::copy_context local_ctx;
        return deep_copy(&local_ctx);
    }

    return visit_with([this, ctx](const auto& value) -> variant_base {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            return variant_base();
        } else if constexpr (std::is_arithmetic_v<T>) {
            return variant_base(value);
        } else if constexpr (std::is_same_v<T, string_type>) {
            return variant_base(value, this->m_alloc);
        } else if constexpr (std::is_same_v<T, array_type>) {
            return variant_base(value.deep_copy(ctx));
        } else if constexpr (std::is_same_v<T, object_type>) {
            return variant_base(value.deep_copy(ctx));
        } else if constexpr (std::is_same_v<T, blob_type>) {
            variant_base result(type_id::blob_type);
            result.m_alloc    = this->m_alloc;
            result.m_blob_ptr = mc::allocate_ptr<blob_type>(this->m_alloc, value.data.data(), value.data.size(), this->m_alloc);
            return result;
        } else if constexpr (std::is_same_v<T, extension_type>) {
            variant_base result(type_id::extension_type);
            result.m_alloc     = this->m_alloc;
            result.m_extension = value.deep_copy(ctx); // 调用 extension 的 deep_copy
            return result;
        } else {
            return variant_base();
        }
    });
}

// 交换函数
template <typename Config>
void variant_base<Config>::swap(variant_base& other) noexcept {
    type_id temp_type = m_type;
    m_type            = other.m_type;
    other.m_type      = temp_type;

    std::swap(m_uint64, other.m_uint64);
    std::swap(m_alloc, other.m_alloc);
}

// 类型名获取函数
template <typename Config>
const char* variant_base<Config>::get_type_name() const {
    if (m_type == type_id::extension_type && m_extension) {
        return m_extension->get_type_name().data();
    }
    return get_type_name_internal(m_type);
}

// 哈希函数
template <typename Config>
size_t variant_base<Config>::hash() const {
    switch (m_type) {
    case type_id::null_type:
        return 0;
    case type_id::bool_type:
        return std::hash<bool>{}(m_bool);
    case type_id::int8_type:
    case type_id::int16_type:
    case type_id::int32_type:
    case type_id::int64_type:
        return std::hash<int64_t>{}(m_int64);
    case type_id::uint8_type:
    case type_id::uint16_type:
    case type_id::uint32_type:
    case type_id::uint64_type:
        return std::hash<uint64_t>{}(m_uint64);
    case type_id::double_type:
        return std::hash<double>{}(m_double);
    case type_id::string_type: {
        const auto& str_data = *m_string_ptr;
        return calculate_str_hash(str_data);
    }
    case type_id::blob_type: {
        const auto& blob_data = *m_blob_ptr;
        return calculate_str_hash(blob_data.as_string_view());
    }
    case type_id::array_type: {
        return calculate_array_hash(m_array);
    }
    case type_id::object_type: {
        // 使用dict的hash方法计算哈希值
        return m_object.hash();
    }
    case type_id::extension_type:
        return m_extension ? m_extension->hash() : 0;
    default:
        return 0;
    }
}

template <typename Config>
typename variant_base<Config>::extension_ptr_type variant_base<Config>::as_extension() const {
    if (m_type != type_id::extension_type) {
        throw_type_error("extension", m_type);
    }
    return m_extension;
}

template <typename Config>
typename variant_base<Config>::array_type variant_base<Config>::as_array() const {
    if (m_type == type_id::array_type) {
        return m_array;
    }

    throw_type_error("array", m_type);
}

template <typename Config>
const std::string& variant_base<Config>::get_string() const {
    if (m_type != type_id::string_type) {
        throw_type_error("string", m_type);
    }
    return *m_string_ptr;
}

template <typename Config>
const typename variant_base<Config>::blob_type& variant_base<Config>::get_blob() const {
    if (m_type != type_id::blob_type) {
        throw_type_error("blob", m_type);
    }
    return *m_blob_ptr;
}

template <typename Config>
const typename variant_base<Config>::array_type& variant_base<Config>::get_array() const {
    if (m_type != type_id::array_type) {
        throw_type_error("array", m_type);
    }
    return m_array;
}

template <typename Config>
const dict& variant_base<Config>::get_object() const {
    if (m_type != type_id::object_type) {
        throw_type_error("object", m_type);
    }
    return m_object;
}

template <typename Config, typename Op>
auto numeric_op(const variant_base<Config>& self, Op&& op, detail::numeric_t rhs, const char* op_name) {
    try {
        using return_type = std::invoke_result_t<Op, int>;
        if constexpr (std::is_same_v<return_type, std::optional<bool>>) {
            std::optional<bool> r = std::visit(op, rhs.data);
            if (r) {
                return *r;
            }
        } else {
            return std::visit(op, rhs.data);
        }
    } catch (...) {
    }

    throw_invalid_type_comparison_error(self.get_type_name(), "numeric", op_name);
}

template <typename Config>
bool variant_base<Config>::operator<(detail::numeric_t rhs) const {
    return numeric_op(*this, [&](auto&& other) -> std::optional<bool> {
        using T = std::decay_t<decltype(other)>;
        if (is_double()) {
            if (std::isnan(m_double)) {
                return false;
            }
            return m_double < other;
        } else if (is_signed_integer()) {
            return m_int64 < other;
        } else if (is_unsigned_integer()) {
            return m_uint64 < other;
        } else if (is_bool()) {
            return m_bool < other;
        } else if (is_string() || is_blob()) {
            return as<T>() < other;
        }
        return std::nullopt;
    }, rhs, "<");
}

template <typename Config>
bool variant_base<Config>::operator>(detail::numeric_t rhs) const {
    return numeric_op(*this, [&](auto&& other) -> std::optional<bool> {
        using T = std::decay_t<decltype(other)>;
        if (is_double()) {
            if (std::isnan(m_double)) {
                return false;
            }
            return m_double > other;
        } else if (is_signed_integer()) {
            return m_int64 > other;
        } else if (is_unsigned_integer()) {
            return m_uint64 > other;
        } else if (is_bool()) {
            return m_bool > other;
        } else if (is_string() || is_blob()) {
            return as<T>() > other;
        }
        return std::nullopt;
    }, rhs, ">");
}

template <typename Config>
bool variant_base<Config>::operator<=(detail::numeric_t rhs) const {
    if (is_double() && std::isnan(m_double)) {
        return false;
    }
    return !(*this > rhs);
}

template <typename Config>
bool variant_base<Config>::operator>=(detail::numeric_t rhs) const {
    if (is_double() && std::isnan(m_double)) {
        return false;
    }
    return !(*this < rhs);
}

template <typename Config>
bool variant_base<Config>::operator==(detail::numeric_t rhs) const {
    return numeric_op(*this, [&](auto&& other) -> bool {
        using T = std::decay_t<decltype(other)>;
        if (is_double()) {
            if (std::isnan(m_double)) {
                return false;
            }
            return MC_FLOAT_EQUAL(m_double, other, VARIANT_FLOAT_EPSILON);
        } else if (is_signed_integer()) {
            return m_int64 == other;
        } else if (is_unsigned_integer()) {
            return m_uint64 == other;
        } else if (is_bool()) {
            return m_bool == other;
        } else if (is_string() || is_blob()) {
            return as<T>() == other;
        }
        return false;
    }, rhs, "==");
}

template <typename T>
inline bool is_unsigned(T value) {
    if constexpr (std::is_unsigned_v<T> || std::is_same_v<T, bool>) {
        return true;
    } else {
        return value >= 0;
    }
}

template <typename Config>
variant_base<Config> variant_base<Config>::operator+(detail::numeric_t rhs) const {
    return numeric_op(*this, [&](auto&& other) -> variant_base<Config> {
        using T = std::decay_t<decltype(other)>;
        if (is_string()) {
            return variant_base<Config>(get_string() + std::to_string(other));
        } else if (is_array()) {
            return *this + variant(other);
        } else if (is_object()) {
            throw_invalid_type_operation_error(get_type_name(), pretty_name<T>(), "+");
        } else if (is_double() || std::is_floating_point_v<T>) {
            return variant_base<Config>(as_double() + static_cast<double>(other));
        } else if (is_unsigned_integer() && is_unsigned(other)) {
            return variant_base<Config>(as_uint64() + static_cast<uint64_t>(other));
        } else {
            return variant_base<Config>(as_int64() + static_cast<int64_t>(other));
        }
    }, rhs, "+");
}

template <typename Config>
variant_base<Config> variant_base<Config>::operator-(detail::numeric_t rhs) const {
    return numeric_op(*this, [&](auto&& other) -> variant_base<Config> {
        using T = std::decay_t<decltype(other)>;
        if (is_double() || std::is_floating_point_v<T>) {
            return variant_base<Config>(as_double() - static_cast<double>(other));
        } else if (is_unsigned_integer() && is_unsigned(other)) {
            if (as_uint64() < static_cast<uint64_t>(other)) {
                return variant_base<Config>(static_cast<int64_t>(as_uint64()) -
                                            static_cast<int64_t>(other));
            }
            return variant_base<Config>(as_uint64() - static_cast<uint64_t>(other));
        }

        return variant_base<Config>(as_int64() - static_cast<int64_t>(other));
    }, rhs, "-");
}

template <typename Config>
variant_base<Config> variant_base<Config>::operator*(detail::numeric_t rhs) const {
    return numeric_op(*this, [&](auto&& other) -> variant_base<Config> {
        using T = std::decay_t<decltype(other)>;
        if (is_double() || std::is_floating_point_v<T>) {
            return variant_base<Config>(as_double() * static_cast<double>(other));
        } else if (is_unsigned_integer() && is_unsigned(other)) {
            return variant_base<Config>(as_uint64() * static_cast<uint64_t>(other));
        } else {
            return variant_base<Config>(as_int64() * static_cast<int64_t>(other));
        }
    }, rhs, "*");
}

static bool is_zero(detail::numeric_t value) {
    return std::visit([](auto&& v) -> bool {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_floating_point_v<T>) {
            return MC_FLOAT_EQUAL(static_cast<double>(v), 0.0, VARIANT_FLOAT_EPSILON);
        } else {
            return v == 0;
        }
    }, value.data);
}

template <typename Config>
variant_base<Config> variant_base<Config>::operator/(detail::numeric_t rhs) const {
    if (is_zero(rhs)) {
        throw_divide_by_zero_exception("除零错误");
    }

    return numeric_op(*this, [&](auto&& other) -> variant_base<Config> {
        using T = std::decay_t<decltype(other)>;

        if (is_double() || std::is_floating_point_v<T> || is_bool() || std::is_same_v<T, bool>) {
            return variant_base<Config>(as_double() / static_cast<double>(other));
        } else if (is_unsigned_integer() && is_unsigned(other)) {
            return variant_base<Config>(as_uint64() / static_cast<uint64_t>(other));
        }

        return variant_base<Config>(as_int64() / static_cast<int64_t>(other));
    }, rhs, "/");
}

template <typename Config>
variant_base<Config> variant_base<Config>::operator%(detail::numeric_t rhs) const {
    if (is_zero(rhs)) {
        throw_divide_by_zero_exception("取模运算除零错误");
    }

    return numeric_op(*this, [&](auto&& other) -> variant_base<Config> {
        if (is_unsigned_integer() && is_unsigned(other)) {
            return variant_base<Config>(as_uint64() % static_cast<uint64_t>(other));
        } else {
            return variant_base<Config>(as_int64() % static_cast<int64_t>(other));
        }
    }, rhs, "%");
}

template <typename Config>
variant_base<Config> variant_base<Config>::operator<<(detail::numeric_t rhs) const {
    return numeric_op(*this, [&](auto&& other) -> variant_base<Config> {
        uint64_t shift_amount = static_cast<uint64_t>(other);
        if (shift_amount >= sizeof(uint64_t) * 8) {
            return {0};
        }
        if (is_unsigned_integer()) {
            return {as_uint64() << shift_amount};
        }
        return {as_int64() << shift_amount};
    }, rhs, "<<");
}

template <typename Config>
variant_base<Config> variant_base<Config>::operator>>(detail::numeric_t rhs) const {
    return numeric_op(*this, [&](auto&& other) -> variant_base<Config> {
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
    }, rhs, ">>");
}

template <typename Config>
variant_base<Config> variant_base<Config>::operator&(detail::numeric_t rhs) const {
    return numeric_op(*this, [&](auto&& other) -> variant_base<Config> {
        if (is_unsigned_integer() && is_unsigned(other)) {
            return variant_base<Config>(as_uint64() & static_cast<uint64_t>(other));
        }
        return variant_base<Config>(as_int64() & static_cast<int64_t>(other));
    }, rhs, "&");
}

template <typename Config>
variant_base<Config> variant_base<Config>::operator|(detail::numeric_t rhs) const {
    return numeric_op(*this, [&](auto&& other) -> variant_base<Config> {
        if (is_unsigned_integer() && is_unsigned(other)) {
            return variant_base<Config>(as_uint64() | static_cast<uint64_t>(other));
        }
        return variant_base<Config>(as_int64() | static_cast<int64_t>(other));
    }, rhs, "|");
}

template <typename Config>
variant_base<Config> variant_base<Config>::operator^(detail::numeric_t rhs) const {
    return numeric_op(*this, [&](auto&& other) -> variant_base<Config> {
        if (is_unsigned_integer() && is_unsigned(other)) {
            return variant_base<Config>(as_uint64() ^ static_cast<uint64_t>(other));
        }
        return variant_base<Config>(as_int64() ^ static_cast<int64_t>(other));
    }, rhs, "^");
}

template <typename Config>
void to_variant(const dict& var, variant_base<Config>& vo) {
    variant_base<Config>(var).swap(vo);
}

template <typename Config>
void from_variant(const variant_base<Config>& var, dict& vo) {
    vo = var.as_dict();
}

// ======== 显式实例化 ========
template class MC_API variant_base<variant_config<>>;

template MC_API variant& variant::set_value<variant_config<>>(const variant& other);
template MC_API variant& variant::set_value<variant_config<>>(variant&& other);
template MC_API          variant::variant_base(const variant& other, const allocator_type& alloc);

template MC_API variant& variant::operator= <std::allocator<char>>(const std::basic_string<char, std::char_traits<char>, std::allocator<char>>& s);
template MC_API variant& variant::operator= <std::allocator<char>>(const blob_base<std::allocator<char>>& b);

template MC_API void to_variant(const dict& var, variant& vo);
template MC_API void from_variant(const variant& var, dict& vo);
} // namespace mc

#include "./variant_base_cmp_op.cpp"
#include "./variant_base_op.cpp"
