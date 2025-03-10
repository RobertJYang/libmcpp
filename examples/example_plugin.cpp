/**
 * @file example_plugin.cpp
 * @brief 示例插件实现
 */

#include "example_plugin.h"
#include <iostream>

namespace po = appbase::po;
namespace example {

/**
 * @brief 示例插件类
 */
class example_plugin : public appbase::plugin {
public:
    example_plugin() {
        std::cout << "示例插件构造" << std::endl;
    }

    ~example_plugin() {
        std::cout << "示例插件析构" << std::endl;
    }

    /**
     * @brief 获取插件名称
     * @return 插件名称
     */
    std::string name() const override {
        return "example";
    }

    /**
     * @brief 初始化插件
     * @return 初始化是否成功
     */
    bool initialize() override {
        std::cout << "示例插件初始化" << std::endl;
        return true;
    }

    /**
     * @brief 启动插件
     */
    void startup() override {
        std::cout << "示例插件启动" << std::endl;
    }

    /**
     * @brief 关闭插件
     */
    void shutdown() override {
        std::cout << "示例插件关闭" << std::endl;
    }

    /**
     * @brief 收集插件的命令行和配置文件配置项
     * @param[out] cmdline_opts 命令行配置项
     * @param[out] config_file_opts 配置文件配置项
     */
    void register_options(po::options_description& cmdline_opts,
                          po::options_description& config_file_opts) override {
        cmdline_opts.add_options()("example.log-level",
                                   po::value<std::string>()->default_value("info"),
                                   "日志级别 (debug, info, warn, error)")(
            "example.output-file", po::value<std::string>(), "输出文件路径");
        config_file_opts.add_options()("example.enable-feature",
                                       po::value<bool>()->default_value(false), "是否启用特定功能");
    }
};

} // namespace example

/**
 * @brief 创建插件实例的导出函数
 * @return 插件实例指针
 */
extern "C" appbase::plugin* create_plugin() {
    return new examples::example_plugin();
}