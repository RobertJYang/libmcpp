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

#include <mc/core/application.h>
#include <mc/engine/engine.h>
#include <mc/log.h>
#include <mc/string.h>

#include <boost/program_options.hpp>

namespace mc::core {

namespace po = boost::program_options;

struct application::impl {
    impl();

    bool initialize();
    bool initialize(int argc, char** argv);

    void start();
    void exec();
    void stop();
    void cleanup();

    bool load_plugins(bool config_loaded);
    bool initialize_supervisors(bool config_loaded);
    bool initialize_services(bool config_loaded);
    void stop_all_services();

    std::string                                     m_version;            // 应用程序版本号
    std::unique_ptr<plugin_manager>                 m_plugin_manager;     // 插件管理器
    std::unique_ptr<service_factory>                m_service_factory;    // 服务工厂
    std::unique_ptr<service_manager>                m_service_manager;    // 服务管理器
    std::unique_ptr<config_manager>                 m_config_manager;     // 配置管理器
    std::unique_ptr<supervisor_manager>             m_supervisor_manager; // 监督器管理器
    std::unordered_map<std::string, supervisor_ptr> m_supervisors;        // 监督器映射表

    int  m_thread_count{1};
    bool m_stopped{false};
    bool m_running{false};
};

application::impl::impl()
    : m_version("1.0.0"), m_plugin_manager(std::make_unique<plugin_manager>()),
      m_service_factory(std::make_unique<service_factory>()),
      m_service_manager(std::make_unique<service_manager>()),
      m_config_manager(std::make_unique<config_manager>()),
      m_supervisor_manager(std::make_unique<supervisor_manager>()) {
}

application::application() : m_impl(std::make_unique<impl>()) {
}

application::~application() {
    m_impl->cleanup();
}

void application::set_version(const std::string& version) {
    m_impl->m_version = version;
}

const std::string& application::version() const {
    return m_impl->m_version;
}

plugin_manager& application::get_plugin_manager() {
    return *m_impl->m_plugin_manager;
}

service_factory& application::get_service_factory() {
    return *m_impl->m_service_factory;
}

service_manager& application::get_service_manager() {
    return *m_impl->m_service_manager;
}

config_manager& application::get_config_manager() {
    return *m_impl->m_config_manager;
}

supervisor_manager& application::get_supervisor_manager() {
    return *m_impl->m_supervisor_manager;
}

bool application::initialize() {
    return m_impl->initialize();
}

bool application::impl::initialize() {
    if (!m_supervisor_manager->init()) {
        return false;
    }

    if (!m_plugin_manager->init_plugins(*m_service_factory)) {
        wlog("some plugin init failed");
    }

    return true;
}

bool application::initialize(int argc, char** argv) {
    return m_impl->initialize(argc, argv);
}

bool application::impl::initialize(int argc, char** argv) {
    if (!m_config_manager->parse_command_line(argc, argv)) {
        return false;
    }

    bool config_loaded = m_config_manager->load_config_file();
    if (!config_loaded) {
        wlog("load config file failed, use default config");
    }

    if (!load_plugins(config_loaded)) {
        return false;
    }

    if (!initialize_supervisors(config_loaded)) {
        return false;
    }

    if (!m_plugin_manager->init_plugins(*m_service_factory)) {
        elog("plugins init failed");
        return false;
    }

    if (!initialize_services(config_loaded)) {
        return false;
    }

    return true;
}

bool application::impl::load_plugins(bool config_loaded) {
    if (config_loaded) {
        std::string plugin_dir = m_config_manager->get_plugin_dir();
        m_plugin_manager->set_plugin_dir(plugin_dir);
    }

    std::vector<std::string> plugin_names = m_config_manager->get_plugin_names();
    if (!m_plugin_manager->load_plugins(plugin_names)) {
        elog("load plugins failed");
        return false;
    }

    ilog("load plugins done, ${count} plugins", ("count", plugin_names.size()));
    return true;
}

bool application::impl::initialize_supervisors(bool config_loaded) {
    if (!config_loaded) {
        return true;
    }

    auto app_configs = m_config_manager->get_configs<config::app_config>();
    if (!app_configs.empty()) {
        m_thread_count = app_configs[0].threads;
        dlog("set thread count: ${count}", ("count", m_thread_count));
    }

    auto supervisor_configs = m_config_manager->get_configs<config::supervisor_config>();
    ilog("load supervisor configs, ${count}", ("count", supervisor_configs.size()));

    return m_supervisor_manager->initialize_from_configs(supervisor_configs);
}

bool application::impl::initialize_services(bool config_loaded) {
    const auto& services = m_config_manager->get_configs<config::service_config>();

    std::vector<std::string> service_names;
    for (const auto& service : services) {
        service_names.push_back(service.meta.name);
    }
    ilog("load service configs, ${count}: ${services}",
         ("count", services.size())("services", mc::string::join(service_names, ", ")));

    if (!config_loaded) {
        return true;
    }

    return m_service_manager->initialize_from_configs(*m_config_manager, *m_supervisor_manager,
                                                      *m_service_factory);
}

bool application::start() {
    m_impl->start();
    return true;
}

void application::impl::start() {
    if (m_running) {
        return;
    }

    m_running = true;
    if (!m_supervisor_manager->start_supervisors()) {
        wlog("start supervisors failed");
    }

    if (!m_service_manager->start_services()) {
        wlog("start services failed");
    }
    m_stopped = false;
}

void application::exec() {
    m_impl->exec();
}

void application::impl::exec() {
    ilog("start application, thread count: ${count}", ("count", m_thread_count));

    m_running    = true;
    auto& engine = mc::engine::engine::get_instance();
    engine.start(m_thread_count);
    engine.join();

    ilog("application stopped");
}

bool application::stop() {
    m_impl->stop();
    return true;
}

void application::impl::stop() {
    if (!m_running) {
        return;
    }

    m_running = false;
    if (m_stopped) {
        return;
    }

    ilog("stopping application...");

    m_supervisor_manager->stop_supervisors();
    stop_all_services();

    auto& engine = mc::engine::engine::get_instance();
    engine.stop();

    m_stopped = true;
}

void application::impl::cleanup() {
    if (!m_running) {
        return;
    }

    if (!m_stopped) {
        stop();
    }

    if (m_service_manager) {
        m_service_manager->stop_services();
    }

    if (m_supervisor_manager) {
        m_supervisor_manager->stop_supervisors();
    }

    m_supervisor_manager.reset();
    m_service_manager.reset();
    m_service_factory.reset();
    m_plugin_manager.reset();
    m_config_manager.reset();
}

bool application::is_stopped() const {
    return m_impl->m_stopped;
}

void application::impl::stop_all_services() {
    if (m_service_manager) {
        m_service_manager->stop_services();
    }
}

io_context& application::get_io_context() {
    return mc::engine::engine::get_instance().get_io_context();
}

} // namespace mc::core