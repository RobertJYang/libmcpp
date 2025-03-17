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

    // 启动应用程序
    app().start();

    // 等待用户输入
    std::cout << "服务已启动，按回车停止..." << std::endl;
    std::cin.get();

    // 停止并清理
    app().stop();
    app().cleanup();

    return 0;
}