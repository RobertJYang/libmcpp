/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gtest/gtest.h>
#include <mc/log.h>
#include <mc/log/appenders/socket_appender.h>
#include <mc/log/log_message.h>
#include <mc/variant.h>
#include <test_utilities/test_base.h>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

using namespace mc::log;

// mock get_log_time_str_c 函数
extern "C" {
[[maybe_unused]] static const char* get_log_time_str_c(int flags)
{
    static thread_local char time_buf[64];
    snprintf(time_buf, sizeof(time_buf), "1970-01-01 00:00:00");
    return time_buf;
}
}

class socket_appender_test : public mc::test::TestBase {
protected:
    void SetUp() override
    {
        mc::test::TestBase::SetUp();
        mc::log::default_logger().set_level(mc::log::level::off);

        // 创建测试用的 socket 路径
        m_test_dir = std::string(TEST_LOG_DIR) + "/socket_test";
        std::filesystem::create_directories(m_test_dir);

        m_socket_path    = m_test_dir + "/test.sock";
        m_hb_socket_path = m_test_dir + "/test.hbsock";

        // 清理可能存在的旧 socket 文件
        if (std::filesystem::exists(m_socket_path)) {
            std::filesystem::remove(m_socket_path);
        }
        if (std::filesystem::exists(m_hb_socket_path)) {
            std::filesystem::remove(m_hb_socket_path);
        }

        m_appender = std::make_shared<socket_appender>();
    }

    void TearDown() override
    {
        if (m_appender) {
            m_appender->disconnect();
        }
        m_appender.reset();

        // 清理测试 socket 文件
        if (std::filesystem::exists(m_socket_path)) {
            std::filesystem::remove(m_socket_path);
        }
        if (std::filesystem::exists(m_hb_socket_path)) {
            std::filesystem::remove(m_hb_socket_path);
        }
        if (std::filesystem::exists(m_test_dir)) {
            std::filesystem::remove_all(m_test_dir);
        }

        mc::test::TestBase::TearDown();
    }

    // 创建一个测试消息
    message create_test_message(level lvl, const std::string& msg, log_category category = log_category::debug)
    {
        mc::log::context ctx("test_file.cpp", "test_function", 123);
        message          msg_obj(lvl, msg, ctx);
        msg_obj.set_category(category);
        return msg_obj;
    }

    // 创建一个格式化测试消息
    message create_format_message(level lvl, const std::string& fmt, const mc::dict& args,
                                  log_category category = log_category::debug)
    {
        mc::log::context ctx("test_file.cpp", "test_function", 123);
        message          msg_obj(lvl, ctx, fmt, args);
        msg_obj.set_category(category);
        return msg_obj;
    }

    // 创建简单的 unix socket 服务器用于测试
    int create_test_socket_server(const std::string& path)
    {
        int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (server_fd < 0) {
            return -1;
        }

        struct sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

        // 删除可能存在的旧 socket 文件
        unlink(path.c_str());

        if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(server_fd);
            return -1;
        }

        if (listen(server_fd, 1) < 0) {
            close(server_fd);
            return -1;
        }

        return server_fd;
    }

    // 在后台线程中运行 socket 服务器
    void start_socket_server(const std::string& path, bool send_ack = true)
    {
        m_server_thread = std::thread([this, path, send_ack]() {
            int server_fd = create_test_socket_server(path);
            if (server_fd < 0) {
                return;
            }

            // 接受连接
            struct sockaddr_un client_addr;
            socklen_t          client_len = sizeof(client_addr);
            int                client_fd  = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd < 0) {
                close(server_fd);
                return;
            }

            // 接收数据
            char    buffer[4096];
            ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes > 0 && send_ack) {
                // 发送确认消息 "ok"
                send(client_fd, "ok", 2, 0);
            }

            close(client_fd);
            close(server_fd);
        });

        // 等待服务器启动
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // 同时启动主 socket 和心跳 socket 服务器
    void start_both_socket_servers(bool send_ack = true)
    {
        // 使用原子变量控制服务器线程退出
        m_server_running    = true;
        m_hb_server_running = true;

        // 启动主 socket 服务器
        m_server_thread = std::thread([this, send_ack]() {
            int server_fd = create_test_socket_server(m_socket_path);
            if (server_fd < 0) {
                m_server_running = false;
                return;
            }

            // 设置 socket 为非阻塞模式，避免 accept 无限等待
            int flags = fcntl(server_fd, F_GETFL, 0);
            fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

            // 轮询等待连接，最多等待 2 秒
            auto deadline  = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            int  client_fd = -1;
            while (m_server_running && std::chrono::steady_clock::now() < deadline) {
                struct sockaddr_un client_addr;
                socklen_t          client_len = sizeof(client_addr);
                client_fd                     = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
                if (client_fd >= 0) {
                    break;
                }
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            if (client_fd >= 0) {
                // 设置客户端 socket 为非阻塞，避免 recv 阻塞
                int flags = fcntl(client_fd, F_GETFL, 0);
                fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

                // 接收数据，使用轮询方式避免阻塞
                char    buffer[4096];
                ssize_t bytes         = 0;
                auto    recv_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
                while (std::chrono::steady_clock::now() < recv_deadline && m_server_running) {
                    bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
                    if (bytes > 0) {
                        break;
                    }
                    // 如果 recv 返回 0，表示连接已关闭
                    if (bytes == 0) {
                        break;
                    }
                    // 如果是 EAGAIN 或 EWOULDBLOCK，继续等待
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // 每次循环都检查退出标志
                        if (!m_server_running) {
                            break;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        continue;
                    }
                    // 其他错误，退出循环
                    break;
                }

                if (bytes > 0 && send_ack) {
                    // 恢复阻塞模式以便发送
                    fcntl(client_fd, F_SETFL, flags);
                    // 发送确认消息 "ok"
                    send(client_fd, "ok", 2, 0);
                    // 等待确认消息发送完成
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                close(client_fd);
            }

            close(server_fd);
            m_server_running = false;
        });

        // 启动心跳 socket 服务器
        m_hb_server_thread = std::thread([this, send_ack]() {
            int server_fd = create_test_socket_server(m_hb_socket_path);
            if (server_fd < 0) {
                m_hb_server_running = false;
                return;
            }

            // 设置 socket 为非阻塞模式
            int flags = fcntl(server_fd, F_GETFL, 0);
            fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

            // 轮询等待连接，最多等待 2 秒
            auto deadline  = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            int  client_fd = -1;
            while (m_hb_server_running && std::chrono::steady_clock::now() < deadline) {
                struct sockaddr_un client_addr;
                socklen_t          client_len = sizeof(client_addr);
                client_fd                     = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
                if (client_fd >= 0) {
                    break;
                }
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            if (client_fd >= 0) {
                // 设置客户端 socket 为非阻塞，避免 recv 阻塞
                int flags = fcntl(client_fd, F_GETFL, 0);
                fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

                // 接收心跳消息 "ok"，使用轮询方式避免阻塞
                char    buffer[10];
                ssize_t bytes         = 0;
                auto    recv_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
                while (std::chrono::steady_clock::now() < recv_deadline && m_hb_server_running) {
                    bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
                    if (bytes > 0) {
                        break;
                    }
                    // 如果 recv 返回 0，表示连接已关闭
                    if (bytes == 0) {
                        break;
                    }
                    // 如果是 EAGAIN 或 EWOULDBLOCK，继续等待
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // 每次循环都检查退出标志
                        if (!m_hb_server_running) {
                            break;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        continue;
                    }
                    // 其他错误，退出循环
                    break;
                }

                if (bytes > 0 && send_ack) {
                    // 恢复阻塞模式以便发送
                    fcntl(client_fd, F_SETFL, flags);
                    // 回显心跳消息
                    send(client_fd, "ok", 2, 0);
                    // 等待确认消息发送完成
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                close(client_fd);
            }

            close(server_fd);
            m_hb_server_running = false;
        });

        // 等待两个服务器都启动
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    void stop_both_socket_servers()
    {
        // 设置标志让服务器线程退出
        m_server_running    = false;
        m_hb_server_running = false;

        // 等待一小段时间让线程检测到退出标志
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        // 直接 join，如果线程已经退出，join 会立即返回
        if (m_server_thread.joinable()) {
            m_server_thread.join();
        }

        if (m_hb_server_thread.joinable()) {
            m_hb_server_thread.join();
        }
    }

    void stop_socket_server()
    {
        m_server_running = false;
        if (m_server_thread.joinable()) {
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            while (m_server_running && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (m_server_thread.joinable()) {
                m_server_thread.join();
            }
        }
    }

    std::shared_ptr<socket_appender> m_appender;
    std::string                      m_test_dir;
    std::string                      m_socket_path;
    std::string                      m_hb_socket_path;
    std::thread                      m_server_thread;
    std::thread                      m_hb_server_thread;
    std::atomic<bool>                m_server_running{false};
    std::atomic<bool>                m_hb_server_running{false};
};

// 测试默认构造函数
TEST_F(socket_appender_test, DefaultConstructor)
{
    ASSERT_NE(m_appender, nullptr);
    EXPECT_FALSE(m_appender->is_connected());
    EXPECT_EQ(m_appender->get_path(), "");
    EXPECT_EQ(m_appender->get_type(), "file");
}

// 测试 init 函数 - 有效配置
TEST_F(socket_appender_test, InitWithValidConfig)
{
    mc::dict dict;
    dict["path"]        = m_socket_path;
    dict["hb_path"]     = m_hb_socket_path;
    dict["module_name"] = "test_module";
    dict["name"]        = "test_socket_appender";

    EXPECT_TRUE(m_appender->init(dict));
    EXPECT_EQ(m_appender->get_name(), "test_socket_appender");
}

// 测试 init 函数 - 缺少 path
TEST_F(socket_appender_test, InitWithoutPath)
{
    mc::dict dict;
    dict["hb_path"] = m_hb_socket_path;

    EXPECT_FALSE(m_appender->init(dict));
}

// 测试 init 函数 - 缺少 hb_path
TEST_F(socket_appender_test, InitWithoutHbPath)
{
    mc::dict dict;
    dict["path"] = m_socket_path;

    EXPECT_FALSE(m_appender->init(dict));
}

// 测试 init 函数 - 非对象参数
TEST_F(socket_appender_test, InitWithNonObjectArgs)
{
    mc::variant non_object = 42;
    EXPECT_FALSE(m_appender->init(non_object));
}

// 测试 set_path 和 get_path
TEST_F(socket_appender_test, SetAndGetPath)
{
    std::string test_path = "/tmp/test_socket.sock";
    m_appender->set_path(test_path);
    EXPECT_EQ(m_appender->get_path(), test_path);
}

// 测试 set_hb_path
TEST_F(socket_appender_test, SetHbPath)
{
    std::string test_hb_path = "/tmp/test_hb_socket.sock";
    m_appender->set_hb_path(test_hb_path);
    // 注意：socket_appender 没有 get_hb_path 方法，所以只能测试设置不抛异常
    ASSERT_NO_THROW(m_appender->set_hb_path(test_hb_path));
}

// 测试 set_type 和 get_type
TEST_F(socket_appender_test, SetAndGetType)
{
    EXPECT_EQ(m_appender->get_type(), "file");

    m_appender->set_type("local");
    EXPECT_EQ(m_appender->get_type(), "local");

    m_appender->set_type("file");
    EXPECT_EQ(m_appender->get_type(), "file");
}

// 测试 connect - 无服务器时连接失败
TEST_F(socket_appender_test, ConnectWithoutServer)
{
    m_appender->set_path(m_socket_path);
    m_appender->set_hb_path(m_hb_socket_path);

    // 没有服务器，连接应该失败
    EXPECT_FALSE(m_appender->connect());
    EXPECT_FALSE(m_appender->is_connected());
}

// 测试 connect - 有服务器时连接成功
TEST_F(socket_appender_test, ConnectWithServer)
{
    start_both_socket_servers();

    m_appender->set_path(m_socket_path);
    m_appender->set_hb_path(m_hb_socket_path);

    // 有服务器，连接应该成功
    EXPECT_TRUE(m_appender->connect());
    EXPECT_TRUE(m_appender->is_connected());

    stop_both_socket_servers();
}

// 测试 disconnect
TEST_F(socket_appender_test, Disconnect)
{
    start_both_socket_servers();

    m_appender->set_path(m_socket_path);
    m_appender->set_hb_path(m_hb_socket_path);
    m_appender->connect();

    EXPECT_TRUE(m_appender->is_connected());

    // 等待一小段时间确保连接稳定
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    m_appender->disconnect();
    EXPECT_FALSE(m_appender->is_connected());

    // 等待一小段时间确保服务器检测到断开
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    stop_both_socket_servers();
}

// 测试 heartbeat - 无服务器时失败
TEST_F(socket_appender_test, HeartbeatWithoutServer)
{
    m_appender->set_path(m_socket_path);
    m_appender->set_hb_path(m_hb_socket_path);

    // 没有服务器，心跳应该失败
    EXPECT_FALSE(m_appender->heartbeat());
}

// 测试 append - type 不是 "local" 时应该不发送
TEST_F(socket_appender_test, AppendWithNonLocalType)
{
    m_appender->set_type("file");
    m_appender->set_path(m_socket_path);
    m_appender->set_hb_path(m_hb_socket_path);

    auto msg = create_test_message(level::debug, "测试消息", log_category::debug);

    // type 不是 "local"，应该不发送，不会抛异常
    ASSERT_NO_THROW(m_appender->append(msg));
}

// 测试 append - category 不是 debug 时应该不发送
TEST_F(socket_appender_test, AppendWithNonDebugCategory)
{
    m_appender->set_type("local");
    m_appender->set_path(m_socket_path);
    m_appender->set_hb_path(m_hb_socket_path);

    auto msg = create_test_message(level::info, "测试消息", log_category::operation);

    // category 不是 debug，应该不发送，不会抛异常
    ASSERT_NO_THROW(m_appender->append(msg));
}

// 测试 append - mdbctl 类别不受 m_type 限制，type 为 "file" 时也应发送
TEST_F(socket_appender_test, AppendMdbctlCategoryWithFileType)
{
    start_both_socket_servers();

    m_appender->set_type("file");
    m_appender->set_path(m_socket_path);
    m_appender->set_hb_path(m_hb_socket_path);
    m_appender->connect();

    auto msg = create_test_message(level::debug, "mdbctl_log 消息", log_category::mdbctl);

    // mdbctl 类别不受 m_type 限制，应发送
    ASSERT_NO_THROW(m_appender->append(msg));

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    stop_both_socket_servers();
}

// 测试 append - type 为 "local" 且 category 为 debug 时发送
TEST_F(socket_appender_test, AppendWithLocalTypeAndDebugCategory)
{
    start_both_socket_servers();

    m_appender->set_type("local");
    m_appender->set_path(m_socket_path);
    m_appender->set_hb_path(m_hb_socket_path);
    m_appender->connect();

    auto msg = create_test_message(level::debug, "测试消息", log_category::debug);

    // type 为 "local" 且 category 为 debug，应该发送
    ASSERT_NO_THROW(m_appender->append(msg));

    // 等待消息发送完成
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    stop_both_socket_servers();
}

// 测试 append - 连接断开后重新连接并发送
TEST_F(socket_appender_test, AppendWithReconnect)
{
    start_both_socket_servers();

    m_appender->set_type("local");
    m_appender->set_path(m_socket_path);
    m_appender->set_hb_path(m_hb_socket_path);
    m_appender->connect();

    auto msg1 = create_test_message(level::debug, "第一条消息", log_category::debug);
    ASSERT_NO_THROW(m_appender->append(msg1));

    // 断开连接
    m_appender->disconnect();
    stop_both_socket_servers();

    // 重新启动服务器
    start_both_socket_servers();

    // 再次发送，应该自动重连
    auto msg2 = create_test_message(level::debug, "第二条消息", log_category::debug);
    ASSERT_NO_THROW(m_appender->append(msg2));

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    stop_both_socket_servers();
}

// 测试 append - 格式化消息
TEST_F(socket_appender_test, AppendFormattedMessage)
{
    start_both_socket_servers();

    m_appender->set_type("local");
    m_appender->set_path(m_socket_path);
    m_appender->set_hb_path(m_hb_socket_path);
    m_appender->connect();

    mc::dict args;
    args["name"]  = "测试";
    args["value"] = 42;

    auto msg = create_format_message(level::debug, "名称: ${name}, 值: ${value}", args, log_category::debug);

    ASSERT_NO_THROW(m_appender->append(msg));

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    stop_both_socket_servers();
}

// 测试 append - 空消息
TEST_F(socket_appender_test, AppendEmptyMessage)
{
    start_both_socket_servers();

    m_appender->set_type("local");
    m_appender->set_path(m_socket_path);
    m_appender->set_hb_path(m_hb_socket_path);
    m_appender->connect();

    auto msg = create_test_message(level::debug, "", log_category::debug);

    ASSERT_NO_THROW(m_appender->append(msg));

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    stop_both_socket_servers();
}

// 测试 append - 不同日志级别
TEST_F(socket_appender_test, AppendDifferentLogLevels)
{
    start_both_socket_servers();

    m_appender->set_type("local");
    m_appender->set_path(m_socket_path);
    m_appender->set_hb_path(m_hb_socket_path);
    m_appender->connect();

    auto debug_msg = create_test_message(level::debug, "调试消息", log_category::debug);
    auto info_msg  = create_test_message(level::info, "信息消息", log_category::debug);
    auto warn_msg  = create_test_message(level::warn, "警告消息", log_category::debug);
    auto error_msg = create_test_message(level::error, "错误消息", log_category::debug);

    ASSERT_NO_THROW(m_appender->append(debug_msg));
    ASSERT_NO_THROW(m_appender->append(info_msg));
    ASSERT_NO_THROW(m_appender->append(warn_msg));
    ASSERT_NO_THROW(m_appender->append(error_msg));

    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    stop_both_socket_servers();
}

// 测试 get_client
TEST_F(socket_appender_test, GetClient)
{
    auto& client = m_appender->get_client();
    ASSERT_NO_THROW(client.is_connected());
}

// 测试多次连接和断开
TEST_F(socket_appender_test, MultipleConnectDisconnect)
{
    m_appender->set_path(m_socket_path);
    m_appender->set_hb_path(m_hb_socket_path);

    for (int i = 0; i < 3; ++i) {
        // 确保前一次的服务器线程已经完全退出
        stop_both_socket_servers();
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

        // 确保socket文件被删除
        if (std::filesystem::exists(m_socket_path)) {
            std::filesystem::remove(m_socket_path);
        }
        if (std::filesystem::exists(m_hb_socket_path)) {
            std::filesystem::remove(m_hb_socket_path);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        start_both_socket_servers();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        EXPECT_TRUE(m_appender->connect());
        EXPECT_TRUE(m_appender->is_connected());

        m_appender->disconnect();
        EXPECT_FALSE(m_appender->is_connected());

        // 等待一小段时间确保服务器处理完连接
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    // 最后清理
    stop_both_socket_servers();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

// 测试 init 后设置路径并连接
TEST_F(socket_appender_test, InitThenConnect)
{
    mc::dict dict;
    dict["path"]        = m_socket_path;
    dict["hb_path"]     = m_hb_socket_path;
    dict["module_name"] = "test_module";

    EXPECT_TRUE(m_appender->init(dict));

    start_both_socket_servers();

    EXPECT_TRUE(m_appender->connect());
    EXPECT_TRUE(m_appender->is_connected());

    stop_both_socket_servers();
}

// 测试空路径时的连接
TEST_F(socket_appender_test, ConnectWithEmptyPath)
{
    m_appender->set_path("");
    m_appender->set_hb_path("");

    EXPECT_FALSE(m_appender->connect());
    EXPECT_FALSE(m_appender->is_connected());
}
