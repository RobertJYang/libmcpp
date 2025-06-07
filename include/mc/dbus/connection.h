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

#include <mc/core/object.h>
#include <mc/dbus/message.h>
#include <mc/future.h>
#include <mc/signal_slot.h>
#include <mc/time.h>
#include <mc/dbus/match.h>

#include <mutex>

namespace mc::dbus {
struct connection_impl;
constexpr mc::milliseconds DBUS_TIMEOUT_DEFAULT = mc::seconds(60);

using filter_message_signal_type = mc::signal<DBusHandlerResult(message&)>;
using path_handler_type          = std::function<DBusHandlerResult(message&)>;

enum class connect_status {
    connected,
    disconnected,
    connecting,
    disconnecting,
};

/**
 * @brief DBus连接对象
 */
class connection {
public:
    template <typename T>
    using future = mc::future<T, mc::core::io_context::executor_type>;

    static connection open_system_bus(mc::core::io_context& executor);
    static connection open_session_bus(mc::core::io_context& executor);

    connection();

    /**
     * @brief 构造函数
     * @param strand 线程池
     * @param conn DBus连接
     * @param add_ref 是否增加引用
     */
    explicit connection(mc::core::io_context& executor, DBusConnection* conn, bool add_ref = false);

    /**
     * @brief 析构函数
     * @note 连接是共享的，析构函数不会关闭连接，如果需要关闭连接，请调用 disconnect 方法
     */
    ~connection();

    connection(const connection&)            = default;
    connection& operator=(const connection&) = default;
    connection(connection&&)                 = default;
    connection& operator=(connection&&)      = default;

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
    future<message> async_send_with_reply(message&&        msg,
                                          mc::milliseconds timeout = DBUS_TIMEOUT_DEFAULT);

    /**
     * @brief 注册路径
     * @param path 路径
     * @param object 对象
     */
    void register_path(std::string_view path, path_handler_type handler);

    /**
     * @brief 注销路径
     * @param path 路径
     */
    void unregister_path(std::string_view path);

    /**
     * @brief 获取当前连接状态
     * @return 连接状态
     */
    bool is_connected() const;

    /**
     * @brief 获取DBus连接
     * @return DBus连接
     */
    DBusConnection* get_connection() const;

    void dispatch();

    /**
     * @brief 获取DBus连接的唯一名称
     * @return 唯一名称
     */
    std::string_view get_unique_name() const;

    void add_match(match_rule& rule, match_cb_t&& cb, uint64_t id);

    void remove_match(uint64_t id);

    match& get_match();

    uint32_t get_next_serial();

    connection_impl& get_impl() const;

    filter_message_signal_type& filter_message() const;

private:
    void ensure_impl() const;

    std::shared_ptr<connection_impl> m_impl;
};

} // namespace mc::dbus

#endif // MC_DBUS_CONNECTION_H