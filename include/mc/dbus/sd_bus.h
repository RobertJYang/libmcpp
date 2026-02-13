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

#ifndef MC_DBUS_SD_BUS_H
#define MC_DBUS_SD_BUS_H

#include <functional>
#include <mc/dbus/connection.h>
#include <mc/dbus/dynamic_object.h>
#include <mc/dbus/match.h>
#include <mc/dbus/shm/shm_tree.h>
#include <mc/runtime.h>
#include <memory>

namespace mc::dbus {

// 前向声明
class bus_mode_impl;
/**
 * @brief sd_bus连接对象
 */
class MC_API sd_bus {
public:
    /**
     * @brief pcall方法的返回值类型
     * @details 第一个元素为错误信息（如果有），第二个元素为方法调用结果
     */
    using pcall_result = std::tuple<std::optional<std::string>, variants>;

    /**
     * @brief 构造函数
     * @param start_now [in] 是否立即启动连接，true表示构造后立即启动，false表示稍后手动启动
     * @param is_blocking [in] 是否为阻塞模式，true表示使用阻塞式DBus调用，false表示优先使用共享内存调用
     */
    sd_bus(bool start_now, bool is_blocking);

    /**
     * @brief 使用已有连接构造
     * @param conn [in] 已有的 D-Bus 连接对象
     * @param is_blocking [in] 是否为阻塞模式
     */
    sd_bus(connection conn, bool is_blocking = false);

    /**
     * @brief 析构函数
     */
    ~sd_bus();

    /**
     * @brief 移动构造函数
     */
    sd_bus(sd_bus&& other) noexcept;

    /**
     * @brief 移动赋值运算符
     */
    sd_bus& operator=(sd_bus&& other) noexcept;

    // 禁用拷贝
    sd_bus(const sd_bus&)            = delete;
    sd_bus& operator=(const sd_bus&) = delete;

    /**
     * @brief 同步调用DBus方法（使用默认超时时间，10分钟）
     * @param params [in] 方法调用参数
     * @return 返回方法调用的结果
     * @throw mc::exception 调用失败时抛出异常
     * @note 自动在第一个参数（如果是字典）中添加Requestor字段
     */
    variants call(const method_call_params& params);

    /**
     * @brief 同步调用DBus方法（返回错误信息而不抛出异常）
     * @param params [in] 方法调用参数
     * @return 返回元组，第一个元素为错误信息（如果有），第二个元素为方法调用结果
     * @note 不会抛出异常，错误信息通过返回值返回
     */
    pcall_result pcall(const method_call_params& params);

    /**
     * @brief 带超时时间的同步调用DBus方法（抛出异常）
     * @param timeout [in] 超时时间
     * @param params [in] 方法调用参数
     * @return 返回方法调用的结果
     * @throw mc::exception 调用失败时抛出异常
     * @note 如果调用时间超过30秒，会记录警告日志
     */
    variants timeout_call(mc::milliseconds timeout, const method_call_params& params);

    /**
     * @brief 带超时时间的同步调用DBus方法（返回错误信息而不抛出异常）
     * @param timeout [in] 超时时间
     * @param params [in] 方法调用参数
     * @return 返回元组，第一个元素为错误信息（如果有），第二个元素为方法调用结果
     * @note 不会抛出异常，错误信息通过返回值返回。如果调用时间超过30秒，会记录警告日志
     */
    pcall_result timeout_pcall(mc::milliseconds timeout, const method_call_params& params);

    /**
     * @brief 通过共享内存调用DBus方法
     * @param timeout [in] 超时时间
     * @param params [in] 方法调用参数
     * @return 如果调用成功返回结果，失败或共享内存不可用返回std::nullopt
     * @note 仅通过共享内存进行调用，不会回退到普通DBus调用。适合需要高性能调用的场景
     */
    std::optional<variants> shm_timeout_call(mc::milliseconds timeout, const method_call_params& params);

    /**
     * @brief 设置是否启用本地请求
     * @param enable [in] 是否启用本地请求
     */
    void set_enable_local_request(bool enable);

    /**
     * @brief 请求DBus服务名称
     * @param service_name [in] 要请求的服务名称
     * @param flags [in] 标志位，默认为0
     * @note 请求名称后，会自动注册到共享内存harbor（如果可用）。请求的名称会用于自动填充Requestor字段
     */
    void request_name(std::string_view service_name, uint32_t flags = 0);

    /**
     * @brief 获取底层连接对象
     * @return 返回底层连接对象的引用
     */
    connection& get_connection();

    /**
     * @brief 获取 bus_mode_impl 指针
     * @return bus_mode_impl 指针
     */
    bus_mode_impl* get_bus_mode_impl() {
        return m_bus.get();
    }

    /**
     * @brief 检查是否为阻塞模式
     * @return 如果是阻塞模式返回 true，否则返回 false
     */
    bool is_blocking() const {
        return m_is_blocking;
    }

    /**
     * @brief 添加匹配规则并订阅信号
     * @param rule [in] 匹配规则
     * @param cb [in] 回调函数
     * @return 返回分配的规则ID
     */
    uint64_t add_match(match_rule& rule, match_cb_t&& cb);

    /**
     * @brief 移除匹配规则并取消订阅信号
     * @param id [in] 规则ID
     */
    void remove_match(uint64_t id);

    /**
     * @brief 注册对象
     * @param object [in] 对象
     */
    void register_object(mc::shared_ptr<dynamic_object> object);

    /**
     * @brief 注销对象
     * @param path [in] 对象路径
     */
    void unregister_object(std::string_view path);

private:
    variants                timeout_call_impl(mc::milliseconds timeout, const method_call_params& params);
    variants                dbus_call(mc::milliseconds timeout, const method_call_params& params);
    std::optional<variants> reroute_call(mc::milliseconds timeout, const method_call_params& params);

    bool                                                  m_is_blocking;
    bool                                                  m_enable_local_request{true};
    std::unique_ptr<bus_mode_impl>                        m_bus;          // 阻塞式/非阻塞式总线实现（持有 connection）
    std::string                                           m_unique_name;  // 缓存的唯一名称
    std::string                                           m_service_name; // 缓存的服务名称
    std::map<std::string, mc::shared_ptr<dynamic_object>> m_objects;
};

} // namespace mc::dbus

#endif // MC_DBUS_SD_BUS_H
