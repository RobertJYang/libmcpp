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

    // 清理之前可能存在的测试实例
    cleanup_stale_instances();

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
            setenv("DBUS_SESSION_BUS_ADDRESS", m_dbus_address.c_str(), 1);
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
    // 先停止socat转发
    stop_socat_forward();

    if (!m_is_running || m_dbus_pid <= 0) {
        return; // 未在运行，无需停止
    }

    // 终止 dbus-daemon 进程
    dlog("正在终止 dbus-daemon 进程 (PID: ${pid})", ("pid", m_dbus_pid));
    kill(m_dbus_pid, SIGTERM);

    // 等待进程终止
    int status;
    waitpid(m_dbus_pid, &status, 0);

    if (WIFEXITED(status)) {
        dlog("dbus-daemon 进程正常退出，退出码: ${code}", ("code", WEXITSTATUS(status)));
    } else if (WIFSIGNALED(status)) {
        dlog("dbus-daemon 进程被信号终止，信号: ${signal}", ("signal", WTERMSIG(status)));
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
            dlog("已清理临时目录: ${dir}", ("dir", m_temp_dir.string()));
        } catch (const std::filesystem::filesystem_error& e) {
            wlog("清理临时目录失败: ${error}", ("error", e.what()));
        }
    }
}

void dbus_daemon_manager::cleanup_stale_instances() {
    dlog("开始清理旧的DBus测试实例...");

    // 清理旧的测试目录
    cleanup_stale_directories();

    // 清理旧的测试进程
    cleanup_stale_processes();

    dlog("DBus测试实例清理完成");
}

void dbus_daemon_manager::cleanup_stale_directories() {
    std::filesystem::path temp_dir = std::filesystem::temp_directory_path();
    try {
        for (const auto& entry : std::filesystem::directory_iterator(temp_dir)) {
            std::string name = entry.path().filename().string();
            if (name.find("dbus_test_") == 0 && std::filesystem::is_directory(entry.path())) {
                try {
                    std::error_code ec;
                    dlog("正在删除旧的测试目录: ${dir}", ("dir", entry.path().string()));
                    std::filesystem::remove_all(entry.path(), ec);
                    if (ec) {
                        wlog("无法删除旧的测试目录: ${dir} (${error})",
                             ("dir", entry.path().string())("error", ec.message()));
                    }
                } catch (const std::exception& e) {
                    wlog("清理旧测试目录时出错: ${error}", ("error", e.what()));
                }
            }
        }
    } catch (const std::exception& e) {
        wlog("扫描临时目录时出错: ${error}", ("error", e.what()));
    }
}

void dbus_daemon_manager::cleanup_stale_processes() {
    dlog("正在清理残留的测试dbus-daemon进程");

// 使用pgrep查找包含"dbus-daemon"和"dbus_test_"的进程，并尝试终止它们
#ifdef __APPLE__
    // macOS下的pgrep语法稍有不同
    std::string cmd = "pgrep -f 'dbus-daemon.*dbus_test_' | xargs -n1 kill 2>/dev/null || true";
#else
    // Linux下的pgrep语法
    std::string cmd = "pgrep -f 'dbus-daemon.*dbus_test_' | xargs -r kill 2>/dev/null || true";
#endif

    int ret = system(cmd.c_str());
    if (ret != 0) {
        // 系统调用可能会返回非零值，即使没有找到进程也是正常的
        // 这里不需要报错，因为我们使用了"|| true"来确保命令总是成功
        dlog("执行清理进程命令完成，返回值: ${ret}", ("ret", ret));
    }

// 清理相关的socat进程
// 查找可能是由我们的测试启动的socat进程（连接到dbus_test_目录中的套接字）
#ifdef __APPLE__
    std::string socat_cmd =
        "pgrep -f 'socat.*UNIX-CONNECT.*dbus_test_' | xargs -n1 kill 2>/dev/null || true";
#else
    std::string socat_cmd =
        "pgrep -f 'socat.*UNIX-CONNECT.*dbus_test_' | xargs -r kill 2>/dev/null || true";
#endif

    ret = system(socat_cmd.c_str());
    if (ret != 0) {
        dlog("执行清理socat进程命令完成，返回值: ${ret}", ("ret", ret));
    }
}
bool dbus_daemon_manager::start_socat_forward(uint16_t tcp_port) {
    if (m_forwarding) {
        wlog("socat转发已经在运行");
        return true;
    }

    if (!m_is_running) {
        elog("DBus守护进程未运行，无法启动转发");
        return false;
    }

    // 使用指定端口或默认端口
    m_tcp_port           = tcp_port ? tcp_port : 8089;
    std::string port_str = std::to_string(m_tcp_port);

    // 打印详细信息便于调试
    ilog("Socket路径: ${path}", ("path", m_socket_path.string()));
    ilog("转发端口: ${port}", ("port", m_tcp_port));

    // 创建更明确的转发规则，指定本地地址
    std::string tcp_listen   = "TCP-LISTEN:" + port_str + ",bind=127.0.0.1,reuseaddr";
    std::string unix_connect = "UNIX-CONNECT:" + m_socket_path.string();

    // 启动socat进程
    m_socat_pid = fork();
    if (m_socat_pid == -1) {
        elog("创建socat进程失败: ${error}", ("error", strerror(errno)));
        return false;
    } else if (m_socat_pid == 0) {
        // 子进程：执行socat，添加-d -d增加调试输出
        execlp("socat", "socat", "-v", "-d", "-d", tcp_listen.c_str(), unix_connect.c_str(),
               nullptr);

        // 如果执行失败，输出错误并退出
        std::cerr << "执行socat失败: " << strerror(errno) << std::endl;
        exit(1);
    }

    // 给socat启动一些时间
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 检查socat是否正在运行
    int   status;
    pid_t result = waitpid(m_socat_pid, &status, WNOHANG);
    if (result == m_socat_pid) {
        // socat已退出
        elog("socat进程意外退出，退出码: ${code}", ("code", WEXITSTATUS(status)));
        m_socat_pid = -1;
        return false;
    }

    m_forwarding = true;
    ilog("已启动socat转发: unix:${socket} -> tcp:127.0.0.1:${port}",
         ("socket", m_socket_path.string())("port", m_tcp_port));

    // 打印tcpdump命令建议
    ilog("建议使用的抓包命令: sudo tcpdump -i lo0 -n -vv -s0 host 127.0.0.1 and port ${port}",
         ("port", m_tcp_port));

    return true;
}

void dbus_daemon_manager::stop_socat_forward() {
    if (!m_forwarding || m_socat_pid <= 0) {
        return; // 未在运行，无需停止
    }

    // 终止socat进程
    dlog("正在终止socat进程 (PID: ${pid})", ("pid", m_socat_pid));
    kill(m_socat_pid, SIGTERM);

    // 等待进程终止
    int status;
    waitpid(m_socat_pid, &status, 0);

    if (WIFEXITED(status)) {
        dlog("socat进程正常退出，退出码: ${code}", ("code", WEXITSTATUS(status)));
    } else if (WIFSIGNALED(status)) {
        dlog("socat进程被信号终止，信号: ${signal}", ("signal", WTERMSIG(status)));
    }

    m_socat_pid  = -1;
    m_tcp_port   = 0;
    m_forwarding = false;

    dlog("socat转发已停止");
}

uint16_t dbus_daemon_manager::get_forward_tcp_port() const {
    return m_tcp_port;
}

} // namespace mc::test