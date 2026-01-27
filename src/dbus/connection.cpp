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

#include <mc/exception.h>
#include <mc/log.h>
#include <chrono>

#include "connection_impl.h"

namespace mc::dbus {

template <DBusBusType bus_type>
connection open_bus(mc::io_context& executor) {
    mc::dbus::error err;
    int             i = 0;
    while (true) {
        dbus_error_init(&err);
        DBusConnection* conn = dbus_bus_get_private(bus_type, &err);
        if (!err.is_set()) {
            return connection(executor, conn, false);
        } else if (i++ == 10) {
            break;
        }

        dlog("DBus connection failed: ${error}, retry ${i}", ("error", err.message)("i", i));
        dbus_error_free(&err);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    MC_THROW(mc::system_exception, "DBus connection failed: ${error}", ("error", err.message));
}

connection connection::open_system_bus(mc::io_context& executor) {
    return open_bus<DBUS_BUS_SYSTEM>(executor);
}

connection connection::open_session_bus(mc::io_context& executor) {
    return open_bus<DBUS_BUS_SESSION>(executor);
}

connection::connection(mc::io_context& executor, DBusConnection* conn, bool add_ref)
    : m_impl(std::make_unique<connection_impl>(executor)) {
    if (add_ref) {
        dbus_connection_ref(conn);
    }

    m_impl->m_connection = conn;
    m_impl->m_status     = connect_status::connecting;
}

connection::connection() {
}

connection::~connection() {
}

void connection::disconnect() {
    if (!m_impl) {
        return;
    }

    m_impl->disconnect();
}

void connection::flush() {
    if (!m_impl) {
        return;
    }

    m_impl->flush();
}

void connection::dispatch() {
    if (!m_impl) {
        return;
    }

    m_impl->dispatch();
}

bool connection::send(message&& msg) {
    ensure_impl();
    auto start = std::chrono::steady_clock::now();
    auto result = m_impl->send(std::forward<message>(msg));
    auto end = std::chrono::steady_clock::now();
    auto duration = end - start;
    dlog("dbus send message cost '${time}' microseconds", ("time", std::chrono::duration_cast<std::chrono::microseconds>(duration)));
    return result;
}

message connection::send_with_reply(message&& msg, mc::milliseconds timeout) {
    ensure_impl();

    return async_send_with_reply(std::forward<message>(msg), timeout).get();
}

message connection::send_with_reply_and_block(message&& msg, mc::milliseconds timeout) {
    ensure_impl();

    return m_impl->send_with_reply_and_block(std::forward<message>(msg), timeout);
}

connection::future<message> connection::async_send_with_reply(message&&        msg,
                                                              mc::milliseconds timeout) {
    ensure_impl();

    return m_impl->async_send_with_reply(std::forward<message>(msg), timeout);
}

void connection::register_path(std::string_view path, path_handler_type handler) {
    ensure_impl();

    m_impl->register_path(path, std::move(handler));
}

void connection::unregister_path(std::string_view path) {
    ensure_impl();

    m_impl->unregister_path(path);
}

bool connection::is_connected() const {
    if (!m_impl) {
        return false;
    }

    return m_impl->is_connected();
}

bool connection::get_is_connected() const {
    if (!m_impl) {
        return false;
    }

    return m_impl->get_is_connected();
}

bool connection::start() {
    if (!m_impl) {
        return false;
    }

    return m_impl->start();
}

std::tuple<bool, std::optional<error>> connection::request_name(std::string_view name,
                                                                uint32_t         flags) {
    if (!m_impl) {
        error err;
        err.set_error(error_names::disconnected, "Connection not initialized");
        return {false, std::move(err)};
    }

    return m_impl->request_name(name, flags);
}

std::string_view connection::get_unique_name() const {
    if (!m_impl) {
        return {};
    }

    return dbus_bus_get_unique_name(m_impl->m_connection);
}

void connection::set_unique_name(std::string_view name) {
    ensure_impl();
    dbus_bus_set_unique_name(m_impl->m_connection, std::string(name).c_str());
}

connection_impl& connection::get_impl() const {
    ensure_impl();

    return *m_impl;
}

DBusConnection* connection::get_connection() const {
    if (!m_impl) {
        return nullptr;
    }

    return m_impl->m_connection;
}

filter_message_signal_type& connection::filter_message() const {
    ensure_impl();

    return m_impl->on_filter_message;
}

std::string connection::get_service_name() const {
    ensure_impl();
    return m_impl->get_service_name();
}

void connection::ensure_impl() const {
    MC_ASSERT(m_impl, "DBus Connection not initialized");
}

 void connection::add_rule(match_rule& rule, match_cb_t&& cb, uint64_t id) {
    ensure_impl();
    m_impl->add_rule(rule, std::forward<match_cb_t>(cb), id);
}

void connection::remove_rule(uint64_t id){
    ensure_impl();
    m_impl->remove_rule(id);
}

void connection::add_match(match_rule& rule, match_cb_t&& cb, uint64_t id) {
    ensure_impl();
    m_impl->add_match(rule, std::forward<match_cb_t>(cb), id);
}

void connection::add_match_only(match_rule& rule, match_cb_t&& cb, uint64_t id) {
 	ensure_impl();
    m_impl->add_match_only(rule, std::forward<match_cb_t>(cb), id);
}

void connection::remove_match(uint64_t id) {
    ensure_impl();
    m_impl->remove_match(id);
}

match& connection::get_match() {
    ensure_impl();
    return m_impl->get_match();
}

uint32_t connection::get_next_serial() {
    ensure_impl();
    return m_impl->get_next_serial();
}

} // namespace mc::dbus
