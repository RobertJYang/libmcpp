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
#include <boost/program_options.hpp>
#include <iostream>
#include <thread>
#include <unordered_set>
#include <algorithm>
#include <mc/log.h>
#include <sstream>

namespace mc {

namespace po = boost::program_options;

// 构造函数
application::application()
    : m_version("0.1.0")
    , m_plugin_manager(std::make_unique<plugin_manager>())
    , m_service_factory(std::make_unique<service_factory>())
    , m_service_manager(std::make_unique<service_manager>())
    , m_config_manager(std::make_unique<config_manager>(true))
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
    // 第一次解析命令行参数和配置文件是为了加载插件
    if (!m_config_manager->parse_command_line(argc, argv)) {
        return false;
    }

    if (m_config_manager->has_option("version")) {
        ilog("版本: ${version}", ("version", m_version));
        return false;
    }

    m_config_manager->load_config_file();

    if (m_config_manager->has_option("plugin-dir")) {
        std::string plugin_dir = m_config_manager->get_option<std::string>("plugin-dir");
        m_plugin_manager->set_plugin_dir(plugin_dir);
    }

    m_plugin_manager->load_plugins(m_config_manager->get_plugin_names());
    std::string config_dir = m_config_manager->get_option<std::string>("config");

    // 重新初始化配置管理器
    m_config_manager = std::make_unique<config_manager>(false);
    auto &options = m_config_manager->get_options();

    // 收集服务的选项配置
    auto& service_options = m_service_factory->get_service_options();
    options.cli.add(service_options->cli);
    options.cfg.add(service_options->cfg);
    m_service_manager->collect_options(options.cli, options.cfg);

    // 重新加载配置文件
    m_config_manager->load_config_file(config_dir);
    if (!m_config_manager->parse_command_line(argc, argv)) {
        return false;
    }

    if (m_config_manager->has_option("help")) {
        std::ostringstream oss;
        oss << m_config_manager->get_options().cli;
        ilog("帮助信息: ${help}", ("help", oss.str()));
        return false;
    }

    return true;
}

// 启动
application& application::start() {
    // 启动监督器
    if (!m_supervisor_manager->start_supervisors()) {
        wlog("部分监督器启动失败");
    }
    
    // 启动插件
    if (!m_plugin_manager->start_plugins()) {
        wlog("部分插件启动失败");
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
}

// 停止
void application::stop() {
    if (m_stopped) {
        return;
    }
    
    // 停止服务
    m_service_manager->stop_services();
    
    // 停止插件
    m_plugin_manager->stop_plugins();
    
    // 停止监督器
    m_supervisor_manager->stop_supervisors();
    
    // 停止IO上下文
    m_work_guard.reset(); // 重置work guard，允许io_context在任务完成后退出
    m_io_context.stop();
    
    m_stopped = true;
}

// 清理
void application::cleanup() {
    if (!m_stopped) {
        stop();
    }
    
    m_config_manager.reset();
    m_service_manager.reset();
    m_supervisor_manager.reset();
    if (m_plugin_manager) {
        m_plugin_manager->unload_all_plugins();
        m_plugin_manager.reset();
    }
    m_service_factory.reset();
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

} // namespace mc