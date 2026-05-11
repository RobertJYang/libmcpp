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

#ifndef MC_DBUS_CONNECTION_IMPL_H
#define MC_DBUS_CONNECTION_IMPL_H

#include "dispatch/pending_call.h"
#include <mc/dbus/connection.h>
#include <mc/dbus/match.h>

namespace mc::dbus {

constexpr uint32_t MAX_SERIAL_RETRY = 1000000;

struct pending_data {
    using promise_type = mc::promise<message>;

    pending_data(promise_type promise, pending_call pending) : promise(std::move(promise)), pending(std::move(pending))
    {}

    ~pending_data()
    {
        if (promise) {
            promise.set_value(message::new_error_message(error_names::disconnected));
        }

        pending.stop();
    }

    promise_type promise;
    pending_call pending;
};

using pending_call_map = std::unordered_map<uint32_t, pending_data>;

struct connection_impl : public std::enable_shared_from_this<connection_impl> {
    connection_impl(mc::io_context& executor);
    ~connection_impl();

    connection_impl(const connection_impl&)            = delete;
    connection_impl& operator=(const connection_impl&) = delete;
    connection_impl(connection_impl&&)                 = delete;
    connection_impl& operator=(connection_impl&&)      = delete;

    void initialize();
    bool start();
    void disconnect();
    void release();
    bool is_connected() const;
    bool get_is_connected() const;

    void register_path(std::string_view path, path_handler_type handler);
    void unregister_path(std::string_view path);

    std::tuple<bool, std::optional<error>> request_name(std::string_view name, uint32_t flags);

    bool                        send(message&& msg);
    message                     send_with_reply_and_block(message&& msg, mc::milliseconds timeout);
    connection::future<message> async_send_with_reply(message&& msg, mc::milliseconds timeout);

    static dbus_bool_t watch_add(DBusWatch* watch, void* data);
    static void        watch_remove(DBusWatch* watch, void* data);
    static void        watch_toggled(DBusWatch* watch, void* data);

    static dbus_bool_t timeout_add(DBusTimeout* timeout, void* data);
    static void        timeout_remove(DBusTimeout* timeout, void* data);
    static void        timeout_toggled(DBusTimeout* timeout, void* data);

    DBusHandlerResult process_message(message msg);
    void              process_reply(uint32_t reply_serial, message& msg);

    void        dispatch();
    void        flush();
    static void dispatch_status_changed(DBusConnection* connection, DBusDispatchStatus new_status, void* user_data);
    static DBusHandlerResult message_filter(DBusConnection* conn, DBusMessage* msg, void* user_data);

    void        add_rule(match_rule& rule, match_cb_t&& cb, uint64_t id);
    void        remove_rule(uint64_t id);
    void        add_match(match_rule& rule, match_cb_t&& cb, uint64_t id);
    void        remove_match(uint64_t id);
    void        add_match_only(match_rule& rule, match_cb_t&& cb, uint64_t id);
    match&      get_match();
    std::string get_service_name() const;

    std::recursive_mutex                      m_mutex;
    pending_call_map                          m_pending_calls;  ///< 等待回复的消息
    uint32_t                                  m_next_serial{1}; ///< 下一个消息序列号
    DBusConnection*                           m_connection{nullptr};
    mc::io_context&                           m_executor;
    connect_status                            m_status{connect_status::disconnected};
    match                                     m_match;
    std::unordered_map<uint64_t, std::string> m_match_strs;

    mc::signal<DBusHandlerResult(message&)> on_filter_message;

    uint32_t get_next_serial();

    std::string m_service_name;
};

using connection_weak_ptr = std::weak_ptr<connection_impl>;
using connection_ptr      = std::shared_ptr<connection_impl>;

} // namespace mc::dbus

#endif // MC_DBUS_CONNECTION_IMPL_H