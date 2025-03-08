#ifndef APPBASE_PLUGIN_H
#define APPBASE_PLUGIN_H

#include <string>
#include <filesystem>
#include <boost/program_options.hpp>

namespace appbase {
namespace fs = std::filesystem;
namespace po = boost::program_options;

/**
 * @brief 插件接口类
 */
class plugin {
public:
    virtual ~plugin() = default;
    
    /**
     * @brief 获取插件名称
     * @return 插件名称
     */
    virtual std::string name() const = 0;
    
    /**
     * @brief 初始化插件
     * @return 初始化是否成功
     */
    virtual bool initialize() = 0;
    
    /**
     * @brief 启动插件
     */
    virtual void startup() = 0;
    
    /**
     * @brief 关闭插件
     */
    virtual void shutdown() = 0;

    /**
     * @brief 收集插件的命令行和配置文件配置项
     * @param[out] cli_opts 命令行配置项
     * @param[out] cfg_opts 配置文件配置项
     */
    virtual void register_options(po::options_description& cli_opts,
                               po::options_description& cfg_opts) {}
};

} // namespace appbase

#endif // APPBASE_PLUGIN_H