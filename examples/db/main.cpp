/*
 * Copyright (c) 2023 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <chrono>
#include <future>
#include <iostream>
#include <mc/core/application.h>
#include <mc/db/client.h>
#include <mc/db/common.h>
#include <mc/db/object.h>
#include <mc/db/property.h>
#include <mc/log.h>
#include <mc/reflect/reflect.h>
#include <memory>
#include <string>
#include <thread>

// 用户对象类定义
namespace mc::db::example {

class user_object : public mc::db::db_object<user_object> {
public:
    user_object() {
        // 初始化属性
        init_properties();
    }
    virtual ~user_object() = default;

    // 属性定义
    mc::db::property<int>         id;
    mc::db::property<std::string> username;
    mc::db::property<std::string> email;
    mc::db::property<int>         age;

    // 辅助函数，用于输出属性值到流
    friend std::ostream& operator<<(std::ostream& os, const user_object& user) {
        os << "用户 [ID: " << (int)user.id << ", 用户名: " << (std::string)user.username
           << ", 邮箱: " << (std::string)user.email << ", 年龄: " << (int)user.age << "]";
        return os;
    }
};

} // namespace mc::db::example

// 使用MC_REFLECT宏反射user_object的属性
MC_REFLECT(mc::db::example::user_object, (id)(username)(email)(age))

// 打印分隔线
void print_separator(const std::string& title) {
    std::cout << "\n================= " << title << " =================\n" << std::endl;
}

using namespace mc;
using namespace mc::db;

int main(int argc, char* argv[]) {
    try {
        // 初始化应用程序
        application& app = application::instance();
        app.set_version("1.0.0");

        // 初始化应用程序（解析命令行参数，加载插件）
        if (!app.initialize(argc, argv)) {
            elog("初始化应用程序失败");
            return 1;
        }

        // 启动应用程序
        ilog("启动应用程序");
        app.start();

        // 等待服务启动完成
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // 创建数据库客户端
        ilog("创建数据库客户端");
        database::client db_client;

        // 连接数据库
        ilog("连接数据库");
        error_code ec = db_client.connect();
        if (ec != error_code::success) {
            elog("连接数据库失败: ${error}", ("error", error_to_string(ec)));
            app.stop();
            app.cleanup();
            return 1;
        }

        // 同步API示例
        print_separator("同步API示例");

        // 创建用户对象
        ec = db_client.create<example::user_object>("/users/user1");
        if (ec != error_code::success) {
            elog("创建用户对象失败: ${error}", ("error", error_to_string(ec)));
        } else {
            ilog("创建用户对象成功: /users/user1");

            // 查询并设置属性
            auto user = db_client.query<example::user_object>("/users/user1", ec);
            if (ec != error_code::success || !user) {
                elog("查询用户对象失败: ${error}", ("error", error_to_string(ec)));
            } else {
                // 设置属性
                user->id       = 1001;
                user->username = "张三";
                user->email    = "zhangsan@example.com";
                user->age      = 28;

                std::cout << "设置用户属性成功: " << *user << std::endl;
            }
        }

        // 异步API示例
        print_separator("异步API示例");

        // 异步创建用户对象
        std::cout << "异步创建用户..." << std::endl;
        std::promise<void> create_promise;
        std::future<void>  create_future = create_promise.get_future();

        db_client.async_create<example::user_object>("/users/user2", [&](error_code ec) {
            if (ec != error_code::success) {
                elog("异步创建用户对象失败: ${error}", ("error", error_to_string(ec)));
            } else {
                ilog("异步创建用户对象成功: /users/user2");
            }
            create_promise.set_value();
        });

        // 等待异步创建完成
        create_future.wait();

        // 异步查询用户对象
        std::cout << "异步查询用户..." << std::endl;
        std::promise<void> query_promise;
        std::future<void>  query_future = query_promise.get_future();

        db_client.async_query<example::user_object>(
            "/users/user2", [&](error_code ec, std::shared_ptr<example::user_object> user) {
                if (ec != error_code::success || !user) {
                    elog("异步查询用户对象失败: ${error}", ("error", error_to_string(ec)));
                } else {
                    // 设置属性
                    user->id       = 1002;
                    user->username = "李四";
                    user->email    = "lisi@example.com";
                    user->age      = 32;

                    std::cout << "异步设置用户属性成功: " << *user << std::endl;
                }
                query_promise.set_value();
            });

        // 等待异步查询完成
        query_future.wait();

        // 列出所有用户
        print_separator("列出所有用户");
        auto children = db_client.list_children("/users", ec);
        if (ec != error_code::success) {
            elog("列出用户失败: ${error}", ("error", error_to_string(ec)));
        } else {
            std::cout << "用户目录下的对象:" << std::endl;
            for (const auto& child : children) {
                std::cout << "  - " << child << std::endl;

                // 查询每个用户的详细信息
                auto path = "/users/" + child;
                auto user = db_client.query<example::user_object>(path, ec);
                if (ec == error_code::success && user) {
                    std::cout << "    " << *user << std::endl;
                }
            }
        }

        // 断开连接
        ilog("断开数据库连接");
        ec = db_client.disconnect();
        if (ec != error_code::success) {
            elog("断开数据库连接失败: ${error}", ("error", error_to_string(ec)));
        }

        // 停止并清理
        ilog("停止应用程序");
        app.stop();
        app.cleanup();

        return 0;
    } catch (const std::exception& e) {
        elog("错误: ${error}", ("error", e.what()));
        return 1;
    }
}