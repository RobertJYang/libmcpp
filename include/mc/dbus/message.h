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

#ifndef MC_DBUS_MESSAGE_H
#define MC_DBUS_MESSAGE_H

#include <mc/dbus/enums.h>
#include <mc/dbus/stream.h>
#include <mc/variant.h>

#include <string_view>
namespace mc::dbus {

struct message_header {
    message_type m_type = message_type::invalid;
    uint8_t      m_flags;
    uint8_t      m_version;
    uint32_t     m_body_length;
    union {
        uint32_t m_serial;
        uint32_t m_reply_serial;
    };

    std::string m_path;
    std::string m_interface;
    std::string m_member;
    std::string m_error_name;
    std::string m_destination;
    std::string m_sender;
    std::string m_signature;
    uint32_t    m_unix_fds;

    void set_field(message_header_field field, const variant& value);
};

stream& operator<<(stream& stream, const message_header& header);
stream& operator>>(stream& stream, message_header& header);

} // namespace mc::dbus

#endif // MC_DBUS_MESSAGE_H