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
#include <mc/dbus/marshalled.h>
#include <mc/dbus/message_header.h>
#include <mc/dbus/signature.h>

namespace mc::dbus {

void message_header::set_field(message_header_field field, const variant& value) {
    switch (field) {
    case message_header_field::path:
        m_path = value.as_string();
        break;
    case message_header_field::interface:
        m_interface = value.as_string();
        break;
    case message_header_field::member:
        m_member = value.as_string();
        break;
    case message_header_field::error_name:
        m_error_name = value.as_string();
        break;
    case message_header_field::reply_serial:
        m_reply_serial = value.as_uint32();
        break;
    case message_header_field::destination:
        m_destination = value.as_string();
        break;
    case message_header_field::sender:
        m_sender = value.as_string();
        break;
    case message_header_field::signature:
        m_signature = value.as_string();
        break;
    case message_header_field::unix_fds:
        m_unix_fds = value.as_uint32();
        break;
    default:
        break;
    }
}

static inline void write_string_field(stream& stream, message_header_field field,
                                      const std::string& str) {
    if (str.empty()) {
        stream.write_value<uint8_t>(static_cast<uint8_t>(message_header_field::invalid));
        return;
    }

    stream.write_value<uint8_t>(static_cast<uint8_t>(field));
    stream.write_signature(std::string_view("s", 1));
    stream << str;
}

static inline void write_uint32_field(stream& stream, message_header_field field, uint32_t value) {
    if (value == 0) {
        stream.write_value<uint8_t>(static_cast<uint8_t>(message_header_field::invalid));
        return;
    }

    stream.write_value<uint8_t>(static_cast<uint8_t>(field));
    stream << value;
}

static inline void write_signature_field(stream& stream, message_header_field field,
                                         const signature& sig) {
    if (sig.str().empty()) {
        stream.write_value<uint8_t>(static_cast<uint8_t>(message_header_field::invalid));
        return;
    }

    stream.write_value<uint8_t>(static_cast<uint8_t>(field));
    stream.write_signature(sig);
}

void message_header::write_field(stream& stream, message_header_field field) {
    stream.write_value<uint8_t>(static_cast<uint8_t>(field));

    switch (field) {
    case message_header_field::path:
        return write_string_field(stream, field, m_path.str());
    case message_header_field::interface:
        return write_string_field(stream, field, m_interface);
    case message_header_field::member:
        return write_string_field(stream, field, m_member);
    case message_header_field::error_name:
        return write_string_field(stream, field, m_error_name);
    case message_header_field::reply_serial:
        return write_uint32_field(stream, field, m_reply_serial);
    case message_header_field::destination:
        return write_string_field(stream, field, m_destination);
    case message_header_field::sender:
        return write_string_field(stream, field, m_sender);
    case message_header_field::signature:
        return write_signature_field(stream, field, m_signature);
    case message_header_field::unix_fds:
        return write_uint32_field(stream, field, m_unix_fds);
    default:
        break;
    }
}

stream& operator>>(stream& stream, message_header& header) {
    ensure_message_size(stream, DBUS_MESSAGE_MIN_SIZE);

    char endianness = stream.read_value<char>();
    if (endianness != 'l' && endianness != 'B') { // 'l' 表示小端，'B' 表示大端
        MC_THROW(mc::dbus::dbus_common_exception, "无效的字节序");
    }
    if (endianness != 'l') {
        stream.set_little_endian(false);
    }

    // 解析消息类型
    uint8_t  msg_type    = stream.read_value<uint8_t>();
    uint8_t  flags       = stream.read_value<uint8_t>();
    uint8_t  version     = stream.read_value<uint8_t>();
    uint32_t body_length = stream.read_value<uint32_t>();
    uint32_t serial      = stream.read_value<uint32_t>();
    uint32_t header_len  = stream.read_value<uint32_t>();

    if (msg_type > static_cast<uint8_t>(message_type::signal)) {
        MC_THROW(mc::dbus::dbus_common_exception, "无效的消息类型");
    }

    if (version != 1) { // 目前DBus协议版本为1
        MC_THROW(mc::dbus::dbus_common_exception, "无效的协议版本");
    }

    if (!serial) {
        MC_THROW(mc::dbus::dbus_common_exception, "无效的序列号");
    }

    if (DBUS_MESSAGE_MIN_SIZE + header_len + body_length > stream.length()) {
        MC_THROW(mc::dbus::dbus_common_exception, "头字段数组和消息体大小超过消息总大小");
    }

    header.m_type    = static_cast<message_type>(msg_type);
    header.m_flags   = flags;
    header.m_version = version;
    header.m_serial  = serial;

    int field_count = 0;
    for (size_t i = 0; i < header_len && field_count < 10;) {
        auto start_pos      = stream.get_read_pos();
        auto field_type_val = static_cast<type_code>(stream.read_value<uint8_t>());
        if (field_type_val == type_code::invalid_type || field_type_val > type_code::unix_fd_type) {
            i += 1;
            continue;
        }

        message_header_field field_type = static_cast<message_header_field>(field_type_val);

        signature sig;
        stream >> sig;

        mc::variant value;
        demarshal(stream, sig, value);
        header.set_field(field_type, value);

        i += stream.get_read_pos() - start_pos;
        field_count++;
    }

    stream.align_read(8);
    return stream;
}

stream& operator<<(stream& stream, message_header& header) {
    char endianness = stream.is_little_endian() ? 'l' : 'B';
    stream.write_value<char>(endianness);

    // 解析消息类型
    stream.write_value<uint8_t>(static_cast<uint8_t>(header.m_type));
    stream.write_value<uint8_t>(header.m_flags);
    stream.write_value<uint8_t>(header.m_version);

    // 预留消息体长度
    stream.write_value<uint32_t>(0);

    stream.write_value<uint32_t>(header.m_serial);

    // 预留头字段长度
    std::size_t header_len_pos = stream.get_write_pos();
    stream.write_value<uint32_t>(0);

    for (size_t i = 1; i <= static_cast<size_t>(message_header_field::unix_fds); i++) {
        header.write_field(stream, static_cast<message_header_field>(i));
    }

    stream.seek_write(header_len_pos, io::seek_mode::begin);
    stream.write_value<uint32_t>(stream.get_write_pos() - header_len_pos);

    stream.align(8);
    return stream;
}

} // namespace mc::dbus