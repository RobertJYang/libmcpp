#include "mc/core/application.h"
#include <iostream>
#include <mc/filesystem.h>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
    mc::core::application& app = mc::core::application::instance();
    app.set_version("1.0.0");

    // 设置插件目录
    mc::filesystem::path plugin_dir = mc::filesystem::current_path() / "plugins";
    app.get_plugin_manager().set_plugin_dir(plugin_dir);

    // 添加命令行参数
    std::vector<char*> new_argv;
    new_argv.push_back(argv[0]);

    // 添加配置文件路径
    std::string config_path = (mc::filesystem::current_path() / "config" / "config.ini").string();
    std::string config_arg  = "--config=" + config_path;
    new_argv.push_back(const_cast<char*>(config_arg.c_str()));

    // 复制其他参数
    for (int i = 1; i < argc; i++) {
        new_argv.push_back(argv[i]);
    }
    new_argv.push_back(nullptr);

    try {
        // 初始化应用程序
        if (!app.initialize(static_cast<int>(new_argv.size()) - 1, new_argv.data())) {
            throw std::runtime_error("初始化应用程序失败");
        }

        // 启动应用程序
        app.start();

        // 运行应用程序
        app.exec();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        app.stop();
        return 1;
    }
}
