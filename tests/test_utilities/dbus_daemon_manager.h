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

#include <filesystem>
#include <string>
#include <unistd.h>

namespace mc::test {

/**
 * @brief 用于管理测试中的 DBus 守护进程
 *
 * 这个类负责启动和关闭 DBus 守护进程，创建和清理相关的临时文件和目录
 */
class dbus_daemon_manager {
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
    std::filesystem::path get_socket_path() const;

    /**
     * @brief 获取 DBus 配置文件路径
     *
     * @return 配置文件路径
     */
    std::filesystem::path get_config_path() const;

    /**
     * @brief 获取临时目录路径
     *
     * @return 临时目录路径
     */
    std::filesystem::path get_temp_dir() const;

private:
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

    pid_t                 m_dbus_pid = -1;      // DBus 守护进程 PID
    std::filesystem::path m_temp_dir;           // 临时目录路径
    std::filesystem::path m_socket_path;        // DBus 套接字路径
    std::filesystem::path m_config_path;        // DBus 配置文件路径
    std::string           m_dbus_address;       // DBus 地址
    bool                  m_is_running = false; // DBus 守护进程是否正在运行
};

} // namespace mc::test

#endif // MC_TEST_UTILITIES_DBUS_DAEMON_MANAGER_H