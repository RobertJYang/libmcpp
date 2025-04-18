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

namespace {
// DBus消息对齐宏
constexpr uint32_t DBUS_HEADER_SIGNATURE_POS = 12;
constexpr uint32_t DBUS_ALIGN_4              = 4;
constexpr uint32_t DBUS_ALIGN_8              = 8;

// DBus消息头部固定长度为12字节
constexpr uint32_t DBUS_MESSAGE_HEADER_SIZE = 12;
constexpr uint32_t DBUS_MESSAGE_MIN_SIZE    = 16; // 消息最小长度：头部 + 长度字段
} // namespace

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

stream& operator<<(stream& stream, const message_header& header) {
    // stream.write_value(header.m_type);
    // stream.write_value(header.m_serial);
    // stream.write_value(header.m_reply_serial);
    // stream.write_value(header.m_path);
    // stream.write_value(header.m_interface);
    // stream.write_value(header.m_member);
    return stream;
}

stream& operator>>(stream& stream, message_header& header) {
    ensure_size(stream, DBUS_MESSAGE_MIN_SIZE, "消息头大小不足");

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
    uint32_t array_len   = stream.read_value<uint32_t>();

    if (msg_type > static_cast<uint8_t>(message_type::signal)) {
        MC_THROW(mc::dbus::dbus_common_exception, "无效的消息类型");
    }

    if (version != 1) { // 目前DBus协议版本为1
        MC_THROW(mc::dbus::dbus_common_exception, "无效的协议版本");
    }

    if (!serial) {
        MC_THROW(mc::dbus::dbus_common_exception, "无效的序列号");
    }

    if (DBUS_MESSAGE_MIN_SIZE + array_len + body_length > stream.length()) {
        MC_THROW(mc::dbus::dbus_common_exception, "头字段数组和消息体大小超过消息总大小");
    }

    header.m_type        = static_cast<message_type>(msg_type);
    header.m_flags       = flags;
    header.m_version     = version;
    header.m_body_length = body_length;
    header.m_serial      = serial;

    int field_count = 0;
    for (size_t i = 0; i < array_len && field_count < 10;) {
        auto start_pos      = stream.get_read_pos();
        auto field_type_val = static_cast<type_code>(stream.read_value<uint8_t>());
        if (field_type_val == type_code::invalid_type || field_type_val > type_code::unix_fd_type) {
            i += 1;
            continue;
        }

        message_header_field field_type = static_cast<message_header_field>(field_type_val);

        mc::variant value;
        stream >> value;

        header.set_field(field_type, value);

        i += stream.get_read_pos() - start_pos;
        field_count++;
    }

    stream.align_read(8);
    return stream;
}

} // namespace mc::dbus