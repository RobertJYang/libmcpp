#ifndef APPBASE_APPLICATION_H
#define APPBASE_APPLICATION_H

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <boost/asio.hpp>
#include "plugin.h"

namespace appbase {

/**
 * @brief 应用程序类，提供应用程序核心功能
 * 
 * application类是应用程序的核心类，提供以下功能：
 * - 配置文件目录管理
 * - 插件管理
 * - 版本号管理
 * - 异步任务处理
 */
class application {
public:
    using io_context_type = boost::asio::io_context;
    using executor_type = io_context_type::executor_type;
    using work_guard_type = boost::asio::executor_work_guard<executor_type>;

    /**
     * @brief 获取应用程序单例实例
     * @return 应用程序实例引用
     */
    static application& instance();
    
    /**
     * @brief 析构函数
     */
    ~application();
    
    /**
     * @brief 设置应用程序版本号
     * @param version 版本号字符串
     */
    void set_version(const std::string& version);
    
    /**
     * @brief 获取应用程序版本号
     * @return 版本号字符串
     */
    const std::string& version() const;
    
    /**
     * @brief 设置配置文件目录
     * @param config_dir 配置文件目录路径
     */
    void set_config_dir(const fs::path& config_dir);
    
    /**
     * @brief 获取配置文件目录
     * @return 配置文件目录路径
     */
    const fs::path& config_dir() const;
    
    /**
     * @brief 注册插件
     * @param plugin 插件指针
     * @return 应用程序实例引用，用于链式调用
     */
    application& register_plugin(std::unique_ptr<plugin> plugin);
    
    /**
     * @brief 查找插件
     * @param name 插件名称
     * @return 插件指针，如果未找到则返回nullptr
     */
    plugin* find_plugin(const std::string& name) const;
    
    /**
     * @brief 初始化所有已注册的插件
     * @return 应用程序实例引用，用于链式调用
     */
    application& initialize();
    
    /**
     * @brief 使用命令行参数初始化应用程序和插件
     * @param argc 命令行参数数量
     * @param argv 命令行参数数组
     * @return 应用程序实例引用，用于链式调用
     */
    application& initialize(int argc, char** argv);
    
    /**
     * @brief 设置插件目录
     * @param plugin_dir 插件目录路径
     * @return 应用程序实例引用，用于链式调用
     */
    application& set_plugin_dir(const fs::path& plugin_dir);
    
    /**
     * @brief 启动所有已初始化的插件
     * @return 应用程序实例引用，用于链式调用
     */
    application& startup();

    /**
     * @brief 退出应用程序
     * 
     * 停止事件循环，等待所有工作线程结束，并确保所有插件正确关闭
     */
    void quit();

    /**
     * @brief 运行应用程序
     * 
     * 启动事件循环，处理异步任务
     */
    void run();

    /**
     * @brief 获取IO上下文
     * @return IO上下文引用
     */
    io_context_type& get_io_context();

    /**
     * @brief 关闭所有已启动的插件
     */
    void shutdown();

    /**
     * @brief 检查应用程序是否处于退出状态
     * @return 如果应用程序正在退出则返回true，否则返回false
     */
    bool is_quit() const;

private:
    /**
     * @brief 构造函数（私有，通过instance()获取实例）
     */
    application();
    
    /**
     * @brief 加载动态插件
     * @param plugin_name 插件名称
     * @return 是否成功加载插件
     */
    bool load_plugin(const std::string& plugin_name);

    class impl;
    std::unique_ptr<impl> pimpl_;
}; // 添加分号

inline application& app() { return application::instance(); }

} // namespace appbase

#endif // APPBASE_APPLICATION_H