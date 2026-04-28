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

#ifndef MC_DBUS_BUS_MODE_IMPL_H
#define MC_DBUS_BUS_MODE_IMPL_H

#include <functional>
#include <mc/dbus/connection.h>
#include <mc/dbus/match.h>
#include <mc/dbus/message.h>
#include <mc/variant.h>
#include <memory>
#include <optional>
#include <string>
#include <tuple>

#if defined(MCDBUS_USE_OLD_SHM) && MCDBUS_USE_OLD_SHM
#include <mc/dbus/shm/shm_tree.h>
#endif

namespace mc::dbus {

#if !defined(MCDBUS_USE_OLD_SHM) || !MCDBUS_USE_OLD_SHM
// use_old_shm=false 时不再编译 shm_tree 实现；
// 仍前向声明该类型以保持 get_shm_tree() 签名稳定（运行时永远返回 nullptr）。
class shm_tree;
#endif

/**
 * @brief 总线模式实现基类
 * @details 用于区分阻塞式和非阻塞式 DBus 连接的不同行为
 */
class __attribute__((visibility("default"))) bus_mode_impl {
protected:
    connection  m_connection;   // D-Bus 连接
    std::string m_unique_name;  // 唯一名称
    std::string m_service_name; // 服务名称

public:
    virtual ~bus_mode_impl();

    /**
     * @brief 初始化连接
     * @param conn [in] 连接对象（移动语义）
     */
    void init_connection(connection&& conn);

    /**
     * @brief 获取连接对象
     */
    connection& get_connection()
    {
        return m_connection;
    }

    /**
     * @brief 获取唯一名称
     */
    std::string_view get_unique_name() const
    {
        return m_unique_name;
    }

    /**
     * @brief 获取服务名称
     */
    std::string_view get_service_name() const
    {
        return m_service_name;
    }

    /**
     * @brief 启动连接
     * @return 启动成功返回 true，失败返回 false
     */
    virtual bool start() = 0;

    /**
     * @brief 关闭连接
     */
    virtual void disconnect() = 0;

    /**
     * @brief 刷新连接
     */
    virtual void flush() = 0;

    /**
     * @brief 分发消息
     */
    virtual void dispatch() = 0;

    /**
     * @brief 请求服务名称
     * @param service_name [in] 服务名称
     * @param flags [in] 标志位
     */
    virtual void request_name(const std::string& service_name, uint32_t flags) = 0;

    /**
     * @brief 添加匹配规则
     * @param rule [in] 匹配规则
     * @param cb [in] 回调函数
     * @return 规则ID，0表示不支持或失败
     */
    virtual uint64_t add_match(match_rule& rule, match_cb_t&& cb) = 0;

    /**
     * @brief 移除匹配规则
     * @param id [in] 规则ID
     */
    virtual void remove_match(uint64_t id) = 0;

    /**
     * @brief 阻塞等待一次消息
     * @param timeout_ms [in] 超时时间（毫秒）
     * @return 是否成功处理了消息
     */
    virtual bool run_once(int timeout_ms) = 0;

    /**
     * @brief 阻塞等待直到条件满足
     * @param condition [in] 条件函数
     * @param total_timeout_ms [in] 总超时时间（毫秒）
     * @param step_ms [in] 每次等待步长（毫秒）
     * @return 条件是否满足
     */
    virtual bool run_until(std::function<bool()> condition, int total_timeout_ms, int step_ms) = 0;

    /**
     * @brief 发送信号
     * @param msg [in] 消息
     */
    virtual void notify_signal(message& msg) = 0;

    /**
     * @brief 获取 shm_tree 对象（仅非阻塞模式有效）
     * @return shm_tree 指针，阻塞模式返回 nullptr
     */
    virtual shm_tree* get_shm_tree() = 0;

    /**
     * @brief 同步调用 D-Bus 方法（阻塞模式）
     * @param service [in] 服务名
     * @param path [in] 对象路径
     * @param interface [in] 接口名
     * @param method [in] 方法名
     * @param signature [in] 参数签名
     * @param args [in] 参数列表
     * @return 返回值列表
     */
    virtual variants call(const std::string& service, const std::string& path, const std::string& interface,
                          const std::string& method, const std::string& signature, variants&& args) = 0;

    /**
     * @brief 带超时的同步调用 D-Bus 方法（阻塞模式）
     * @param conn [in] 连接对象
     * @param timeout_ms [in] 超时时间（毫秒）
     * @param service [in] 服务名
     * @param path [in] 对象路径
     * @param interface [in] 接口名
     * @param method [in] 方法名
     * @param signature [in] 参数签名
     * @param args [in] 参数列表
     * @return 返回值列表
     */
    virtual variants timeout_call(int timeout_ms, const std::string& service, const std::string& path,
                                  const std::string& interface, const std::string& method, const std::string& signature,
                                  variants&& args) = 0;

    /**
     * @brief 异步调用 D-Bus 方法（非阻塞模式）
     * @param service [in] 服务名
     * @param path [in] 对象路径
     * @param interface [in] 接口名
     * @param method [in] 方法名
     * @param signature [in] 参数签名
     * @param args [in] 参数列表
     * @return (error, result) 错误信息和返回值
     */
    virtual std::tuple<std::optional<std::string>, variants>
    async_call(const std::string& service, const std::string& path, const std::string& interface,
               const std::string& method, const std::string& signature, variants&& args) = 0;

    /**
     * @brief 带超时的异步调用 D-Bus 方法（非阻塞模式）
     * @param timeout_ms [in] 超时时间（毫秒）
     * @param service [in] 服务名
     * @param path [in] 对象路径
     * @param interface [in] 接口名
     * @param method [in] 方法名
     * @param signature [in] 参数签名
     * @param args [in] 参数列表
     * @return (error, result) 错误信息和返回值
     */
    virtual std::tuple<std::optional<std::string>, variants>
    async_timeout_call(int timeout_ms, const std::string& service, const std::string& path,
                       const std::string& interface, const std::string& method, const std::string& signature,
                       variants&& args) = 0;

    /**
     * @brief 通过共享内存的异步调用（非阻塞模式）
     * @param timeout_ms [in] 超时时间（毫秒）
     * @param service [in] 服务名
     * @param path [in] 对象路径
     * @param interface [in] 接口名
     * @param method [in] 方法名
     * @param signature [in] 参数签名
     * @param args [in] 参数列表
     * @return (error, result) 错误信息和返回值
     */
    virtual std::tuple<std::optional<std::string>, variants>
    async_shm_timeout_call(int timeout_ms, const std::string& service, const std::string& path,
                           const std::string& interface, const std::string& method, const std::string& signature,
                           variants&& args) = 0;
};

/**
 * @brief 阻塞式总线实现
 */
class __attribute__((visibility("default"))) blocking_bus_impl : public bus_mode_impl {
public:
    bool start() override;
    void disconnect() override;
    void flush() override;
    void dispatch() override;
    void request_name(const std::string& service_name, uint32_t flags) override;

    uint64_t add_match(match_rule& rule, match_cb_t&& cb) override;

    void remove_match(uint64_t id) override;

    bool run_once(int timeout_ms) override;

    bool run_until(std::function<bool()> condition, int total_timeout_ms, int step_ms) override;

    void notify_signal(message& msg) override;

    shm_tree* get_shm_tree() override;

    variants call(const std::string& service, const std::string& path, const std::string& interface,
                  const std::string& method, const std::string& signature, variants&& args) override;

    variants timeout_call(int timeout_ms, const std::string& service, const std::string& path,
                          const std::string& interface, const std::string& method, const std::string& signature,
                          variants&& args) override;

    std::tuple<std::optional<std::string>, variants> async_call(const std::string& service, const std::string& path,
                                                                const std::string& interface, const std::string& method,
                                                                const std::string& signature, variants&& args) override;

    std::tuple<std::optional<std::string>, variants>
    async_timeout_call(int timeout_ms, const std::string& service, const std::string& path,
                       const std::string& interface, const std::string& method, const std::string& signature,
                       variants&& args) override;

    std::tuple<std::optional<std::string>, variants>
    async_shm_timeout_call(int timeout_ms, const std::string& service, const std::string& path,
                           const std::string& interface, const std::string& method, const std::string& signature,
                           variants&& args) override;
};

/**
 * @brief 非阻塞式总线实现
 */
class __attribute__((visibility("default"))) nonblocking_bus_impl : public bus_mode_impl {
public:
    ~nonblocking_bus_impl() override;

    bool start() override;
    void disconnect() override;
    void flush() override;
    void dispatch() override;
    void request_name(const std::string& service_name, uint32_t flags) override;

    uint64_t add_match(match_rule& rule, match_cb_t&& cb) override;

    void remove_match(uint64_t id) override;

    bool run_once(int timeout_ms) override;

    bool run_until(std::function<bool()> condition, int total_timeout_ms, int step_ms) override;

    void notify_signal(message& msg) override;

    shm_tree* get_shm_tree() override;

    variants call(const std::string& service, const std::string& path, const std::string& interface,
                  const std::string& method, const std::string& signature, variants&& args) override;

    variants timeout_call(int timeout_ms, const std::string& service, const std::string& path,
                          const std::string& interface, const std::string& method, const std::string& signature,
                          variants&& args) override;

    std::tuple<std::optional<std::string>, variants> async_call(const std::string& service, const std::string& path,
                                                                const std::string& interface, const std::string& method,
                                                                const std::string& signature, variants&& args) override;

    std::tuple<std::optional<std::string>, variants>
    async_timeout_call(int timeout_ms, const std::string& service, const std::string& path,
                       const std::string& interface, const std::string& method, const std::string& signature,
                       variants&& args) override;

    std::tuple<std::optional<std::string>, variants>
    async_shm_timeout_call(int timeout_ms, const std::string& service, const std::string& path,
                           const std::string& interface, const std::string& method, const std::string& signature,
                           variants&& args) override;

private:
#if defined(MCDBUS_USE_OLD_SHM) && MCDBUS_USE_OLD_SHM
    shm_tree* m_shm_tree{nullptr};
#endif
};

} // namespace mc::dbus

#endif // MC_DBUS_BUS_MODE_IMPL_H
