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
#include <mc/exception.h>
#include <mc/filesystem.h>
#include <mc/log.h>

#include <fstream>
#include <sstream>

namespace mc::core {

namespace po = boost::program_options;

variant json_config_loader::load(const std::string& file_path) {
    if (!mc::filesystem::exists(file_path)) {
        MC_THROW(file_not_found_exception, "config file not found: ${path}", ("path", file_path));
    }

    std::ifstream file(file_path);
    if (!file.is_open()) {
        MC_THROW(file_open_exception, "failed to open config file: ${path}", ("path", file_path));
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    try {
        return json::json_decode(content);
    } catch (const std::exception& e) {
        MC_THROW(parse_error_exception, "failed to parse json config file: ${error}",
                 ("error", e.what()));
    }
}

variant toml_config_loader::load(const std::string& file_path) {
    MC_THROW(mc::not_implemented_exception, "toml config loader not implemented");
}

config_manager::config_manager()
    : m_loader(std::make_unique<json_config_loader>()), m_opts("config options"),
      m_config_file("./config.json"), m_plugin_dir("./plugins") {
    m_opts.add_options()("help,h", "show help info")("version,v", "show version info")(
        "config,c", po::value<std::string>()->default_value("./config.json"),
        "config file path")("plugin-dir", po::value<std::string>(), "plugin directory path")(
        "plugin,p", po::value<std::vector<std::string>>()->composing(),
        "plugin list")("threads,t", po::value<unsigned int>()->default_value(0), "thread count");
}

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

bool config_manager::parse_command_line(int argc, char** argv) {
    try {
        po::parsed_options parsed =
            po::command_line_parser(argc, argv).options(m_opts).allow_unregistered().run();

        po::store(parsed, m_variables);
        po::notify(m_variables);

        // 处理未识别的选项
        std::vector<std::string> unrecognized =
            po::collect_unrecognized(parsed.options, po::include_positional);
        if (!unrecognized.empty()) {
            wlog("warning: unrecognized command line options:");
            for (const auto& opt : unrecognized) {
                wlog("- ${option}", ("option", opt));
            }
        }

        if (m_variables.count("config")) {
            m_config_file = m_variables["config"].as<std::string>();
        }

        if (m_variables.count("plugin-dir")) {
            m_plugin_dir = m_variables["plugin-dir"].as<std::string>();
        }

        if (m_variables.count("threads")) {
            m_thread_count = m_variables["threads"].as<unsigned int>();
        }

        return true;
    } catch (const po::error& e) {
        elog("failed to parse command line arguments: ${error}", ("error", e.what()));
        return false;
    }
}

bool config_manager::load_config_file(const std::string& file_path) {
    std::string path = file_path.empty() ? m_config_file : file_path;

    try {
        m_loader = create_loader(path);

        variant config = m_loader->load(path);

        if (config.is_array()) {
            for (const auto& item : config.as<mc::variants>()) {
                process_config(item);
            }
        } else if (config.is_dict()) {
            process_config(config);
        } else {
            elog("config file format error: root node must be an object or an array");
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        wlog("failed to load config file: ${error}, using default config", ("error", e.what()));
        return false;
    }
}

void config_manager::process_config(const variant& config) {
    if (!config.is_dict()) {
        wlog("skip non-object config item");
        return;
    }

    auto dict_config = config.as<mc::dict>();
    if (!dict_config.contains("kind") || !dict_config.contains("api_version")) {
        wlog("skip config item with missing required fields(kind, api_version)");
        return;
    }

    std::string kind = dict_config["kind"].as<std::string>();
    if (!validate_config(kind, config)) {
        return;
    }

    m_configs[kind].push_back(config);

    if (kind == "Application") {
        process_app_config(config);
    } else if (kind == "Logging") {
        process_logging_config(config);
    }
}

void config_manager::process_app_config(const variant& config) {
    config::app_config app;
    try {
        from_variant(config, app);
        if (!app.plugin_dir.empty()) {
            m_plugin_dir = app.plugin_dir;
        }
        if (app.threads > 0) {
            m_thread_count = app.threads;
        }
        if (!app.plugins.empty()) {
            m_plugin_names = app.plugins;
        }
    } catch (const std::exception& e) {
        elog("failed to parse application config: ${error}", ("error", e.what()));
    }
}

void config_manager::process_logging_config(const variant& config) {
    try {
        mc::log::logging_config log_config;
        from_variant(config, log_config);

        // 直接应用日志配置
        mc::log::log_manager::instance().apply_config(log_config);
        ilog("Logging configuration loaded and applied from config file");
    } catch (const std::exception& e) {
        elog("failed to parse logging config: ${error}", ("error", e.what()));
    }
}

bool config_manager::validate_config(const std::string& kind, const variant& config) {
    try {
        if (kind == "Application") {
            config::app_config app;
            from_variant(config, app);
            return config::config_validator::validate_app_config(app);
        } else if (kind == "Supervisor") {
            config::supervisor_config supervisor;
            from_variant(config, supervisor);
            return config::config_validator::validate_supervisor_config(supervisor);
        } else if (kind == "Service") {
            config::service_config service;
            from_variant(config, service);
            return config::config_validator::validate_service_config(service);
        } else if (kind == "Plugin") {
            config::plugin_config plugin;
            from_variant(config, plugin);
            return config::config_validator::validate_plugin_config(plugin);
        } else if (kind == "Logging") {
            // Logging配置不需要特殊验证，直接返回true
            return true;
        } else {
            MC_THROW(mc::parse_error_exception, "unknown type: ${kind}", ("kind", kind));
        }
    } catch (const std::exception& e) {
        elog("config validation failed: ${error}", ("error", e.what()));
        return false;
    }
}

std::vector<std::string> config_manager::get_plugin_names() const {
    if (m_variables.count("plugin")) {
        return m_variables["plugin"].as<std::vector<std::string>>();
    }

    if (!m_plugin_names.empty()) {
        return m_plugin_names;
    }

    return {};
}

std::string config_manager::get_plugin_dir() const {
    return m_plugin_dir;
}

unsigned int config_manager::get_thread_count() const {
    return m_thread_count;
}

bool config_manager::add_config(const variant& config) {
    try {
        process_config(config);
        return true;
    } catch (const std::exception& e) {
        elog("failed to add config: ${error}", ("error", e.what()));
        return false;
    }
}

} // namespace mc::core

// 反射元数据定义
MC_REFLECT(mc::config::metadata, (name)(labels)(annotations))
MC_REFLECT(mc::config::resource_base, (api_version)(kind)(meta))
MC_REFLECT(mc::config::app_config, MC_BASE_CLASS(mc::config::resource_base),
           (plugin_dir)(plugins)(threads)(work_threads))
MC_REFLECT_ENUM(mc::config::supervisor_strategy, (one_for_one)(one_for_all)(rest_for_one))
MC_REFLECT(mc::config::supervisor_config, MC_BASE_CLASS(mc::config::resource_base),
           (strategy)(max_restarts)(services))
MC_REFLECT(mc::config::service_config, MC_BASE_CLASS(mc::config::resource_base),
           (type)(dependencies)(properties))
MC_REFLECT(mc::config::plugin_config, MC_BASE_CLASS(mc::config::resource_base),
           (version)(properties))
