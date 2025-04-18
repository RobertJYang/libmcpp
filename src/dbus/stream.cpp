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

#include <mc/dbus/error.h>
#include <mc/dbus/path.h>
#include <mc/dbus/signature.h>
#include <mc/dbus/stream.h>
#include <mc/dbus/validator.h>
#include <mc/exception.h>

#include <algorithm>
#include <cstring>
#include <optional>

namespace mc::dbus {
namespace detail {

static inline void add_type(signature& sig, type_code type) {
    sig += type;
}

static bool check_array_all_type(const mc::variants& v, mc::type_id type) {
    for (const auto& item : v) {
        if (item.get_type() != type) {
            return false;
        }
    }

    return true;
}

static void to_dbus_signature(signature& sig, const mc::variant& v) {
    switch (v.get_type()) {
    case mc::type_id::bool_type:
        return add_type(sig, type_code::boolean_type);
    case mc::type_id::int8_type:
    case mc::type_id::uint8_type:
        return add_type(sig, type_code::byte_type);
    case mc::type_id::int16_type:
        return add_type(sig, type_code::int16_type);
    case mc::type_id::uint16_type:
        return add_type(sig, type_code::uint16_type);
    case mc::type_id::int32_type:
        return add_type(sig, type_code::int32_type);
    case mc::type_id::uint32_type:
        return add_type(sig, type_code::uint32_type);
    case mc::type_id::int64_type:
        return add_type(sig, type_code::int64_type);
    case mc::type_id::uint64_type:
        return add_type(sig, type_code::uint64_type);
    case mc::type_id::double_type:
        return add_type(sig, type_code::double_type);
    case mc::type_id::string_type:
        return add_type(sig, type_code::string_type);
    case mc::type_id::object_type:
        return to_dbus_signature(sig, v.get_object());
    case mc::type_id::array_type:
        return to_dbus_signature(sig, v.get_array());
    case mc::type_id::blob_type:
        sig += container::array_of_byte;
        break;
    default:
        throw_unknow_type_error(v.get_type());
    }
}
static void to_dbus_signature(signature& sig, const mc::variants& v) {
    // 如果数组是空，无法探测到数组类型，默认使用byte类型
    if (v.empty()) {
        sig += container::array_of_byte;
        return;
    }

    // 如果数组中所有元素类型相同，则认为是dbus数组
    if (check_array_all_type(v, v[0].get_type())) {
        sig += type_code::array_start;
        to_dbus_signature(sig, v[0]);
        return;
    }

    // 否则是dbus元组
    sig += type_code::struct_start;
    for (const auto& item : v) {
        to_dbus_signature(sig, item);
    }
    sig += type_code::struct_end;
}

static void to_dbus_signature(signature& sig, const mc::dict& v) {
    // 如果字典是空，无法探测到字典类型，默认使用a{sv}类型
    if (v.empty()) {
        sig += container::dict_string_var;
        return;
    }

    sig += type_code::array_start;

    auto it = v.begin();
    sig += type_code::dict_entry_start;
    to_dbus_signature(sig, it->key);
    to_dbus_signature(sig, it->value);
    sig += type_code::dict_entry_end;
}
} // namespace detail

stream::stream() {
}

stream::stream(std::unique_ptr<mc::io::io_buffer> buffer, bool writable)
    : io_stream(std::move(buffer), writable) {
}

stream::~stream() {
}

void stream::read_variant_array(signature_iterator it, mc::variant& v, std::size_t depth) {
    if (depth >= validator::maximum_message_depth) {
        MC_THROW(mc::dbus::invalid_message_exception, "message depth is too large");
    }

    if (it.current_type_code() == type_code::dict_entry_start) {
        return read_variant_dict(it, v, depth);
    }

    uint32_t array_size;
    *this >> array_size;

    mc::variants arr;
    while (!it.at_end() && array_size > 0) {
        auto start_pos = get_read_pos();

        mc::variant item;
        read_variant(it, item, depth + 1);
        arr.emplace_back(std::move(item));

        MC_ASSERT(get_read_pos() - start_pos <= array_size, "读取数组元素失败");
        array_size -= get_read_pos() - start_pos;
    }

    v = std::move(arr);
}

void stream::read_variant_struct(signature_iterator it, mc::variant& v, std::size_t depth) {
    if (depth >= validator::maximum_message_depth) {
        MC_THROW(mc::dbus::invalid_message_exception, "message depth is too large");
    }

    align_read(8);

    mc::variants arr;
    while (!it.at_end()) {
        signature_iterator item_it(it.current_type());

        mc::variant item;
        read_variant(item_it, item, depth + 1);

        arr.emplace_back(std::move(item));
        it.next();
    }

    v = std::move(arr);
}

void stream::read_variant_dict(signature_iterator it, mc::variant& v, std::size_t depth) {
    if (depth >= validator::maximum_message_depth) {
        MC_THROW(mc::dbus::invalid_message_exception, "message depth is too large");
    }

    uint32_t dict_size;
    *this >> dict_size;

    signature_iterator key_it   = it.get_dict_key_iterator();
    signature_iterator value_it = it.get_dict_value_iterator();

    mc::mutable_dict dict;
    this->align_read(8);
    while (dict_size > 0) {
        auto start_pos = get_read_pos();
        this->align_read(8);

        mc::variant key;
        read_variant(key_it, key, depth + 1);

        mc::variant value;
        read_variant(value_it, value, depth + 1);

        dict[key] = value;

        MC_ASSERT(get_read_pos() - start_pos <= dict_size, "读取字典元素失败");
        dict_size -= get_read_pos() - start_pos;
    }

    v = std::move(dict);
}

namespace detail {
template <typename T>
static void demarshal_variant_basic(stream& stream, mc::variant& v) {
    T t;
    stream >> t;
    v = t;
}

static void demarshal_variant_string(stream& stream, mc::variant& v) {
    std::string_view sv;
    stream >> sv;
    v = sv;
}

} // namespace detail

void stream::read_variant(const signature& sig, mc::variant& v) {
    signature_iterator it(sig);
    read_variant(it, v, 0);
}

void stream::read_variant(mc::variant& v, std::size_t depth) {
    signature sig;
    *this >> sig;
    sig.validate();

    read_variant(signature_iterator(sig), v, depth + 1);
}

void stream::read_variant(signature_iterator it, mc::variant& v, std::size_t depth) {
    if (depth >= validator::maximum_message_depth) {
        MC_THROW(mc::dbus::invalid_message_exception, "message depth is too large");
    }

    switch (it.current_type_code()) {
    case type_code::byte_type:
        return detail::demarshal_variant_basic<uint8_t>(*this, v);
    case type_code::boolean_type:
        return detail::demarshal_variant_basic<bool>(*this, v);
    case type_code::int16_type:
        return detail::demarshal_variant_basic<int16_t>(*this, v);
    case type_code::uint16_type:
        return detail::demarshal_variant_basic<uint16_t>(*this, v);
    case type_code::int32_type:
        return detail::demarshal_variant_basic<int32_t>(*this, v);
    case type_code::uint32_type:
        return detail::demarshal_variant_basic<uint32_t>(*this, v);
    case type_code::int64_type:
        return detail::demarshal_variant_basic<int64_t>(*this, v);
    case type_code::uint64_type:
        return detail::demarshal_variant_basic<uint64_t>(*this, v);
    case type_code::double_type:
        return detail::demarshal_variant_basic<double>(*this, v);
    case type_code::string_type:
        return detail::demarshal_variant_string(*this, v);
    case type_code::signature_type:
        return detail::demarshal_variant_basic<mc::dbus::signature>(*this, v);
    case type_code::object_path_type:
        return detail::demarshal_variant_basic<mc::dbus::path>(*this, v);
    case type_code::array_start:
        return read_variant_array(it.get_content_iterator(), v, depth + 1);
    case type_code::struct_start:
        return read_variant_struct(it.get_content_iterator(), v, depth + 1);
    case type_code::variant_type: {
        return read_variant(v, depth + 1);
    }
    default:
        MC_THROW(mc::invalid_arg_exception, "unknown type: ${type}",
                 ("type", it.current_type_char()));
    }
}

stream& operator<<(stream& stream, const mc::variant& v) {
    signature sig;
    detail::to_dbus_signature(sig, v);
    stream << sig;

    switch (v.get_type()) {
    case mc::type_id::bool_type:
        stream << v.as_bool();
        break;
    case mc::type_id::int8_type:
        stream << v.as_int8();
        break;
    case mc::type_id::uint8_type:
        stream << v.as_uint8();
        break;
    case mc::type_id::int16_type:
        stream << v.as_int16();
        break;
    case mc::type_id::uint16_type:
        stream << v.as_uint16();
        break;
    case mc::type_id::int32_type:
        stream << v.as_int32();
        break;
    case mc::type_id::uint32_type:
        stream << v.as_uint32();
        break;
    case mc::type_id::int64_type:
        stream << v.as_int64();
        break;
    case mc::type_id::uint64_type:
        stream << v.as_uint64();
        break;
    case mc::type_id::double_type:
        stream << v.as_double();
        break;
    case mc::type_id::string_type:
        stream << v.as_string();
        break;
    case mc::type_id::object_type:
        stream << v.get_object();
        break;
    case mc::type_id::array_type:
        stream << v.get_array();
        break;
    case mc::type_id::blob_type:
        stream << v.get_blob();
        break;
    default:
        throw_unknow_type_error(v.get_type());
    }

    return stream;
}

stream& operator>>(stream& stream, mc::variant& v) {
    signature sig;
    stream >> sig;

    stream.read_variant(sig, v);
    return stream;
}

stream& operator<<(stream& stream, const mc::dict& v) {
    stream.align(4);
    std::size_t start_pos = stream.get_write_pos();
    stream.write_value(uint32_t(0), stream.is_little_endian());

    for (const auto& entry : v) {
        stream.align(8);
        stream << entry.key << entry.value;
    }

    std::size_t end_pos = stream.get_write_pos();
    std::size_t size    = end_pos - start_pos;
    stream.seek_write(start_pos, io::seek_mode::begin);
    stream.write_value(size, stream.is_little_endian());
    stream.seek_write(end_pos, io::seek_mode::begin);

    return stream;
}

stream& operator>>(stream& stream, mc::dict& v) {
    stream.align(4);
    uint32_t size = stream.read_value<uint32_t>(stream.is_little_endian());

    mc::mutable_dict m_v(v);
    m_v.clear();

    if (size == 0) {
        return stream;
    } else if (size < 4) {
        MC_THROW(mc::dbus::invalid_message_exception, "dict size is too small");
    }

    size -= 4;

    for (auto read_size = 0; read_size < size;) {
        std::size_t start_pos = stream.get_read_pos();
        stream.align(8);

        mc::variant key;
        stream >> key;

        mc::variant value;
        stream >> value;

        m_v[key] = value;

        read_size += stream.get_read_pos() - start_pos;
    }

    return stream;
}

stream& operator<<(stream& stream, const mc::variants& v) {
    stream.align(4);
    stream.write_value<uint32_t>(v.size(), stream.is_little_endian());

    signature sig;
    detail::to_dbus_signature(sig, v);
    stream << sig;

    return stream;
}

stream& operator>>(stream& stream, mc::variants& v) {
    return stream;
}

stream& operator<<(stream& stream, const std::string& v) {
    stream << std::string_view(v);
    return stream;
}

stream& operator<<(stream& stream, std::string_view v) {
    stream.align(4);

    stream.write_value<uint32_t>(v.size(), stream.is_little_endian());
    stream.write(v.data(), v.size());

    // 添加尾部的0
    stream.write_value<uint8_t>(0);

    return stream;
}

stream& operator<<(stream& stream, const char* v) {
    stream << std::string_view(v);
    return stream;
}

stream& operator>>(stream& stream, std::string& v) {
    std::string_view sv;
    stream >> sv;
    v = std::string(sv);
    return stream;
}

stream& operator>>(stream& stream, std::string_view& v) {
    stream.align_read(4);

    uint32_t size = stream.read_value<uint32_t>(stream.is_little_endian());
    ensure_size(stream, size + 1, "无法读取字符串");
    v = stream.read(size);

    // 跳过尾部的0
    auto tail = stream.read_value<uint8_t>();
    if (tail != 0) {
        MC_THROW(mc::dbus::invalid_message_exception, "字符串尾部不是0");
    }

    return stream;
}

stream& operator<<(stream& stream, const mc::dbus::path& v) {
    stream << std::string_view(v.str());
    return stream;
}

stream& operator>>(stream& stream, mc::dbus::path& v) {
    std::string sv;
    stream >> sv;
    v = mc::dbus::path(std::move(sv));
    return stream;
}

stream& operator<<(stream& stream, const mc::dbus::signature& v) {
    stream << uint8_t(v.size());
    stream.write(std::string_view(v.str()));

    // 添加尾部的0
    stream.write_value<uint8_t>(0, stream.is_little_endian());

    return stream;
}

stream& operator>>(stream& stream, mc::dbus::signature& v) {
    uint8_t size;
    stream >> size;
    if (size == 0) {
        MC_THROW(mc::dbus::invalid_message_exception, "signature size is too small");
    }

    ensure_size(stream, size + 1, "无法读取签名");

    std::string sig(stream.read(size));
    v = signature(std::move(sig));

    // 跳过尾部的0
    stream.skip(1);

    return stream;
}
} // namespace mc::dbus