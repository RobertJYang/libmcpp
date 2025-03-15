/**
 * @file main.cpp
 * @brief 示例应用程序，演示如何使用模块动态加载功能
 */

#include "mc/core/application.h"
#include <iostream>
#include <filesystem>
#include <vector>

using namespace mc;

int main(int argc, char** argv) {
    try {
        // 获取应用程序实例
        application& app = application::instance();

        // 设置版本号
        app.set_version("1.0.0");

        // 设置模块目录
        std::filesystem::path module_path = std::filesystem::absolute("./modules");
        std::cout << "模块目录: " << module_path << std::endl;
        app.get_module_manager().set_module_dir(module_path);

        // 初始化应用程序（会处理命令行参数，包括加载模块）
        // 可以通过 --module=example 或 -m example 参数指定要加载的模块
        if (!app.initialize(argc, argv)) {
           return 0;
        }

        // 启动应用程序
        app.start();

        // 运行应用程序
        // std::cout << "应用程序运行中，按 Ctrl+C 退出..." << std::endl;
        // app.exec();

        // 清理资源
        app.cleanup();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
}