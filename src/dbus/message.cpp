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
#include <mc/dbus/message.h>
#include <mc/dbus/signature.h>

namespace mc::dbus {

void message_header::set_field(message_header_field field, const variant& value) {
    switch (field) {
    case message_header_field::path:
        path = value.as_string();
        break;
    case message_header_field::interface:
        interface = value.as_string();
        break;
    case message_header_field::member:
        member = value.as_string();
        break;
    case message_header_field::error_name:
        error_name = value.as_string();
        break;
    case message_header_field::reply_serial:
        reply_serial = value.as_uint32();
        break;
    case message_header_field::destination:
        destination = value.as_string();
        break;
    case message_header_field::sender:
        sender = value.as_string();
        break;
    case message_header_field::signature:
        signature = value.as_string();
        break;
    case message_header_field::unix_fds:
        unix_fds = value.as_uint32();
        break;
    default:
        break;
    }
}

static inline void write_string_field(stream& stream, message_header_field field,
                                      const std::string& str, char type = 's') {
    if (str.empty()) {
        return;
    }

    stream.align(8);
    stream.write_value<uint8_t>(static_cast<uint8_t>(field));
    stream.write_signature(std::string_view(&type, 1));
    if (type == 'g') {
        stream.write_signature(str);
    } else {
        stream << str;
    }
}

static inline void write_uint32_field(stream& stream, message_header_field field, uint32_t value) {
    if (value == 0) {
        return;
    }

    stream.write_value<uint8_t>(static_cast<uint8_t>(field));
    stream << value;
}

void message_header::write_field(stream& stream, message_header_field field) {
    switch (field) {
    case message_header_field::path:
        return write_string_field(stream, field, path.str(),
                                  type_to_char(type_code::object_path_type));
    case message_header_field::interface:
        return write_string_field(stream, field, interface);
    case message_header_field::member:
        return write_string_field(stream, field, member);
    case message_header_field::error_name:
        return write_string_field(stream, field, error_name);
    case message_header_field::reply_serial:
        if (type == message_type::method_return) {
            write_uint32_field(stream, field, reply_serial);
        }
        break;
    case message_header_field::destination:
        return write_string_field(stream, field, destination);
    case message_header_field::sender:
        return write_string_field(stream, field, sender);
    case message_header_field::signature:
        return write_string_field(stream, field, signature.str(),
                                  type_to_char(type_code::signature_type));
    case message_header_field::unix_fds:
        return write_uint32_field(stream, field, unix_fds);
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

    if (DBUS_MESSAGE_MIN_SIZE + header_len + body_length > stream.length()) {
        MC_THROW(mc::dbus::dbus_common_exception, "头字段数组和消息体大小超过消息总大小");
    }

    header.type       = static_cast<message_type>(msg_type);
    header.flags      = flags;
    header.version    = version;
    header.body_len   = body_length;
    header.serial     = serial;
    header.header_len = header_len;

    int field_count = 0;
    for (size_t i = 0; i < header_len && field_count < 10;) {
        auto start_pos = stream.get_read_pos();

        stream.align_read(8);
        auto field_type_val = static_cast<type_code>(stream.read_value<uint8_t>());
        if (field_type_val != type_code::invalid_type &&
            field_type_val <= type_code::unix_fd_type) {
            message_header_field field_type = static_cast<message_header_field>(field_type_val);

            mc::variant value;
            stream >> value;
            header.set_field(field_type, value);
        }

        i += stream.get_read_pos() - start_pos;
        field_count++;
    }

    stream.align_read(8);
    return stream;
}

stream& operator<<(stream& stream, message_header& header) {
    char endianness = stream.is_little_endian() ? 'l' : 'B';
    stream.write_value<char>(endianness);
    stream.write_value<uint8_t>(static_cast<uint8_t>(header.type));
    stream.write_value<uint8_t>(header.flags);
    stream.write_value<uint8_t>(header.version);

    // 预留消息体长度
    stream.write_value<uint32_t>(0);

    stream.write_value<uint32_t>(header.serial);

    stream::write_length_guard<uint32_t> guard(stream);
    guard.prepare_length_field();
    guard.set_body_start_pos();

    for (size_t i = 1; i <= static_cast<size_t>(message_header_field::unix_fds); i++) {
        header.write_field(stream, static_cast<message_header_field>(i));
    }
    guard.fire();

    stream.align(8);
    return stream;
}

stream& operator>>(stream& stream, variants_message& msg) {
    stream >> static_cast<message_header&>(msg);
    signature_iterator it(msg.signature);
    while (!it.at_end()) {
        mc::variant value;
        stream.read_variant(it, value, 0);
        msg.body.emplace_back(std::move(value));
        it.next();
    }
    return stream;
}

stream& operator<<(stream& stream, variants_message& msg) {
    stream << static_cast<message_header&>(msg);
    signature_iterator it(msg.signature);

    stream::write_length_guard<uint32_t> guard(stream, stream.is_little_endian());
    guard.set_write_pos(DBUS_MESSAGE_BODY_LEN_POS);
    guard.set_body_start_pos();
    for (std::size_t i = 0; i < msg.body.size() && !it.at_end(); ++i) {
        stream.write_variant(it.current_type(), msg.body[i], 0);
        it.next();
    }
    guard.fire();
    return stream;
}

} // namespace mc::dbus