/**
 * @file main.cpp
 * @brief 示例应用程序，演示如何使用插件动态加载功能
 */

#include "appbase/application.h"
#include <iostream>

int main(int argc, char** argv) {
    // 获取应用程序实例
    appbase::application& app = appbase::application::instance();
    
    // 设置应用程序版本号
    app.set_version("1.0.0");
    
    // 设置插件目录
    // 注意：在实际使用中，应该使用绝对路径或相对于可执行文件的路径
    app.set_plugin_dir("./plugins");
    
    // 初始化应用程序，解析命令行参数
    // 这将自动加载通过--plugin参数指定的插件
    app.initialize(argc, argv);
    
    // 启动所有已初始化的插件
    app.startup();

    // 关闭应用程序，这将关闭所有插件并卸载动态库
    app.shutdown();
    
    return 0;
}