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
#include <mc/dbus/signature_helper.h>
#include <mc/io/io_stream.h>

namespace mc::dbus {
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

    void read_variant(const signature& sig, mc::variant& v);

    template <typename T, std::enable_if_t<mc::reflect::is_reflectable<T>(), int> = 0>
    void read_struct(const signature& sig, T& v) {
        // TODO:: 反射类型先通过variant中转，后续可以优化成直接从stream中读取到T中
        mc::variant tmp;
        read_variant(sig, tmp);
        from_variant(tmp, v);
    }

private:
    void read_variant(mc::variant& v, std::size_t depth);
    void read_variant(signature_iterator it, mc::variant& v, std::size_t depth);
    void read_variant_array(signature_iterator it, mc::variant& v, std::size_t depth);
    void read_variant_struct(signature_iterator it, mc::variant& v, std::size_t depth);
    void read_variant_dict(signature_iterator it, mc::variant& v, std::size_t depth);

    bool m_is_little_endian{true};
};

inline void ensure_size(stream& stream, std::size_t size, const char* msg) {
    if (size > stream.readable_bytes()) {
        MC_THROW(mc::eof_exception, msg);
    }
}

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

stream& operator<<(stream& stream, const mc::variant& v);
stream& operator>>(stream& stream, mc::variant& v);

stream& operator<<(stream& stream, const mc::dict& v);
stream& operator>>(stream& stream, mc::dict& v);

stream& operator<<(stream& stream, const mc::variants& v);
stream& operator>>(stream& stream, mc::variants& v);

template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
stream& operator<<(stream& stream, const T& v) {
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

stream& operator<<(stream& stream, const std::string& v);
stream& operator<<(stream& stream, std::string_view v);
stream& operator<<(stream& stream, const char* v);
stream& operator>>(stream& stream, std::string& v);
stream& operator>>(stream& stream, std::string_view& v);

stream& operator<<(stream& stream, const mc::dbus::path& v);
stream& operator>>(stream& stream, mc::dbus::path& v);

stream& operator<<(stream& stream, const mc::dbus::signature& v);
stream& operator>>(stream& stream, mc::dbus::signature& v);

template <typename T, typename std::enable_if_t<mc::reflect::is_reflectable<T>(), int> = 0>
stream& operator<<(stream& stream, const T& v) {
    stream << mc::reflect::to_variant(v);
    return stream;
}

template <typename T, typename std::enable_if_t<mc::reflect::is_reflectable<T>(), int> = 0>
stream& operator>>(stream& stream, T& v) {
    signature sig;
    get_signature<T>(sig);

    stream.read_struct(sig, v);
    return stream;
}
} // namespace mc::dbus

#endif // MC_DBUS_STREAM_H