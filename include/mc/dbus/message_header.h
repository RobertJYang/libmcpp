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

#ifndef MC_DBUS_MESSAGE_HEADER_H
#define MC_DBUS_MESSAGE_HEADER_H

#include <mc/dbus/enums.h>
#include <mc/dbus/stream.h>
#include <mc/variant.h>

#include <string_view>
namespace mc::dbus {
// DBus消息对齐宏
constexpr uint32_t DBUS_HEADER_SIGNATURE_POS = 12;
constexpr uint32_t DBUS_ALIGN_4              = 4;
constexpr uint32_t DBUS_ALIGN_8              = 8;

// DBus消息头部固定长度为12字节
constexpr uint32_t DBUS_MESSAGE_HEADER_SIZE = 12;
constexpr uint32_t DBUS_MESSAGE_MIN_SIZE    = 16; // 消息最小长度：头部 + 长度字段

struct message_header {
    message_type m_type = message_type::invalid;
    uint8_t      m_flags{0};
    uint8_t      m_version{1};
    union {
        uint32_t m_serial{0};
        uint32_t m_reply_serial;
    };

    dbus::path  m_path;
    std::string m_interface;
    std::string m_member;
    std::string m_error_name;
    std::string m_destination;
    std::string m_sender;
    signature   m_signature;
    uint32_t    m_unix_fds{0};

    void set_field(message_header_field field, const variant& value);
    void write_field(stream& stream, message_header_field field);
};

stream& operator>>(stream& stream, message_header& header);
stream& operator<<(stream& stream, message_header& header);

} // namespace mc::dbus

#endif // MC_DBUS_MESSAGE_HEADER_H