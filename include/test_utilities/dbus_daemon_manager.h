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

#ifndef MC_TEST_UTILITIES_DBUS_DAEMON_MANAGER_H
#define MC_TEST_UTILITIES_DBUS_DAEMON_MANAGER_H

#include <string>
#include <unistd.h>
#include <vector>

#include <dbus/dbus.h>
#include <mc/common.h>
#include <mc/filesystem.h>

namespace mc::test {

/**
 * @brief 用于管理测试中的 DBus 守护进程
 *
 * 这个类负责启动和关闭 DBus 守护进程，创建和清理相关的临时文件和目录
 */
class MC_API dbus_daemon_manager {
public:
    /**
     * @brief 构造函数
     */
    dbus_daemon_manager();

    /**
     * @brief 析构函数，自动清理资源
     */
    ~dbus_daemon_manager();

    /**
     * @brief 启动 DBus 守护进程
     *
     * @return 是否成功启动
     */
    bool start();

    /**
     * @brief 停止 DBus 守护进程
     */
    void stop();

    /**
     * @brief 获取 DBus 地址
     *
     * @return DBus 地址字符串
     */
    std::string get_address() const;

    /**
     * @brief 获取 DBus 套接字路径
     *
     * @return 套接字路径
     */
    mc::filesystem::path get_socket_path() const;

    /**
     * @brief 获取 DBus 配置文件路径
     *
     * @return 配置文件路径
     */
    mc::filesystem::path get_config_path() const;

    /**
     * @brief 获取临时目录路径
     *
     * @return 临时目录路径
     */
    mc::filesystem::path get_temp_dir() const;

private:
    /**
     * @brief 查找并尝试连接到现有的 DBus 守护进程
     *
     * @return 是否找到并成功连接到现有实例
     */
    bool find_existing_dbus();

    /**
     * @brief 扫描临时目录，查找所有可能存在的 DBus 测试目录
     *
     * @return 所有找到的 DBus 测试目录路径
     */
    std::vector<mc::filesystem::path> scan_dbus_test_dirs();

    /**
     * @brief 测试 DBus 连接是否可用
     *
     * @param socket_path 套接字路径
     * @return 连接是否可用
     */
    bool test_dbus_connection(const mc::filesystem::path& socket_path);

    /**
     * @brief 创建临时目录
     *
     * @return 是否成功创建
     */
    bool create_temp_dir();

    /**
     * @brief 创建 DBus 配置文件
     *
     * @return 是否成功创建
     */
    bool create_config_file();

    /**
     * @brief 清理临时目录
     */
    void cleanup_temp_dir();

    /**
     * @brief 清理历史遗留的 DBus 测试实例
     */
    void cleanup_stale_instances();

    /**
     * @brief 清理历史遗留的测试目录
     */
    void cleanup_stale_directories();

    /**
     * @brief 清理历史遗留的 DBus 守护进程
     */
    void cleanup_stale_processes();

    /**
     * @brief 创建 DBus 连接
     *
     * @param address DBus 地址字符串
     * @param error 错误信息
     * @return DBus 连接对象
     */
    DBusConnection* create_dbus_connection(const std::string& address, DBusError* error, int retry,
                                           int max_retries);

    /**
     * @brief 注册到 DBus 总线
     *
     * @param conn DBus 连接对象
     * @param error 错误信息
     * @return 是否成功注册
     */
    bool register_to_bus(DBusConnection* conn, DBusError* error, const std::string& socket_path);

    /**
     * @brief 关闭并释放 DBus 连接
     *
     * @param conn DBus 连接对象
     */
    void close_connection(DBusConnection* conn);

    pid_t                m_dbus_pid = -1;      // DBus 守护进程 PID
    mc::filesystem::path m_temp_dir;           // 临时目录路径
    mc::filesystem::path m_socket_path;        // DBus 套接字路径
    mc::filesystem::path m_config_path;        // DBus 配置文件路径
    std::string          m_dbus_address;       // DBus 地址
    bool                 m_is_running = false; // DBus 守护进程是否正在运行
};

} // namespace mc::test

#endif // MC_TEST_UTILITIES_DBUS_DAEMON_MANAGER_H