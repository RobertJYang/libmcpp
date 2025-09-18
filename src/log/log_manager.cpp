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

#include <algorithm>
#include <fstream>

#include <mc/filesystem.h>
#include <mc/json.h>
#include <mc/log.h>
#include <mc/log/log_level.h>
#include <mc/log/log_manager.h>
#include <mc/log/log_message.h>
#include <mc/log/logger.h>
#include <mc/reflect.h>
#include <mc/variant.h>
#include <unordered_set>
namespace mc {
namespace log {

log_manager& log_manager::instance() {
    static log_manager manager;
    return manager;
}

log_manager::log_manager() {
    logger default_logger(DEFAULT_LOGGER);

    mc::dict default_config{
        {"stream", "std_out"},
        {"use_color", true},
        {"flush", true},
        {"level_colors", mc::variants{mc::dict{{"level", "debug"}, {"color", "cyan"}},
                                      mc::dict{{"level", "info"}, {"color", "green"}},
                                      mc::dict{{"level", "warn"}, {"color", "blue"}},
                                      mc::dict{{"level", "error"}, {"color", "red"}},
                                      mc::dict{{"level", "fatal"}, {"color", "magenta"}}}}};
    appender_ptr console_appender =
        appender_factory::instance().create("default_console", "console", default_config);

    if (console_appender) {
        default_logger.add_appender(console_appender);
    }

    appender_ptr file_appender =
        appender_factory::instance().create("default_file", "file", {});

    if (file_appender) {
        default_logger.add_appender(file_appender);
    }

    default_logger.set_level(mc::log::level::info);
    m_loggers[DEFAULT_LOGGER] = default_logger;
}

logger log_manager::get_logger(const char* name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_loggers.find(name);
    if (it != m_loggers.end()) {
        return it->second;
    }

    logger new_logger(name);
    m_loggers.emplace(name, name);
    return new_logger;
}

bool log_manager::load_appender(const std::string& lib_path, const std::string& appender_name) {
    return appender_factory::instance().load(lib_path, appender_name);
}

void log_manager::load_appenders(const std::string& dir_path) {
    appender_factory::instance().load_all(dir_path);
}

bool log_manager::load_single_appender(const appender_config& app_config) {
    if (!app_config.lib_path.empty()) {
        if (!appender_factory::instance().load(app_config.lib_path, app_config.type)) {
            elog("Failed to load appender[${name}] dynamic library: ${lib_path}",
                 ("name", app_config.name)("lib_path", app_config.lib_path));
            return false;
        }
    }

    appender_ptr appender = appender_factory::instance().get_or_create_appender(
        app_config.name, app_config.type, app_config.properties);

    if (!appender) {
        elog("Failed to create or configure appender[${name}]", ("name", app_config.name));
        return false;
    }

    return true;
}

bool log_manager::load_appenders_from_config(const std::vector<appender_config>& appender_configs) {
    bool has_error = false;
    for (const auto& app_config : appender_configs) {
        if (!load_single_appender(app_config)) {
            has_error = true;
            continue;
        }
    }

    return !has_error;
}

void log_manager::update_existing_logger(logger& log, const logger_config& log_config) {
    log.set_level(log_config.level);

    // 找出要删除的appender（在当前列表中但不在配置中）
    std::vector<std::string> to_remove;
    for (const auto& appender : log.get_appenders()) {
        const std::string& name = appender->get_name();
        if (std::find(log_config.appenders.begin(), log_config.appenders.end(), name) ==
            log_config.appenders.end()) {
            to_remove.push_back(name);
        }
    }

    // 找出要新增的appender（在配置中但不在当前列表中）
    std::vector<std::string> to_add;
    for (const auto& config_name : log_config.appenders) {
        if (!log.find_appender(config_name)) {
            to_add.push_back(config_name);
        }
    }

    // 如果没有需要删除或新增的appender，则不需要更新
    if (to_remove.empty() && to_add.empty()) {
        return;
    }

    // 删除需要删除的appender
    for (const auto& name : to_remove) {
        if (log.remove_appender(name)) {
            ilog("Removed appender[${name}] from logger[${logger}]",
                 ("logger", log.get_name())("name", name));
        }
    }

    // 添加需要新增的appender
    for (const auto& name : to_add) {
        appender_ptr appender = appender_factory::instance().get_appender(name);
        if (appender) {
            log.add_appender(appender);
            ilog("Added appender[${name}] to logger[${logger}]",
                 ("logger", log.get_name())("name", name));
        } else {
            wlog("Failed to add appender to logger[${logger}]: appender[${name}] not found",
                 ("logger", log.get_name())("name", name));
        }
    }
}

logger log_manager::create_new_logger(const logger_config& log_config) {
    logger new_logger(log_config.name);
    new_logger.set_level(log_config.level);

    for (const auto& app_name : log_config.appenders) {
        appender_ptr appender = appender_factory::instance().get_appender(app_name);
        if (appender) {
            new_logger.add_appender(appender);
            ilog("Added appender[${name}] to new logger[${logger}]",
                 ("logger", new_logger.get_name())("name", app_name));
        } else {
            wlog("Failed to add appender to new logger[${logger}]: appender[${name}] not found",
                 ("logger", new_logger.get_name())("name", app_name));
        }
    }

    return new_logger;
}

bool log_manager::apply_config(const logging_config& config) {
    // 加载appender配置
    if (!load_appenders_from_config(config.appenders)) {
        wlog("Some appenders failed to load, continuing with other configurations");
    }

    // 更新现有logger
    for (const auto& log_config : config.loggers) {
        try {
            auto it = m_loggers.find(log_config.name);
            if (it != m_loggers.end()) {
                update_existing_logger(it->second, log_config);
            } else {
                m_loggers[log_config.name] = create_new_logger(log_config);
            }
        } catch (const std::exception& e) {
            elog("Failed to update logger[${name}] configuration: ${error}",
                 ("name", log_config.name)("error", e.what()));
            continue;
        }
    }

    return true;
}

} // namespace log
} // namespace mc

MC_REFLECT(mc::log::appender_config, (name)(type)(lib_path)(properties))
MC_REFLECT(mc::log::logger_config, (name)(level)(appenders))
MC_REFLECT(mc::log::logging_config, (appenders)(loggers))
