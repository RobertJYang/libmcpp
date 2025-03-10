#include "mc/core/application.h"
#include <filesystem>
#include <iostream>
#include <thread>

int main(int argc, char** argv) {
    mc::application& app = mc::application::instance();
    app.set_version("1.0.0");

    // 设置配置文件目录
    std::filesystem::path config_dir = std::filesystem::current_path() / "config";
    app.set_config_dir(config_dir);

    // 设置插件目录
    std::filesystem::path plugin_dir = std::filesystem::current_path() / "plugins";
    app.set_plugin_dir(plugin_dir);

    try {
        // 初始化应用程序
        app.initialize(argc, argv).start();

        // 运行应用程序
        app.exec();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        app.cleanup();
        return 1;
    }
}