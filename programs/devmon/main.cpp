#include "appbase/application.h"
#include <iostream>
#include <filesystem>
#include <thread>

int main(int argc, char** argv) {
    appbase::application& app = appbase::application::instance();
    app.set_version("1.0.0");
    
    // 设置配置文件目录
    std::filesystem::path config_dir = std::filesystem::current_path() / "config";
    app.set_config_dir(config_dir);
    
    // 设置插件目录
    std::filesystem::path plugin_dir = std::filesystem::current_path() / "plugins";
    app.set_plugin_dir(plugin_dir);
    
    try {
        app.initialize(argc, argv)
           .startup();
        
        std::cout << "设备监控应用程序已启动，按Ctrl+C退出..." << std::endl;
        
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    // 关闭应用程序，这将关闭所有插件并卸载动态库
    app.shutdown();
    
    return 0;
}