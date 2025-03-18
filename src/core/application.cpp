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
#include <boost/program_options.hpp>
#include <iostream>
#include <thread>
#include <unordered_set>
#include <algorithm>
#include <mc/log.h>
#include <sstream>
#include <mc/string.h>
#include <unordered_map>
#include <queue>

namespace mc {

namespace po = boost::program_options;

// 构造函数
application::application()
    : m_version("1.0.0")
    , m_plugin_manager(std::make_unique<plugin_manager>())
    , m_service_factory(std::make_unique<service_factory>())
    , m_service_manager(std::make_unique<service_manager>())
    , m_config_manager(std::make_unique<config_manager>())
    , m_supervisor_manager(std::make_unique<supervisor_manager>())
    , m_io_context()
    , m_strand(m_io_context.get_executor())
    , m_work_guard(boost::asio::make_work_guard(m_io_context))
    , m_thread_count(std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 1)
    , m_stopped(false) {
}

// 析构函数
application::~application() {
    cleanup();
}

// 设置版本
void application::set_version(const std::string& version) {
    m_version = version;
}

// 获取版本
const std::string& application::version() const {
    return m_version;
}

// 获取插件管理器
plugin_manager& application::get_plugin_manager() {
    return *m_plugin_manager;
}

// 获取服务工厂
service_factory& application::get_service_factory() {
    return *m_service_factory;
}

// 获取服务管理器
service_manager& application::get_service_manager() {
    return *m_service_manager;
}

// 获取配置管理器
config_manager& application::get_config_manager() {
    return *m_config_manager;
}

// 获取监督器管理器
supervisor_manager& application::get_supervisor_manager() {
    return *m_supervisor_manager;
}

// 初始化
bool application::initialize() {
    // 初始化监督器管理器
    if (!m_supervisor_manager->init()) {
        return false;
    }
    
    // 初始化插件
    if (!m_plugin_manager->init_plugins(*m_service_factory)) {
        wlog("部分插件初始化失败");
    }
    
    return true;
}

bool application::initialize(int argc, char** argv) {
    // 解析命令行参数
    if (!m_config_manager->parse_command_line(argc, argv)) {
        return false;
    }

    // 加载配置文件
    bool config_loaded = m_config_manager->load_config_file();
    if (!config_loaded) {
        wlog("加载配置文件失败，将使用默认配置");
    }

    // 加载插件
    if (!load_plugins(config_loaded)) {
        return false;
    }

    // 初始化监督器
    if (!initialize_supervisors(config_loaded)) {
        return false;
    }

    // 初始化插件
    if (!m_plugin_manager->init_plugins(*m_service_factory)) {
        elog("插件初始化失败，无法继续");
        return false;
    }

    // 初始化服务
    if (!initialize_services(config_loaded)) {
        return false;
    }

    return true;
}

bool application::load_plugins(bool config_loaded) {
    if (config_loaded) {
        std::string plugin_dir = m_config_manager->get_plugin_dir();
        m_plugin_manager->set_plugin_dir(plugin_dir);
    }
    
    std::vector<std::string> plugin_names = m_config_manager->get_plugin_names();
    if (!m_plugin_manager->load_plugins(plugin_names)) {
        elog("加载插件失败");
        return false;
    }
    
    ilog("加载插件完成，共 ${count} 个插件", ("count", plugin_names.size()));
    return true;
}

bool application::initialize_supervisors(bool config_loaded) {
    if (!config_loaded) {
        return true; // 没有配置时使用默认
    }

    // 设置线程数量
    auto app_configs = m_config_manager->get_configs<config::app_config>();
    if (!app_configs.empty()) {
        m_thread_count = app_configs[0].threads;
        dlog("设置线程数: ${count}", ("count", m_thread_count));
    }
    
    // 处理监督器配置
    auto supervisor_configs = m_config_manager->get_configs<config::supervisor_config>();
    ilog("加载监督器配置，共 ${count} 个", ("count", supervisor_configs.size()));
    
    return m_supervisor_manager->initialize_from_configs(supervisor_configs, m_supervisors);
}

bool application::initialize_services(bool config_loaded) {
    // 加载服务配置
    const auto& services = m_config_manager->get_configs<config::service_config>();
    std::vector<std::string> service_names;
    for (const auto& service : services) {
        service_names.push_back(service.meta.name);
    }
    ilog("加载服务配置，共 ${count} 个: ${services}", 
         ("count", services.size())("services", mc::string::join(service_names, ", ")));

    if (!config_loaded) {
        return true; // 没有配置时使用默认
    }

    return m_service_manager->initialize_from_configs(
        m_config_manager->get_configs<config::service_config>(),
        m_supervisors,
        *m_service_factory
    );
}

// 启动
application& application::start() {
    // 启动监督器
    if (!m_supervisor_manager->start_supervisors()) {
        wlog("部分监督器启动失败");
    }
    
    // 启动服务
    if (!m_service_manager->start_services()) {
        wlog("部分服务启动失败");
    }
    
    m_stopped = false;
    return *this;
}

// 执行
void application::exec() {
    ilog("启动应用，线程数: ${count}", ("count", m_thread_count));
    
    // 创建工作线程
    std::vector<std::thread> threads;
    for (unsigned int i = 1; i < m_thread_count; ++i) {
        threads.emplace_back([this]() {
            m_io_context.run();
        });
    }
    
    // 主线程也参与工作
    m_io_context.run();
    
    // 等待所有线程结束
    for (auto& t : threads) {
        t.join();
    }
    
    ilog("应用已停止");
}

// 停止
void application::stop() {
    if (m_stopped) {
        return;
    }
    
    ilog("正在停止应用...");
    
    // 停止监督器
    m_supervisor_manager->stop_supervisors();
    
    // 停止IO上下文
    m_work_guard.reset(); // 重置work guard，允许io_context在任务完成后退出
    m_io_context.stop();
    
    m_stopped = true;
}

// 清理
void application::cleanup() {
    // 停止所有服务
    stop();
    
    // 清理资源
    m_supervisor_manager.reset();
    m_service_manager.reset();
    m_service_factory.reset();
    m_plugin_manager.reset();
    m_config_manager.reset();
    
    // 停止IO上下文
    m_io_context.stop();
}

// 是否已停止
bool application::is_stopped() const {
    return m_stopped;
}

// 获取IO上下文
application::io_context_type& application::get_io_context() {
    return m_io_context;
}

// 获取执行器
application::strand_type& application::get_strand() {
    return m_strand;
}

void application::restart_all_services() {
    for (auto& [name, supervisor] : m_supervisors) {
        supervisor->restart_all_services();
    }
}

} // namespace mc