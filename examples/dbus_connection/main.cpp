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

#include <mc/dbus/connection.h>
#include <mc/dbus/message.h>
#include <mc/log.h>

#include <boost/asio.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

// 测试函数：创建一个DBus连接
void test_dbus_connection() {
    try {
        // 创建IO上下文
        boost::asio::io_context io_context;

        // 创建工作守卫，防止IO上下文在没有操作时退出
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard =
            boost::asio::make_work_guard(io_context);

        // 创建IO线程
        std::thread io_thread([&io_context]() {
            ilog("IO线程启动");
            io_context.run();
            ilog("IO线程退出");
        });

        // 创建连接对象
        auto connection = mc::dbus::connection::create(io_context);

        // 连接到系统DBus
        // 标准D-Bus地址格式: unix:path=/var/run/dbus/system_bus_socket
        // 也可以连接到会话总线: unix:abstract=/tmp/dbus-*
        // 或者远程总线: tcp:host=example.com,port=1234
        std::string address = "unix:path=/var/run/dbus/system_bus_socket";
        ilog("正在连接到DBus: ${address}", ("address", address));

        connection->connect(address, [connection](const boost::system::error_code& ec) {
            if (ec) {
                elog("连接失败: ${error}", ("error", ec.message()));
            } else {
                ilog("连接成功");

                // 发送一个简单的Hello消息
                auto msg         = std::make_shared<mc::dbus::variants_message>();
                msg->type        = mc::dbus::message_type::method_call;
                msg->path        = "/org/freedesktop/DBus";
                msg->interface   = "org.freedesktop.DBus";
                msg->member      = "Hello";
                msg->destination = "org.freedesktop.DBus";

                // 发送消息并等待回复
                connection->send(msg, [](const std::shared_ptr<mc::dbus::variants_message>& reply) {
                    if (reply->type == mc::dbus::message_type::method_return) {
                        ilog("收到回复: ${reply}",
                             ("reply",
                              reply->body.size() > 0 ? reply->body[0].as<std::string>() : "Empty"));
                    } else if (reply->type == mc::dbus::message_type::error) {
                        elog("收到错误: ${error}", ("error", reply->error_name));
                    }
                });
            }
        });

        // 等待用户输入退出
        std::cout << "按Enter键退出..." << std::endl;
        std::string line;
        std::getline(std::cin, line);

        // 断开连接并清理
        connection->disconnect();

        // 停止IO上下文
        work_guard.reset();
        io_context.stop();

        // 等待IO线程结束
        if (io_thread.joinable()) {
            io_thread.join();
        }

    } catch (const std::exception& e) {
        elog("异常: ${error}", ("error", e.what()));
    }
}

int main(int argc, char* argv[]) {
    // 初始化日志
    // TODO: 添加日志初始化代码

    ilog("DBus连接示例启动");

    // 运行测试
    test_dbus_connection();

    ilog("DBus连接示例结束");
    return 0;
}