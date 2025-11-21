/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef MC_DBUS_LOCAL_MSG_H
#define MC_DBUS_LOCAL_MSG_H

#include <dbus/dbus.h>
#include <mc/dbus/message.h>
#include <mc/dbus/shm/serialize.h>
#include <mc/dict.h>
#include <mc/variant.h>

namespace mc::dbus {

inline bool is_unique_name(std::string_view name) {
    return !name.empty() && name.at(0) == ':';
}

class MC_API local_msg {
public:
    local_msg() = default;

    local_msg(std::string_view service_name, std::string_view path, std::string_view interface,
              std::string_view member);

    local_msg(const variants& v);

    std::tuple<std::string, std::string> get_error() const;

    void method_return();

    void error(std::string_view error_name, std::string_view message);

    void set_member(std::string_view member);

    uint32_t msg_type() const;

    std::string_view path() const;

    std::string_view interface() const;

    std::string_view member() const;

    std::string_view signature() const;

    void append_args(std::string_view signature, const variants& args);

    template <typename... Args>
    void append(std::string_view signature, Args&&... args) {
        m_signature = signature;
        m_args      = variants();
        signature_iterator it(signature);
        auto               func = [this, &it, signature](auto&& arg) {
            signature_iterator item_it(it.current_type());
            MC_ASSERT(!item_it.at_end() && !it.at_end(),
                                    "invalid args size for signature: ${signature}", ("signature", signature));
            m_args.push_back(parse_variant(item_it, std::forward<decltype(arg)>(arg), 0));
            it.next();
        };
        (func(std::forward<Args>(args)), ...);
        // 检查签名是否还有剩余（参数数量少于签名要求）
        MC_ASSERT(it.at_end(), "invalid args size for signature: ${signature}, expected more args", ("signature", signature));
    }

    void append_return_args(std::string_view signature, const variant& arg);

    const variants& read() const;

    void set_sender(std::string_view sender);

    std::string_view sender() const;

    void set_serial(uint32_t serial);

    uint32_t get_serial() const;

    uint32_t get_reply_serial() const;

    std::string_view destination() const;

    void set_local_call(bool v);

    bool is_local_call() const;

    message new_dbus_msg() const;

    std::string pack() const;

    static variant parse_variant(signature_iterator it, const variant& v, size_t depth);

    static variants parse_variant_elements(signature_iterator it, const variants& v, size_t depth);

private:
    std::string get_error_message() const;

    bool        m_local_call;
    uint32_t    m_type;
    uint32_t    m_serial;
    uint32_t    m_reply_serial;
    std::string m_service_name;
    std::string m_path;
    std::string m_interface;
    std::string m_member;
    std::string m_error_name;
    std::string m_signature;
    std::string m_sender;
    variants    m_args;
};

using shm_msg_promise  = mc::promise<local_msg>;
using serial_map_type  = std::unordered_map<uint32_t, shm_msg_promise>;
using service_map_type = std::unordered_map<std::string, serial_map_type>;

class MC_API shm_pending_msgs {
public:
    bool send(std::string_view source_unique_name, uint32_t serial, shm_msg_promise promise);
    bool reply(std::string_view destination_unique_name, uint32_t serial, local_msg& msg);
    void clear(std::string_view unique_name);

private:
    std::mutex       m_mutex;
    service_map_type m_pending_msgs;
};

} // namespace mc::dbus

#endif // MC_DBUS_LOCAL_MSG_H