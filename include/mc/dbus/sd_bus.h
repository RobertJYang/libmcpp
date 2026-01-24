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

#include <mc/dbus/connection.h>
#include <mc/runtime.h>

namespace mc::dbus {
/**
 * @brief sd_bus连接对象
 */
class MC_API sd_bus {
public:
    /**
     * @brief pcall方法的返回值类型
     * @details 第一个元素为错误信息（如果有），第二个元素为方法调用结果
     */
    using pcall_result = std::tuple<std::optional<std::string>, variant>;

    /**
     * @brief 构造函数
     * @param start_now [in] 是否立即启动连接，true表示构造后立即启动，false表示稍后手动启动
     * @param is_blocking [in] 是否为阻塞模式，true表示使用阻塞式DBus调用，false表示优先使用共享内存调用
     */
    sd_bus(bool start_now, bool is_blocking);

    /**
     * @brief 从已有连接构造
     * @param conn [in] D-Bus连接对象
     * @param is_blocking [in] 是否为阻塞模式，true表示使用阻塞式DBus调用，false表示优先使用共享内存调用
     */
    sd_bus(connection conn, bool is_blocking = false);

    /**
     * @brief 同步调用DBus方法（使用默认超时时间，10分钟）
     * @param service_name [in] 服务名称
     * @param path [in] 对象路径
     * @param interface [in] 接口名称
     * @param method [in] 方法名称
     * @param signature [in] 参数签名
     * @param args [in] 参数列表
     * @return 返回方法调用的结果
     * @throw mc::exception 调用失败时抛出异常
     * @note 自动在第一个参数（如果是字典）中添加Requestor字段
     */
    variant call(std::string_view service_name, std::string_view path, std::string_view interface,
                 std::string_view method, std::string_view signature, const variants& args);

    /**
     * @brief 同步调用DBus方法（返回错误信息而不抛出异常）
     * @param service_name [in] 服务名称
     * @param path [in] 对象路径
     * @param interface [in] 接口名称
     * @param method [in] 方法名称
     * @param signature [in] 参数签名
     * @param args [in] 参数列表
     * @return 返回元组，第一个元素为错误信息（如果有），第二个元素为方法调用结果
     * @note 不会抛出异常，错误信息通过返回值返回
     */
    pcall_result pcall(std::string_view service_name, std::string_view path, std::string_view interface,
                       std::string_view method, std::string_view signature, const variants& args);

    /**
     * @brief 带超时时间的同步调用DBus方法（抛出异常）
     * @param timeout [in] 超时时间
     * @param service_name [in] 服务名称
     * @param path [in] 对象路径
     * @param interface [in] 接口名称
     * @param method [in] 方法名称
     * @param signature [in] 参数签名
     * @param args [in] 参数列表
     * @return 返回方法调用的结果
     * @throw mc::exception 调用失败时抛出异常
     * @note 如果调用时间超过30秒，会记录警告日志
     */
    variant timeout_call(mc::milliseconds timeout, std::string_view service_name,
                         std::string_view path, std::string_view interface, std::string_view method,
                         std::string_view signature, const variants& args);

    /**
     * @brief 带超时时间的同步调用DBus方法（返回错误信息而不抛出异常）
     * @param timeout [in] 超时时间
     * @param service_name [in] 服务名称
     * @param path [in] 对象路径
     * @param interface [in] 接口名称
     * @param method [in] 方法名称
     * @param signature [in] 参数签名
     * @param args [in] 参数列表
     * @return 返回元组，第一个元素为错误信息（如果有），第二个元素为方法调用结果
     * @note 不会抛出异常，错误信息通过返回值返回。如果调用时间超过30秒，会记录警告日志
     */
    pcall_result timeout_pcall(mc::milliseconds timeout, std::string_view service_name,
                               std::string_view path, std::string_view interface, std::string_view method,
                               std::string_view signature, const variants& args);

    /**
     * @brief 通过共享内存调用DBus方法
     * @param timeout [in] 超时时间
     * @param service_name [in] 服务名称
     * @param path [in] 对象路径
     * @param interface [in] 接口名称
     * @param method [in] 方法名称
     * @param signature [in] 参数签名
     * @param args [in] 参数列表
     * @return 如果调用成功返回结果，失败或共享内存不可用返回std::nullopt
     * @note 仅通过共享内存进行调用，不会回退到普通DBus调用。适合需要高性能调用的场景
     */
    std::optional<variant> shm_timeout_call(mc::milliseconds timeout, std::string_view service_name,
                                            std::string_view path, std::string_view interface,
                                            std::string_view method, std::string_view signature,
                                            const variants& args);

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
     * @brief 检查是否为阻塞模式
     * @return 如果是阻塞模式返回 true，否则返回 false
     */
    bool is_blocking() const {
        return m_is_blocking;
    }

private:
    variant                timeout_call_impl(mc::milliseconds timeout, std::string_view service_name,
                                             std::string_view path, std::string_view interface, std::string_view method,
                                             std::string_view signature, const variants& args);
    variant                dbus_call(mc::milliseconds timeout, std::string_view service_name,
                                     std::string_view path, std::string_view interface, std::string_view method,
                                     std::string_view signature, const variants& args);
    std::optional<variant> devmon_chip_call(mc::milliseconds timeout,
                                            std::string_view path, std::string_view interface, std::string_view method,
                                            std::string_view signature, const variants& args);

    bool        m_is_blocking;
    bool        m_enable_local_request{true};
    connection  m_connection;
    std::string m_unique_name;
    std::string m_service_name;
};

} // namespace mc::dbus

#endif // MC_DBUS_SD_BUS_H
