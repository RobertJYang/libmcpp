/**
 * @file main.cpp
 * @brief 示例应用程序，演示如何使用插件动态加载功能
 */

#include "mc/core/application.h"
#include "example_plugin.h"
#include <iostream>

using namespace mc;

// 创建插件实例的函数
extern "C" plugin* create_plugin() {
    return new examples::example_plugin();
}

int main(int argc, char** argv) {
    try {
        // 获取应用程序实例
        application& app = application::instance();

        // 设置版本号
        app.set_version("1.0.0");

        // 设置插件目录
        // 注意：在实际使用中，应该使用绝对路径或相对于可执行文件的路径
        app.set_plugin_dir("./plugins");

        // 初始化应用程序
        app.initialize(argc, argv);

        // 启动应用程序
        app.start();

        // 运行应用程序
        app.exec();

        // 清理资源
        app.cleanup();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
}