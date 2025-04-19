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

#include <algorithm>
#include <cstring>
#include <optional>

namespace mc::dbus {
namespace detail {

template <typename T>
static void demarshal_variant_basic(stream& stream, mc::variant& v) {
    T t;
    stream >> t;
    v = t;
}

template <typename T>
static void marshal_variant_basic(stream& stream, const mc::variant& v) {
    if constexpr (std::is_same_v<T, bool>) {
        uint32_t value = v.as_bool() ? 1 : 0;
        stream << value;
    } else if constexpr (std::is_arithmetic_v<T>) {
        stream << v.as<T>();
    } else if constexpr (std::is_same_v<T, std::string_view> || std::is_same_v<T, std::string> ||
                         std::is_same_v<T, mc::dbus::path>) {
        if (v.is_string()) {
            stream << v.get_string();
        } else {
            stream << v.as<std::string>();
        }
    } else if constexpr (std::is_same_v<T, mc::dbus::signature>) {
        if (v.is_string()) {
            stream.write_signature(v.get_string());
        } else {
            stream.write_signature(v.as<std::string>());
        }
    } else {
        static_assert(std::is_same_v<T, void>, "unsupported type");
    }
}

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

static void variant_to_dbus_signature(signature& sig, const mc::variant& v);

static void array_to_dbus_signature(signature& sig, const mc::variants& v) {
    // 如果数组是空，无法探测到数组类型，默认使用byte类型
    if (v.empty()) {
        sig += container::array_of_byte;
        return;
    }

    // 如果数组中所有元素类型相同，则认为是dbus数组
    if (check_array_all_type(v, v[0].get_type())) {
        sig += type_code::array_start;
        variant_to_dbus_signature(sig, v[0]);
        return;
    }

    // 否则是dbus元组
    sig += type_code::struct_start;
    for (const auto& item : v) {
        variant_to_dbus_signature(sig, item);
    }
    sig += type_code::struct_end;
}

static void dict_to_dbus_signature(signature& sig, const mc::dict& v) {
    // 如果字典是空，无法探测到字典类型，默认使用a{sv}类型
    if (v.empty()) {
        sig += container::dict_string_var;
        return;
    }

    sig += type_code::array_start;

    auto it = v.begin();
    sig += type_code::dict_entry_start;
    variant_to_dbus_signature(sig, it->key);
    variant_to_dbus_signature(sig, it->value);
    sig += type_code::dict_entry_end;
}

static void variant_to_dbus_signature(signature& sig, const mc::variant& v) {
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
        return dict_to_dbus_signature(sig, v.get_object());
    case mc::type_id::array_type:
        return array_to_dbus_signature(sig, v.get_array());
    case mc::type_id::blob_type:
        sig += container::array_of_byte;
        break;
    default:
        throw_unknow_type_error(v.get_type());
    }
}

} // namespace detail

void ensure_message_depth(std::size_t depth) {
    if (depth >= validator::maximum_message_depth) {
        MC_THROW(mc::dbus::invalid_message_exception,
                 "超过最大消息深度限制，最大 ${max_depth}, 当前 ${current_depth}",
                 ("max_depth", validator::maximum_message_depth)("current_depth", depth));
    }
}

void ensure_message_size(stream& stream, std::size_t size) {
    if (size > validator::maximum_message_size) {
        MC_THROW(mc::dbus::invalid_message_exception,
                 "超过最大消息大小限制，最大 ${max_size}, 当前 ${current_size}",
                 ("max_size", validator::maximum_message_size)("current_size", size));
    }

    if (stream.readable_bytes() < size) {
        MC_THROW(mc::eof_exception, "消息大小不足，需要 ${needed_size}, 当前 ${current_size}",
                 ("needed_size", size)("current_size", stream.readable_bytes()));
    }
}

stream::stream() {
}

stream::stream(std::unique_ptr<mc::io::io_buffer> buffer, bool writable)
    : io_stream(std::move(buffer), writable) {
}

stream::~stream() {
}

void stream::read_variant_array_or_dict(signature_iterator it, mc::variant& v, std::size_t depth) {
    ensure_message_depth(depth);

    if (it.current_type_code() == type_code::dict_entry_start) {
        mc::mutable_dict dict;
        read_variant_dict(it, dict, depth);
        v = std::move(dict);
        return;
    }

    mc::variants arr;
    read_variant_array(it, arr, depth);
    v = std::move(arr);
}

void stream::read_variant_array(signature_iterator it, mc::variants& arr, std::size_t depth) {
    ensure_message_depth(depth);

    uint32_t array_size;
    *this >> array_size;
    ensure_message_size(*this, array_size);

    int32_t remain_size = array_size;
    while (remain_size > 0) {
        auto start_pos = get_read_pos();

        mc::variant item;
        read_variant(it, item, depth + 1);
        arr.emplace_back(std::move(item));

        remain_size -= get_read_pos() - start_pos;
    }
}

void stream::read_variant_struct(signature_iterator it, mc::variant& v, std::size_t depth) {
    ensure_message_depth(depth);

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

void stream::read_variant_dict(signature_iterator it, mc::mutable_dict& dict, std::size_t depth) {
    ensure_message_depth(depth);

    uint32_t dict_size;
    *this >> dict_size;
    ensure_message_size(*this, dict_size);

    signature_iterator key_it   = it.get_dict_key_iterator();
    signature_iterator value_it = it.get_dict_value_iterator();

    align_read(8);

    int32_t remain_size = dict_size;
    while (remain_size > 0) {
        auto start_pos = get_read_pos();
        auto padding   = try_align_read(8);
        if (!padding || padding.value() >= remain_size) {
            break;
        }

        mc::variant key;
        read_variant(key_it, key, depth + 1);

        mc::variant value;
        read_variant(value_it, value, depth + 1);

        dict[key] = value;

        remain_size -= get_read_pos() - start_pos;
    }
}

void stream::read_variant(mc::variant& v, std::size_t depth) {
    signature sig;
    read_signature(sig);
    sig.validate();

    read_variant(sig, v, depth + 1);
}

void stream::read_variant(signature_iterator it, mc::variant& v, std::size_t depth) {
    ensure_message_depth(depth);

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
        return detail::demarshal_variant_basic<std::string_view>(*this, v);
    case type_code::signature_type:
        return detail::demarshal_variant_basic<mc::dbus::signature>(*this, v);
    case type_code::object_path_type:
        return detail::demarshal_variant_basic<mc::dbus::path>(*this, v);
    case type_code::array_start:
        return read_variant_array_or_dict(it.get_content_iterator(), v, depth + 1);
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

void stream::read_signature(mc::dbus::signature& v) {
    uint8_t size = read_value<uint8_t>();
    if (size == 0) {
        MC_THROW(mc::dbus::invalid_message_exception, "无法读取签名，签名大小为0");
    }
    ensure_message_size(*this, size + 1);

    std::string sig(read(size));
    v = signature(std::move(sig));

    // 跳过尾部的0
    skip(1);
}

void stream::write_variant(const mc::variant& v, std::size_t depth) {
    signature sig;
    detail::variant_to_dbus_signature(sig, v);
    write_signature(sig);

    write_variant(sig, v, depth + 1);
}

void stream::write_variant(signature_iterator it, const mc::variant& v, std::size_t depth) {
    ensure_message_depth(depth);

    switch (it.current_type_code()) {
    case type_code::byte_type:
        return detail::marshal_variant_basic<uint8_t>(*this, v);
    case type_code::boolean_type:
        return detail::marshal_variant_basic<bool>(*this, v);
    case type_code::int16_type:
        return detail::marshal_variant_basic<int16_t>(*this, v);
    case type_code::uint16_type:
        return detail::marshal_variant_basic<uint16_t>(*this, v);
    case type_code::int32_type:
        return detail::marshal_variant_basic<int32_t>(*this, v);
    case type_code::uint32_type:
        return detail::marshal_variant_basic<uint32_t>(*this, v);
    case type_code::int64_type:
        return detail::marshal_variant_basic<int64_t>(*this, v);
    case type_code::uint64_type:
        return detail::marshal_variant_basic<uint64_t>(*this, v);
    case type_code::double_type:
        return detail::marshal_variant_basic<double>(*this, v);
    case type_code::string_type:
        return detail::marshal_variant_basic<std::string_view>(*this, v);
    case type_code::signature_type:
        return detail::marshal_variant_basic<mc::dbus::signature>(*this, v);
    case type_code::object_path_type:
        return detail::marshal_variant_basic<mc::dbus::path>(*this, v);
    case type_code::array_start:
        return write_variant_array_or_dict(it.get_content_iterator(), v, depth + 1);
    case type_code::struct_start:
        return write_variant_struct(it.get_content_iterator(), v, depth + 1);
    case type_code::variant_type: {
        return write_variant(v, depth + 1);
    }
    default:
        MC_THROW(mc::invalid_arg_exception, "unknown type: ${type}",
                 ("type", it.current_type_char()));
    }
}

void stream::write_variant_array_or_dict(signature_iterator it, const mc::variant& v,
                                         std::size_t depth) {
    ensure_message_depth(depth);
    if (it.current_type_code() == type_code::dict_entry_start) {
        write_variant_dict(it, v.get_object(), depth + 1);
        return;
    }

    write_variant_array(it, v.get_array(), depth + 1);
}

void stream::write_variant_array(signature_iterator it, const mc::variants& arr,
                                 std::size_t depth) {
    ensure_message_depth(depth);
    ensure_container_max_length(arr);

    // 预留数组长度字段，析构的时候自动回填
    write_length_guard<uint32_t> guard(*this, is_little_endian());
    for (const auto& item : arr) {
        write_variant(it, item, depth + 1);
    }
}

void stream::write_variant_struct(signature_iterator it, const mc::variant& v, std::size_t depth) {
    ensure_message_depth(depth);

    align(8);
    const mc::variants& arr = v.get_array();
    for (const auto& item : arr) {
        signature_iterator item_it(it.current_type());
        MC_ASSERT(!item_it.at_end() && !it.at_end(), "结构体元素类型错误 ${type}, ${pos}",
                  ("type", it.str())("pos", it.str().substr(item_it.pos())));

        write_variant(item_it, item, depth + 1);
        it.next();
    }

    MC_ASSERT(it.at_end(), "结构体元素数量错误, ${type}, ${pos}",
              ("type", it.str())("pos", it.str().substr(it.pos())));
}

void stream::write_variant_dict(signature_iterator it, const mc::dict& dict, std::size_t depth) {
    ensure_message_depth(depth);
    ensure_container_max_length(dict);

    // 预留字典长度字段，析构的时候自动回填
    write_length_guard<uint32_t> guard(*this, is_little_endian());

    align(8);
    for (const auto& entry : dict) {
        align(8);

        write_variant(it.get_dict_key_iterator(), entry.key, depth + 1);
        write_variant(it.get_dict_value_iterator(), entry.value, depth + 1);
    }
}

void stream::write_signature(const signature& sig) {
    write_signature(sig.str());
}

void stream::write_signature(std::string_view sig) {
    *this << uint8_t(sig.size());
    write(sig);

    // 添加尾部的0
    write_value<uint8_t>(0);
}

// operator>>
stream& operator>>(stream& stream, std::string& v) {
    std::string_view sv;
    stream >> sv;
    v = std::string(sv);
    return stream;
}

stream& operator>>(stream& stream, std::string_view& v) {
    uint32_t size;
    stream >> size; // align 4
    ensure_message_size(stream, size + 1);

    v = stream.read(size);

    // 跳过尾部的0
    stream.skip(1);
    return stream;
}

stream& operator>>(stream& stream, mc::dbus::path& v) {
    std::string sv;
    stream >> sv;
    v = mc::dbus::path(std::move(sv));
    return stream;
}

stream& operator>>(stream& stream, mc::dbus::signature& v) {
    uint8_t size = stream.read_value<uint8_t>();
    if (size == 0) {
        MC_THROW(mc::dbus::invalid_message_exception, "无法读取签名，签名大小为0");
    }
    ensure_message_size(stream, size + 1);

    std::string sig(stream.read(size));
    v = signature(std::move(sig));

    // 跳过尾部的0
    stream.skip(1);
    return stream;
}

stream& operator>>(stream& stream, mc::blob& v) {
    uint32_t size;
    stream >> size; // align 4
    ensure_message_size(stream, size);

    v.data.resize(size);
    stream.read(&v.data[0], size);

    return stream;
}

stream& operator>>(stream& stream, mc::variant& v) {
    stream.read_variant(v, 0);
    return stream;
}

stream& operator>>(stream& stream, mc::variants& v) {
    // 在这里已经知道是 av 类型了，所以直接按 v 类型去读取内容
    stream.read_variant_array(container::array_of_variant.substr(1), v, 0);
    return stream;
}

stream& operator>>(stream& stream, mc::dict& v) {
    mc::mutable_dict tmp;
    stream.read_variant_dict(container::dict_string_var.substr(1), tmp, 0);
    v = std::move(tmp);
    return stream;
}

stream& operator>>(stream& stream, mc::mutable_dict& v) {
    stream.read_variant_dict(container::dict_string_var.substr(1), v, 0);
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

stream& operator<<(stream& stream, const mc::dbus::path& v) {
    stream << std::string_view(v.str());
    return stream;
}

stream& operator<<(stream& stream, const mc::dbus::signature& v) {
    stream.write_signature(v);
    return stream;
}

stream& operator<<(stream& stream, const mc::blob& v) {
    stream.align(4);

    stream.write_value<uint32_t>(v.data.size(), stream.is_little_endian());
    stream.write(v.data.data(), v.data.size());

    return stream;
}

stream& operator<<(stream& stream, const mc::variant& v) {
    stream.write_variant(v, 0);
    return stream;
}

stream& operator<<(stream& stream, const mc::variants& v) {
    stream.write_variant_array(container::array_of_variant.substr(1), v, 0);
    return stream;
}

stream& operator<<(stream& stream, const mc::dict& v) {
    stream.write_variant_dict(container::dict_string_var.substr(1), v, 0);
    return stream;
}

stream& operator<<(stream& stream, const mc::mutable_dict& v) {
    stream.write_variant_dict(container::dict_string_var.substr(1), v, 0);
    return stream;
}

} // namespace mc::dbus