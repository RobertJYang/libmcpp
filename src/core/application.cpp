/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
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
#include "mc/core/default_supervisor.h"
#include <algorithm>
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

    // 版本和配置管理
    void set_version(const std::string& version) { m_version = version; }
    const std::string& version() const { return m_version; }
    void set_config_dir(const fs::path& config_dir) { m_config_dir = config_dir; }
    const fs::path& config_dir() const { return m_config_dir; }
    void set_module_dir(const fs::path& module_dir) { m_module_dir = module_dir; }
    const fs::path& module_dir() const { return m_module_dir; }

    // 模块管理
    bool register_module(module_ptr module);
    module* find_module(const std::string& name) const;
    bool load_module(const std::string& name);
    bool unload_module(const std::string& name);

    // 服务管理
    bool register_service(const std::string& type, std::function<service_ptr()> factory);
    service_ptr create_service(const std::string& type, const service_config& config);
    service_ptr get_service(const std::string& name) const;

    // 监督器管理
    supervisor_ptr get_root_supervisor() const { return m_root_supervisor; }
    supervisor_ptr create_supervisor(const supervisor_config& config);

    // 应用程序生命周期
    void initialize();
    void initialize(int argc, char** argv);
    void start();
    void exec();
    void stop();
    void cleanup();
    bool is_stopped() const { return m_is_stopped; }

    // IO上下文和执行器
    io_context_type& get_io_context() { return m_io_context; }
    priority_executor_type& get_priority_executor() { return m_priority_executor; }

private:
    // 配置管理
    void parse_command_line(int argc, char** argv);
    void collect_module_options();
    void load_config_file();

    // 模块依赖管理
    bool check_module_dependencies(const module_ptr& module) const;
    std::vector<module_ptr> get_dependent_modules(const module_ptr& module) const;

    // 成员变量
    std::string m_version;    // 应用程序版本号
    fs::path m_config_dir;    // 配置文件目录
    fs::path m_module_dir;    // 模块目录

    // 模块管理
    std::unordered_map<std::string, module_ptr> m_modules;  // 模块映射表
    std::vector<void*> m_module_handles;                    // 动态库句柄列表

    // 服务管理
    std::unordered_map<std::string, std::function<service_ptr()>> m_service_factories;  // 服务工厂映射表
    supervisor_ptr m_root_supervisor;  // 根监督器

    // 配置管理
    struct options {
        po::options_description cli{"命令行配置项"};   // 命令行配置项
        po::options_description cfg{"配置文件配置项"}; // 配置文件配置项
    };
    std::unique_ptr<options> m_opts;
    po::variables_map m_options;  // 所有配置项

    // IO和线程管理
    io_context_type m_io_context;
    priority_executor_type m_priority_executor;
    std::unique_ptr<work_guard_type> m_work_guard;
    std::vector<std::thread> m_threads;
    bool m_is_stopped{false};

    // 新增方法
    void load_modules_from_list(const std::vector<std::string>& modules);
    bool load_dynamic_module(const std::string& module_name);
};

// 构造函数
application::impl::impl()
    : m_priority_executor(m_io_context),
      m_work_guard(std::make_unique<work_guard_type>(boost::asio::make_work_guard(m_io_context))) {
    
    // 创建根监督器
    supervisor_config root_config;
    root_config.name = "root";
    root_config.strategy = supervisor_strategy::one_for_one;
    root_config.max_restarts = 3;
    root_config.restart_period = std::chrono::seconds(5);

    m_root_supervisor = std::make_shared<default_supervisor>();
    m_root_supervisor->init(root_config);
}

// 析构函数
application::impl::~impl() {
    cleanup();
}

// 注册模块
bool application::impl::register_module(module_ptr module) {
    if (!module) {
        return false;
    }

    const auto& name = module->get_info().name;
    if (m_modules.find(name) != m_modules.end()) {
        return false;
    }

    m_modules[name] = module;
    return true;
}

// 查找模块
module* application::impl::find_module(const std::string& name) const {
    auto it = m_modules.find(name);
    return it != m_modules.end() ? it->second.get() : nullptr;
}

// 加载模块
bool application::impl::load_module(const std::string& name) {
    auto it = m_modules.find(name);
    if (it == m_modules.end()) {
        return false;
    }

    if (!check_module_dependencies(it->second)) {
        return false;
    }

    return it->second->load();
}

// 卸载模块
bool application::impl::unload_module(const std::string& name) {
    auto it = m_modules.find(name);
    if (it == m_modules.end()) {
        return false;
    }

    // 检查是否有其他模块依赖此模块
    auto dependents = get_dependent_modules(it->second);
    if (!dependents.empty()) {
        return false;
    }

    // 执行卸载
    it->second->pre_unload();
    bool result = it->second->unload();
    if (result) {
        m_modules.erase(it);
    }
    return result;
}

// 注册服务工厂
bool application::impl::register_service(const std::string& type, std::function<service_ptr()> factory) {
    if (m_service_factories.find(type) != m_service_factories.end()) {
        return false;
    }

    m_service_factories[type] = factory;
    return true;
}

// 创建服务
service_ptr application::impl::create_service(const std::string& type, const service_config& config) {
    auto it = m_service_factories.find(type);
    if (it == m_service_factories.end()) {
        return nullptr;
    }

    auto service = it->second();
    if (!service || !service->init(config)) {
        return nullptr;
    }

    return service;
}

// 获取服务
service_ptr application::impl::get_service(const std::string& name) const {
    return m_root_supervisor->get_service(name);
}

// 创建监督器
supervisor_ptr application::impl::create_supervisor(const supervisor_config& config) {
    auto supervisor = std::make_shared<default_supervisor>();
    if (!supervisor->init(config)) {
        return nullptr;
    }
    return supervisor;
}

// 初始化应用程序
void application::impl::initialize() {
    // 初始化所有模块
    for (auto& [name, module] : m_modules) {
        if (load_module(name)) {
            module->post_load();
        }
    }

    // 启动根监督器
    m_root_supervisor->start();
}

// 使用命令行参数初始化
void application::impl::initialize(int argc, char** argv) {
    parse_command_line(argc, argv);
    
    // 加载命令行指定的模块
    if (m_options.count("module")) {
        load_modules_from_list(m_options["module"].as<std::vector<std::string>>());
    }
    
    initialize();
}

// 启动应用程序
void application::impl::start() {
    m_is_stopped = false;

    // 创建工作线程
    const auto thread_count = std::thread::hardware_concurrency();
    m_threads.reserve(thread_count);

    // 启动工作线程
    for (size_t i = 0; i < thread_count; ++i) {
        m_threads.emplace_back([this]() {
            m_io_context.run();
        });
    }
}

// 执行应用程序
void application::impl::exec() {
    // 主线程运行IO上下文
    m_io_context.run();
}

// 停止应用程序
void application::impl::stop() {
    m_is_stopped = true;
    m_root_supervisor->stop();
    
    if (m_work_guard) {
        m_work_guard.reset();
    }
    m_io_context.stop();
}

// 清理资源
void application::impl::cleanup() {
    // 停止所有服务和监督器
    m_root_supervisor->cleanup();

    // 卸载所有模块
    for (auto& [name, module] : m_modules) {
        module->pre_unload();
        module->unload();
        module->cleanup_resources();
    }
    m_modules.clear();

    // 等待所有线程结束
    for (auto& thread : m_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_threads.clear();
}

// 检查模块依赖
bool application::impl::check_module_dependencies(const module_ptr& module) const {
    for (const auto& dep_name : module->get_info().dependencies) {
        auto it = m_modules.find(dep_name);
        if (it == m_modules.end()) {
            return false;
        }
    }
    return true;
}

// 获取依赖此模块的其他模块
std::vector<module_ptr> application::impl::get_dependent_modules(const module_ptr& module) const {
    std::vector<module_ptr> dependents;
    const auto& module_name = module->get_info().name;

    for (const auto& [name, other] : m_modules) {
        const auto& deps = other->get_info().dependencies;
        if (std::find(deps.begin(), deps.end(), module_name) != deps.end()) {
            dependents.push_back(other);
        }
    }

    return dependents;
}

// 从列表加载模块
void application::impl::load_modules_from_list(const std::vector<std::string>& modules) {
    for (const auto& module_arg : modules) {
        // 支持逗号分隔的模块列表
        std::vector<std::string> module_names;
        std::string::size_type start = 0;
        std::string::size_type pos = module_arg.find(',');
        
        while (pos != std::string::npos) {
            module_names.push_back(module_arg.substr(start, pos - start));
            start = pos + 1;
            pos = module_arg.find(',', start);
        }
        
        // 添加最后一个模块名
        if (start < module_arg.length()) {
            module_names.push_back(module_arg.substr(start));
        }
        
        // 加载每个模块
        for (const auto& name : module_names) {
            if (!name.empty()) {
                load_dynamic_module(name);
            }
        }
    }
}

// 动态加载模块
bool application::impl::load_dynamic_module(const std::string& module_name) {
    // 检查模块是否已加载
    if (m_modules.find(module_name) != m_modules.end()) {
        std::cout << "模块 '" << module_name << "' 已加载" << std::endl;
        return true;
    }
    
    // 检查模块路径
    if (m_module_dir.empty()) {
        std::cerr << "错误: 未设置模块目录，无法加载模块 '" << module_name << "'" << std::endl;
        return false;
    }
    
    // 构建模块库路径
    fs::path module_path = m_module_dir / ("lib" + module_name + ".so");
    if (!fs::exists(module_path)) {
        std::cerr << "错误: 模块文件 '" << module_path << "' 不存在" << std::endl;
        return false;
    }
    
    // 加载动态库
    void* handle = dlopen(module_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        std::cerr << "错误: 无法加载模块 '" << module_name << "': " << dlerror() << std::endl;
        return false;
    }
    
    // 查找创建模块的函数
    using create_module_func = module* (*)();
    create_module_func create_module = 
        reinterpret_cast<create_module_func>(dlsym(handle, "create_module"));
    
    if (!create_module) {
        std::cerr << "错误: 模块 '" << module_name << "' 没有导出 'create_module' 函数: " 
                  << dlerror() << std::endl;
        dlclose(handle);
        return false;
    }
    
    // 创建模块实例
    module* module_instance = create_module();
    if (!module_instance) {
        std::cerr << "错误: 无法创建模块 '" << module_name << "' 的实例" << std::endl;
        dlclose(handle);
        return false;
    }
    
    // 注册模块
    module_ptr module_ptr(module_instance);
    if (!register_module(module_ptr)) {
        std::cerr << "错误: 无法注册模块 '" << module_name << "'" << std::endl;
        dlclose(handle);
        return false;
    }
    
    // 保存动态库句柄
    m_module_handles.push_back(handle);
    
    std::cout << "成功加载模块 '" << module_name << "'" << std::endl;
    return true;
}

// Application类的实现
application& application::instance() {
    static application instance;
    return instance;
}

application::application() : pimpl_(std::make_unique<impl>()) {}
application::~application() = default;

void application::set_version(const std::string& version) {
    pimpl_->set_version(version);
}

const std::string& application::version() const {
    return pimpl_->version();
}

void application::set_config_dir(const fs::path& config_dir) {
    pimpl_->set_config_dir(config_dir);
}

const fs::path& application::config_dir() const {
    return pimpl_->config_dir();
}

void application::set_module_dir(const fs::path& module_dir) {
    pimpl_->set_module_dir(module_dir);
}

const fs::path& application::module_dir() const {
    return pimpl_->module_dir();
}

application& application::register_module(module_ptr module) {
    pimpl_->register_module(std::move(module));
    return *this;
}

module* application::find_module(const std::string& name) const {
    return pimpl_->find_module(name);
}

bool application::load_module(const std::string& name) {
    return pimpl_->load_module(name);
}

bool application::unload_module(const std::string& name) {
    return pimpl_->unload_module(name);
}

bool application::register_service(const std::string& type, std::function<service_ptr()> factory) {
    return pimpl_->register_service(type, factory);
}

service_ptr application::create_service(const std::string& type, const service_config& config) {
    return pimpl_->create_service(type, config);
}

service_ptr application::get_service(const std::string& name) const {
    return pimpl_->get_service(name);
}

supervisor_ptr application::get_root_supervisor() const {
    return pimpl_->get_root_supervisor();
}

supervisor_ptr application::create_supervisor(const supervisor_config& config) {
    return pimpl_->create_supervisor(config);
}

application& application::initialize() {
    pimpl_->initialize();
    return *this;
}

application& application::initialize(int argc, char** argv) {
    pimpl_->initialize(argc, argv);
    return *this;
}

application& application::start() {
    pimpl_->start();
    return *this;
}

void application::exec() {
    pimpl_->exec();
}

void application::stop() {
    pimpl_->stop();
}

void application::cleanup() {
    pimpl_->cleanup();
}

bool application::is_stopped() const {
    return pimpl_->is_stopped();
}

application::io_context_type& application::get_io_context() {
    return pimpl_->get_io_context();
}

application::priority_executor_type& application::get_priority_executor() {
    return pimpl_->get_priority_executor();
}

// 解析命令行参数
void application::impl::parse_command_line(int argc, char** argv) {
    m_opts = std::make_unique<options>();
    
    // 添加基本命令行选项
    m_opts->cli.add_options()
        ("help,h", "显示帮助信息")
        ("version,v", "显示版本信息")
        ("config,c", po::value<std::string>()->default_value("config.ini"), "配置文件名称（相对于配置目录）")
        ("config-dir", po::value<std::string>(), "配置目录路径")
        ("module,m", po::value<std::vector<std::string>>()->composing(), "要加载的模块名称【可以重复多个或以逗号分隔多个】")
        ("module-dir", po::value<std::string>(), "模块目录路径")
        ("threads", po::value<unsigned int>()->default_value(std::thread::hardware_concurrency()), "线程数量");

    // 收集模块选项
    collect_module_options();
    
    // 加载配置文件
    load_config_file();

    try {
        // 解析命令行参数
        po::parsed_options parsed = po::command_line_parser(argc, argv).options(m_opts->cli).run();
        po::store(parsed, m_options);
        
        // 检查未知选项
        std::vector<std::string> positionals =
            po::collect_unrecognized(parsed.options, po::include_positional);
        if (!positionals.empty()) {
            throw std::runtime_error("未知命令行参数选项 '" + positionals[0] + "'");
        }
        
        po::notify(m_options);
    } catch (const boost::program_options::unknown_option& e) {
        throw std::runtime_error("未知命令行参数选项 '" + e.get_option_name() + "'");
    }

    // 处理版本和帮助选项
    if (m_options.count("version")) {
        std::cout << "版本: " << m_version << std::endl;
        exit(0);
    }

    if (m_options.count("help")) {
        std::cout << m_opts->cli << std::endl;
        exit(0);
    }

    // 设置模块目录
    if (m_options.count("module-dir")) {
        m_module_dir = m_options["module-dir"].as<std::string>();
    }
}

// 收集模块选项
void application::impl::collect_module_options() {
    for (auto& [name, module] : m_modules) {
        po::options_description cli_opts("模块'" + name + "'命令行选项");
        po::options_description cfg_opts("模块'" + name + "'配置文件选项");
        
        // 这里可以添加模块特定的选项
        module->register_options(cli_opts, cfg_opts);
        
        if (!cli_opts.options().empty()) {
            m_opts->cli.add(cli_opts);
        }
        if (!cfg_opts.options().empty()) {
            m_opts->cli.add(cfg_opts);
            m_opts->cfg.add(cfg_opts);
        }
    }
}

// 加载配置文件
void application::impl::load_config_file() {
    if (!m_config_dir.empty() && m_opts) {
        fs::path config_file = m_config_dir / "config.ini";
        if (fs::exists(config_file)) {
            po::store(po::parse_config_file<char>(config_file.c_str(), m_opts->cfg), m_options);
            po::notify(m_options);
        }
    }
}

} // namespace mc