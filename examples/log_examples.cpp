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

#include <mc/log/log_macros.h>
#include <mc/log/elk_log_backend.h>
#include <mc/log/console_appender.h>
#include <mc/log/file_appender.h>
#include <mc/dict.h>
#include <mc/variant.h>
#include <string>
#include <iostream>

int main() {
    // 初始化日志系统
    mc::log::logger main_logger = mc::log::logger::get("main");
    
    // 添加控制台输出
    auto console = std::make_shared<mc::log::console_appender>();
    main_logger.add_appender(console);
    
    // 添加文件输出
    auto file = std::make_shared<mc::log::file_appender>("logs/app.log");
    main_logger.add_appender(file);
    
    // 添加ELK后端（可选）
    auto elk_backend = std::make_shared<mc::log::elk_log_backend>();
    mc::log::log_manager::instance().add_backend(elk_backend);
    
    // 设置日志级别
    main_logger.set_level(mc::log::level::trace);
    
    // 示例1: 使用基本日志宏
    mc_ilog(main_logger, "这是一条简单的日志消息");
    
    // 示例2: 使用参数构建器
    mc_ilog(main_logger, "用户 ${username} 登录成功",
        mc::log::params_builder()("username", "admin"));
    
    // 示例3: 使用链式参数构建
    mc_ilog(main_logger, "用户 ${username} 登录成功，IP: ${ip}，时间: ${time}", 
        ("username", "admin")("ip", "192.168.1.1")("time", "2024-03-12 15:30:00"));
    
    // 示例4: 使用嵌套字典
    mc::dict user_info = {
        {"id", 123},
        {"role", "admin"},
        {"permissions", mc::dict{
            {"read", true},
            {"write", true},
            {"admin", false}
        }}
    };
    
    mc_dlog(main_logger, "用户详情: ${info}", ("info", user_info));
    
    // 示例5: 使用默认日志记录器
    ilog("这是使用默认日志记录器的消息");
    
    // 示例6: 使用默认日志记录器和参数
    ilog("处理请求 ${method} ${path}", 
        ("method", "GET")("path", "/api/users")("latency_ms", 45));
    
    // 示例7: 记录异常信息
    try {
        throw std::runtime_error("演示异常");
    } catch (const std::exception& e) {
        elog("捕获到异常: ${error}", ("error", e.what())("stack_trace", "..."));
    }
    
    // 示例8: 使用动态构建的参数字典
    mc::log::params_builder params;
    params("request_id", "req-12345");
    params("user_id", 42);
    params("status", "success");
    
    mc_ilog(main_logger, "请求处理完成: ${request_id}, 用户: ${user_id}, 状态: ${status}", params);
    
    // 示例9: 结合多个参数源
    mc::log::params_builder base_params;
    base_params("service", "auth-service");
    base_params("instance", "i-12345");
    
    // 合并参数
    mc_ilog(main_logger, "服务 ${service}/${instance} 处理了请求 ${request_id}", 
        base_params.add_all(params));
    
    return 0;
} 