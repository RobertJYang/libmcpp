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

#include <mc/dbus/validator.h>
#include <mc/log.h>

#include "connection_impl.h"
#include "dbus/dispatch/timeout.h"
#include "dbus/dispatch/watch.h"

namespace mc::dbus {

static std::once_flag s_init_dbus;

connection_impl::connection_impl(mc::core::io_context& executor) : m_executor(executor) {
    std::call_once(s_init_dbus, []() {
        dbus_threads_init_default();
    });
}

connection_impl::~connection_impl() {
    disconnect();
}

uint32_t connection_impl::get_next_serial() {
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

void connection_impl::disconnect() {
    std::lock_guard lock(m_mutex);
    if (!m_connection || m_status == connect_status::disconnected ||
        m_status == connect_status::disconnecting) {
        return;
    }

    m_status = connect_status::disconnecting;
    release();
}

bool connection_impl::send(message&& msg) {
    std::lock_guard lock(m_mutex);
    if (!is_connected()) {
        return false;
    }

    auto serial = msg.get_serial();
    if (serial == 0) {
        serial = get_next_serial();
        msg.set_serial(serial);
    }

    return dbus_connection_send(m_connection, msg.get_dbus_message(), nullptr);
}

connection::future<message> connection_impl::async_send_with_reply(message&&        msg,
                                                                   mc::milliseconds timeout) {
    std::lock_guard lock(m_mutex);

    auto promise = mc::make_promise<message>(m_executor.get_executor());
    auto future  = promise.get_future();

    if (!is_connected()) {
        promise.set_value(message::new_error(msg, error_names::disconnected, "连接已断开"));
        return future;
    }

    auto serial = msg.get_serial();
    if (serial == 0) {
        serial = get_next_serial();
        msg.set_serial(serial);
    }

    DBusPendingCall* dbus_pending_call = nullptr;

    auto ret = dbus_connection_send_with_reply(m_connection, msg.get_dbus_message(),
                                               &dbus_pending_call, timeout.count());
    if (ret != TRUE) {
        promise.set_value(message::new_error(msg, error_names::failed, "发送消息失败"));
        return future;
    }

    auto on_reply = [this, serial](message msg) {
        process_reply(serial, msg);
    };

    auto [it, inserted] = m_pending_calls.emplace(
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
    auto handler = static_cast<path_handler_type*>(user_data);
    try {
        auto message = mc::dbus::message(msg, true);
        return (*handler)(message);
    } catch (const std::exception& e) {
        elog("DBus path handler failed: ${error}", ("error", e.what()));
        if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }

        // 对于方法调用，主动返回一个错误
        // TODO:: 为了调试方便这里直接返回错误，后续应该隐藏程序内部信息避免安全隐患
        auto reply = dbus_message_new_error(msg, "org.freedesktop.DBus.Error.Failed", e.what());
        dbus_connection_send(conn, reply, nullptr);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
}

static void path_unregister(DBusConnection* conn, void* user_data) {
    auto handler = static_cast<path_handler_type*>(user_data);
    delete handler;
}

void connection_impl::register_path(std::string_view path, path_handler_type handler) {
    std::lock_guard lock(m_mutex);
    if (!is_connected()) {
        return;
    }

    DBusObjectPathVTable vtable;
    vtable.message_function    = path_handler;
    vtable.unregister_function = path_unregister;
    auto handler_data          = new path_handler_type(std::move(handler));
    dbus_connection_register_object_path(m_connection, path.data(), &vtable, handler_data);
}

void connection_impl::unregister_path(std::string_view path) {
    std::lock_guard lock(m_mutex);
    if (!is_connected()) {
        return;
    }

    dbus_connection_unregister_object_path(m_connection, path.data());
}

bool connection_impl::is_connected() const {
    return m_connection && m_status == connect_status::connected;
}

void connection_impl::release() {
    if (m_connection) {
        dbus_connection_close(m_connection);
        dbus_connection_unref(m_connection);
        m_connection = nullptr;
        m_status     = connect_status::disconnected;
    }
}

bool connection_impl::start() {
    std::lock_guard lock(m_mutex);

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

bool connection_impl::request_name(std::string_view name, uint32_t flags) {
    if (name.empty() || !validator::is_valid_bus_name(name)) {
        return false;
    }

    std::lock_guard lock(m_mutex);
    if (!is_connected()) {
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

void connection_impl::initialize() {
    dbus_connection_set_exit_on_disconnect(m_connection, false);
    dbus_connection_set_watch_functions(m_connection, watch_add, watch_remove, watch_toggled, this,
                                        nullptr);
    dbus_connection_set_timeout_functions(m_connection, timeout_add, timeout_remove,
                                          timeout_toggled, this, nullptr);
    dbus_connection_add_filter(m_connection, message_filter, this, nullptr);
    dbus_connection_set_dispatch_status_function(m_connection, dispatch_status_changed, this,
                                                 nullptr);
}

DBusHandlerResult connection_impl::process_message(mc::dbus::message message) {
    auto msg_type = message.get_type();
    if (msg_type == message_type::method_return || msg_type == message_type::error) {
        uint32_t reply_serial = message.get_reply_serial();
        if (reply_serial != 0) {
            process_reply(reply_serial, message);
            return DBUS_HANDLER_RESULT_HANDLED;
        }

        // 没有 reply_serial 的 message 发送到 on_message 处理
    } else if (msg_type == message_type::signal) {
        m_match.run_msg(message.get_dbus_message());
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    return on_filter_message(message).get_value_or(DBUS_HANDLER_RESULT_NOT_YET_HANDLED);
}

void connection_impl::process_reply(uint32_t reply_serial, message& msg) {
    std::lock_guard lock(m_mutex);

    auto it = m_pending_calls.find(reply_serial);
    if (it == m_pending_calls.end()) {
        return;
    }

    auto promise = std::move(it->second.promise);
    it->second.pending.stop();
    m_pending_calls.erase(it);

    promise.set_value(msg);
}

void connection_impl::dispatch() {
    while (is_connected() && dbus_connection_dispatch(m_connection) == DBUS_DISPATCH_DATA_REMAINS) {
    }
}

dbus_bool_t connection_impl::watch_add(DBusWatch* watch, void* data) {
    connection_impl* conn = static_cast<connection_impl*>(data);
    if (!dbus_watch_get_enabled(watch)) {
        return TRUE;
    }

    auto w = mc::make_shared<mc::dbus::watch>(conn->m_executor, watch);
    dbus_watch_set_data(watch, w.get(), [](void* data) {
        auto w = mc::dbus::watch::from_raw(data);
        w->stop();
    });
    w->start(conn->weak_from_this());
    w.detach();
    return TRUE;
}

void connection_impl::watch_remove(DBusWatch* watch, void*) {
    auto* watch_data = dbus_watch_get_data(watch);
    if (watch_data) {
        static_cast<mc::dbus::watch*>(watch_data)->stop();
    }
}

void connection_impl::watch_toggled(DBusWatch* watch, void* data) {
    auto* watch_data = dbus_watch_get_data(watch);
    if (!watch_data) {
        return;
    }

    if (dbus_watch_get_enabled(watch)) {
        connection_impl* conn = static_cast<connection_impl*>(data);
        static_cast<mc::dbus::watch*>(watch_data)->start(conn->weak_from_this());
    } else {
        static_cast<mc::dbus::watch*>(watch_data)->stop();
    }
}

dbus_bool_t connection_impl::timeout_add(DBusTimeout* timeout, void* data) {
    connection_impl* conn = static_cast<connection_impl*>(data);
    if (!dbus_timeout_get_enabled(timeout)) {
        return TRUE;
    }

    auto t = mc::make_shared<mc::dbus::timeout>(conn->m_executor, timeout);
    dbus_timeout_set_data(timeout, t.get(), [](void* data) {
        auto t = mc::dbus::timeout::from_raw(data);
        t->stop();
    });

    t->start(conn->weak_from_this());
    t.detach();
    return TRUE;
}

void connection_impl::timeout_remove(DBusTimeout* timeout, void*) {
    auto* timeout_data = dbus_timeout_get_data(timeout);
    if (timeout_data) {
        static_cast<mc::dbus::timeout*>(timeout_data)->stop();
    }
}

void connection_impl::timeout_toggled(DBusTimeout* timeout, void* data) {
    auto* timeout_data = dbus_timeout_get_data(timeout);
    if (!timeout_data) {
        return;
    }

    if (dbus_timeout_get_enabled(timeout)) {
        connection_impl* conn = static_cast<connection_impl*>(data);
        static_cast<mc::dbus::timeout*>(timeout_data)->start(conn->weak_from_this());
    } else {
        static_cast<mc::dbus::timeout*>(timeout_data)->stop();
    }
}

DBusHandlerResult connection_impl::message_filter(DBusConnection*, DBusMessage* msg,
                                                  void* user_data) {
    connection_impl* conn = static_cast<connection_impl*>(user_data);
    return conn->process_message(mc::dbus::message(msg, true));
}

void connection_impl::dispatch_status_changed(DBusConnection*, DBusDispatchStatus new_status,
                                              void* user_data) {
    connection* conn = static_cast<connection*>(user_data);

    if (new_status == DBUS_DISPATCH_DATA_REMAINS) {
        conn->dispatch();
    }
}

void connection_impl::add_match(match_rule& rule, match_cb_t&& cb, uint64_t id) {
    m_match.add_rule(rule, std::forward<match_cb_t>(cb), id);
    auto            str = rule.as_string();
    mc::dbus::error err;
    dbus_bus_add_match(m_connection, str.c_str(), &err);
    if (err.is_set()) {
        elog("dbus add match failed: ${error}", ("error", err.message));
    }
    m_match_strs.emplace(id, std::move(str));
}

void connection_impl::remove_match(uint64_t id) {
    m_match.remove_rule(id);
    auto it = m_match_strs.find(id);
    if (it == m_match_strs.end()) {
        return;
    }
    mc::dbus::error err;
    dbus_bus_remove_match(m_connection, it->second.c_str(), &err);
    if (err.is_set()) {
        elog("dbus remove match failed: ${error}", ("error", err.message));
    }
    m_match_strs.erase(it);
}

match& connection_impl::get_match() {
    return m_match;
}
} // namespace mc::dbus
