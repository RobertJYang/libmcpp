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

#include <mc/dbus/connection.h>
#include <mc/dbus/message.h>
#include <mc/exception.h>
#include <mc/log/log.h>

namespace mc::dbus {

// 静态回调函数实现
dbus_bool_t connection::watch_add(DBusWatch* watch, void* data) {
    connection* conn = static_cast<connection*>(data);
    if (!dbus_watch_get_enabled(watch)) {
        return TRUE;
    }

    std::lock_guard<std::mutex> lock(conn->m_mutex);
    // 创建新的watch
    auto asio_watch        = std::make_unique<connection::asio_watch>(conn, watch);
    conn->m_watches[watch] = std::move(asio_watch);
    conn->m_watches[watch]->start();

    return TRUE;
}

void connection::watch_remove(DBusWatch* watch, void* data) {
    connection*                 conn = static_cast<connection*>(data);
    std::lock_guard<std::mutex> lock(conn->m_mutex);
    conn->m_watches.erase(watch);
}

void connection::watch_toggled(DBusWatch* watch, void* data) {
    connection*                 conn = static_cast<connection*>(data);
    std::lock_guard<std::mutex> lock(conn->m_mutex);

    auto it = conn->m_watches.find(watch);
    if (it != conn->m_watches.end()) {
        if (dbus_watch_get_enabled(watch)) {
            it->second->start();
        } else {
            it->second->stop();
        }
    }
}

dbus_bool_t connection::timeout_add(DBusTimeout* timeout, void* data) {
    connection* conn = static_cast<connection*>(data);
    if (!dbus_timeout_get_enabled(timeout)) {
        return TRUE;
    }

    std::lock_guard<std::mutex> lock(conn->m_mutex);
    // 创建新的timeout
    auto asio_timeout         = std::make_unique<connection::asio_timeout>(conn, timeout);
    conn->m_timeouts[timeout] = std::move(asio_timeout);
    conn->m_timeouts[timeout]->start();

    return TRUE;
}

void connection::timeout_remove(DBusTimeout* timeout, void* data) {
    connection*                 conn = static_cast<connection*>(data);
    std::lock_guard<std::mutex> lock(conn->m_mutex);
    conn->m_timeouts.erase(timeout);
}

void connection::timeout_toggled(DBusTimeout* timeout, void* data) {
    connection*                 conn = static_cast<connection*>(data);
    std::lock_guard<std::mutex> lock(conn->m_mutex);

    auto it = conn->m_timeouts.find(timeout);
    if (it != conn->m_timeouts.end()) {
        if (dbus_timeout_get_enabled(timeout)) {
            it->second->start();
        } else {
            it->second->stop();
        }
    }
}

void connection::dispatch_status_changed(DBusConnection* connection, DBusDispatchStatus new_status,
                                         void* data) {
    if (new_status == DBUS_DISPATCH_DATA_REMAINS) {
        connection* conn = static_cast<connection*>(data);
        conn->dispatch_local();
    }
}

DBusHandlerResult connection::message_filter(DBusConnection* connection, DBusMessage* message,
                                             void* user_data) {
    connection* conn = static_cast<connection*>(user_data);
    return conn->handle_message(message);
}

// 类方法实现
connection_ptr connection::create(io_context_type& io_context) {
    return std::make_shared<connection>(io_context);
}

connection::connection(io_context_type& io_context) : m_io_context(io_context) {
}

connection::~connection() {
    disconnect();
}

mc::future<error_code> connection::connect(const std::string& address) {
    if (m_state.load() != connection_state::disconnected) {
        promise<error_code> promise;
        promise.set_value(
            boost::system::errc::make_error_code(boost::system::errc::operation_in_progress));
        return promise.get_future();
    }

    // 更新状态为连接中
    m_state.store(connection_state::connecting);

    // 创建Promise
    promise<error_code> promise;
    auto                future = promise.get_future();

    try {
        // 创建DBus连接对象
        DBusError error;
        dbus_error_init(&error);

        // 注意：这里使用了私有连接，即连接的所有权由我们的应用程序管理
        m_connection = dbus_connection_open_private(address.c_str(), &error);
        if (!m_connection) {
            if (dbus_error_is_set(&error)) {
                std::string error_msg = "DBus连接失败: " + std::string(error.message);
                dbus_error_free(&error);
                m_state.store(connection_state::disconnected);
                promise.set_value(
                    boost::system::errc::make_error_code(boost::system::errc::connection_refused));
                return future;
            }
            m_state.store(connection_state::disconnected);
            promise.set_value(boost::system::errc::make_error_code(boost::system::errc::no_memory));
            return future;
        }

        // 初始化libdbus连接
        initialize_libdbus();

        // 注册到总线
        m_state.store(connection_state::establishing);
        if (!dbus_bus_register(m_connection, &error)) {
            if (dbus_error_is_set(&error)) {
                std::string error_msg = "DBus注册失败: " + std::string(error.message);
                dbus_error_free(&error);
                disconnect();
                promise.set_value(
                    boost::system::errc::make_error_code(boost::system::errc::connection_refused));
                return future;
            }
            disconnect();
            promise.set_value(boost::system::errc::make_error_code(boost::system::errc::no_memory));
            return future;
        }

        // 设置过滤器用于接收所有消息
        if (!dbus_connection_add_filter(m_connection, message_filter, this, nullptr)) {
            disconnect();
            promise.set_value(boost::system::errc::make_error_code(boost::system::errc::no_memory));
            return future;
        }

        // 连接成功
        m_state.store(connection_state::connected);
        promise.set_value(boost::system::errc::make_error_code(boost::system::errc::success));

    } catch (const std::exception& e) {
        disconnect();
        promise.set_value(
            boost::system::errc::make_error_code(boost::system::errc::operation_canceled));
    }

    return future;
}

void connection::disconnect() {
    auto state = m_state.exchange(connection_state::disconnecting);
    if (state == connection_state::disconnected || state == connection_state::disconnecting) {
        return;
    }

    // 清空watches和timeouts
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_watches.clear();
        m_timeouts.clear();
    }

    // 断开libdbus连接
    if (m_connection) {
        dbus_connection_close(m_connection);
        dbus_connection_unref(m_connection);
        m_connection = nullptr;
    }

    m_state.store(connection_state::disconnected);
}

connection_state connection::get_state() const {
    return m_state.load();
}

void connection::initialize_libdbus() {
    if (!m_connection) {
        return;
    }

    // 不使用自动连接模式
    dbus_connection_set_exit_on_disconnect(m_connection, FALSE);

    // 设置调度状态回调
    dbus_connection_set_dispatch_status_function(m_connection, dispatch_status_changed, this,
                                                 nullptr);

    // 设置watch回调
    dbus_connection_set_watch_functions(m_connection, watch_add, watch_remove, watch_toggled, this,
                                        nullptr);

    // 设置timeout回调
    dbus_connection_set_timeout_functions(m_connection, timeout_add, timeout_remove,
                                          timeout_toggled, this, nullptr);
}

DBusHandlerResult connection::handle_message(DBusMessage* dbus_msg) {
    // 只处理方法返回和信号
    int msg_type = dbus_message_get_type(dbus_msg);
    if (msg_type != DBUS_MESSAGE_TYPE_METHOD_RETURN && msg_type != DBUS_MESSAGE_TYPE_ERROR &&
        msg_type != DBUS_MESSAGE_TYPE_SIGNAL) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    // 创建message对象
    message_ptr_type msg = std::make_shared<variants_message>();
    msg->parse_from_dbus_message(dbus_msg);

    // 检查是否是回复消息
    if (msg_type == DBUS_MESSAGE_TYPE_METHOD_RETURN || msg_type == DBUS_MESSAGE_TYPE_ERROR) {
        uint32_t reply_serial = msg->reply_serial;
        if (reply_serial != 0) {
            // 寻找对应的回调
            std::lock_guard<std::mutex> lock(m_mutex);
            auto                        it = m_pending_calls.find(reply_serial);
            if (it != m_pending_calls.end()) {
                // 调用回调
                message_handler handler = std::move(it->second);
                m_pending_calls.erase(it);
                lock.~lock_guard(); // 提前释放锁

                try {
                    handler(msg);
                } catch (const std::exception& e) {
                    ilog("处理回调异常: ${error}", ("error", e.what()));
                }
                return DBUS_HANDLER_RESULT_HANDLED;
            }
        }
    }

    // 发送信号给监听器
    on_message(msg);

    return DBUS_HANDLER_RESULT_HANDLED;
}

void connection::handle_pending_call_reply(DBusMessage* reply) {
    uint32_t reply_serial = dbus_message_get_reply_serial(reply);
    if (reply_serial == 0) {
        return;
    }

    // 创建message对象
    message_ptr_type msg = std::make_shared<variants_message>();
    msg->parse_from_dbus_message(reply);

    // 寻找对应的回调
    std::lock_guard<std::mutex> lock(m_mutex);
    auto                        it = m_pending_calls.find(reply_serial);
    if (it != m_pending_calls.end()) {
        // 调用回调
        message_handler handler = std::move(it->second);
        m_pending_calls.erase(it);
        lock.~lock_guard(); // 提前释放锁

        try {
            handler(msg);
        } catch (const std::exception& e) {
            ilog("处理回调异常: ${error}", ("error", e.what()));
        }
    }
}

void connection::dispatch_local() {
    if (!m_connection) {
        return;
    }

    // 设置定时器进行调度
    m_dispatch_timer.expires_from_now(std::chrono::milliseconds(0));
    m_dispatch_timer.async_wait([this](const boost::system::error_code& ec) {
        if (ec) {
            return;
        }

        if (m_connection) {
            dbus_connection_ref(m_connection);
            while (dbus_connection_dispatch(m_connection) == DBUS_DISPATCH_DATA_REMAINS)
                ;
            dbus_connection_unref(m_connection);
        }
    });
}

} // namespace mc::dbus
