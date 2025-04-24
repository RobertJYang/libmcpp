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

#ifndef MC_DBUS_CONNECTION_H
#define MC_DBUS_CONNECTION_H

#include <dbus/dbus.h>

#include <boost/asio/io_context.hpp>
#include <mc/dbus/message.h>
#include <mc/future.h>
#include <mc/signal_slot.h>
#include <mc/time.h>

#include <mutex>

namespace mc::dbus {

class connection;
using connection_ptr      = std::shared_ptr<connection>;
using connection_weak_ptr = std::weak_ptr<connection>;

constexpr mc::milliseconds DBUS_TIMEOUT_DEFAULT = mc::seconds(60);

/**
 * @brief DBus连接对象
 */
class connection : public std::enable_shared_from_this<connection> {
public:
    using io_context_type = boost::asio::io_context;

    static connection_ptr open_system_bus(io_context_type& io_context);
    static connection_ptr open_session_bus(io_context_type& io_context);

    enum class connect_status {
        connected,
        disconnected,
        connecting,
        disconnecting,
    };

    /**
     * @brief 构造函数
     * @param io_context IO上下文
     */
    explicit connection(io_context_type& io_context, DBusConnection* conn, bool add_ref = false);

    /**
     * @brief 析构函数
     */
    ~connection();

    connection(const connection&)            = delete;
    connection& operator=(const connection&) = delete;
    connection(connection&&)                 = delete;
    connection& operator=(connection&&)      = delete;

    /**
     * @brief 断开连接
     */
    void disconnect();

    /**
     * @brief 注册名称
     * @param name 名称
     * @return 是否成功注册
     */
    bool start();

    /**
     * @brief 请求名称
     * @param name 名称
     * @return 是否成功请求
     */
    bool request_name(std::string_view name, uint32_t flags = 0);

    /**
     * @brief 发送消息
     * @param msg 要发送的消息
     * @return 是否成功发送
     */
    bool send(message&& msg);

    /**
     * @brief 发送消息并等待回复
     * @param msg 要发送的消息
     * @param timeout 超时时间（毫秒）
     * @return 返回响应消息
     */
    message send_with_reply(message&& msg, mc::milliseconds timeout = DBUS_TIMEOUT_DEFAULT);

    /**
     * @brief 发送消息并等待回复
     * @param msg 要发送的消息
     * @param timeout 超时时间（毫秒）
     * @return 完成后的future，包含回复消息
     */
    mc::future<message> async_send_with_reply(message&&        msg,
                                              mc::milliseconds timeout = DBUS_TIMEOUT_DEFAULT);

    /**
     * @brief 获取当前连接状态
     * @return 连接状态
     */
    bool is_connected() const;

    /**
     * @brief 获取DBus连接
     * @return DBus连接
     */
    DBusConnection* get_connection() const {
        return m_connection;
    }

    /**
     * @brief 获取IO上下文
     * @return IO上下文引用
     */
    io_context_type& get_io_context() const {
        return m_io_context;
    }

    void dispatch();

    mc::signal<void(message)> on_message;

private:
    void initialize();
    bool check_connected() const;
    void release();
    void delay_dispatch();

    DBusHandlerResult process_message(message msg);
    void              process_reply(uint32_t reply_serial, message& msg);

    static dbus_bool_t watch_add(DBusWatch* watch, void* data);
    static void        watch_remove(DBusWatch* watch, void* data);
    static void        watch_toggled(DBusWatch* watch, void* data);

    static dbus_bool_t timeout_add(DBusTimeout* timeout, void* data);
    static void        timeout_remove(DBusTimeout* timeout, void* data);
    static void        timeout_toggled(DBusTimeout* timeout, void* data);

    static void dispatch_status_changed(DBusConnection* connection, DBusDispatchStatus new_status,
                                        void* user_data);
    static DBusHandlerResult message_filter(DBusConnection* conn, DBusMessage* msg,
                                            void* user_data);

    struct connection_impl;
    std::unique_ptr<connection_impl> m_impl;
    DBusConnection*                  m_connection{nullptr};
    io_context_type&                 m_io_context;
    connect_status                   m_status{connect_status::disconnected};
};

} // namespace mc::dbus

#endif // MC_DBUS_CONNECTION_H