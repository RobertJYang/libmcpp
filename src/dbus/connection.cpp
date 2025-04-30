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

#include <dbus/dbus.h>
#include <mc/dbus/connection.h>
#include <mc/dbus/dispatch/pending_call.h>
#include <mc/dbus/dispatch/timeout.h>
#include <mc/dbus/dispatch/watch.h>
#include <mc/dbus/error.h>
#include <mc/dbus/message.h>
#include <mc/dbus/validator.h>
#include <mc/exception.h>
#include <mc/log.h>

#include <algorithm>
#include <chrono>
#include <limits>
#include <string.h>
#include <thread>

namespace mc::dbus {

struct pending_data {
    using promise_type = mc::promise<message, connection::strand_type>;

    pending_data(promise_type promise, pending_call pending)
        : promise(std::move(promise)), pending(std::move(pending)) {
    }

    ~pending_data() {
        if (promise) {
            promise.set_value(message::new_error_message(error_names::disconnected));
        }

        pending.stop();
    }

    promise_type promise;
    pending_call pending;
};

using pending_call_map = std::unordered_map<uint32_t, pending_data>;
using io_context_type  = connection::io_context_type;

constexpr uint32_t MAX_SERIAL_RETRY = 1000000;
struct connection::connection_impl {
    std::recursive_mutex m_mutex;
    pending_call_map     m_pending_calls;  ///< 等待回复的消息
    uint32_t             m_next_serial{1}; ///< 下一个消息序列号

    uint32_t get_next_serial() {
        std::size_t retry = 0;
        do {
            if (m_next_serial == std::numeric_limits<uint32_t>::max()) {
                m_next_serial = 1;
            }
            auto next_serial = m_next_serial++;
            if (m_pending_calls.find(next_serial) == m_pending_calls.end()) {
                return next_serial;
            }
        } while (++retry < MAX_SERIAL_RETRY);
        MC_THROW(mc::system_exception, "分配消息序列号异常");
    }
};

connection_ptr connection::open_system_bus(strand_type& strand) {
    mc::dbus::error err;
    DBusConnection* conn = dbus_bus_get_private(DBUS_BUS_SYSTEM, &err);
    MC_ASSERT(!err.is_set(), "DBus连接失败: ${error}", ("error", err.message));

    return std::make_shared<connection>(strand, conn, false);
}

connection_ptr connection::open_session_bus(strand_type& strand) {
    mc::dbus::error err;
    DBusConnection* conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
    MC_ASSERT(!err.is_set(), "DBus连接失败: ${error}", ("error", err.message));

    return std::make_shared<connection>(strand, conn, false);
}

connection::connection(strand_type& strand, DBusConnection* conn, bool add_ref)
    : m_impl(std::make_unique<connection_impl>()), m_connection(conn), m_strand(strand) {
    if (add_ref) {
        dbus_connection_ref(m_connection);
    }
    m_status = connect_status::connecting;
}

connection::~connection() {
    release();
}

void connection::disconnect() {
    std::lock_guard lock(m_impl->m_mutex);
    if (!m_connection || m_status == connect_status::disconnected ||
        m_status == connect_status::disconnecting) {
        return;
    }

    m_status = connect_status::disconnecting;
    boost::asio::post(m_strand, [conn = shared_from_this()]() {
        conn->release();
    });
}

bool connection::send(message&& msg) {
    std::lock_guard lock(m_impl->m_mutex);
    if (!check_connected()) {
        return false;
    }

    auto serial = msg.get_serial();
    if (serial == 0) {
        serial = m_impl->get_next_serial();
        msg.set_serial(serial);
    }

    return dbus_connection_send(m_connection, msg.get_dbus_message(), nullptr);
}

message connection::send_with_reply(message&& msg, mc::milliseconds timeout) {
    return async_send_with_reply(std::forward<message>(msg), timeout).get();
}

connection::future<message> connection::async_send_with_reply(message&&        msg,
                                                              mc::milliseconds timeout) {
    std::lock_guard lock(m_impl->m_mutex);

    auto promise = mc::make_promise<message>(m_strand);
    auto future  = promise.get_future();

    if (!check_connected()) {
        promise.set_value(message::new_error(msg, error_names::disconnected, "连接已断开"));
        return future;
    }

    auto serial = msg.get_serial();
    if (serial == 0) {
        serial = m_impl->get_next_serial();
        msg.set_serial(serial);
    }

    DBusPendingCall* dbus_pending_call = nullptr;
    auto             ret = dbus_connection_send_with_reply(m_connection, msg.get_dbus_message(),
                                                           &dbus_pending_call, timeout.count());

    if (ret != TRUE) {
        promise.set_value(message::new_error(msg, error_names::failed, "发送消息失败"));
        return future;
    }

    auto on_reply = [this, serial](message msg) {
        process_reply(serial, msg);
    };

    auto [it, inserted] = m_impl->m_pending_calls.emplace(
        std::piecewise_construct, std::forward_as_tuple(serial),
        std::forward_as_tuple(promise, pending_call(dbus_pending_call, std::move(on_reply))));

    if (!inserted) {
        promise.set_value(message::new_error(msg, error_names::failed, "发送消息失败"));
        return future;
    }

    it->second.pending.start();
    return future;
}

static DBusHandlerResult path_handler(DBusConnection* conn, DBusMessage* msg, void* user_data) {
    auto handler = static_cast<connection::path_handler_type*>(user_data);
    try {
        auto message = mc::dbus::message(msg, true);
        return (*handler)(message);
    } catch (const std::exception& e) {
        elog("DBus路径处理失败: ${error}", ("error", e.what()));
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
}

static void path_unregister(DBusConnection* conn, void* user_data) {
    auto handler = static_cast<connection::path_handler_type*>(user_data);
    delete handler;
}

void connection::register_path(std::string_view path, path_handler_type handler) {
    std::lock_guard lock(m_impl->m_mutex);
    if (!check_connected()) {
        return;
    }

    DBusObjectPathVTable vtable;
    vtable.message_function    = path_handler;
    vtable.unregister_function = path_unregister;
    auto handler_data          = new path_handler_type(std::move(handler));
    dbus_connection_register_object_path(m_connection, path.data(), &vtable, handler_data);
}

void connection::unregister_path(std::string_view path) {
    std::lock_guard lock(m_impl->m_mutex);
    if (!check_connected()) {
        return;
    }

    dbus_connection_unregister_object_path(m_connection, path.data());
}

bool connection::is_connected() const {
    std::lock_guard lock(m_impl->m_mutex);

    return check_connected();
}

bool connection::check_connected() const {
    return m_connection && m_status == connect_status::connected;
}

void connection::release() {
    if (m_connection) {
        dbus_connection_close(m_connection);
        dbus_connection_unref(m_connection);
        m_connection = nullptr;
        m_status     = connect_status::disconnected;
    }
}

bool connection::start() {
    std::lock_guard lock(m_impl->m_mutex);

    if (!m_connection || m_status != connect_status::connecting) {
        return false;
    }

    initialize();

    mc::dbus::error err;
    dbus_bus_register(m_connection, &err);
    if (err.is_set()) {
        elog("DBus注册失败: ${error}", ("error", err.message));
        return false;
    }

    m_status = connect_status::connected;
    return true;
}

bool connection::request_name(std::string_view name, uint32_t flags) {
    if (name.empty() || !validator::is_valid_bus_name(name)) {
        return false;
    }

    std::lock_guard lock(m_impl->m_mutex);
    if (!check_connected()) {
        return false;
    }

    const int max_retries = 3;
    int       retry_count = 0;
    while (retry_count < max_retries) {
        mc::dbus::error err;
        dbus_bus_request_name(m_connection, name.data(), flags, &err);
        if (!err.is_set()) {
            return true;
        }

        if (err.name == error_names::no_reply) {
            // 出现 no_reply 错误时重试几次
            // 在 dbus-daemon 刚启动后立即连接可能会发生这种情况，重试几次基本可以解决
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * (retry_count + 1)));
            retry_count++;
            continue;
        }

        elog("DBus请求名称失败: ${error}", ("error", err.message));
        return false;
    }

    elog("DBus请求名称重试${max}次后失败", ("max", max_retries));
    return false;
}

void connection::initialize() {
    dbus_connection_set_exit_on_disconnect(m_connection, false);
    dbus_connection_set_watch_functions(m_connection, watch_add, watch_remove, watch_toggled, this,
                                        nullptr);
    dbus_connection_set_timeout_functions(m_connection, timeout_add, timeout_remove,
                                          timeout_toggled, this, nullptr);
    dbus_connection_add_filter(m_connection, message_filter, this, nullptr);
    dbus_connection_set_dispatch_status_function(m_connection, dispatch_status_changed, this,
                                                 nullptr);
}
DBusHandlerResult connection::process_message(mc::dbus::message message) {
    auto msg_type = message.get_type();
    if (msg_type == message_type::method_return || msg_type == message_type::error) {
        uint32_t reply_serial = message.get_reply_serial();
        if (reply_serial != 0) {
            process_reply(reply_serial, message);
            return DBUS_HANDLER_RESULT_HANDLED;
        }

        // 没有 reply_serial 的 message 发送到 on_message 处理
    }

    return on_filter_message(message).get_value_or(DBUS_HANDLER_RESULT_NOT_YET_HANDLED);
}

void connection::process_reply(uint32_t reply_serial, message& msg) {
    std::lock_guard lock(m_impl->m_mutex);

    auto it = m_impl->m_pending_calls.find(reply_serial);
    if (it == m_impl->m_pending_calls.end()) {
        return;
    }

    auto promise = std::move(it->second.promise);
    m_impl->m_pending_calls.erase(it);

    promise.set_value(msg);
}

void connection::dispatch() {
    std::lock_guard lock(m_impl->m_mutex);

    while (dbus_connection_dispatch(m_connection) == DBUS_DISPATCH_DATA_REMAINS) {
    }
}

dbus_bool_t connection::watch_add(DBusWatch* watch, void* data) {
    connection* conn = static_cast<connection*>(data);
    if (!dbus_watch_get_enabled(watch)) {
        return TRUE;
    }

    auto w = new mc::dbus::watch(conn->m_strand, watch);
    dbus_watch_set_data(watch, w, [](void* data) {
        auto w = static_cast<mc::dbus::watch*>(data);
        w->stop();
        delete w;
    });

    w->start(conn);
    return TRUE;
}

void connection::watch_remove(DBusWatch* watch, void*) {
    auto* watch_data = dbus_watch_get_data(watch);
    if (watch_data) {
        static_cast<mc::dbus::watch*>(watch_data)->stop();
    }
}

void connection::watch_toggled(DBusWatch* watch, void* data) {
    auto* watch_data = dbus_watch_get_data(watch);
    if (!watch_data) {
        return;
    }

    if (dbus_watch_get_enabled(watch)) {
        connection* conn = static_cast<connection*>(data);
        static_cast<mc::dbus::watch*>(watch_data)->start(conn);
    } else {
        static_cast<mc::dbus::watch*>(watch_data)->stop();
    }
}

dbus_bool_t connection::timeout_add(DBusTimeout* timeout, void* data) {
    connection* conn = static_cast<connection*>(data);
    if (!dbus_timeout_get_enabled(timeout)) {
        return TRUE;
    }

    auto t = new mc::dbus::timeout(conn->m_strand, timeout);
    dbus_timeout_set_data(timeout, t, [](void* data) {
        auto t = static_cast<mc::dbus::timeout*>(data);
        t->stop();
        delete t;
    });

    t->start(conn);
    return TRUE;
}

void connection::timeout_remove(DBusTimeout* timeout, void*) {
    auto* timeout_data = dbus_timeout_get_data(timeout);
    if (timeout_data) {
        static_cast<mc::dbus::timeout*>(timeout_data)->stop();
    }
}

void connection::timeout_toggled(DBusTimeout* timeout, void* data) {
    auto* timeout_data = dbus_timeout_get_data(timeout);
    if (!timeout_data) {
        return;
    }

    if (dbus_timeout_get_enabled(timeout)) {
        connection* conn = static_cast<connection*>(data);
        static_cast<mc::dbus::timeout*>(timeout_data)->start(conn);
    } else {
        static_cast<mc::dbus::timeout*>(timeout_data)->stop();
    }
}

DBusHandlerResult connection::message_filter(DBusConnection*, DBusMessage* msg, void* user_data) {
    connection* conn = static_cast<connection*>(user_data);
    return conn->process_message(mc::dbus::message(msg, true));
}

void connection::dispatch_status_changed(DBusConnection*, DBusDispatchStatus new_status,
                                         void* user_data) {
    connection* conn = static_cast<connection*>(user_data);

    if (new_status == DBUS_DISPATCH_DATA_REMAINS) {
        conn->dispatch();
    }
}

} // namespace mc::dbus
