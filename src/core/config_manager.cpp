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

#include <mc/core/config_manager.h>
#include <mc/core/config_schema.h>
#include <mc/filesystem.h>
#include <mc/exception.h>
#include <fstream>
#include <thread>
#include <sstream>

namespace mc {

namespace po = boost::program_options;

// JSON配置加载器实现
variant json_config_loader::load(const std::string& file_path) {
    if (!mc::filesystem::exists(file_path)) {
        MC_THROW(file_not_found_exception, "配置文件不存在: ${path}", ("path", file_path));
    }

    std::ifstream file(file_path);
    if (!file.is_open()) {
        MC_THROW(file_open_exception, "无法打开配置文件: ${path}", ("path", file_path));
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    try {
        // 解析JSON内容
        return json::json_decode(content);
    } catch (const std::exception& e) {
        MC_THROW(parse_error_exception, "解析JSON配置文件失败: ${error}", ("error", e.what()));
    }
}

// TOML配置加载器实现（预留）
variant toml_config_loader::load(const std::string& file_path) {
    // TODO: 实现TOML配置加载
    MC_THROW(mc::not_implemented_exception, "TOML配置加载器尚未实现");
}

// 构造函数
config_manager::config_manager()
    : m_loader(std::make_unique<json_config_loader>())
    , m_opts("配置选项")
    , m_config_file("./config.json")
    , m_plugin_dir("./plugins")
    , m_thread_count(std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 1) {
    
    // 定义命令行选项
    m_opts.add_options()
        ("help,h", "显示帮助信息")
        ("version,v", "显示版本信息")
        ("config,c", po::value<std::string>()->default_value("./config.json"), "配置文件路径")
        ("plugin-dir", po::value<std::string>(), "插件目录路径")
        ("plugin,p", po::value<std::vector<std::string>>()->composing(), "要加载的插件列表")
        ("threads,t", po::value<unsigned int>()->default_value(m_thread_count), "线程数量");
}

// 析构函数
config_manager::~config_manager() = default;

// 根据文件扩展名选择合适的加载器
std::unique_ptr<config_loader> config_manager::create_loader(const std::string& file_path) const {
    std::string ext = mc::filesystem::extension(file_path);
    if (ext == ".json") {
        return std::make_unique<json_config_loader>();
    } else if (ext == ".toml") {
        return std::make_unique<toml_config_loader>();
    }
    
    // 默认使用JSON加载器
    return std::make_unique<json_config_loader>();
}

// 解析命令行参数
bool config_manager::parse_command_line(int argc, char** argv) {
    try {
        po::parsed_options parsed = po::command_line_parser(argc, argv)
            .options(m_opts)
            .allow_unregistered()
            .run();
        
        po::store(parsed, m_variables);
        po::notify(m_variables);
        
        // 处理未识别的选项
        std::vector<std::string> unrecognized = po::collect_unrecognized(parsed.options, po::include_positional);
        if (!unrecognized.empty()) {
            wlog("警告: 未识别的命令行选项:");
            for (const auto& opt : unrecognized) {
                wlog("- ${option}", ("option", opt));
            }
        }
        
        // 更新配置文件路径
        if (m_variables.count("config")) {
            m_config_file = m_variables["config"].as<std::string>();
        }
        
        // 更新插件目录
        if (m_variables.count("plugin-dir")) {
            m_plugin_dir = m_variables["plugin-dir"].as<std::string>();
        }
        
        // 更新线程数
        if (m_variables.count("threads")) {
            m_thread_count = m_variables["threads"].as<unsigned int>();
        }
        
        return true;
    } catch (const po::error& e) {
        elog("解析命令行参数失败: ${error}", ("error", e.what()));
        return false;
    }
}

// 加载配置文件
bool config_manager::load_config_file(const std::string& file_path) {
    std::string path = file_path.empty() ? m_config_file : file_path;
    
    try {
        // 根据文件扩展名选择合适的加载器
        m_loader = create_loader(path);
        
        // 尝试加载配置文件
        variant config = m_loader->load(path);
        
        // 检查配置类型
        if (config.is_array()) {
            // 多资源配置
            for (const auto& item : config.as<std::vector<variant>>()) {
                process_config(item);
            }
        } else if (config.is_dict()) {
            // 单资源配置
            process_config(config);
        } else {
            elog("配置文件格式错误: 根节点必须是对象或数组");
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        wlog("加载配置文件失败: ${error}，将使用默认配置", ("error", e.what()));
        return false;
    }
}

// 处理单个配置对象
void config_manager::process_config(const variant& config) {
    if (!config.is_dict()) {
        wlog("跳过非对象配置项");
        return;
    }
    
    auto dict_config = config.as<mc::dict>();
    if (!dict_config.contains("kind") || !dict_config.contains("api_version")) {
        wlog("跳过缺少必要字段(kind, api_version)的配置项");
        return;
    }
    
    std::string kind = dict_config["kind"].as<std::string>();
    
    // 验证配置
    bool valid = false;
    try {
        if (kind == "Application") {
            config::app_config app;
            from_variant(config, app);
            valid = config::config_validator::validate_app_config(app);
        } else if (kind == "Supervisor") {
            config::supervisor_config supervisor;
            from_variant(config, supervisor);
            valid = config::config_validator::validate_supervisor_config(supervisor);
        } else if (kind == "Service") {
            config::service_config service;
            from_variant(config, service);
            valid = config::config_validator::validate_service_config(service);
        } else if (kind == "Plugin") {
            config::plugin_config plugin;
            from_variant(config, plugin);
            valid = config::config_validator::validate_plugin_config(plugin);
        } else {
            MC_THROW(mc::parse_error_exception, "未知类型：${kind}", ("kind", kind));
        }
    } catch (const std::exception& e) {
        elog("配置验证失败: ${error}", ("error", e.what()));
        return;
    }
    
    if (!valid) {
        return;
    }
    
    m_configs[kind].push_back(config);
    
    if (kind == "Application") {
        // 应用程序配置特殊处理
        config::app_config app;
        try {
            from_variant(config, app);
            // 更新插件目录和线程数量
            if (!app.plugin_dir.empty()) {
                m_plugin_dir = app.plugin_dir;
            }
            if (app.threads > 0) {
                m_thread_count = app.threads;
            }
            // 更新插件列表
            if (!app.plugins.empty()) {
                m_plugin_names = app.plugins;
            }
        } catch (const std::exception& e) {
            elog("解析应用程序配置失败: ${error}", ("error", e.what()));
        }
    }
}

// 获取插件列表
std::vector<std::string> config_manager::get_plugin_names() const {
    // 首先检查命令行参数
    if (m_variables.count("plugin")) {
        return m_variables["plugin"].as<std::vector<std::string>>();
    }
    
    // 然后检查配置文件中的应用程序配置
    if (!m_plugin_names.empty()) {
        return m_plugin_names;
    }
    
    return {};
}

// 获取插件目录
std::string config_manager::get_plugin_dir() const {
    return m_plugin_dir;
}

// 获取线程数
unsigned int config_manager::get_thread_count() const {
    return m_thread_count;
}

// 添加配置
bool config_manager::add_config(const variant& config) {
    try {
        process_config(config);
        return true;
    } catch (const std::exception& e) {
        elog("添加配置失败: ${error}", ("error", e.what()));
        return false;
    }
}

} // namespace mc 