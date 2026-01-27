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
#include <mc/dbus/match.h>
#include <mc/dbus/message.h>
#include <mc/future.h>
#include <mc/signal_slot.h>
#include <mc/time.h>

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
class MC_API connection {
public:
    template <typename T>
    using future = mc::future<T>;

    /**
     * @brief 打开系统总线连接
     * @param executor [in] IO上下文执行器
     * @return 返回DBus系统总线连接对象
     * @exception 连接失败时抛出异常
     */
    static connection open_system_bus(mc::io_context& executor);

    /**
     * @brief 打开会话总线连接
     * @param executor [in] IO上下文执行器
     * @return 返回DBus会话总线连接对象
     * @exception 连接失败时抛出异常
     */
    static connection open_session_bus(mc::io_context& executor);

    /**
     * @brief 默认构造函数
     */
    connection();

    /**
     * @brief 构造函数
     * @param executor [in] IO上下文执行器
     * @param conn [in] DBus连接指针
     * @param add_ref [in] 是否增加引用计数
     */
    explicit connection(mc::io_context& executor, DBusConnection* conn, bool add_ref = false);

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
     * @brief 刷新连接，将缓冲的消息发送到底层传输
     */
    void flush();

    /**
     * @brief 启动连接并开始消息分发
     * @return 是否成功启动
     * @exception 启动失败时抛出异常
     */
    bool start();

    /**
     * @brief 请求名称
     * @param name [in] 名称
     * @param flags [in] 标志位
     * @return tuple<是否成功, 可选的错误信息>
     */
    std::tuple<bool, std::optional<error>> request_name(std::string_view name, uint32_t flags = 0);

    /**
     * @brief 发送消息
     * @param msg [in] 要发送的消息
     * @return 是否成功发送
     */
    bool send(message&& msg);

    /**
     * @brief 发送消息并等待回复
     * @param msg [in] 要发送的消息
     * @param timeout [in] 超时时间（毫秒）
     * @return 返回响应消息
     */
    message send_with_reply(message&& msg, mc::milliseconds timeout = DBUS_TIMEOUT_DEFAULT);

    /**
     * @brief 发送消息并阻塞等待回复（send_with_reply的别名）
     * @param msg [in] 要发送的消息
     * @param timeout [in] 超时时间（毫秒）
     * @return 返回响应消息
     */
    message send_with_reply_and_block(message&& msg, mc::milliseconds timeout = DBUS_TIMEOUT_DEFAULT);

    /**
     * @brief 发送消息并等待回复
     * @param msg [in] 要发送的消息
     * @param timeout [in] 超时时间（毫秒）
     * @return 完成后的future，包含回复消息
     */
    future<message> async_send_with_reply(message&&        msg,
                                          mc::milliseconds timeout = DBUS_TIMEOUT_DEFAULT);

    /**
     * @brief 注册路径
     * @param path [in] 路径
     * @param handler [in] 路径处理器
     */
    void register_path(std::string_view path, path_handler_type handler);

    /**
     * @brief 注销路径
     * @param path [in] 路径
     */
    void unregister_path(std::string_view path);

    /**
     * @brief 获取当前连接状态
     * @return 连接状态
     */
    bool is_connected() const;

    /**
     * @brief 获取DBus连接的实际状态（使用dbus_connection_get_is_connected）
     * @return 连接状态
     */
    bool get_is_connected() const;

    /**
     * @brief 获取DBus连接
     * @return DBus连接
     */
    DBusConnection* get_connection() const;

    /**
     * @brief 分发消息
     * @constraint 处理队列中的待处理消息
     */
    void dispatch();

    /**
     * @brief 获取DBus连接的唯一名称
     * @return 唯一名称
     */
    std::string_view get_unique_name() const;

    /**
 	 * @brief 添加匹配规则，但不发送订阅信号
 	 * @param rule [in] 匹配规则
 	 * @param cb [in] 匹配成功时的回调函数
 	 * @param id [in] 规则的唯一标识符
 	 * @exception 添加失败时抛出异常
 	 */
 	void add_rule(match_rule& rule, match_cb_t&& cb, uint64_t id);

    /**
 	 * @brief 移除匹配规则，但不移除订阅信号
 	 * @param id [in] 规则的唯一标识符
 	 * @exception 移除失败时抛出异常
 	 */
 	void remove_rule(uint64_t id);

    /**
     * @brief 设置DBus连接的唯一名称
     * @param name [in] 唯一名称
     */
    void set_unique_name(std::string_view name);

    /**
     * @brief 添加匹配规则
     * @param rule [in] 匹配规则
     * @param cb [in] 匹配成功时的回调函数
     * @param id [in] 规则的唯一标识符
     * @exception 添加失败时抛出异常
     */
    void add_match(match_rule& rule, match_cb_t&& cb, uint64_t id);

    /**
 	 * @brief 不添加匹配规则，只发送订阅信号
 	 * @param rule [in] 匹配规则
 	 * @param cb [in] 匹配成功时的回调函数
 	 * @param id [in] 规则的唯一标识符
 	 * @exception 发送失败时抛出异常
 	 */
 	void add_match_only(match_rule& rule, match_cb_t&& cb, uint64_t id);

    /**
     * @brief 移除匹配规则
     * @param id [in] 规则的唯一标识符
     * @exception 移除失败时抛出异常
     */
    void remove_match(uint64_t id);

    /**
     * @brief 获取匹配对象
     * @return 返回匹配对象的引用
     */
    match& get_match();

    /**
     * @brief 获取下一个消息序列号
     * @return 返回下一个可用的消息序列号
     */
    uint32_t get_next_serial();

    /**
     * @brief 获取连接实现对象
     * @return 返回连接实现对象的引用
     */
    connection_impl& get_impl() const;

    /**
     * @brief 获取消息过滤信号对象
     * @return 返回消息过滤信号对象的引用
     * @constraint 用于注册消息过滤回调
     */
    filter_message_signal_type& filter_message() const;

    /**
     * @brief 获取连接的服务名
     * @return 返回连接的服务名
     */
    std::string get_service_name() const;

private:
    void ensure_impl() const;

    std::shared_ptr<connection_impl> m_impl;
};

} // namespace mc::dbus

#endif // MC_DBUS_CONNECTION_H