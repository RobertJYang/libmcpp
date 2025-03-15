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
#include "mc/string.h"
#include <algorithm>
#include <boost/program_options.hpp>
#include <dlfcn.h>
#include <iostream>
#include <unordered_map>

namespace mc {

namespace po = boost::program_options;

// 类型别名定义
using module_ptr = std::shared_ptr<class module>;
using service_ptr = std::shared_ptr<class service>;
using supervisor_ptr = std::shared_ptr<class supervisor>;

/**
 * @brief application类的实现类
 */
class application::impl {
public:
    impl()
        : m_io_context()
        , m_strand(m_io_context.get_executor()) {
        // 创建根监督器
        supervisor_config root_config;
        root_config.name = "root";
        root_config.strategy = supervisor_strategy::one_for_one;
        root_config.max_restarts = 3;
        root_config.restart_period = std::chrono::seconds(5);

        m_root_supervisor = std::make_shared<default_supervisor>();
        if (!m_root_supervisor->init(root_config)) {
            throw std::runtime_error("初始化根监督器失败");
        }

        // 添加基本选项
        m_opts = std::make_unique<options>();
        m_opts->cli.add_options()
            ("help,h", "显示帮助信息")
            ("version,v", "显示版本信息")
            ("config,c", po::value<std::string>(), "配置文件路径")
            ("module", po::value<std::vector<std::string>>(), "要加载的模块列表");
    }
    ~impl();

    // 版本和配置管理
    void set_version(const std::string& version) { m_version = version; }
    const std::string& version() const { return m_version; }
    void set_config_dir(const std::string& config_dir) { m_config_dir = config_dir; }
    const std::string& config_dir() const { return m_config_dir; }
    void set_module_dir(const std::string& module_dir) { m_module_dir = module_dir; }
    const std::string& module_dir() const { return m_module_dir; }

    // 模块管理
    bool register_module(module_ptr module);
    module* find_module(const std::string& name) const;

    // 服务管理
    bool register_service(const std::string& type, 
                         std::function<service_ptr()> factory,
                         std::function<void(po::options_description&, po::options_description&)> register_options);
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
    bool is_stopped() const { return m_stopped; }

    // IO上下文和执行器
    io_context_type& get_io_context() { return m_io_context; }
    strand_type& get_strand() { return m_strand; }

private:
    // 配置管理
    void parse_command_line(int argc, char** argv);
    void load_config_file();

    // 模块依赖管理
    bool check_module_dependencies(const module_ptr& module) const;
    std::vector<module_ptr> get_dependent_modules(const module_ptr& module) const;

    // 成员变量
    std::string m_version;    // 应用程序版本号
    std::string m_config_dir;    // 配置文件目录
    std::string m_module_dir;    // 模块目录

    // 模块管理
    std::unordered_map<std::string, std::shared_ptr<class module>> m_modules;  // 模块映射表
    std::vector<void*> m_module_handles;                    // 动态库句柄列表

    // 服务管理
    std::unordered_map<std::string, service_type_info> m_service_types;  // 服务类型注册表
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
    strand_type m_strand;
    std::vector<work_guard_type> m_work_guards;
    std::vector<std::thread> m_threads;
    bool m_stopped{false};

    // 新增方法
    void load_modules_from_list(const std::vector<std::string>& modules);
    bool load_dynamic_module(const std::string& module_name);

    // 两阶段初始化相关方法
    void parse_basic_command_line(int argc, char** argv);
    void pre_load_modules(const std::vector<std::string>& modules);
    bool pre_load_dynamic_module(const std::string& module_name);
    void initialize_modules();

    // 收集所有配置选项
    void collect_all_options();
    
    // 解析完整命令行参数
    void parse_full_command_line(int argc, char** argv);
    
    // 初始化服务
    void initialize_services();

    // 加载模块
    void load_modules(const std::vector<std::string>& module_names);

    // 启动模块
    void start_modules();

    // 停止模块
    void stop_modules();

    // 卸载模块
    void unload_modules();

    // 加载动态库并创建模块实例
    bool load_dynamic_library(const std::string& module_name, void*& handle, std::shared_ptr<class module>& module);
};

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

    m_modules[name] = std::move(module);
    return true;
}

// 查找模块
module* application::impl::find_module(const std::string& name) const {
    auto it = m_modules.find(name);
    return it != m_modules.end() ? it->second.get() : nullptr;
}

// 注册服务类型和工厂函数
bool application::impl::register_service(
    const std::string& type, std::function<service_ptr()> factory,
    std::function<void(po::options_description&, po::options_description&)> register_options) {
    if (m_service_types.find(type) != m_service_types.end()) {
        std::cerr << "服务类型 '" << type << "' 已注册" << std::endl;
        return false;
    }
    
    m_service_types[type] = {factory, register_options};
    std::cout << "注册服务类型: " << type << std::endl;
    return true;
}

// 收集所有配置选项
void application::impl::collect_all_options() {
    for (const auto& [type, info] : m_service_types) {
        po::options_description cli_opts("服务'" + type + "'命令行选项");
        po::options_description cfg_opts("服务'" + type + "'配置文件选项");
        
        // 调用服务类型的注册选项函数
        info.register_options(cli_opts, cfg_opts);
        
        if (!cli_opts.options().empty()) {
            m_opts->cli.add(cli_opts);
        }
        if (!cfg_opts.options().empty()) {
            m_opts->cfg.add(cfg_opts);
        }
    }
}

// 创建服务实例
service_ptr application::impl::create_service(const std::string& type, const service_config& config) {
    auto it = m_service_types.find(type);
    if (it == m_service_types.end()) {
        std::cerr << "未找到服务类型 '" << type << "'" << std::endl;
        return nullptr;
    }
    
    auto service = it->second.factory();
    if (!service) {
        std::cerr << "创建服务实例失败: " << type << std::endl;
        return nullptr;
    }
    
    if (!service->init(config)) {
        std::cerr << "初始化服务失败: " << type << std::endl;
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
    // 初始化所有模块（如果没有通过命令行参数加载）
    bool has_modules_from_cmd = m_options.count("module") > 0;
    
    if (!has_modules_from_cmd) {
        // 启动根监督器
        m_root_supervisor->start();
    }
}

// 使用命令行参数初始化
void application::impl::initialize(int argc, char** argv) {
    parse_basic_command_line(argc, argv);
    if (m_options.count("module")) {
        load_modules(m_options["module"].as<std::vector<std::string>>());
    }
    
    // 第二阶段：配置解析
    collect_all_options();
    load_config_file();
    parse_full_command_line(argc, argv);
    
    // 第三阶段：启动
    start_modules();
    
    // 启动根监督器
    m_root_supervisor->start();
}

// 启动应用程序
void application::impl::start() {
    m_stopped = false;

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
    m_stopped = true;
    m_root_supervisor->stop();
    
    if (m_work_guards.size() > 0) {
        for (auto& guard : m_work_guards) {
            guard.reset();
        }
    }
    m_io_context.stop();
}

// 清理资源
void application::impl::cleanup() {
    // 停止所有服务和监督器
    m_root_supervisor->cleanup();

    m_service_types.clear();
    m_opts.reset();

    // 卸载所有模块
    unload_modules();

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
    std::string module_path = mc::filesystem::join(m_module_dir, "lib" + module_name + ".so");
    if (!mc::filesystem::exists(module_path)) {
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

// 加载配置文件
void application::impl::load_config_file() {
    if (!m_config_dir.empty() && m_opts) {
        std::string config_file = mc::filesystem::join(m_config_dir, "config.ini");
        if (mc::filesystem::exists(config_file)) {
            try {
                po::store(po::parse_config_file<char>(config_file.c_str(), m_opts->cfg), m_options);
                po::notify(m_options);
            } catch (const po::error& e) {
                std::cerr << "解析配置文件失败: " << e.what() << std::endl;
            }
        }
    }
}

// 预加载模块但不初始化
void application::impl::pre_load_modules(const std::vector<std::string>& modules) {
    for (const auto& module_arg : modules) {
        // 支持逗号分隔的模块列表
        std::vector<std::string> module_names = mc::string::split(module_arg, ",");
        
        // 预加载每个模块
        for (const auto& name : module_names) {
            if (!name.empty()) {
                pre_load_dynamic_module(name);
            }
        }
    }
}

// 预加载动态模块（不调用load方法）
bool application::impl::pre_load_dynamic_module(const std::string& module_name) {
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
    std::string module_path = mc::filesystem::join(m_module_dir, "lib" + module_name + ".so");
    if (!mc::filesystem::exists(module_path)) {
        std::cerr << "错误: 模块文件 '" << module_path << "' 不存在" << std::endl;
        return false;
    }
    
    // 加载动态库
    void* handle = dlopen(module_path.c_str(), RTLD_LAZY | RTLD_GLOBAL);
    if (!handle) {
        std::string s = dlerror();
        std::cerr << "错误: 无法加载模块 '" << module_name << "': " << s << std::endl;
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
    
    // 注册模块（但不调用load方法）
    module_ptr module_ptr(module_instance);
    if (!register_module(module_ptr)) {
        std::cerr << "错误: 无法注册模块 '" << module_name << "'" << std::endl;
        dlclose(handle);
        return false;
    }
    
    // 保存动态库句柄
    m_module_handles.push_back(handle);
    
    std::cout << "成功预加载模块 '" << module_name << "'" << std::endl;
    return true;
}

// 初始化所有预加载的模块
void application::impl::initialize_modules() {
    for (auto& [name, module] : m_modules) {
        try {
            if (!module->init()) {
                std::cerr << "初始化模块 '" << name << "' 失败" << std::endl;
                continue;
            }
        } catch (const std::exception& e) {
            std::cerr << "初始化模块 '" << name << "' 失败: " << e.what() << std::endl;
        }
    }
}

// 解析基本命令行参数（第一阶段）
void application::impl::parse_basic_command_line(int argc, char** argv) {
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

    // 处理版本选项
    if (m_options.count("version")) {
        std::cout << "版本: " << m_version << std::endl;
        exit(0);
    }

    // 设置模块目录
    if (m_options.count("module-dir")) {
        m_module_dir = m_options["module-dir"].as<std::string>();
    }

    // 如果有--help选项，先加载模块并收集选项，然后再显示帮助信息
    if (m_options.count("help")) {
        // 预加载模块
        if (m_options.count("module")) {
            pre_load_modules(m_options["module"].as<std::vector<std::string>>());
        }
        
        // 显示帮助信息
        std::cout << m_opts->cli << std::endl;
        exit(0);
    }
}

// 解析完整命令行参数
void application::impl::parse_full_command_line(int argc, char** argv) {
    try {
        // 解析命令行参数
        po::parsed_options parsed = po::command_line_parser(argc, argv)
            .options(m_opts->cli)
            .allow_unregistered()
            .run();

        po::store(parsed, m_options);
        
        // 获取未识别的选项
        std::vector<std::string> unrecognized = 
            po::collect_unrecognized(parsed.options, po::include_positional);
        
        if (!unrecognized.empty()) {
            std::cerr << "警告: 未识别的选项:";
            for (const auto& opt : unrecognized) {
                std::cerr << " " << opt;
            }
            std::cerr << std::endl;
        }
        
        po::notify(m_options);
    } catch (const po::error& e) {
        std::cerr << "解析命令行参数失败: " << e.what() << std::endl;
        throw;
    }
}

// 初始化服务
void application::impl::initialize_services() {
    // 这里可以添加服务初始化逻辑
    // 例如，可以根据配置创建和初始化服务实例
}

// 加载模块
void application::impl::load_modules(const std::vector<std::string>& module_names) {
    for (const auto& name : module_names) {
        try {
            // 加载动态库并创建模块实例
            void* handle = nullptr;
            std::shared_ptr<class module> module;
            
            if (!load_dynamic_library(name, handle, module)) {
                continue;
            }
            
            // 初始化模块（注册服务）
            if (!module->init()) {
                std::cerr << "初始化模块 '" << name << "' 失败" << std::endl;
                dlclose(handle);
                continue;
            }
            
            // 保存模块和句柄
            m_modules[name] = std::move(module);
            m_module_handles.push_back(handle);
            
            std::cout << "加载模块 '" << name << "' 成功" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "加载模块 '" << name << "' 失败: " << e.what() << std::endl;
        }
    }
}

// 启动模块
void application::impl::start_modules() {
    for (auto& [name, module] : m_modules) {
        try {
            if (!module->start()) {
                std::cerr << "启动模块 '" << name << "' 失败" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "启动模块 '" << name << "' 失败: " << e.what() << std::endl;
        }
    }
}

// 停止模块
void application::impl::stop_modules() {
    for (auto& [name, module] : m_modules) {
        try {
            if (!module->stop()) {
                std::cerr << "停止模块 '" << name << "' 失败" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "停止模块 '" << name << "' 失败: " << e.what() << std::endl;
        }
    }
}

// 卸载模块
void application::impl::unload_modules() {
    // 先停止所有模块
    stop_modules();

    // 按照依赖关系的反序卸载模块
    std::vector<std::string> unload_order;
    std::unordered_set<std::string> visited;

    // 辅助函数：深度优先遍历获取卸载顺序
    std::function<void(const std::string&)> visit = [&](const std::string& name) {
        if (visited.find(name) != visited.end()) {
            return;
        }
        visited.insert(name);

        auto it = m_modules.find(name);
        if (it != m_modules.end()) {
            const auto& module = it->second;
            for (const auto& dep : module->get_info().dependencies) {
                visit(dep);
            }
            unload_order.push_back(name);
        }
    };

    // 获取卸载顺序
    for (const auto& [name, _] : m_modules) {
        visit(name);
    }

    // 按顺序卸载模块
    for (auto it = unload_order.rbegin(); it != unload_order.rend(); ++it) {
        const auto& name = *it;
        auto module_it = m_modules.find(name);
        if (module_it == m_modules.end()) {
            continue;
        }

        try {
            auto& module = module_it->second;
            if (!module->unload()) {
                std::cerr << "卸载模块 '" << name << "' 失败" << std::endl;
                continue;
            }
            
            std::cout << "卸载模块 '" << name << "' 成功" << std::endl;
            m_modules.erase(module_it);
        } catch (const std::exception& e) {
            std::cerr << "卸载模块 '" << name << "' 失败: " << e.what() << std::endl;
        }
    }

    // 最后关闭所有动态库
    for (void* handle : m_module_handles) {
        dlclose(handle);
    }
    m_module_handles.clear();
}

// 加载动态库并创建模块实例
bool application::impl::load_dynamic_library(const std::string& module_name, void*& handle, std::shared_ptr<class module>& module) {
    // 检查模块是否已加载
    if (m_modules.find(module_name) != m_modules.end()) {
        std::cout << "模块 '" << module_name << "' 已加载" << std::endl;
        return false;
    }
    
    // 检查模块路径
    if (m_module_dir.empty()) {
        std::cerr << "错误: 未设置模块目录，无法加载模块 '" << module_name << "'" << std::endl;
        return false;
    }
    
    // 构建模块库路径
    std::string module_path = mc::filesystem::join(m_module_dir, "lib" + module_name + ".so");
    if (!mc::filesystem::exists(module_path)) {
        std::cerr << "错误: 模块文件 '" << module_path << "' 不存在" << std::endl;
        return false;
    }
    
    // 加载动态库
    handle = dlopen(module_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        std::cerr << "错误: 无法加载模块 '" << module_name << "': " << dlerror() << std::endl;
        return false;
    }
    
    // 查找创建模块的函数
    using create_module_func = class module* (*)();
    create_module_func create_module = 
        reinterpret_cast<create_module_func>(dlsym(handle, "create_module"));
    
    if (!create_module) {
        std::cerr << "错误: 模块 '" << module_name << "' 没有导出 'create_module' 函数: " 
                  << dlerror() << std::endl;
        dlclose(handle);
        return false;
    }
    
    // 创建模块实例
    module = std::shared_ptr<class module>(create_module());
    if (!module) {
        std::cerr << "错误: 无法创建模块 '" << module_name << "' 的实例" << std::endl;
        dlclose(handle);
        return false;
    }
    
    return true;
}

} // namespace mc

// Application类的实现
namespace mc {

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

void application::set_config_dir(const std::string& config_dir) {
    pimpl_->set_config_dir(config_dir);
}

const std::string& application::config_dir() const {
    return pimpl_->config_dir();
}

void application::set_module_dir(const std::string& module_dir) {
    pimpl_->set_module_dir(module_dir);
}

const std::string& application::module_dir() const {
    return pimpl_->module_dir();
}

application& application::register_module(module_ptr module) {
    if (pimpl_->register_module(std::move(module))) {
        return *this;
    }
    throw std::runtime_error("注册模块失败");
}

module* application::find_module(const std::string& name) const {
    return pimpl_->find_module(name);
}

bool application::register_service(const std::string& type, 
                                 std::function<service_ptr()> factory,
                                 std::function<void(po::options_description&, po::options_description&)> register_options) {
    return pimpl_->register_service(type, factory, register_options);
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

application::strand_type& application::get_strand() {
    return pimpl_->get_strand();
}

} // namespace mc