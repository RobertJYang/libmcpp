/**
 * @file main.cpp
 * @brief 示例应用程序，演示如何使用模块动态加载功能
 */

#include "mc/core/application.h"
#include <iostream>
#include <filesystem>
#include <vector>
#include <mc/log.h>

using namespace mc;

int main(int argc, char** argv) {
    try {
        // 获取应用程序实例
        application& app = application::instance();

        // 设置版本号
        app.set_version("1.0.0");

        // 初始化应用程序（会处理命令行参数，包括加载模块）
        // 可以通过 --module=example 或 -m example 参数指定要加载的模块
        if (!app.initialize(argc, argv)) {
           return 0;
        }

        // 启动应用程序
        app.start();

        // 运行应用程序
        // ilog("应用程序运行中，按 Ctrl+C 退出...");
        // app.exec();

        // 清理资源
        app.cleanup();

        return 0;
    } catch (const std::exception& e) {
        elog("错误: ${error}", ("error", e.what()));
        return 1;
    }
}