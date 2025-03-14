/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "mc/core/application.h"
#include <algorithm>
#include <mc/string.h>
#include <boost/program_options.hpp>
#include <dlfcn.h>
#include <iostream>
#include <unordered_map>

namespace mc {

/**
 * @brief application类的实现类
 */
class application::impl {
public:
    impl();
    ~impl();

    // 插件管理
    void    load_plugins();
    bool    load_plugin(const std::string& plugin_name);
    void    load_plugins_from_list(const std::vector<std::string>& plugins);
    bool    is_plugin_loaded(const std::string& plugin_name) const;
    bool    check_plugin_path(const std::string& plugin_name, fs::path& plugin_path) const;
    void*   load_plugin_library(const std::string& plugin_name, const fs::path& plugin_path);
    plugin* create_plugin_instance(const std::string& plugin_name, void* handle);

    // 配置管理
    void parse_command_line(int argc, char** argv);
    void collect_plugin_options();
    void load_config_file();

    // 插件生命周期管理
    void initialize_plugins();
    void shutdown();
    void run();

    // 插件依赖管理
    template <typename ActionFunc>
    bool process_plugins_with_dependencies(const std::vector<plugin*>& source_plugins,
                                           std::vector<plugin*>&       processed_plugins,
                                           ActionFunc action, const std::string& action_name);

    /**
     * @brief 插件处理上下文
     */
    struct plugin_process_context {
        std::vector<plugin*>& pending_plugins;   ///< 待处理的插件列表
        std::vector<plugin*>& processed_plugins; ///< 已处理的插件列表
        std::string           action_name;       ///< 操作名称（用于日志）
    };

    template <typename ActionFunc>
    bool process_plugins_round(plugin_process_context& context, ActionFunc action);

    template <typename ActionFunc, typename IteratorType>
    bool process_single_plugin(plugin* p, IteratorType& it, plugin_process_context& context,
                               ActionFunc action);

    void report_unprocessed_plugins(const std::vector<plugin*>& pending_plugins,
                                    const std::string&          action_name);

    bool check_dependencies(plugin* p, const std::vector<plugin*>& processed_plugins,
                            const std::string& action_name);

    // 成员变量
    std::string                                              m_version;    ///< 应用程序版本号
    fs::path                                                 m_config_dir; ///< 配置文件目录
    fs::path                                                 m_plugin_dir; ///< 插件目录
    std::unordered_map<std::string, std::unique_ptr<plugin>> m_plugins;    ///< 插件映射表
    std::vector<plugin*> m_initialized_plugins;                            ///< 已初始化的插件列表
    std::vector<plugin*> m_started_plugins;                                ///< 已启动的插件列表
    std::vector<void*>   m_plugin_handles;                                 ///< 动态库句柄列表

    struct options {
        po::options_description cli{"命令行配置项"};   ///< 命令行配置项
        po::options_description cfg{"配置文件配置项"}; ///< 配置文件配置项
    };
    std::unique_ptr<options> m_opts;
    po::variables_map        m_options; ///< 所有配置项

    boost::asio::io_context                                                  m_io_context;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> m_executor;
    priority_queue_executor<boost::asio::io_context>                         m_priority_executor;
    std::unique_ptr<work_guard_type>                                         m_work_guard;
    std::vector<std::thread>                                                 m_threads;
    bool                                                                     m_is_quitting{false};
};

// impl类的实现
application::impl::impl()
    : m_opts(std::make_unique<options>()), m_executor(m_io_context.get_executor()),
      m_priority_executor(m_io_context),
      m_work_guard(std::make_unique<work_guard_type>(boost::asio::make_work_guard(m_io_context))) {
}

application::impl::~impl() {
    if (m_work_guard) {
        m_work_guard.reset();
    }
    m_io_context.stop();
    for (auto& thread : m_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void application::impl::parse_command_line(int argc, char** argv) {
    m_opts->cli.add_options()
        ("help,h", "显示帮助信息")
        ("version,v", "显示版本信息")
        ("config,c", po::value<std::string>()->default_value("config.ini"), "配置文件名称（相对于配置目录）")
        ("config-dir", po::value<std::string>(), "配置目录路径")
        ("plugin,p", po::value<std::vector<std::string>>()->composing(), "要加载的插件名称【可以重复多个或以逗号分隔多个】")
        ("plugin-dir", po::value<std::string>(), "插件目录路径")(
        "threads", po::value<unsigned int>()->default_value(1), "线程数量");

    load_config_file();

    try {
        po::parsed_options parsed = po::command_line_parser(argc, argv).options(m_opts->cli).run();
        po::store(parsed, m_options);
        std::vector<std::string> positionals =
            po::collect_unrecognized(parsed.options, po::include_positional);
        if (!positionals.empty()) {
            throw std::runtime_error("未知命令行参数选项 '" + positionals[0]);
        }
    } catch (const boost::program_options::unknown_option& e) {
        throw std::runtime_error("未知命令行参数选项 '" + e.get_option_name());
    }

    if (m_options.count("version")) {
        std::cout << "版本: " << m_version << std::endl;
        exit(0);
    }

    if (m_options.count("help")) {
        std::cout << m_opts->cli << std::endl;
        exit(0);
    }
}

void application::impl::run() {
    // 创建工作线程
    const auto thread_count = std::thread::hardware_concurrency();
    m_threads.reserve(thread_count);

    // 启动工作线程
    for (size_t i = 0; i < thread_count; ++i) {
        m_threads.emplace_back([this]() {
            m_io_context.run();
        });
    }

    // 主线程也运行IO上下文
    m_io_context.run();

    // 等待所有工作线程结束
    for (auto& t : m_threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    m_threads.clear();
}

/**
 * @brief 检查插件依赖是否已处理
 * @param p 要检查的插件
 * @param processed_plugins 已处理的插件列表
 * @param action_name 操作名称（用于日志）
 * @return 如果所有依赖都已处理，则返回true
 */
bool application::impl::check_dependencies(plugin* p, const std::vector<plugin*>& processed_plugins,
                                           const std::string& action_name) {
    for (const auto& dep_name : p->dependencies()) {
        auto dep_it = m_plugins.find(dep_name);
        if (dep_it == m_plugins.end()) {
            std::cerr << "警告: 插件 " << p->name() << " 依赖的插件 " << dep_name << " 未找到"
                      << std::endl;
            continue;
        }

        plugin* dep = dep_it->second.get();
        if (std::find(processed_plugins.begin(), processed_plugins.end(), dep) ==
            processed_plugins.end()) {
            return false;
        }
    }
    return true;
}

/**
 * @brief 处理单个插件
 * @param p 要处理的插件
 * @param it 插件在待处理列表中的迭代器
 * @param context 处理上下文
 * @param action 处理插件的函数
 * @return 是否成功处理插件
 */
template <typename ActionFunc, typename IteratorType>
bool application::impl::process_single_plugin(plugin* p, IteratorType& it,
                                              plugin_process_context& context, ActionFunc action) {
    // 检查所有依赖是否已经处理
    if (!check_dependencies(p, context.processed_plugins, context.action_name)) {
        ++it;
        return false;
    }

    // 处理插件
    try {
        if (action(p)) {
            context.processed_plugins.push_back(p);
            it = context.pending_plugins.erase(it);
            return true;
        } else {
            std::cerr << "警告: 插件 " << p->name() << " " << context.action_name << "失败"
                      << std::endl;
            ++it;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "错误: 插件 " << p->name() << " " << context.action_name
                  << "时发生异常: " << e.what() << std::endl;
        ++it;
        return false;
    }
}

/**
 * @brief 处理一轮插件
 * @param context 处理上下文
 * @param action 处理插件的函数
 * @return 是否有插件被处理
 */
template <typename ActionFunc>
bool application::impl::process_plugins_round(plugin_process_context& context, ActionFunc action) {
    bool processed_any = false;
    auto it            = context.pending_plugins.begin();

    while (it != context.pending_plugins.end()) {
        plugin* p         = *it;
        bool    processed = process_single_plugin(p, it, context, action);
        processed_any     = processed_any || processed;
    }

    return processed_any;
}

/**
 * @brief 报告未处理的插件
 * @param pending_plugins 未处理的插件列表
 * @param action_name 操作名称（用于日志）
 */
void application::impl::report_unprocessed_plugins(const std::vector<plugin*>& pending_plugins,
                                                   const std::string&          action_name) {
    std::cerr << "错误: 无法" << action_name
              << "以下插件，可能存在循环依赖或依赖缺失:" << std::endl;
    for (auto* p : pending_plugins) {
        std::cerr << "  - " << p->name() << std::endl;
    }
}

/**
 * @brief 按依赖顺序处理插件
 * @param source_plugins 源插件列表
 * @param processed_plugins 已处理的插件列表
 * @param action 处理插件的函数
 * @param action_name 操作名称（用于日志）
 * @return 如果所有插件都成功处理，则返回true
 */
template <typename ActionFunc>
bool application::impl::process_plugins_with_dependencies(
    const std::vector<plugin*>& source_plugins, std::vector<plugin*>& processed_plugins,
    ActionFunc action, const std::string& action_name) {
    // 创建一个待处理的插件列表
    std::vector<plugin*> pending_plugins = source_plugins;

    // 创建处理上下文
    plugin_process_context context{pending_plugins, processed_plugins, action_name};

    // 按照依赖顺序处理插件
    while (!pending_plugins.empty()) {
        bool processed_any = process_plugins_round(context, action);

        // 如果没有处理任何插件，说明存在循环依赖或者依赖缺失
        if (!processed_any) {
            report_unprocessed_plugins(pending_plugins, action_name);
            return false;
        }
    }

    return true;
}

void application::impl::initialize_plugins() {
    // 创建一个待初始化的插件列表
    std::vector<plugin*> pending_plugins;
    for (auto& pair : m_plugins) {
        plugin* p = pair.second.get();
        if (std::find(m_initialized_plugins.begin(), m_initialized_plugins.end(), p) ==
            m_initialized_plugins.end()) {
            pending_plugins.push_back(p);
        }
    }

    // 按照依赖顺序初始化插件
    process_plugins_with_dependencies(
        pending_plugins, m_initialized_plugins,
        [](plugin* p) {
            return p->initialize();
        },
        "初始化");
}

void application::impl::load_plugins_from_list(const std::vector<std::string>& plugins) {
    for (const auto& plugin_arg : plugins) {
        std::vector<std::string> plugin_names = mc::string::split(plugin_arg, ",");
        for (const auto& name : plugin_names) {
            if (!name.empty()) {
                load_plugin(name);
            }
        }
    }
}

void application::impl::load_plugins() {
    if (m_options.count("plugin-dir")) {
        m_plugin_dir = m_options["plugin-dir"].as<std::string>();
    }

    if (m_options.count("plugin")) {
        load_plugins_from_list(m_options["plugin"].as<std::vector<std::string>>());
    }
}

void application::impl::collect_plugin_options() {
    for (auto& pair : m_plugins) {
        po::options_description cli_opts("插件'" + pair.second->name() + "'命令行选项");
        po::options_description cfg_opts("插件'" + pair.second->name() + "'配置文件选项");
        pair.second->register_options(cli_opts, cfg_opts);
        if (cli_opts.options().size()) {
            m_opts->cli.add(cli_opts);
        }
        if (cfg_opts.options().size()) {
            m_opts->cli.add(cfg_opts);
            m_opts->cfg.add(cfg_opts);
        }
    }
}

void application::impl::load_config_file() {
    if (!m_config_dir.empty()) {
        fs::path config_file = m_config_dir / "config.ini";
        if (exists(config_file)) {
            po::store(po::parse_config_file<char>(config_file.c_str(), m_opts->cli), m_options);
            po::notify(m_options);
        }
    }
}

bool application::impl::is_plugin_loaded(const std::string& plugin_name) const {
    auto it = m_plugins.find(plugin_name);
    if (it != m_plugins.end()) {
        std::cout << "插件 '" << plugin_name << "' 已加载" << std::endl;
        return true;
    }
    return false;
}

bool application::impl::check_plugin_path(const std::string& plugin_name,
                                          fs::path&          plugin_path) const {
    if (m_plugin_dir.empty()) {
        std::cerr << "错误: 未设置插件目录，无法加载插件 '" << plugin_name << "'" << std::endl;
        return false;
    }

    plugin_path = m_plugin_dir / ("lib" + plugin_name + ".so");

    if (!fs::exists(plugin_path)) {
        std::cerr << "错误: 插件文件 '" << plugin_path << "' 不存在" << std::endl;
        return false;
    }
    return true;
}

void* application::impl::load_plugin_library(const std::string& plugin_name,
                                             const fs::path&    plugin_path) {
    void* handle = dlopen(plugin_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        std::cerr << "错误: 无法加载插件 '" << plugin_name << "': " << dlerror() << std::endl;
        return nullptr;
    }
    return handle;
}

plugin* application::impl::create_plugin_instance(const std::string& plugin_name, void* handle) {
    using create_plugin_func = plugin* (*)();
    create_plugin_func create_plugin =
        reinterpret_cast<create_plugin_func>(dlsym(handle, "create_plugin"));

    if (!create_plugin) {
        std::cerr << "错误: 插件 '" << plugin_name
                  << "' 没有导出 'create_plugin' 函数: " << dlerror() << std::endl;
        dlclose(handle);
        return nullptr;
    }

    plugin* plugin_instance = create_plugin();
    if (!plugin_instance) {
        std::cerr << "错误: 无法创建插件 '" << plugin_name << "' 的实例" << std::endl;
        dlclose(handle);
        return nullptr;
    }
    return plugin_instance;
}

bool application::impl::load_plugin(const std::string& plugin_name) {
    if (is_plugin_loaded(plugin_name)) {
        return true;
    }

    fs::path plugin_path;
    if (!check_plugin_path(plugin_name, plugin_path)) {
        return false;
    }

    void* handle = load_plugin_library(plugin_name, plugin_path);
    if (!handle) {
        return false;
    }

    plugin* plugin_instance = create_plugin_instance(plugin_name, handle);
    if (!plugin_instance) {
        return false;
    }

    m_plugins[plugin_name] = std::unique_ptr<plugin>(plugin_instance);
    m_plugin_handles.push_back(handle);

    std::cout << "成功加载插件 '" << plugin_name << "'" << std::endl;
    return true;
}

void application::impl::shutdown() {
    if (m_is_quitting) {
        return;
    }

    m_is_quitting = true;

    // 停止所有已启动的插件（按启动顺序的逆序）
    for (auto it = m_started_plugins.rbegin(); it != m_started_plugins.rend(); ++it) {
        try {
            (*it)->shutdown();
        } catch (const std::exception& e) {
            std::cerr << "插件 " << (*it)->name() << " 关闭时发生异常: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "插件 " << (*it)->name() << " 关闭时发生未知异常" << std::endl;
        }
    }
    m_started_plugins.clear();

    // 卸载所有插件
    m_initialized_plugins.clear();
    m_plugins.clear();

    // 卸载动态库
    for (auto handle : m_plugin_handles) {
        if (handle) {
            dlclose(handle);
        }
    }
    m_plugin_handles.clear();
}

// application类的实现
application& application::instance() {
    static application app;
    return app;
}

application::application() : pimpl_(std::make_unique<impl>()) {
}

application::~application() {
    cleanup();
}

void application::exec() {
    pimpl_->run();
}

application& application::start() {
    // 按照依赖顺序启动插件
    pimpl_->process_plugins_with_dependencies(
        pimpl_->m_initialized_plugins, pimpl_->m_started_plugins,
        [](plugin* p) {
            p->startup();
            return true;
        },
        "启动");

    // 创建工作守卫，防止IO上下文过早结束
    pimpl_->m_work_guard =
        std::make_unique<work_guard_type>(boost::asio::make_work_guard(pimpl_->m_io_context));

    return *this;
}

void application::stop() {
    if (pimpl_->m_is_quitting) {
        return;
    }

    pimpl_->m_is_quitting = true;

    // 移除工作守卫，允许IO上下文在没有任务时结束
    if (pimpl_->m_work_guard) {
        pimpl_->m_work_guard.reset();
    }

    // 停止IO上下文
    pimpl_->m_io_context.stop();
}

void application::cleanup() {
    stop(); // 确保应用程序已停止
    pimpl_->shutdown();
}

bool application::is_stopped() const {
    return pimpl_->m_is_quitting;
}

application::io_context_type& application::get_io_context() {
    return pimpl_->m_io_context;
}

application::priority_executor_type& application::get_priority_executor() {
    return pimpl_->m_priority_executor;
}

void application::set_version(const std::string& version) {
    pimpl_->m_version = version;
}

const std::string& application::version() const {
    return pimpl_->m_version;
}

void application::set_config_dir(const fs::path& config_dir) {
    pimpl_->m_config_dir = config_dir;
}

const fs::path& application::config_dir() const {
    return pimpl_->m_config_dir;
}

application& application::register_plugin(std::unique_ptr<plugin> plugin) {
    // 检查插件是否已经注册
    auto name = plugin->name();
    if (pimpl_->m_plugins.find(name) != pimpl_->m_plugins.end()) {
        throw std::runtime_error("插件 " + name + " 已经注册");
    }

    // 注册插件
    auto* plugin_ptr        = plugin.get();
    pimpl_->m_plugins[name] = std::move(plugin);

    // 检查并加载插件依赖
    for (const auto& dep : plugin_ptr->dependencies()) {
        if (pimpl_->m_plugins.find(dep) == pimpl_->m_plugins.end()) {
            // 尝试加载依赖的插件
            if (!pimpl_->load_plugin(dep)) {
                std::cerr << "警告: 无法加载插件 " << name << " 的依赖项 " << dep << std::endl;
            }
        }
    }

    return *this;
}

plugin* application::find_plugin(const std::string& name) const {
    auto it = pimpl_->m_plugins.find(name);
    return (it != pimpl_->m_plugins.end()) ? it->second.get() : nullptr;
}

application& application::initialize() {
    pimpl_->collect_plugin_options();
    pimpl_->load_config_file();
    pimpl_->initialize_plugins();
    return *this;
}

application& application::initialize(int argc, char** argv) {
    pimpl_->parse_command_line(argc, argv);
    pimpl_->load_plugins();
    return initialize();
}

application& application::set_plugin_dir(const fs::path& plugin_dir) {
    pimpl_->m_plugin_dir = plugin_dir;
    return *this;
}

} // namespace mc