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
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <thread>

#include <mc/log.h>

namespace mc::test {

dbus_daemon_manager::dbus_daemon_manager() {
}

dbus_daemon_manager::~dbus_daemon_manager() {
    stop();
}

bool dbus_daemon_manager::start() {
    if (m_is_running) {
        return true;
    }

    // 首先尝试复用现有的 DBus 实例
    if (find_existing_dbus()) {
        ilog("复用现有的 DBus 实例，设置环境变量 \nexport DBUS_SESSION_BUS_ADDRESS=${address}",
             ("address", m_dbus_address));
        setenv("DBUS_SESSION_BUS_ADDRESS", m_dbus_address.c_str(), 1);
        m_is_running = true;
        return true;
    }

    // 如果没有可用的现有实例，清理不可用的实例然后创建新的
    dlog("未找到可用的 DBus 实例，将创建新实例");
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
            ilog("设置环境变量 \nexport DBUS_SESSION_BUS_ADDRESS=${address}",
                 ("address", m_dbus_address));
            setenv("DBUS_SESSION_BUS_ADDRESS", m_dbus_address.c_str(), 1);
        } else {
            // 读取失败，使用默认地址
            m_dbus_address = "unix:path=" + m_socket_path.string();
            wlog("无法从 dbus-daemon 读取地址，使用默认地址: ${address}",
                 ("address", m_dbus_address));
            setenv("DBUS_SESSION_BUS_ADDRESS", m_dbus_address.c_str(), 1);
        }

        // 等待一小段时间，确保 dbus-daemon 已经启动
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        m_is_running = true;
        return true;
    }
}

void dbus_daemon_manager::stop() {
    if (!m_is_running) {
        return; // 未在运行，无需停止
    }

    // 注意：我们不再终止 dbus-daemon 进程或清理文件，而是把它留给下一个测试程序使用
    dlog("保留 dbus-daemon 进程 (PID: ${pid}) 以供后续测试使用", ("pid", m_dbus_pid));

    // 重置实例状态，但不真正终止进程
    m_dbus_pid   = -1;
    m_is_running = false;
}

bool dbus_daemon_manager::find_existing_dbus() {
    dlog("正在查找可用的 DBus 实例...");

    // 扫描所有可能的DBus测试目录
    auto test_dirs = scan_dbus_test_dirs();

    for (const auto& dir : test_dirs) {
        mc::filesystem::path socket_path = dir / "dbus.socket";
        mc::filesystem::path config_path = dir / "dbus.conf";

        // 检查套接字和配置文件是否存在
        if (!mc::filesystem::exists(socket_path) || !mc::filesystem::exists(config_path)) {
            dlog("跳过目录 ${dir}：缺少必要文件", ("dir", dir.string()));
            continue;
        }

        // 测试连接是否可用
        if (!test_dbus_connection(socket_path)) {
            dlog("目录 ${dir} 中的 DBus 实例不可用", ("dir", dir.string()));
            continue;
        }

        // 找到可用的实例
        m_temp_dir     = dir;
        m_socket_path  = socket_path;
        m_config_path  = config_path;
        m_dbus_address = "unix:path=" + m_socket_path.string();

        dlog("找到可用的 DBus 实例，目录: ${dir}", ("dir", dir.string()));
        return true;
    }

    dlog("未找到可用的 DBus 实例");
    return false;
}

std::vector<mc::filesystem::path> dbus_daemon_manager::scan_dbus_test_dirs() {
    std::vector<mc::filesystem::path> result;
    mc::filesystem::path              temp_dir = mc::filesystem::temp_directory_path();

    try {
        for (const auto& entry : mc::filesystem::directory_iterator(temp_dir)) {
            std::string name = entry.path().filename().string();
            if (name.find("dbus_test_") == 0 && mc::filesystem::is_directory(entry.path())) {
                result.push_back(entry.path());
                dlog("发现可能的 DBus 测试目录: ${dir}", ("dir", entry.path().string()));
            }
        }
    } catch (const std::exception& e) {
        wlog("扫描临时目录时出错: ${error}", ("error", e.what()));
    }

    return result;
}

bool dbus_daemon_manager::test_dbus_connection(const mc::filesystem::path& socket_path) {
    // 尝试连接到套接字
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        wlog("创建套接字失败: ${error}", ("error", strerror(errno)));
        return false;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

    // 设置非阻塞模式进行连接测试
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    int  ret          = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    bool is_connected = (ret == 0 || (ret < 0 && errno == EINPROGRESS));

    close(sock);

    if (!is_connected) {
        wlog("连接到 DBus 套接字失败: ${path}, 错误: ${error}",
             ("path", socket_path.string())("error", strerror(errno)));
    } else {
        dlog("成功连接到 DBus 套接字: ${path}", ("path", socket_path.string()));
    }

    return is_connected;
}

std::string dbus_daemon_manager::get_address() const {
    return m_dbus_address;
}

mc::filesystem::path dbus_daemon_manager::get_socket_path() const {
    return m_socket_path;
}

mc::filesystem::path dbus_daemon_manager::get_config_path() const {
    return m_config_path;
}

mc::filesystem::path dbus_daemon_manager::get_temp_dir() const {
    return m_temp_dir;
}

bool dbus_daemon_manager::create_temp_dir() {
    // 创建临时目录用于 DBus 套接字和配置文件
    m_temp_dir                    = mc::filesystem::temp_directory_path() / "dbus_test_XXXXXX";
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
    if (!m_temp_dir.empty() && mc::filesystem::exists(m_temp_dir)) {
        try {
            mc::filesystem::remove_all(m_temp_dir);
            dlog("已清理临时目录: ${dir}", ("dir", m_temp_dir.string()));
        } catch (const mc::filesystem::filesystem_error& e) {
            wlog("清理临时目录失败: ${error}", ("error", e.what()));
        }
    }
}

void dbus_daemon_manager::cleanup_stale_instances() {
    dlog("开始清理不可用的DBus测试实例...");

    // 仅清理无法连接的实例和目录
    auto test_dirs = scan_dbus_test_dirs();
    for (const auto& dir : test_dirs) {
        mc::filesystem::path socket_path = dir / "dbus.socket";

        // 如果套接字文件不存在或者无法连接，则清理该目录
        if (!mc::filesystem::exists(socket_path) || !test_dbus_connection(socket_path)) {
            try {
                dlog("清理不可用的测试目录: ${dir}", ("dir", dir.string()));
                mc::filesystem::remove_all(dir);
            } catch (const std::exception& e) {
                wlog("清理目录时出错: ${error}", ("error", e.what()));
            }
        }
    }

    dlog("DBus测试实例清理完成");
}

void dbus_daemon_manager::cleanup_stale_directories() {
    mc::filesystem::path temp_dir = mc::filesystem::temp_directory_path();
    try {
        for (const auto& entry : mc::filesystem::directory_iterator(temp_dir)) {
            std::string name = entry.path().filename().string();
            if (name.find("dbus_test_") == 0 && mc::filesystem::is_directory(entry.path())) {
                // 检查目录中是否有socket文件，如果没有或者不可用，才删除
                mc::filesystem::path socket_path = entry.path() / "dbus.socket";
                if (!mc::filesystem::exists(socket_path) || !test_dbus_connection(socket_path)) {
                    try {
                        dlog("正在删除不可用的测试目录: ${dir}", ("dir", entry.path().string()));
                        mc::filesystem::remove_all(entry.path());
                    } catch (const std::exception& e) {
                        wlog("清理旧测试目录时出错: ${error}", ("error", e.what()));
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        wlog("扫描临时目录时出错: ${error}", ("error", e.what()));
    }
}

void dbus_daemon_manager::cleanup_stale_processes() {
    // 不要清理任何dbus-daemon进程，因为我们希望复用它们
    // 只有在清理不可用的实例时才会间接地清理对应的进程
    dlog("跳过清理dbus-daemon进程，保留进程以供后续测试使用");
}

} // namespace mc::test