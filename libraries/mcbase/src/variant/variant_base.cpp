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
#include <mc/json.h>
#include <mc/string_utils.h>
#include <mc/traits.h>
#include <mc/variant/variant_base.h>
#include <mc/variant/variant_reference.h>
#include <string>
#include <string_view>

namespace mc {

mc::string variant_base::to_string() const
{
    return json::json_encode(*this);
}

variant_base::variant_base(type_id type) : m_uint64(0), m_type(type), m_is_fixed(false)
{
    switch (type) {
        case type_id::string_type:
            new (&m_string) string_type();
            break;
        case type_id::array_type:
            new (&m_array) array_type();
            break;
        case type_id::object_type:
            new (&m_object) object_type();
            break;
        case type_id::blob_type:
            m_blob_ptr = mc::allocate_ptr<blob_type>(allocator_type());
            break;
        case type_id::extension_type:
            new (&m_extension) extension_ptr_type();
            break;
        default:
            break;
    }
}

variant_base::variant_base(const variant_base& other) : m_type(other.m_type), m_is_fixed(other.m_is_fixed)
{
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
            new (&m_string) string_type(other.m_string);
            break;
        case type_id::array_type:
            new (&m_array) array_type(other.m_array);
            break;
        case type_id::object_type:
            new (&m_object) object_type(other.m_object);
            break;
        case type_id::blob_type:
            m_blob_ptr = mc::allocate_ptr<blob_type>(allocator_type(), other.m_blob_ptr->data.data(),
                                                     other.m_blob_ptr->data.size());
            break;
        case type_id::extension_type:
            new (&m_extension) extension_ptr_type(other.m_extension);
            break;
        default:
            break;
    }
}

variant_base::variant_base(variant_base&& other) noexcept
{
    m_type     = other.m_type;
    m_is_fixed = other.m_is_fixed;
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
            new (&m_string) string_type(std::move(other.m_string));
            break;
        case type_id::array_type:
            new (&m_array) array_type(std::move(other.m_array));
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

    if (other.m_type == type_id::string_type) {
        // 已从 other 移走内容，须显式析构内嵌 string，避免随后 m_uint64=0 覆盖未析构对象
        other.m_string.~string_type();
    }

    other.m_type     = type_id::null_type;
    other.m_is_fixed = false;
    other.m_uint64   = 0;
}

mc::dict variant_base::as_dict() const
{
    return as_object();
}

mc::dict variant_base::as_object() const
{
    if (!is_object()) {
        throw_type_error("object", m_type);
    }
    return m_object;
}

void variant_base::clear()
{
    switch (m_type) {
        case type_id::string_type:
            m_string.~string_type();
            break;
        case type_id::array_type:
            m_array.~array_type();
            break;
        case type_id::object_type:
            m_object.~object_type();
            break;
        case type_id::blob_type:
            mc::destroy_ptr(allocator_type(), m_blob_ptr);
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

size_t variant_base::size() const
{
    switch (m_type) {
        case type_id::array_type:
            return m_array.size();
        case type_id::object_type:
            return m_object.size();
        case type_id::string_type:
            return m_string.size();
        case type_id::blob_type:
            return m_blob_ptr->data.size();
        default:
            return 0;
    }
}

variant_reference variant_base::operator[](mc::string_view key)
{
    if (is_object()) {
        return variant_reference(m_object[key]);
    } else if (is_extension()) {
        if (!m_extension) {
            throw_extension_null_error();
        }
        // 优化：如果 extension 支持零开销引用访问，直接返回指针
        if (m_extension->supports_reference_access()) {
            if (auto* ptr = m_extension->get_ptr(key)) {
                return variant_reference(*ptr);
            }
        }
        // 否则使用值访问模式（有拷贝开销）
        return variant_reference(m_extension, key);
    } else {
        throw_type_error("object", get_type());
    }
}

variant_reference variant_base::operator[](mc::string_view key) const
{
    if (is_object()) {
        return variant_reference(const_cast<variant_base&>((m_object)[key]));
    } else if (is_extension()) {
        if (!m_extension) {
            throw_extension_null_error();
        }
        // 优化：如果 extension 支持零开销引用访问，直接返回指针
        if (m_extension->supports_reference_access()) {
            if (auto* ptr = m_extension->get_ptr(key)) {
                return variant_reference(const_cast<variant_base&>(*ptr));
            }
        }
        // 否则使用值访问模式（有拷贝开销）
        return variant_reference(m_extension, key);
    } else {
        throw_type_error("object", get_type());
    }
}

const variant_base& variant_base::get(mc::string_view key, const variant_base& default_value) const
{
    if (!is_object()) {
        return default_value;
    }

    return m_object.get(key, default_value);
}

bool variant_base::contains(mc::string_view key) const
{
    if (!is_object()) {
        return false;
    }

    return m_object.contains(key);
}

bool variant_base::operator==(const dict& other) const
{
    if (!is_object()) {
        return false;
    }
    return m_object == other;
}

variant_base& variant_base::set_value(const variant_base& other)
{
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
            m_string = other.as_string();
            break;
        }
        case type_id::array_type: {
            m_array = other.as_array();
            break;
        }
        case type_id::object_type: {
            m_object = other.as_object();
            break;
        }
        case type_id::blob_type: {
            *m_blob_ptr = other.as_blob();
            break;
        }
        case type_id::extension_type: {
            m_extension = other.as_extension();
            break;
        }
        default:
            throw_unknow_type_error(get_type());
            break;
    }

    return *this;
}

variant_base& variant_base::set_value(variant_base&& other)
{
    if (m_type == other.m_type) {
        // 如果类型相同，直接移动内容
        swap(other);
        return *this;
    } else {
        // 调用普通的常量引用版本
        return set_value(other);
    }
}

void variant_base::visit(const visitor& v) const
{
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
            v.handle(m_string.view());
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
            throw_unknow_type_error(get_type());
    }
}

bool variant_base::is_numeric() const
{
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

bool variant_base::is_integer() const
{
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

bool variant_base::is_signed_integer() const
{
    type_id t = get_type();
    return t == type_id::int8_type || t == type_id::int16_type || t == type_id::int32_type || t == type_id::int64_type;
}

bool variant_base::is_unsigned_integer() const
{
    type_id t = get_type();
    return t == type_id::uint8_type || t == type_id::uint16_type || t == type_id::uint32_type ||
           t == type_id::uint64_type;
}

int64_t variant_base::as_int64() const
{
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
            return mc::strings::to_number<int64_t>(m_string.view());
        case type_id::blob_type:
            return mc::strings::to_number<int64_t>(m_blob_ptr->as_string_view());
        case type_id::extension_type:
            return m_extension ? m_extension->as_int64() : 0;
        default:
            throw_type_error("int64", get_type());
    }
}

uint64_t variant_base::as_uint64() const
{
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
            return mc::strings::to_number<uint64_t>(m_string.view());
        case type_id::blob_type:
            return mc::strings::to_number<uint64_t>(m_blob_ptr->as_string_view());
        case type_id::extension_type:
            return m_extension ? m_extension->as_uint64() : 0;
        default:
            throw_type_error("uint64", get_type());
            return 0;
    }
}

bool variant_base::as_bool(bool strict) const
{
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
            return mc::strings::to_bool_with_default(m_string.view(), !strict);
        }
        case type_id::blob_type: {
            return mc::strings::to_bool_with_default(m_blob_ptr->as_string_view(), !strict);
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

double variant_base::as_double() const
{
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
            return mc::strings::to_number<double>(m_string.view());
        }
        case type_id::blob_type: {
            return mc::strings::to_number<double>(m_blob_ptr->as_string_view());
        }
        case type_id::extension_type:
            return m_extension ? m_extension->as_double() : 0.0;
        default:
            throw_type_error("double", get_type());
            return 0.0;
    }
}

variant_base::blob_type variant_base::as_blob() const
{
    switch (m_type) {
        case type_id::blob_type:
            return *m_blob_ptr;
        case type_id::string_type: {
            blob_type b;
            b.data.assign(m_string.begin(), m_string.end());
            return b;
        }
        case type_id::extension_type: {
            if (!m_extension) {
                return {};
            }

            blob_type b;
            auto      s = m_extension->as_string();
            b.data.assign(s.begin(), s.end());
            return b;
        }
        default:
            throw_type_error("blob_base", get_type());
    }
}

mc::string variant_base::as_string() const
{
    switch (m_type) {
        case type_id::string_type:
            return m_string;
        case type_id::int8_type:
        case type_id::int16_type:
        case type_id::int32_type:
        case type_id::int64_type:
            return mc::to_string(m_int64);
        case type_id::uint8_type:
        case type_id::uint16_type:
        case type_id::uint32_type:
        case type_id::uint64_type:
            return mc::to_string(m_uint64);
        case type_id::double_type: {
            return mc::to_string(m_double);
        }
        case type_id::bool_type:
            return m_bool ? "true" : "false";
        case type_id::null_type:
            return "null";
        case type_id::blob_type: {
            return mc::string(m_blob_ptr->as_string_view());
        }
        case type_id::extension_type:
            return m_extension ? m_extension->as_string() : mc::string();
        default:
            return to_string();
    }
}

variant_reference variant_base::operator[](std::size_t pos)
{
    if (is_array()) {
        auto& arr = m_array;
        if (pos >= arr.size()) {
            throw_out_of_range_error(pos, arr.size());
        }
        return arr[pos];
    } else if (is_extension()) {
        if (!m_extension) {
            throw_extension_null_error();
        }
        // 优化：如果 extension 支持零开销引用访问，直接返回指针
        if (m_extension->supports_reference_access()) {
            if (auto* ptr = m_extension->get_ptr(pos)) {
                return variant_reference(*ptr);
            }
        }
        // 否则使用值访问模式（有拷贝开销）
        return variant_reference(m_extension, pos);
    } else {
        throw_type_error("array", get_type());
    }
}

variant_reference variant_base::operator[](std::size_t pos) const
{
    if (is_array()) {
        const auto& arr = m_array;
        if (pos >= arr.size()) {
            throw_out_of_range_error(pos, arr.size());
        }
        return variant_reference(arr[pos]);
    } else if (is_extension()) {
        if (!m_extension) {
            throw_extension_null_error();
        }
        // 优化：如果 extension 支持零开销引用访问，直接返回指针
        if (m_extension->supports_reference_access()) {
            if (auto* ptr = m_extension->get_ptr(pos)) {
                return variant_reference(const_cast<variant_base&>(*ptr));
            }
        }
        // 否则使用值访问模式（有拷贝开销）
        return variant_reference(m_extension, pos);
    } else {
        throw_type_error("array", get_type());
    }
}

variant_base& variant_base::operator=(const variant_base& other)
{
    if (this == &other) {
        return *this;
    }

    if (!is_fixed_type()) {
        variant_base(other).swap(*this);
    } else {
        if (other.m_type == m_type || is_null()) {
            variant_base(other).swap(*this);
        } else {
            set_value(other);
        }
    }
    return *this;
}

variant_base& variant_base::operator=(variant_base&& other)
{
    if (this == &other) {
        return *this;
    }

    if (!is_fixed_type()) {
        variant_base(std::move(other)).swap(*this);
    } else {
        if (other.m_type == m_type || is_null()) {
            variant_base(std::move(other)).swap(*this);
        } else {
            // 类型不一样那就一定存在转换，相比move会有一定开销
            set_value(other);
            other.clear();
        }
    }
    return *this;
}

variant_base& variant_base::operator=(const char* s)
{
    if (s) {
        return operator=(mc::string_view(s));
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
            } else if (is_string()) {
                // 对于固定类型来说，给字符串赋值 0(nullptr) 会清空字符串
                m_string.clear();
            } else {
                throw_type_error(get_type_name(), type_id::string_type);
            }
            break;
        }
    }
    return *this;
}

variant_base& variant_base::operator=(mc::string_view s)
{
    if (is_string()) {
        m_string.clear();
        m_string.append(s.data(), s.size());
    } else if (is_blob()) {
        // 字符串可以赋值给二进制类型
        m_blob_ptr->data.assign(s.begin(), s.end());
    } else {
        *this = variant_base(s);
    }
    return *this;
}

variant_base& variant_base::operator=(const blob& b)
{
    if (is_blob()) {
        m_blob_ptr->data.assign(b.data.begin(), b.data.end());
    } else {
        *this = variant_base(b);
    }
    return *this;
}

variant_base variant_base::copy() const
{
    return visit_with([](const auto& value) -> variant_base {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            return variant_base();
        } else if constexpr (std::is_arithmetic_v<T>) {
            return variant_base(value);
        } else if constexpr (std::is_same_v<T, string_type>) {
            return variant_base(value);
        } else if constexpr (std::is_same_v<T, array_type>) {
            return variant_base(value.copy());
        } else if constexpr (std::is_same_v<T, object_type>) {
            return variant_base(value.copy());
        } else if constexpr (std::is_same_v<T, blob_type>) {
            variant_base result(type_id::blob_type);
            result.m_blob_ptr = mc::allocate_ptr<blob_type>(allocator_type(), value.data.data(), value.data.size());
            return result;
        } else if constexpr (std::is_same_v<T, extension_type>) {
            variant_base result(type_id::extension_type);
            result.m_extension = value.copy();
            return result;
        } else {
            return variant_base();
        }
    });
}

variant_base variant_base::deep_copy(mc::detail::copy_context* ctx) const
{
    if (!ctx) {
        mc::detail::copy_context local_ctx;
        return deep_copy(&local_ctx);
    }

    return visit_with([ctx](const auto& value) -> variant_base {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            return variant_base();
        } else if constexpr (std::is_arithmetic_v<T>) {
            return variant_base(value);
        } else if constexpr (std::is_same_v<T, string_type>) {
            return variant_base(value);
        } else if constexpr (std::is_same_v<T, array_type>) {
            return variant_base(value.deep_copy(ctx));
        } else if constexpr (std::is_same_v<T, object_type>) {
            return variant_base(value.deep_copy(ctx));
        } else if constexpr (std::is_same_v<T, blob_type>) {
            variant_base result(type_id::blob_type);
            result.m_blob_ptr = mc::allocate_ptr<blob_type>(allocator_type(), value.data.data(), value.data.size());
            return result;
        } else if constexpr (std::is_same_v<T, extension_type>) {
            variant_base result(type_id::extension_type);
            result.m_extension = value.deep_copy(ctx); // 调用 extension 的 deep_copy
            return result;
        } else {
            return variant_base();
        }
    });
}

void variant_base::swap(variant_base& other) noexcept
{
    type_id temp_type = m_type;
    m_type            = other.m_type;
    other.m_type      = temp_type;

    std::swap(m_uint64, other.m_uint64);
}

// 类型名获取函数

mc::string_view variant_base::get_type_name() const
{
    if (is_extension() && m_extension) {
        return m_extension->get_type_name();
    }
    return get_type_name_internal(get_type());
}

size_t variant_base::hash() const
{
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
            return calculate_str_hash(m_string.view());
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

typename variant_base::extension_ptr_type variant_base::as_extension() const
{
    if (!is_extension()) {
        throw_type_error("extension", get_type());
    }
    return m_extension;
}

typename variant_base::array_type variant_base::as_array() const
{
    if (is_array()) {
        return m_array;
    }

    throw_type_error("array", get_type());
}

mc::string_view variant_base::get_string() const
{
    if (!is_string()) {
        throw_type_error("string", get_type());
    }
    return m_string.view();
}

const typename variant_base::blob_type& variant_base::get_blob() const
{
    if (!is_blob()) {
        throw_type_error("blob_type", get_type());
    }
    return *m_blob_ptr;
}

const typename variant_base::array_type& variant_base::get_array() const
{
    if (!is_array()) {
        throw_type_error("array", get_type());
    }
    return m_array;
}

const dict& variant_base::get_object() const
{
    if (!is_object()) {
        throw_type_error("object", get_type());
    }
    return m_object;
}

template <typename Op>
auto numeric_op(const variant_base& self, Op&& op, detail::numeric_t rhs, const char* op_name)
{
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

bool variant_base::operator<(detail::numeric_t rhs) const
{
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

bool variant_base::operator>(detail::numeric_t rhs) const
{
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

bool variant_base::operator<=(detail::numeric_t rhs) const
{
    if (is_double() && std::isnan(m_double)) {
        return false;
    }
    return !(*this > rhs);
}

bool variant_base::operator>=(detail::numeric_t rhs) const
{
    if (is_double() && std::isnan(m_double)) {
        return false;
    }
    return !(*this < rhs);
}

bool variant_base::operator==(detail::numeric_t rhs) const
{
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

bool variant_base::operator==(const variants& other) const
{
    if (!is_array()) {
        return false;
    }
    return m_array == other;
}

bool variant_base::operator<(const variants& other) const
{
    if (!is_array()) {
        throw_type_error("array", get_type());
    }
    return m_array < other;
}
bool variant_base::operator>(const variants& other) const
{
    if (!is_array()) {
        throw_type_error("array", get_type());
    }
    return m_array > other;
}

template <typename T>
inline bool is_unsigned(T value)
{
    if constexpr (std::is_unsigned_v<T> || std::is_same_v<T, bool>) {
        MC_UNUSED(value);
        return true;
    } else {
        return value >= 0;
    }
}

variant_base variant_base::operator+(detail::numeric_t rhs) const
{
    return numeric_op(*this, [&](auto&& other) -> variant_base {
        using T = std::decay_t<decltype(other)>;
        if (is_string()) {
            mc::string concat(get_string());
            concat += mc::to_string(other);
            return variant_base(concat);
        } else if (is_array()) {
            return *this + variant(other);
        } else if (is_object()) {
            throw_invalid_type_operation_error(get_type_name(), pretty_name<T>(), "+");
        } else if (is_double() || std::is_floating_point_v<T>) {
            return variant_base(as_double() + static_cast<double>(other));
        } else if (is_unsigned_integer() && is_unsigned(other)) {
            return variant_base(as_uint64() + static_cast<uint64_t>(other));
        } else {
            return variant_base(as_int64() + static_cast<int64_t>(other));
        }
    }, rhs, "+");
}

variant_base variant_base::operator-(detail::numeric_t rhs) const
{
    return numeric_op(*this, [&](auto&& other) -> variant_base {
        using T = std::decay_t<decltype(other)>;
        if (is_double() || std::is_floating_point_v<T>) {
            return variant_base(as_double() - static_cast<double>(other));
        } else if (is_unsigned_integer() && is_unsigned(other)) {
            if (as_uint64() < static_cast<uint64_t>(other)) {
                return variant_base(static_cast<int64_t>(as_uint64()) - static_cast<int64_t>(other));
            }
            return variant_base(as_uint64() - static_cast<uint64_t>(other));
        }

        return variant_base(as_int64() - static_cast<int64_t>(other));
    }, rhs, "-");
}

variant_base variant_base::operator*(detail::numeric_t rhs) const
{
    return numeric_op(*this, [&](auto&& other) -> variant_base {
        using T = std::decay_t<decltype(other)>;
        if (is_double() || std::is_floating_point_v<T>) {
            return variant_base(as_double() * static_cast<double>(other));
        } else if (is_unsigned_integer() && is_unsigned(other)) {
            return variant_base(as_uint64() * static_cast<uint64_t>(other));
        } else {
            return variant_base(as_int64() * static_cast<int64_t>(other));
        }
    }, rhs, "*");
}

static bool is_zero(detail::numeric_t value)
{
    return std::visit([](auto&& v) -> bool {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_floating_point_v<T>) {
            return MC_FLOAT_EQUAL(static_cast<double>(v), 0.0, VARIANT_FLOAT_EPSILON);
        } else {
            return v == 0;
        }
    }, value.data);
}

variant_base variant_base::operator/(detail::numeric_t rhs) const
{
    if (is_zero(rhs)) {
        throw_divide_by_zero_exception("除零错误");
    }

    return numeric_op(*this, [&](auto&& other) -> variant_base {
        using T = std::decay_t<decltype(other)>;

        if (is_double() || std::is_floating_point_v<T> || is_bool() || std::is_same_v<T, bool>) {
            return variant_base(as_double() / static_cast<double>(other));
        } else if (is_unsigned_integer() && is_unsigned(other)) {
            return variant_base(as_uint64() / static_cast<uint64_t>(other));
        }

        return variant_base(as_int64() / static_cast<int64_t>(other));
    }, rhs, "/");
}

variant_base variant_base::operator%(detail::numeric_t rhs) const
{
    if (is_zero(rhs)) {
        throw_divide_by_zero_exception("取模运算除零错误");
    }

    return numeric_op(*this, [&](auto&& other) -> variant_base {
        if (is_unsigned_integer() && is_unsigned(other)) {
            return variant_base(as_uint64() % static_cast<uint64_t>(other));
        } else {
            return variant_base(as_int64() % static_cast<int64_t>(other));
        }
    }, rhs, "%");
}

variant_base variant_base::operator<<(detail::numeric_t rhs) const
{
    return numeric_op(*this, [&](auto&& other) -> variant_base {
        uint64_t shift_amount = static_cast<uint64_t>(other);
        if (shift_amount >= sizeof(uint64_t) * 8) {
            return {0};
        }
        if (is_unsigned_integer()) {
            return {as_uint64() << shift_amount};
        }
        return {static_cast<int64_t>(static_cast<uint64_t>(as_int64()) << shift_amount)};
    }, rhs, "<<");
}

variant_base variant_base::operator>>(detail::numeric_t rhs) const
{
    return numeric_op(*this, [&](auto&& other) -> variant_base {
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
        // 有符号右移需要保持符号位（算术右移）
        return {as_int64() >> shift_amount};
    }, rhs, ">>");
}

variant_base variant_base::operator&(detail::numeric_t rhs) const
{
    return numeric_op(*this, [&](auto&& other) -> variant_base {
        if (is_unsigned_integer() && is_unsigned(other)) {
            return variant_base(as_uint64() & static_cast<uint64_t>(other));
        }
        return variant_base(static_cast<int64_t>(static_cast<uint64_t>(as_int64()) & static_cast<uint64_t>(other)));
    }, rhs, "&");
}

variant_base variant_base::operator|(detail::numeric_t rhs) const
{
    return numeric_op(*this, [&](auto&& other) -> variant_base {
        if (is_unsigned_integer() && is_unsigned(other)) {
            return variant_base(as_uint64() | static_cast<uint64_t>(other));
        }
        return variant_base(static_cast<int64_t>(static_cast<uint64_t>(as_int64()) | static_cast<uint64_t>(other)));
    }, rhs, "|");
}

variant_base variant_base::operator^(detail::numeric_t rhs) const
{
    return numeric_op(*this, [&](auto&& other) -> variant_base {
        if (is_unsigned_integer() && is_unsigned(other)) {
            return variant_base(as_uint64() ^ static_cast<uint64_t>(other));
        }
        return variant_base(static_cast<int64_t>(static_cast<uint64_t>(as_int64()) ^ static_cast<uint64_t>(other)));
    }, rhs, "^");
}

// dict
void to_variant(const dict& var, variant_base& vo)
{
    variant_base(var).swap(vo);
}
void from_variant(const variant_base& var, dict& vo)
{
    vo = var.as_dict();
}

// array_type
void to_variant(const variants& var, variant_base& vo)
{
    variant_base(var).swap(vo);
}
void from_variant(const variant_base& var, variants& vo)
{
    vo = var.get_array();
}

// const char*
void to_variant(const char* var, variant_base& vo)
{
    variant_base(var ? mc::string_view(var) : mc::string_view()).swap(vo);
}
void from_variant(const variant_base& var, const char*& vo)
{
    vo = var.get_string().data();
}

// char *
void to_variant(char* var, variant_base& vo)
{
    variant_base(var ? mc::string_view(var) : mc::string_view()).swap(vo);
}
void from_variant(const variant_base& var, char*& vo)
{
    vo = const_cast<char*>(var.get_string().data());
}

// bool
void to_variant(bool var, variant_base& vo)
{
    variant_base(var).swap(vo);
}
void from_variant(const variant_base& var, bool& vo)
{
    vo = var.as_bool();
}

mc::string to_string(const variant_base& v)
{
    return v.to_string();
}

std::ostream& operator<<(std::ostream& os, const variant_base& v)
{
    switch (v.get_type()) {
        case type_id::null_type:
            // 空值类型直接输出字符串 "null"
            return os << "null";
        case type_id::bool_type:
            // 布尔类型根据值输出 "true" 或 "false"
            return os << (v.as_bool() ? "true" : "false");
        case type_id::int8_type:
        case type_id::int16_type:
        case type_id::int32_type:
        case type_id::int64_type:
            // 所有有符号整数类型统一转换为 int64_t 后输出
            return os << v.as_int64();
        case type_id::uint8_type:
        case type_id::uint16_type:
        case type_id::uint32_type:
        case type_id::uint64_type:
            // 所有无符号整数类型统一转换为 uint64_t 后输出
            return os << v.as_uint64();
        case type_id::double_type:
            // 浮点类型直接输出 double 值
            return os << v.as_double();
        case type_id::string_type:
            // 字符串类型直接输出内容(不带引号)
            return os << v.get_string();
        case type_id::array_type:
            // 数组类型通过 mc::to_string 转换为字符串后输出
            return os << mc::to_string(v);
        case type_id::object_type:
            // 对象类型通过 mc::to_string 转换为字符串后输出
            return os << mc::to_string(v);
        case type_id::blob_type:
            // 二进制数据输出为 "blob[大小]" 格式
            os << "blob[" << v.get_blob().data.size() << "]";
            return os;
        case type_id::extension_type: {
            if (auto extension = v.as_extension()) {
                return os << extension->get_type_name();
            }
            return os;
        }
        default:
            // 未知类型或未处理的类型输出为 "unknown_type"
            return os << "unknown_type";
    }
}

} // namespace mc

#include "./variant_base_cmp_op.cpp"
#include "./variant_base_op.cpp"
