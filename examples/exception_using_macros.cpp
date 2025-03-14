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

#include <iostream>
#include <mc/exception.h>
#include <string>
#include <vector>

// 使用宏定义自定义异常类
// 参数1：异常类名
// 参数2：异常代码枚举值
// 参数3：默认异常消息
// 参数4：异常类名字符串
MC_DEFINE_EXCEPTION_CLASS(network_error, 200, "网络连接错误", "network_error")
MC_DEFINE_EXCEPTION_CLASS(database_error, 201, "数据库操作错误", "database_error")
MC_DEFINE_EXCEPTION_CLASS(auth_error, 202, "认证授权错误", "auth_error")

// 模拟网络操作函数
void connect_to_server(const std::string& server_url) {
    if (server_url.empty()) {
        MC_THROW(network_error, "服务器URL不能为空");
    }

    if (server_url.find("http") != 0) {
        MC_THROW(network_error, "无效的协议: " + server_url);
    }

    std::cout << "成功连接到服务器: " << server_url << std::endl;
}

// 模拟数据库操作函数
void query_database(const std::string& sql) {
    if (sql.empty()) {
        MC_THROW(database_error, "SQL查询语句不能为空");
    }

    if (sql.find("SELECT") != 0 && sql.find("INSERT") != 0 && sql.find("UPDATE") != 0 &&
        sql.find("DELETE") != 0) {
        MC_THROW(database_error, "不支持的SQL操作: " + sql);
    }

    std::cout << "成功执行SQL: " << sql << std::endl;
}

// 模拟认证操作函数
void authenticate_user(const std::string& username, const std::string& password) {
    if (username.empty() || password.empty()) {
        MC_THROW(auth_error, "用户名和密码不能为空");
    }

    if (password.length() < 8) {
        MC_THROW(auth_error, "密码长度不足");
    }

    std::cout << "用户 " << username << " 认证成功" << std::endl;
}

// 模拟综合操作
void process_user_request(const std::string& username, const std::string& password,
                          const std::string& operation, const std::string& server) {
    try {
        // 尝试认证用户
        authenticate_user(username, password);

        // 尝试连接服务器
        connect_to_server(server);

        // 尝试执行数据库操作
        query_database(operation);

        std::cout << "请求处理成功！" << std::endl;
    } catch (const network_error& e) {
        // 捕获网络错误
        std::cerr << "网络错误: " << e.to_string() << std::endl;
    } catch (const database_error& e) {
        // 捕获数据库错误
        std::cerr << "数据库错误: " << e.to_string() << std::endl;
    } catch (const auth_error& e) {
        // 捕获认证错误
        std::cerr << "认证错误: " << e.to_string() << std::endl;
    } catch (const mc::exception& e) {
        // 捕获其他MC库异常
        std::cerr << "其他错误: " << e.to_detail_string() << std::endl;
    }
}

int main() {
    std::cout << "==== 异常宏使用示例 ====" << std::endl;

    // 注册自定义异常类
    MC_REGISTER_EXCEPTION(network_error);
    MC_REGISTER_EXCEPTION(database_error);
    MC_REGISTER_EXCEPTION(auth_error);

    // 测试场景1: 认证错误
    std::cout << "\n测试场景1: 认证错误" << std::endl;
    process_user_request("admin", "123", "SELECT * FROM users", "http://example.com");

    // 测试场景2: 网络错误
    std::cout << "\n测试场景2: 网络错误" << std::endl;
    process_user_request("admin", "password123", "SELECT * FROM users", "ftp://example.com");

    // 测试场景3: 数据库错误
    std::cout << "\n测试场景3: 数据库错误" << std::endl;
    process_user_request("admin", "password123", "DROP TABLE users", "http://example.com");

    // 测试场景4: 成功场景
    std::cout << "\n测试场景4: 成功场景" << std::endl;
    process_user_request("admin", "password123", "SELECT * FROM users", "http://example.com");

    return 0;
}