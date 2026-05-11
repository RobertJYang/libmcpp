/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <atomic>
#include <chrono>
#include <mc/dbus/bus_mode_impl.h>
#include <mc/dbus/message.h>
#include <mc/dbus/sd_bus.h>
#include <mc/dbus/signal.h>
#include <mc/log/log.h>

#if defined(MCDBUS_USE_OLD_SHM) && MCDBUS_USE_OLD_SHM
#include <mc/dbus/shm/harbor.h>
#endif

namespace mc::dbus {

// ==================== bus_mode_impl ====================

bus_mode_impl::~bus_mode_impl()
{}

void bus_mode_impl::init_connection(connection&& conn)
{
    m_connection  = std::move(conn);
    m_unique_name = std::string(m_connection.get_unique_name());
}

// ==================== blocking_bus_impl ====================

bool blocking_bus_impl::start()
{
    return m_connection.start();
}

void blocking_bus_impl::disconnect()
{
    m_connection.disconnect();
}

void blocking_bus_impl::flush()
{
    m_connection.flush();
}

void blocking_bus_impl::dispatch()
{
    m_connection.dispatch();
}

void blocking_bus_impl::request_name(const std::string& service_name, uint32_t flags)
{
    // 阻塞模式请求名称
    m_connection.request_name(service_name, flags);
    m_service_name = service_name;
}

uint64_t blocking_bus_impl::add_match(match_rule& rule, match_cb_t&& cb)
{
    elog("add_match failed: blocking mode does not support signal subscription");
    return 0;
}

void blocking_bus_impl::remove_match(uint64_t id)
{
    // 阻塞模式不支持信号订阅
}

bool blocking_bus_impl::run_once(int timeout_ms)
{
    if (!m_connection.is_connected()) {
        return false;
    }

    DBusConnection* raw_conn = m_connection.get_connection();

    // 阻塞等待消息，带超时
    dbus_connection_read_write(raw_conn, timeout_ms);

    // 取出一条消息
    DBusMessage* msg = dbus_connection_pop_message(raw_conn);
    if (!msg) {
        return false;
    }

    // 分发消息（通过内部的filter处理）
    m_connection.dispatch();

    // 释放消息
    dbus_message_unref(msg);

    return true;
}

bool blocking_bus_impl::run_until(std::function<bool()> condition, int total_timeout_ms, int step_ms)
{
    if (!m_connection.is_connected()) {
        return false;
    }

    auto start_time = std::chrono::steady_clock::now();
    auto timeout    = std::chrono::milliseconds(total_timeout_ms);

    while (true) {
        // 执行一次消息循环
        DBusConnection* raw_conn = m_connection.get_connection();
        dbus_connection_read_write(raw_conn, step_ms);
        DBusMessage* msg = dbus_connection_pop_message(raw_conn);
        if (msg) {
            m_connection.dispatch();
            dbus_message_unref(msg);
        }

        // 检查条件
        if (condition && condition()) {
            return true;
        }

        // 检查总超时时间
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed >= timeout) {
            return false;
        }
    }
}

void blocking_bus_impl::notify_signal(message& msg)
{
    // 阻塞模式直接通过连接发送信号（创建拷贝以保留原消息）
    message msg_copy(msg);
    m_connection.send(std::move(msg_copy));
}

shm_tree* blocking_bus_impl::get_shm_tree()
{
    return nullptr;
}

variants blocking_bus_impl::call(const std::string& service, const std::string& path, const std::string& interface,
                                 const std::string& method, const std::string& signature, variants&& args)
{
    // 直接使用 m_connection 发送消息
    auto               msg    = message::new_method_call(service, path, interface, method);
    auto               writer = msg.writer();
    signature_iterator it(signature);
    for (const auto& arg : args) {
        if (it.at_end()) {
            break;
        }
        writer.write_variant(it, arg, 0);
        it.next();
    }
    auto reply = m_connection.send_with_reply_and_block(std::move(msg));
    if (reply.is_valid() && reply.is_method_return()) {
        return reply.read_args();
    }
    std::string error_name(reply.get_error_name());
    std::string error_message = reply.get_error_message();

    // 使用error_message作为JSON字符串抛出error_exception
    throw mc::error_exception(error_name.c_str(), error_message);
}

variants blocking_bus_impl::timeout_call(int timeout_ms, const std::string& service, const std::string& path,
                                         const std::string& interface, const std::string& method,
                                         const std::string& signature, variants&& args)
{
    // 直接使用 m_connection 发送消息
    auto               msg    = message::new_method_call(service, path, interface, method);
    auto               writer = msg.writer();
    signature_iterator it(signature);
    for (const auto& arg : args) {
        if (it.at_end()) {
            break;
        }
        writer.write_variant(it, arg, 0);
        it.next();
    }
    auto reply = m_connection.send_with_reply_and_block(std::move(msg), mc::milliseconds(timeout_ms));
    if (reply.is_valid() && reply.is_method_return()) {
        return reply.read_args();
    }
    std::string error_name(reply.get_error_name());
    std::string error_message = reply.get_error_message();

    // 使用error_message作为JSON字符串抛出error_exception
    throw mc::error_exception(error_name.c_str(), error_message);
}

std::tuple<std::optional<std::string>, variants>
blocking_bus_impl::async_call(const std::string& service, const std::string& path, const std::string& interface,
                              const std::string& method, const std::string& signature, variants&& args)
{
    // 阻塞模式不支持异步调用，返回错误
    return {std::string("async_call is not supported in blocking mode"), variants{}};
}

std::tuple<std::optional<std::string>, variants>
blocking_bus_impl::async_timeout_call(int timeout_ms, const std::string& service, const std::string& path,
                                      const std::string& interface, const std::string& method,
                                      const std::string& signature, variants&& args)
{
    // 阻塞模式不支持异步调用，返回错误
    return {std::string("async_timeout_call is not supported in blocking mode"), variants{}};
}

std::tuple<std::optional<std::string>, variants>
blocking_bus_impl::async_shm_timeout_call(int timeout_ms, const std::string& service, const std::string& path,
                                          const std::string& interface, const std::string& method,
                                          const std::string& signature, variants&& args)
{
    // 阻塞模式不支持异步调用，返回错误
    return {std::string("async_shm_timeout_call is not supported in blocking mode"), variants{}};
}

// ==================== nonblocking_bus_impl ====================

nonblocking_bus_impl::~nonblocking_bus_impl()
{
#if defined(MCDBUS_USE_OLD_SHM) && MCDBUS_USE_OLD_SHM
    if (m_shm_tree != nullptr) {
        delete m_shm_tree;
        m_shm_tree = nullptr;
    }
#endif
}

bool nonblocking_bus_impl::start()
{
    return m_connection.start();
}

void nonblocking_bus_impl::disconnect()
{
    m_connection.disconnect();
}

void nonblocking_bus_impl::flush()
{
    m_connection.flush();
}

void nonblocking_bus_impl::dispatch()
{
    m_connection.dispatch();
}

void nonblocking_bus_impl::request_name(const std::string& service_name, uint32_t flags)
{
    // 先请求名称
    m_connection.request_name(service_name, flags);
    m_service_name = service_name;

#if defined(MCDBUS_USE_OLD_SHM) && MCDBUS_USE_OLD_SHM
    // 旧 shm 兼容模式：初始化 harbor 与 shm_tree，启用进程间共享内存加速
    auto& harbor = mc::dbus::harbor::get_instance();
    harbor.set_harbor_name_if_empty("harbor." + service_name);
    harbor.register_unique_name(m_unique_name, service_name);
    // shm_tree 构造函数内部会调用 create_shm_tree
    m_shm_tree = new shm_tree(harbor.get_harbor_name(), service_name, m_unique_name);
    harbor.start();
#endif
}

uint64_t nonblocking_bus_impl::add_match(match_rule& rule, match_cb_t&& cb)
{
#if defined(MCDBUS_USE_OLD_SHM) && MCDBUS_USE_OLD_SHM
    if (m_shm_tree == nullptr) {
        elog("add_match failed: shm_tree not initialized, call request_name first");
        return 0;
    }
    return m_shm_tree->add_match(rule, std::forward<match_cb_t>(cb));
#else
    // 无 shm_tree 兼容时，退化为通过 dbus connection 原生 add_match 注册规则；
    // id 由 connection 内部分配。
    static std::atomic<uint64_t> s_match_id_seed{1};
    auto                         id = s_match_id_seed.fetch_add(1, std::memory_order_relaxed);
    m_connection.add_match(rule, std::forward<match_cb_t>(cb), id);
    return id;
#endif
}

void nonblocking_bus_impl::remove_match(uint64_t id)
{
#if defined(MCDBUS_USE_OLD_SHM) && MCDBUS_USE_OLD_SHM
    if (m_shm_tree == nullptr) {
        return;
    }
    m_shm_tree->remove_match(id);
#else
    if (id == 0U) {
        return;
    }
    m_connection.remove_match(id);
#endif
}

bool nonblocking_bus_impl::run_once(int timeout_ms)
{
    elog("run_once is not supported in non-blocking mode");
    return false;
}

bool nonblocking_bus_impl::run_until(std::function<bool()> condition, int total_timeout_ms, int step_ms)
{
    elog("run_until is not supported in non-blocking mode");
    return false;
}

void nonblocking_bus_impl::notify_signal(message& msg)
{
    // 非阻塞模式通过共享内存发送信号
    mc::dbus::send_signal(m_connection, msg);
}

shm_tree* nonblocking_bus_impl::get_shm_tree()
{
#if defined(MCDBUS_USE_OLD_SHM) && MCDBUS_USE_OLD_SHM
    return m_shm_tree;
#else
    return nullptr;
#endif
}

variants nonblocking_bus_impl::call(const std::string& service, const std::string& path, const std::string& interface,
                                    const std::string& method, const std::string& signature, variants&& args)
{
    // 非阻塞模式不支持同步调用
    MC_THROW_ERROR_WITH_MESSAGE("InvalidOperation",
                                MC_LOG_MESSAGE(error, "call is not supported in non-blocking mode"));
}

variants nonblocking_bus_impl::timeout_call(int timeout_ms, const std::string& service, const std::string& path,
                                            const std::string& interface, const std::string& method,
                                            const std::string& signature, variants&& args)
{
    auto               msg    = message::new_method_call(service, path, interface, method);
    auto               writer = msg.writer();
    signature_iterator it(signature);
    for (auto arg : args) {
        if (it.at_end()) {
            break;
        }
        writer.write_variant(it, arg, 0);
        it.next();
    }
    auto reply = m_connection.send_with_reply(std::move(msg), mc::milliseconds(timeout_ms));
    if (reply.is_valid() && reply.is_method_return()) {
        return reply.read_args();
    }
    std::string error_name(reply.get_error_name());
    std::string error_message = reply.get_error_message();

    // 使用error_message作为JSON字符串抛出error_exception
    throw mc::error_exception(error_name.c_str(), error_message);
}

std::tuple<std::optional<std::string>, variants>
nonblocking_bus_impl::async_call(const std::string& service, const std::string& path, const std::string& interface,
                                 const std::string& method, const std::string& signature, variants&& args)
{
    // 直接调用 async_timeout_call，使用默认超时
    return async_timeout_call(60000, service, path, interface, method, signature, std::move(args));
}

std::tuple<std::optional<std::string>, variants>
nonblocking_bus_impl::async_timeout_call(int timeout_ms, const std::string& service, const std::string& path,
                                         const std::string& interface, const std::string& method,
                                         const std::string& signature, variants&& args)
{
    // 直接使用 m_connection 发送异步消息
    try {
        auto               msg    = message::new_method_call(service, path, interface, method);
        auto               writer = msg.writer();
        signature_iterator it(signature);
        for (const auto& arg : args) {
            if (it.at_end()) {
                break;
            }
            writer.write_variant(it, arg, 0);
            it.next();
        }
        // 使用 send_with_reply_and_block 实现同步调用（虽然名为 async，但实际是同步的）
        auto reply = m_connection.send_with_reply_and_block(std::move(msg), mc::milliseconds(timeout_ms));
        if (reply.is_valid() && reply.is_method_return()) {
            return {std::nullopt, reply.read_args()};
        }
        std::string error_name(reply.get_error_name());
        std::string error_message = reply.get_error_message();
        return {error_name + ": " + error_message, variants{}};
    } catch (const mc::exception& e) {
        return {e.what(), variants{}};
    }
}

std::tuple<std::optional<std::string>, variants>
nonblocking_bus_impl::async_shm_timeout_call(int timeout_ms, const std::string& service, const std::string& path,
                                             const std::string& interface, const std::string& method,
                                             const std::string& signature, variants&& args)
{
    // 非阻塞模式下的共享内存调用实际上是同步的
    // 这里简单地委托给 async_timeout_call
    // TODO: 实现真正的共享内存调用逻辑
    return async_timeout_call(timeout_ms, service, path, interface, method, signature, std::move(args));
}

} // namespace mc::dbus
