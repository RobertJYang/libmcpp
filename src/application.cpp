#include "appbase/application.h"
#include <unordered_map>
#include <algorithm>
#include <dlfcn.h>
#include <iostream>
#include <thread>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>

namespace appbase {

/**
 * @brief application类的实现类
 */
class application::impl {
public:
    impl();
    ~impl();
    
    void load_plugins(int argc, char** argv);
    bool load_plugin(const std::string& plugin_name);
    void load_plugins_from_list(const std::vector<std::string>& plugins);
    void parse_command_line(int argc, char** argv);
    void collect_plugin_options();
    void load_config_file();
    void initialize_plugins();
    void shutdown();
    void run();

    bool is_plugin_loaded(const std::string& plugin_name) const;
    bool check_plugin_path(const std::string& plugin_name, fs::path& plugin_path) const;
    void* load_plugin_library(const std::string& plugin_name, const fs::path& plugin_path);
    plugin* create_plugin_instance(const std::string& plugin_name, void* handle);

    std::string m_version; ///< 应用程序版本号
    fs::path m_config_dir; ///< 配置文件目录
    fs::path m_plugin_dir; ///< 插件目录
    std::unordered_map<std::string, std::unique_ptr<plugin>> m_plugins; ///< 插件映射表
    std::vector<plugin*> m_initialized_plugins; ///< 已初始化的插件列表
    std::vector<plugin*> m_started_plugins; ///< 已启动的插件列表
    std::vector<void*> m_plugin_handles; ///< 动态库句柄列表

    struct options {
        po::options_description cli{"命令行配置项"}; ///< 命令行配置项
        po::options_description cfg{"配置文件配置项"}; ///< 配置文件配置项
    };
    std::unique_ptr<options> m_opts;
    po::variables_map        m_options; ///< 所有配置项

    boost::asio::io_context m_io_context;
    std::unique_ptr<work_guard_type> m_work_guard;
    std::vector<std::thread> m_threads;
    bool m_is_quitting{false};
};

// impl类的实现
application::impl::impl(): m_opts(std::make_unique<options>()) {
    m_work_guard = std::make_unique<work_guard_type>(boost::asio::make_work_guard(m_io_context));
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
        ("config,c", po::value<std::string>()->default_value( "config.ini" ), "配置文件名称（相对于配置目录）")
        ("config-dir", po::value<std::string>(), "配置目录路径")
        ("plugin,p", po::value<std::vector<std::string>>()->composing(), "要加载的插件名称【可以重复多个或以逗号分隔多个】")
        ("plugin-dir", po::value<std::string>(), "插件目录路径")
        ("threads", po::value<unsigned int>()->default_value(1), "线程数量");

    load_config_file();

    try {
        po::parsed_options parsed = po::command_line_parser(argc, argv)
            .options(m_opts->cli)
            .run();
        po::store(parsed, m_options);
        std::vector<std::string> positionals = po::collect_unrecognized(parsed.options, po::include_positional);
        if(!positionals.empty()) {
            throw std::runtime_error("未知命令行参数选项 '" + positionals[0]);
        }
     } catch( const boost::program_options::unknown_option& e ) {
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
    unsigned int thread_count = m_options["threads"].as<unsigned int>();
    
    for (unsigned int i = 0; i < thread_count - 1; ++i) {
        m_threads.emplace_back([this]() {
            m_io_context.run();
        });
    }
    
    m_io_context.run();
}

void application::impl::initialize_plugins() {
    for (auto& pair : m_plugins) {
        plugin* p = pair.second.get();
        
        auto it = std::find(m_initialized_plugins.begin(), m_initialized_plugins.end(), p);
        if (it == m_initialized_plugins.end()) {
            if (p->initialize()) {
                m_initialized_plugins.push_back(p);
            }
        }
    }
}

void application::impl::load_plugins_from_list(const std::vector<std::string>& plugins) {
    for (const auto& plugin_arg : plugins) {
        std::vector<std::string> plugin_names;
        boost::split(plugin_names, plugin_arg, boost::is_any_of(","));
        
        for (const auto& name : plugin_names) {
            if (!name.empty()) {
                load_plugin(name);
            }
        }
    }
}

void application::impl::load_plugins(int argc, char** argv) {
    po::variables_map vm;

    po::options_description desc("命令行配置项【插件】");
    desc.add_options()
        ("plugin,p", po::value<std::vector<std::string>>()->composing(), "要加载的插件名称【可以重复多个或以逗号分隔多个】")
        ("plugin-dir", po::value<std::string>(), "插件目录路径");

    try {
        po::parsed_options parsed = po::command_line_parser(argc, argv)
            .options(desc)
            .allow_unregistered()
            .run();
        
        po::store(parsed, vm);
        po::notify(vm);
    } catch( const boost::program_options::unknown_option& e ) {
        throw std::runtime_error("未知命令行参数选项 '" + e.get_option_name());
     }

    if (vm.count("plugin-dir")) {
        m_plugin_dir = vm["plugin-dir"].as<std::string>();
    }
    
    if (vm.count("plugin")) {
        load_plugins_from_list(vm["plugin"].as<std::vector<std::string>>());
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

bool application::impl::check_plugin_path(const std::string& plugin_name, fs::path& plugin_path) const {
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

void* application::impl::load_plugin_library(const std::string& plugin_name, const fs::path& plugin_path) {
    void* handle = dlopen(plugin_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        std::cerr << "错误: 无法加载插件 '" << plugin_name << "': " << dlerror() << std::endl;
        return nullptr;
    }
    return handle;
}

plugin* application::impl::create_plugin_instance(const std::string& plugin_name, void* handle) {
    using create_plugin_func = plugin* (*)();
    create_plugin_func create_plugin = reinterpret_cast<create_plugin_func>(dlsym(handle, "create_plugin"));
    
    if (!create_plugin) {
        std::cerr << "错误: 插件 '" << plugin_name << "' 没有导出 'create_plugin' 函数: " << dlerror() << std::endl;
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
    // 按照启动的相反顺序关闭插件
    for (auto it = m_started_plugins.rbegin(); it != m_started_plugins.rend(); ++it) {
        (*it)->shutdown();
    }

    // 按照启动的相反顺序析构插件
    for(auto it = m_started_plugins.rbegin(); it != m_started_plugins.rend(); ++it) {
        m_plugins.erase((*it)->name());
    }
    
    m_started_plugins.clear();
    m_initialized_plugins.clear();
    m_plugins.clear();

    // 卸载插件动态库前必须把配置项清除，因为这些地方可能会引用插件动态库的静态数据
    m_opts.reset();
    m_options.clear();

    // 卸载插件动态库
    for (void* handle : m_plugin_handles) {
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

application::application()
    : pimpl_(std::make_unique<impl>())
{
}

application::~application() {
    shutdown();
}

void application::run() {
    pimpl_->run();
}

application::io_context_type& application::get_io_context() {
    return pimpl_->m_io_context;
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
    if (!plugin) {
        return *this;
    }
    
    const std::string& name = plugin->name();
    if (pimpl_->m_plugins.find(name) != pimpl_->m_plugins.end()) {
        return *this;
    }
    
    pimpl_->m_plugins[name] = std::move(plugin);
    return *this;
}

plugin* application::find_plugin(const std::string& name) const {
    auto it = pimpl_->m_plugins.find(name);
    if (it != pimpl_->m_plugins.end()) {
        return it->second.get();
    }
    return nullptr;
}

application& application::initialize() {
    pimpl_->initialize_plugins();
    return *this;
}

application& application::initialize(int argc, char** argv) {
    // 加载插件
    pimpl_->load_plugins(argc, argv);

    // 加载插件后收集配置项
    pimpl_->collect_plugin_options();

    // 解析命令行参数
    pimpl_->parse_command_line(argc, argv);

    // 初始化已加载的插件
    pimpl_->initialize_plugins();
    
    return *this;
}

application& application::startup() {
    for (auto* p : pimpl_->m_initialized_plugins) {
        auto it = std::find(pimpl_->m_started_plugins.begin(), pimpl_->m_started_plugins.end(), p);
        if (it == pimpl_->m_started_plugins.end()) {
            p->startup();
            pimpl_->m_started_plugins.push_back(p);
        }
    }
    
    return *this;
}

application& application::set_plugin_dir(const fs::path& plugin_dir) {
    pimpl_->m_plugin_dir = plugin_dir;
    return *this;
}

bool application::load_plugin(const std::string& plugin_name) {
    return pimpl_->load_plugin(plugin_name);
}

void application::shutdown() {
    pimpl_->shutdown();
}

void application::quit() {
    pimpl_->m_is_quitting = true;
    pimpl_->m_work_guard.reset();
    pimpl_->m_io_context.stop();
    shutdown();
}

bool application::is_quit() const {
    return pimpl_->m_is_quitting;
}

} // namespace appbase