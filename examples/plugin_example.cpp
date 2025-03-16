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

/**
 * @file plugin_example.cpp
 * @brief 插件系统使用示例
 */

#include "mc/core/application.h"
#include "mc/core/plugin_manager.h"
#include "mc/core/service_factory.h"
#include "mc/core/service_manager.h"
#include "mc/log.h"
#include <iostream>

using namespace mc;

int main(int argc, char* argv[]) {
    // 初始化应用程序
    app().set_version("1.0.0");
    if (!app().initialize(argc, argv)) {
        std::cerr << "初始化应用程序失败" << std::endl;
        return 1;
    }
    
    // 设置插件目录
    app().get_plugin_manager().set_plugin_dir("./plugins");
    
    // 加载example插件
    if (!app().get_plugin_manager().load_plugin("example_plugin")) {
        std::cerr << "加载示例插件失败" << std::endl;
        return 1;
    }
    
    // 初始化插件（这会触发服务类型注册）
    if (!app().get_plugin_manager().init_plugins(app().get_service_factory())) {
        std::cerr << "初始化插件失败" << std::endl;
        return 1;
    }
    
    // 创建服务实例
    mc::mutable_dict service1_config;
    service1_config["message"] = "你好，我是服务1！";
    service1_config["repeat_count"] = 2;
    
    auto service1 = app().get_service_factory().create_service("example", "service1", service1_config);
    if (!service1) {
        std::cerr << "创建服务1失败" << std::endl;
        return 1;
    }
    
    // 添加服务到服务管理器
    app().get_service_manager().add_service("service1", service1);
    
    // 创建另一个服务实例
    mc::mutable_dict service2_config;
    service2_config["message"] = "你好，我是服务2！";
    service2_config["repeat_count"] = 3;
    
    auto service2 = app().get_service_factory().create_service("example", "service2", service2_config);
    if (!service2) {
        std::cerr << "创建服务2失败" << std::endl;
        return 1;
    }
    
    app().get_service_manager().add_service("service2", service2);
    
    // 启动应用程序（这会启动所有服务）
    app().start();
    
    // 等待一会儿
    std::cout << "服务已启动，按回车停止..." << std::endl;
    std::cin.get();
    
    // 停止并清理
    app().stop();
    app().cleanup();
    
    return 0;
} 