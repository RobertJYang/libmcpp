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

#ifndef MC_DBUS_STREAM_H
#define MC_DBUS_STREAM_H

#include <mc/dbus/enums.h>
#include <mc/dbus/error.h>
#include <mc/dbus/signature_helper.h>
#include <mc/dbus/validator.h>
#include <mc/io/io_stream.h>
#include <mc/pretty_name.h>
#include <mc/variant.h>

namespace mc::dbus {

template <typename T>
constexpr std::size_t type_alignment() {
    using type = mc::traits::remove_cvref_t<T>;

    if constexpr (std::is_same_v<type, bool>) {
        return 4;
    } else if constexpr (std::is_same_v<type, int8_t> || std::is_same_v<type, uint8_t>) {
        return 1;
    } else if constexpr (std::is_same_v<type, int16_t> || std::is_same_v<type, uint16_t>) {
        return 2;
    } else if constexpr (std::is_same_v<type, int32_t> || std::is_same_v<type, uint32_t>) {
        return 4;
    } else if constexpr (std::is_same_v<type, int64_t> || std::is_same_v<type, uint64_t>) {
        return 8;
    } else if constexpr (std::is_same_v<type, double> || std::is_same_v<type, float>) {
        return 8;
    }

    static_assert(!std::is_same_v<type, void>, "Unsupported type");
}

class stream : public mc::io::io_stream {
public:
    stream();
    stream(std::unique_ptr<mc::io::io_buffer> buffer, bool writable = true);
    ~stream();

    bool is_little_endian() const {
        return m_is_little_endian;
    }
    void set_little_endian(bool is_little_endian) {
        m_is_little_endian = is_little_endian;
    }

    void read_variant(mc::variant& v, std::size_t depth);
    void read_variant(signature_iterator it, mc::variant& v, std::size_t depth);
    void read_variant_array_or_dict(signature_iterator it, mc::variant& v, std::size_t depth);
    void read_variant_array(signature_iterator it, mc::variants& arr, std::size_t depth);
    void read_variant_struct(signature_iterator it, mc::variant& v, std::size_t depth);
    void read_variant_dict(signature_iterator it, mc::mutable_dict& dict, std::size_t depth);
    void read_signature(signature& sig);

    void write_variant(const mc::variant& v, std::size_t depth);
    void write_variant(signature_iterator it, const mc::variant& v, std::size_t depth);
    void write_variant_array_or_dict(signature_iterator it, const mc::variant& v,
                                     std::size_t depth);
    void write_variant_array(signature_iterator it, const mc::variants& arr, std::size_t depth);
    void write_variant_struct(signature_iterator it, const mc::variant& v, std::size_t depth);
    void write_variant_dict(signature_iterator it, const mc::dict& dict, std::size_t depth);
    void write_signature(const signature& sig);
    void write_signature(std::string_view sig);

private:
    bool m_is_little_endian{true};
};

template <typename T>
void ensure_container_max_length(T& container) {
    if (container.size() > validator::maximum_array_size) {
        MC_THROW(mc::dbus::invalid_message_exception,
                 "类型 ${type} 的大小超过了最大限制, 最大 ${max_size}, 当前 ${current_size}",
                 ("type", mc::pretty_name<T>())("max_size", validator::maximum_array_size)(
                     "current_size", container.size()));
    }
}
void ensure_message_depth(std::size_t depth);
void ensure_message_size(stream& stream, std::size_t size);

/*------------------------------------------------*/
// 重载 operator>> 用于读取基本类型
/*------------------------------------------------*/
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
stream& operator>>(stream& stream, T& v) {
    stream.align_read(type_alignment<T>());
    if constexpr (std::is_same_v<T, float>) {
        v = static_cast<T>(stream.read_value<double>(stream.is_little_endian()));
    } else if constexpr (std::is_same_v<T, bool>) {
        v = stream.read_value<uint32_t>(stream.is_little_endian()) != 0;
    } else {
        stream.read_value(v, stream.is_little_endian());
    }
    return stream;
}
stream& operator>>(stream& stream, std::string& v);
stream& operator>>(stream& stream, std::string_view& v);
stream& operator>>(stream& stream, mc::dbus::path& v);
stream& operator>>(stream& stream, mc::dbus::signature& v);
stream& operator>>(stream& stream, mc::blob& v);
stream& operator>>(stream& stream, mc::variant& v);

// mc::variants 类型签名默认是 av，如果需要指定其他类型，请直接调用 read_variant_array
stream& operator>>(stream& stream, mc::variants& v);

// mc::dict 类型签名默认是 a{sv}，如果需要指定其他类型，请直接调用 read_variant_dict
stream& operator>>(stream& stream, mc::dict& v);

// mc::mutable_dict 类型签名默认是 a{sv}，如果需要指定其他类型，请直接调用 read_variant_dict
stream& operator>>(stream& stream, mc::mutable_dict& v);

// 读取反射类型，自动按照反射类型签名读取
template <typename T, std::enable_if_t<mc::reflect::is_reflectable<T>(), int> = 0>
stream& operator>>(stream& stream, T& v) {
    std::string_view sig = get_signature<T>();

    // TODO:: 反射类型先通过variant中转，后续可以优化成直接从stream中读取到T中
    mc::variant tmp;
    stream.read_variant(sig, tmp, 0);
    from_variant(tmp, v);
    return stream;
}

/*------------------------------------------------*/
// 重载 operator<< 用于写入基本类型
/*------------------------------------------------*/
template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
stream& operator<<(stream& stream, T v) {
    stream.align(type_alignment<T>());
    if constexpr (std::is_same_v<T, float>) {
        stream.write_value(static_cast<double>(v), stream.is_little_endian());
    } else if constexpr (std::is_same_v<T, bool>) {
        stream.write_value<uint32_t>(v ? 1 : 0, stream.is_little_endian());
    } else {
        stream.write_value(v, stream.is_little_endian());
    }
    return stream;
}
stream& operator<<(stream& stream, const std::string& v);
stream& operator<<(stream& stream, std::string_view v);
stream& operator<<(stream& stream, const char* v);
stream& operator<<(stream& stream, const mc::dbus::path& v);
stream& operator<<(stream& stream, const mc::dbus::signature& v);
stream& operator<<(stream& stream, const mc::blob& v);

// mc::variant 默认会根据 variant 的类型自动设置类型签名，
// 如果需要指定其他类型，请直接调用write_variant
stream& operator<<(stream& stream, const mc::variant& v);

// mc::variants 类型签名默认是 av，如果需要指定其他类型，请直接调用 write_variant_array
stream& operator<<(stream& stream, const mc::variants& v);

// mc::dict 类型签名默认是 a{sv}，如果需要指定其他类型，请直接调用 write_variant_dict
stream& operator<<(stream& stream, const mc::dict& v);

// mc::mutable_dict 类型签名默认是 a{sv}，如果需要指定其他类型，请直接调用 write_variant_dict
stream& operator<<(stream& stream, const mc::mutable_dict& v);

template <typename T, typename Allocator>
stream& operator<<(stream& stream, const std::vector<T, Allocator>& v) {
    ensure_container_max_length(v);
    dbus::stream::write_length_guard<uint32_t> guard(stream, stream.is_little_endian());
    for (const auto& item : v) {
        stream << item;
    }
    return stream;
}

template <typename K, typename V, typename Comp, typename Alloc>
stream& operator<<(stream& stream, const std::map<K, V, Comp, Alloc>& v) {
    ensure_container_max_length(v);
    dbus::stream::write_length_guard<uint32_t> guard(stream, stream.is_little_endian());

    stream.align(8);
    for (const auto& item : v) {
        stream.align(8);
        stream << item;
    }
    return stream;
}

namespace detail {
// 辅助标签类型
struct const_ref_tag {};
struct value_tag {};

// 检测函数指针转换的主模板
template <typename T, typename Tag = void, typename = void>
struct has_stream_operator : std::false_type {};

// 检测 const T& 重载版本
template <typename T>
struct has_stream_operator<
    T, const_ref_tag,
    std::void_t<decltype(static_cast<stream& (*)(stream&, const T&)>(&operator<<))>>
    : std::true_type {};

// 检测 T 值重载版本
template <typename T>
struct has_stream_operator<T, value_tag,
                           std::void_t<decltype(static_cast<stream& (*)(stream&, T)>(&operator<<))>>
    : std::true_type {};

template <typename T>

// 侦测 T 类型是否支持通过 operator<< 直接写入到 dbus::stream 中，防止出现类型隐士转换导致
// 写入的类型签名与 get_signature<> 获取的类型签名不一致
inline constexpr bool has_stream_operator_v =
    has_stream_operator<T, const_ref_tag>::value || has_stream_operator<T, value_tag>::value;
} // namespace detail

// 写入反射类型，自动按照反射类型签名写入
template <typename T, std::enable_if_t<mc::reflect::is_reflectable<T>(), int> = 0>
stream& operator<<(stream& s, const T& v) {
    mc::traits::tuple_for_each(mc::reflect::reflector<T>::get_properties(), [&](auto&& item) {
        using item_type     = std::decay_t<decltype(item)>;
        using property_type = typename item_type::member_type;

        static_assert(detail::has_stream_operator_v<property_type>,
                      "属性类型T不支持通过 operator<< 写入到 dbus::stream 中");

        s << v.*item.member_ptr;
    });

    return s;
}

} // namespace mc::dbus

#endif // MC_DBUS_STREAM_H