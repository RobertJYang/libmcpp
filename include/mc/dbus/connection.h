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

#include <mc/dbus/transport/transport.h>
#include <mc/engine/engine.h>
#include <mc/io/io_buffer.h>
#include <mc/signal_slot.h>

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

/**
 * @brief DBus连接对象
 *
 * 处理与DBus服务器的连接和通信状态管理。
 */
class connection : public std::enable_shared_from_this<connection> {
public:
    using io_context_type  = boost::asio::io_context;
    using message_handler  = std::function<void(const std::shared_ptr<variants_message>&)>;
    using connect_handler  = std::function<void(const boost::system::error_code&)>;
    using message_map      = std::unordered_map<uint32_t, message_handler>;
    using buffer_type      = mc::io::io_buffer;
    using buffer_ptr_type  = std::unique_ptr<buffer_type>;
    using message_ptr_type = std::shared_ptr<variants_message>;

    /**
     * @brief 创建连接实例
     * @param io_context IO上下文
     * @return 连接对象智能指针
     */
    static std::shared_ptr<connection> create(io_context_type& io_context);

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
     * @param handler 连接完成回调
     *
     * 地址字符串格式：
     * method1:key1=value1,key2=value2;method2:key1=value1,key2=value2
     *
     * 支持的传输方式：
     * - unix:path=/path/to/socket
     * - unix:abstract=/tmp/abstract-socket
     * - tcp:host=hostname,port=1234
     */
    void connect(const std::string& address, connect_handler handler);

    /**
     * @brief 断开连接
     */
    void disconnect();

    /**
     * @brief 发送消息
     * @param msg 要发送的消息
     */
    template <typename T>
    bool send(const mc::dbus::message<T>& msg) {
        if (m_state.load() != connection_state::connected || !m_transport) {
            return false;
        }

        uint32_t serial = m_next_serial.fetch_add(1);
        msg.set_serial(serial);

        std::lock_guard lock(m_mutex);
        if (!m_is_writing) {
            m_write_stream << msg;
        } else {
            MC_THROW(mc::busy_exception, "当前正在写入消息，不能发送新消息");
        }
        start_write();
        return true;
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
    /**
     * @brief 设置传输层对象
     * @param transport 传输层对象
     */
    void set_transport(transport_ptr transport);

    /**
     * @brief 处理连接完成事件
     * @param ec 错误码
     * @param handler 用户定义的连接回调
     */
    void handle_connect(const boost::system::error_code& ec, connect_handler handler);

    /**
     * @brief 处理认证完成事件
     * @param ec 错误码
     */
    void handle_authenticate(const boost::system::error_code& ec);

    void start_read();
    void start_write();

    void handle_read(const boost::system::error_code& ec, std::size_t bytes_transferred);
    void handle_write(const boost::system::error_code& ec, std::size_t bytes_transferred);
    void process_message();
    bool read_message();

    io_context_type&              m_io_context;    ///< IO上下文引用
    transport_ptr                 m_transport;     ///< 传输层对象
    std::atomic<connection_state> m_state;         ///< 当前连接状态
    std::atomic<uint32_t>         m_next_serial;   ///< 下一个消息序列号
    message_map                   m_pending_calls; ///< 待处理的消息回调

    stream           m_read_stream; ///< 读取流对象
    message_ptr_type m_current_message;

    stream m_write_stream; ///< 写入流对象
    bool   m_is_writing{false};

    mutable std::mutex m_mutex; ///< 保护共享数据的互斥锁
};

} // namespace mc::dbus

#endif // MC_DBUS_CONNECTION_H