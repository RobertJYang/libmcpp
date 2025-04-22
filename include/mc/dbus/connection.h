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

#include <boost/asio.hpp>

#include <mc/dbus/message.h>
#include <mc/future.h>
#include <mc/signal_slot.h>

#include <atomic>
#include <dbus/dbus.h>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace mc::dbus {

/**
 * @brief DBus连接状态枚举
 */
enum class connection_state {
    disconnected,   ///< 未连接
    connecting,     ///< 正在连接
    authenticating, ///< 正在认证
    establishing,   ///< 正在建立连接
    connected,      ///< 已连接
    disconnecting   ///< 正在断开连接
};

using error_code = boost::system::error_code;

class connection;
using connection_ptr = std::shared_ptr<connection>;

/**
 * @brief DBus连接对象
 *
 * 处理与DBus服务器的连接和通信状态管理，基于libdbus和boost::asio
 */
class connection : public std::enable_shared_from_this<connection> {
public:
    using io_context_type    = boost::asio::io_context;
    using message_handler    = std::function<void(const std::shared_ptr<variants_message>&)>;
    using message_map        = std::unordered_map<uint32_t, message_handler>;
    using message_ptr_type   = std::shared_ptr<variants_message>;
    using watch_descriptor   = int;
    using timeout_descriptor = int;

    /**
     * @brief 创建连接实例
     * @param io_context IO上下文
     * @return 连接对象智能指针
     */
    static connection_ptr create(io_context_type& io_context);

    /**
     * @brief 构造函数
     * @param io_context IO上下文
     */
    explicit connection(io_context_type& io_context);

    /**
     * @brief 析构函数
     */
    virtual ~connection();

    /**
     * @brief 连接到DBus服务器
     * @param address 连接地址
     * @return 完成后的future，包含错误码
     *
     * 地址字符串格式：
     * method1:key1=value1,key2=value2;method2:key1=value1,key2=value2
     *
     * 支持的传输方式：
     * - unix:path=/path/to/socket
     * - unix:abstract=/tmp/abstract-socket
     * - tcp:host=hostname,port=1234
     */
    mc::future<error_code> connect(const std::string& address);

    /**
     * @brief 断开连接
     */
    void disconnect();

    /**
     * @brief 发送消息
     * @param msg 要发送的消息
     * @return 是否成功发送
     */
    template <typename T>
    bool send(mc::dbus::message<T>& msg) {
        if (m_state.load() != connection_state::connected || !m_connection) {
            return false;
        }

        // 设置序列号
        uint32_t serial = m_next_serial.fetch_add(1);
        msg.set_serial(serial);

        // 构建DBusMessage
        DBusMessage* dbus_msg = msg.build_message();
        if (!dbus_msg) {
            return false;
        }

        // 发送消息
        std::lock_guard<std::mutex> lock(m_mutex);
        dbus_bool_t                 sent = dbus_connection_send(m_connection, dbus_msg, nullptr);
        dbus_connection_flush(m_connection);

        return sent ? true : false;
    }

    /**
     * @brief 发送消息并等待回复
     * @param msg 要发送的消息
     * @param timeout 超时时间（毫秒）
     * @return 完成后的future，包含回复消息
     */
    template <typename T>
    mc::future<message_ptr_type> send_with_reply(mc::dbus::message<T>& msg, int timeout = -1) {
        if (m_state.load() != connection_state::connected || !m_connection) {
            promise<message_ptr_type> promise;
            promise.set_exception(std::make_exception_ptr(mc::exception("连接未建立或未就绪")));
            return promise.get_future();
        }

        // 设置序列号
        uint32_t serial = m_next_serial.fetch_add(1);
        msg.set_serial(serial);

        // 构建DBusMessage
        DBusMessage* dbus_msg = msg.build_message();
        if (!dbus_msg) {
            promise<message_ptr_type> promise;
            promise.set_exception(std::make_exception_ptr(mc::exception("无法构建DBus消息")));
            return promise.get_future();
        }

        // 设置为需要回复
        msg.set_no_reply(false);

        // 创建Promise
        promise<message_ptr_type> reply_promise;
        auto                      future = reply_promise.get_future();

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            // 创建pending call
            DBusPendingCall* pending = nullptr;
            dbus_bool_t      sent =
                dbus_connection_send_with_reply(m_connection, dbus_msg, &pending, timeout);

            if (!sent || !pending) {
                reply_promise.set_exception(
                    std::make_exception_ptr(mc::exception("发送消息失败或无法创建挂起调用")));
                return future;
            }

            // 保存promise用于后续处理
            m_pending_calls[serial] =
                [this, promise = std::move(reply_promise)](const message_ptr_type& reply) mutable {
                    promise.set_value(reply);
                };

            // 设置回调，在收到回复时处理
            dbus_pending_call_set_notify(
                pending,
                [](DBusPendingCall* pending, void* user_data) {
                    auto*        conn      = static_cast<connection*>(user_data);
                    DBusMessage* reply_msg = dbus_pending_call_steal_reply(pending);
                    if (reply_msg) {
                        conn->handle_pending_call_reply(reply_msg);
                        dbus_message_unref(reply_msg);
                    }
                    dbus_pending_call_unref(pending);
                },
                this, nullptr);

            dbus_connection_flush(m_connection);
        }

        return future;
    }

    /**
     * @brief 获取当前连接状态
     * @return 连接状态
     */
    connection_state get_state() const;

    /**
     * @brief 获取IO上下文
     * @return IO上下文引用
     */
    io_context_type& get_io_context() const {
        return m_io_context;
    }

    mc::signal<void(message_ptr_type)> on_message;

private:
    // asio watch封装类
    class asio_watch {
    public:
        asio_watch(connection* conn, DBusWatch* watch)
            : m_connection(conn), m_watch(watch), m_descriptor(dbus_watch_get_unix_fd(watch)),
              m_socket(conn->get_io_context(), m_descriptor) {
        }

        void start() {
            if (dbus_watch_get_enabled(m_watch)) {
                unsigned int flags = dbus_watch_get_flags(m_watch);
                if (flags & DBUS_WATCH_READABLE) {
                    watch_readable();
                }
                if (flags & DBUS_WATCH_WRITABLE) {
                    watch_writable();
                }
            }
        }

        void stop() {
            m_socket.cancel();
        }

    private:
        void watch_readable() {
            m_socket.async_wait(boost::asio::posix::stream_descriptor::wait_read,
                                [this](const boost::system::error_code& ec) {
                                    if (!ec) {
                                        handle_watch_ready(DBUS_WATCH_READABLE);
                                        if (dbus_watch_get_enabled(m_watch) &&
                                            (dbus_watch_get_flags(m_watch) & DBUS_WATCH_READABLE)) {
                                            watch_readable();
                                        }
                                    }
                                });
        }

        void watch_writable() {
            m_socket.async_wait(boost::asio::posix::stream_descriptor::wait_write,
                                [this](const boost::system::error_code& ec) {
                                    if (!ec) {
                                        handle_watch_ready(DBUS_WATCH_WRITABLE);
                                        if (dbus_watch_get_enabled(m_watch) &&
                                            (dbus_watch_get_flags(m_watch) & DBUS_WATCH_WRITABLE)) {
                                            watch_writable();
                                        }
                                    }
                                });
        }

        void handle_watch_ready(unsigned int flags) {
            dbus_watch_handle(m_watch, flags);
            if (m_connection->m_connection) {
                dbus_connection_ref(m_connection->m_connection);
                while (dbus_connection_dispatch(m_connection->m_connection) ==
                       DBUS_DISPATCH_DATA_REMAINS)
                    ;
                dbus_connection_unref(m_connection->m_connection);
            }
        }

        connection*                           m_connection;
        DBusWatch*                            m_watch;
        int                                   m_descriptor;
        boost::asio::posix::stream_descriptor m_socket;
    };

    // asio timeout封装类
    class asio_timeout {
    public:
        asio_timeout(connection* conn, DBusTimeout* timeout)
            : m_connection(conn), m_timeout(timeout), m_timer(conn->get_io_context()) {
        }

        void start() {
            if (dbus_timeout_get_enabled(m_timeout)) {
                int interval = dbus_timeout_get_interval(m_timeout);
                m_timer.expires_from_now(std::chrono::milliseconds(interval));
                m_timer.async_wait([this](const boost::system::error_code& ec) {
                    if (!ec) {
                        handle_timeout();
                    }
                });
            }
        }

        void stop() {
            m_timer.cancel();
        }

        void reschedule() {
            stop();
            start();
        }

    private:
        void handle_timeout() {
            dbus_timeout_handle(m_timeout);
            if (dbus_timeout_get_enabled(m_timeout)) {
                start();
            }
        }

        connection*               m_connection;
        DBusTimeout*              m_timeout;
        boost::asio::steady_timer m_timer;
    };

    // libdbus watch管理回调
    static dbus_bool_t watch_add(DBusWatch* watch, void* data);
    static void        watch_remove(DBusWatch* watch, void* data);
    static void        watch_toggled(DBusWatch* watch, void* data);

    // libdbus timeout管理回调
    static dbus_bool_t timeout_add(DBusTimeout* timeout, void* data);
    static void        timeout_remove(DBusTimeout* timeout, void* data);
    static void        timeout_toggled(DBusTimeout* timeout, void* data);

    // libdbus dispatcher调度回调
    static void dispatch_status_changed(DBusConnection* connection, DBusDispatchStatus new_status,
                                        void* data);

    // 初始化连接
    void initialize_libdbus();

    // 处理收到的消息
    DBusHandlerResult handle_message(DBusMessage* message);

    // 处理pending call回复
    void handle_pending_call_reply(DBusMessage* reply);

    // 处理来自libdbus的本地调度
    void dispatch_local();

    io_context_type&              m_io_context;                            ///< IO上下文引用
    DBusConnection*               m_connection{nullptr};                   ///< libdbus连接对象
    std::atomic<connection_state> m_state{connection_state::disconnected}; ///< 当前连接状态
    std::atomic<uint32_t>         m_next_serial{1};                        ///< 下一个消息序列号
    message_map                   m_pending_calls;                         ///< 待处理的消息回调

    std::unordered_map<DBusWatch*, std::unique_ptr<asio_watch>>     m_watches;  ///< 活动的watch
    std::unordered_map<DBusTimeout*, std::unique_ptr<asio_timeout>> m_timeouts; ///< 活动的timeout

    boost::asio::steady_timer m_dispatch_timer{m_io_context}; ///< 调度定时器

    mutable std::mutex m_mutex; ///< 保护共享数据的互斥锁

    // 静态消息处理回调
    static DBusHandlerResult message_filter(DBusConnection* connection, DBusMessage* message,
                                            void* user_data);
};

} // namespace mc::dbus

#endif // MC_DBUS_CONNECTION_H