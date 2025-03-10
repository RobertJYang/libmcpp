#include "appbase/application.h"
#include "example_plugin.h"
#include <iostream>

using namespace appbase;
using namespace examples;

int main(int argc, char** argv) {
    try {
        // 设置应用程序版本
        app().set_version("1.0.0");

        // 注册插件
        app().register_plugin(std::make_unique<api_plugin>());

        // 初始化应用程序
        app().initialize(argc, argv);

        // 启动应用程序
        app().start();

        // 执行应用程序
        app().exec();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
}