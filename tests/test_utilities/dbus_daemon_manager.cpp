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

#include "dbus_daemon_manager.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <sys/wait.h>
#include <thread>

#include <mc/log.h>

namespace mc::test {

dbus_daemon_manager::dbus_daemon_manager() {
    // 在构造函数中不自动启动进程，由用户显式调用 start()
}

dbus_daemon_manager::~dbus_daemon_manager() {
    // 在析构函数中自动停止进程
    stop();
}

bool dbus_daemon_manager::start() {
    if (m_is_running) {
        return true; // 已经在运行，无需再次启动
    }

    // 创建临时目录
    if (!create_temp_dir()) {
        return false;
    }

    // 创建配置文件
    if (!create_config_file()) {
        cleanup_temp_dir();
        return false;
    }

    // 创建管道用于捕获 dbus-daemon 的输出
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        elog("创建管道失败: ${error}", ("error", strerror(errno)));
        cleanup_temp_dir();
        return false;
    }

    // 启动 dbus-daemon 进程
    m_dbus_pid = fork();
    if (m_dbus_pid == -1) {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        elog("创建进程失败: ${error}", ("error", strerror(errno)));
        cleanup_temp_dir();
        return false;
    } else if (m_dbus_pid == 0) {
        // 子进程：执行 dbus-daemon
        close(pipe_fd[0]); // 关闭读端

        // 将标准输出重定向到管道
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[1]);

        // 执行 dbus-daemon
        execlp("dbus-daemon", "dbus-daemon", "--config-file", m_config_path.c_str(),
               "--nofork", // 添加 --nofork 选项，使进程不会后台运行
               "--print-address", nullptr);

        // 如果 execlp 失败，则输出错误并退出
        std::cerr << "执行 dbus-daemon 失败: " << strerror(errno) << std::endl;
        exit(1);
    } else {
        // 父进程
        close(pipe_fd[1]); // 关闭写端

        // 从管道读取 dbus-daemon 输出的地址
        char    buffer[256];
        ssize_t bytes_read = read(pipe_fd[0], buffer, sizeof(buffer) - 1);
        close(pipe_fd[0]);

        if (bytes_read > 0) {
            // 成功读取地址
            buffer[bytes_read] = '\0';
            m_dbus_address     = std::string(buffer);
            // 去除可能的换行符
            if (!m_dbus_address.empty() && m_dbus_address.back() == '\n') {
                m_dbus_address.pop_back();
            }
            ilog("使用 DBus 地址: ${address}", ("address", m_dbus_address));
        } else {
            // 读取失败，使用默认地址
            m_dbus_address = "unix:path=" + m_socket_path.string();
            wlog("无法从 dbus-daemon 读取地址，使用默认地址: ${address}",
                 ("address", m_dbus_address));
        }

        // 等待一小段时间，确保 dbus-daemon 已经启动
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        m_is_running = true;
        return true;
    }
}

void dbus_daemon_manager::stop() {
    if (!m_is_running || m_dbus_pid <= 0) {
        return; // 未在运行，无需停止
    }

    // 终止 dbus-daemon 进程
    ilog("正在终止 dbus-daemon 进程 (PID: ${pid})", ("pid", m_dbus_pid));
    kill(m_dbus_pid, SIGTERM);

    // 等待进程终止
    int status;
    waitpid(m_dbus_pid, &status, 0);

    if (WIFEXITED(status)) {
        ilog("dbus-daemon 进程正常退出，退出码: ${code}", ("code", WEXITSTATUS(status)));
    } else if (WIFSIGNALED(status)) {
        ilog("dbus-daemon 进程被信号终止，信号: ${signal}", ("signal", WTERMSIG(status)));
    }

    m_dbus_pid   = -1;
    m_is_running = false;

    // 清理临时目录
    cleanup_temp_dir();
}

std::string dbus_daemon_manager::get_address() const {
    return m_dbus_address;
}

std::filesystem::path dbus_daemon_manager::get_socket_path() const {
    return m_socket_path;
}

std::filesystem::path dbus_daemon_manager::get_config_path() const {
    return m_config_path;
}

std::filesystem::path dbus_daemon_manager::get_temp_dir() const {
    return m_temp_dir;
}

bool dbus_daemon_manager::create_temp_dir() {
    // 创建临时目录用于 DBus 套接字和配置文件
    m_temp_dir                    = std::filesystem::temp_directory_path() / "dbus_test_XXXXXX";
    std::string temp_dir_template = m_temp_dir.string();

    if (mkdtemp(&temp_dir_template[0]) == nullptr) {
        elog("创建临时目录失败: ${error}", ("error", strerror(errno)));
        return false;
    }

    m_temp_dir    = temp_dir_template;
    m_socket_path = m_temp_dir / "dbus.socket";
    m_config_path = m_temp_dir / "dbus.conf";

    return true;
}

bool dbus_daemon_manager::create_config_file() {
    // 创建 DBus 配置文件
    std::ofstream config_file(m_config_path);
    if (!config_file.is_open()) {
        elog("创建 DBus 配置文件失败: ${path}", ("path", m_config_path.string()));
        return false;
    }

    config_file << "<!DOCTYPE busconfig PUBLIC \"-//freedesktop//DTD D-Bus Bus Configuration "
                   "1.0//EN\"\n"
                << "\"http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd\">\n"
                << "<busconfig>\n"
                << "  <type>session</type>\n"
                << "  <listen>unix:path=" << m_socket_path.string() << "</listen>\n"
                << "  <auth>ANONYMOUS</auth>\n"
                << "  <allow_anonymous/>\n"
                << "  <policy context=\"default\">\n"
                << "    <allow send_destination=\"*\" eavesdrop=\"true\"/>\n"
                << "    <allow eavesdrop=\"true\"/>\n"
                << "    <allow own=\"*\"/>\n"
                << "  </policy>\n"
                << "</busconfig>\n";

    config_file.close();
    return true;
}

void dbus_daemon_manager::cleanup_temp_dir() {
    if (!m_temp_dir.empty() && std::filesystem::exists(m_temp_dir)) {
        try {
            std::filesystem::remove_all(m_temp_dir);
            ilog("已清理临时目录: ${dir}", ("dir", m_temp_dir.string()));
        } catch (const std::filesystem::filesystem_error& e) {
            wlog("清理临时目录失败: ${error}", ("error", e.what()));
        }
    }
}

} // namespace mc::test