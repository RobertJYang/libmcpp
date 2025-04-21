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
#include <mc/dbus/signature.h>
#include <mc/dbus/signature_helper.h>
#include <mc/dbus/stream.h>
#include <mc/reflect.h>
#include <mc/variant.h>

#include <string_view>
namespace mc::dbus {

constexpr std::size_t DBUS_MESSAGE_BODY_LEN_POS = 4;  // 消息体长度字段位置
constexpr uint32_t    DBUS_MESSAGE_HEADER_SIZE  = 12; // DBus消息头部固定长度为12字节
constexpr uint32_t    DBUS_MESSAGE_MIN_SIZE     = 16; // 消息最小长度：头部 + 长度字段

constexpr uint8_t DBUS_MESSAGE_NO_REPLY_EXPECTED  = 0x01;
constexpr uint8_t DBUS_MESSAGE_NO_AUTO_START_FLAG = 0x02;

struct message_header {
    message_type type = message_type::invalid;
    uint8_t      flags{0};
    uint8_t      version{1};
    union {
        uint32_t serial{0};
        uint32_t reply_serial;
    };

    dbus::path  path;
    std::string interface;
    std::string member;
    std::string error_name;
    std::string destination;
    std::string sender;
    signature   signature;
    uint32_t    unix_fds{0};

    // 一些协议辅助字段，不需要手动设置
    uint32_t body_len{0};   // 消息体长度
    uint32_t header_len{0}; // 消息头长度

    void set_field(message_header_field field, const variant& value);
    void write_field(stream& stream, message_header_field field);

    void set_auto_start(bool auto_start) {
        if (auto_start) {
            flags &= ~DBUS_MESSAGE_NO_AUTO_START_FLAG;
        } else {
            flags |= DBUS_MESSAGE_NO_AUTO_START_FLAG;
        }
    }

    bool auto_start() {
        return flags & DBUS_MESSAGE_NO_AUTO_START_FLAG;
    }

    void set_no_reply(bool no_reply) {
        if (no_reply) {
            flags |= DBUS_MESSAGE_NO_REPLY_EXPECTED;
        } else {
            flags &= (~DBUS_MESSAGE_NO_REPLY_EXPECTED);
        }
    }

    bool expects_reply() const {
        return flags & DBUS_MESSAGE_NO_REPLY_EXPECTED;
    }

    void set_serial(uint32_t serial) {
        this->serial = serial;
    }

    uint32_t get_serial() const {
        return serial;
    }

    std::size_t need_bytes() const {
        if (body_len > 0) {
            return MC_ALIGN_UP(header_len, 8) + body_len;
        }
        return header_len;
    }
};

template <typename T = mc::variants, typename = void>
struct message : message_header {
    template <typename U>
    message& operator<<(U&& value) {
        signature += get_signature<mc::traits::remove_cvref_t<U>>();
        body.emplace_back(std::forward<U>(value));
        return *this;
    }

    mc::variants body;
};
using variants_message = message<mc::variants>;

// 针对 T 的特化，用于序列化反射对象
template <typename T>
struct message<T,
               std::enable_if_t<mc::reflect::is_reflectable<T>() && !std::is_reference_v<T>, void>>
    : message_header {
    T body;
};

// 针对 T& 的特化，用于序列号已经存在的对象，避免拷贝
template <typename T>
struct message<T&, std::enable_if_t<mc::reflect::is_reflectable<T>(), void>> : message_header {
    message(T& body) : body(body) {
    }

    T& body;
};

// 消息头解析和序列化
stream& operator>>(stream& stream, message_header& header);
stream& operator<<(stream& stream, message_header& header);

// 消息解析和序列化
stream& operator>>(stream& stream, variants_message& msg);
stream& operator<<(stream& stream, variants_message& msg);

// 反射消息解析和序列化
template <typename T, std::enable_if_t<mc::reflect::is_reflectable<T>(), int> = 0>
stream& operator>>(stream& stream, message<T>& msg) {
    stream >> static_cast<message_header&>(msg) >> msg.body;
    return stream;
}

template <typename T, std::enable_if_t<mc::reflect::is_reflectable<T>(), int> = 0>
stream& operator<<(stream& stream, message<T>& msg) {
    if (msg.signature.str().empty()) {
        msg.signature = get_signature<T>();
    }
    stream << static_cast<message_header&>(msg);

    stream::write_length_guard<uint32_t> guard(stream, stream.is_little_endian());
    guard.set_write_pos(DBUS_MESSAGE_BODY_LEN_POS);
    guard.set_body_start_pos();
    stream << msg.body;
    guard.fire();
    return stream;
}

} // namespace mc::dbus

#endif // MC_DBUS_MESSAGE_HEADER_H