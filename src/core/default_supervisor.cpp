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

#include "include/default_supervisor.h"
#include "include/dependency_sorter.h"

#include <mc/exception.h>
#include <mc/log.h>

namespace mc::core {

default_supervisor::default_supervisor() : m_started(false)
{}

bool default_supervisor::init(const config::supervisor_config& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_config                 = config;
    m_restart_count          = 0;
    m_last_restart_time      = std::chrono::steady_clock::now();
    m_name                   = config.meta.name;
    m_strategy               = config.strategy;
    m_max_restarts           = config.max_restarts;
    m_restart_window_seconds = config.restart_window_seconds;

    ilog("init supervisor: ${name}, strategy: ${strategy}, max_restarts: ${max_restarts}, restart_window: ${window}s",
         ("name", m_name)("strategy", get_strategy_name(m_strategy))("max_restarts", m_max_restarts)(
             "window", m_restart_window_seconds));

    return true;
}

bool default_supervisor::start()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_started) {
        return true;
    }

    bool success = true;

    for (auto& pair : m_child_supervisors) {
        const auto& name       = pair.first;
        auto&       supervisor = pair.second;
        if (!supervisor->start()) {
            wlog("start child supervisor '${name}' failed", ("name", name));
            success = false;
        }
    }

    auto get_dependencies = [](const service_base_ptr& service) -> std::vector<std::string> {
        return service->get_dependencies();
    };

    try {
        auto graph =
            core::internal::dependency_sorter::build_dependency_graph<service_base_ptr>(m_services, get_dependencies);
        auto start_order = core::internal::dependency_sorter::sort_for_startup(graph);

        // 按顺序启动服务
        for (const auto& name : start_order) {
            if (!start_one_service(name)) {
                success = false;
                break; // 依赖服务启动失败，停止后续启动
            }
        }
    } catch (const mc::parse_error_exception& e) {
        elog("service dependency cycle: ${error}", ("error", e.what()));
        success = false;
    }

    m_started = success;
    return success;
}

bool default_supervisor::stop()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_started) {
        return true;
    }

    bool success = true;

    // 获取服务停止顺序
    auto stop_order = get_service_stop_order(true); // 传入 true 表示已经持有锁

    // 按照顺序停止服务
    for (const auto& name : stop_order) {
        if (!stop_one_service(name)) {
            success = false;
        }
    }

    // 再停止子监督器
    for (const auto& [name, _] : m_child_supervisors) {
        if (!stop_one_child_supervisor(name)) {
            success = false;
        }
    }

    m_started = false;
    return success;
}

void default_supervisor::cleanup()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 清理所有服务
    for (auto& [name, service] : m_services) {
        service->cleanup();
    }

    // 清理所有子监督器
    for (auto& [name, child] : m_child_supervisors) {
        child->cleanup();
    }

    m_services.clear();
    m_child_supervisors.clear();
}

bool default_supervisor::add_service(service_base_ptr service)
{
    if (!service) {
        elog("add service failed: service pointer is null");
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    const auto& name = service->name();
    if (m_services.find(name) != m_services.end()) {
        elog("add service failed: service '${name}' already exists", ("name", name));
        return false;
    }

    m_services[name] = service;
    ilog("add service success: ${name}", ("name", name));

    // 获取所有服务名称并用逗号分隔
    std::vector<std::string> service_names;
    for (const auto& [name, _] : m_services) {
        service_names.push_back(name);
    }

    ilog("supervisor ${name} current manage services: ${services}",
         ("name", m_name)("services", mc::string::join(service_names, ", ")));

    return true;
}

bool default_supervisor::remove_service(const std::string& name)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_services.find(name);
    if (it == m_services.end()) {
        return false;
    }

    // 停止服务
    it->second->stop();

    // 移除服务
    m_services.erase(it);
    return true;
}

service_base_ptr default_supervisor::get_service(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_services.find(name);
    return (it != m_services.end()) ? it->second : nullptr;
}

bool default_supervisor::add_child(supervisor_ptr child)
{
    if (!child) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    const auto& name = child->get_config().meta.name;
    if (m_child_supervisors.find(name) != m_child_supervisors.end()) {
        return false;
    }

    m_child_supervisors[name] = child;
    return true;
}

bool default_supervisor::remove_child(const std::string& name)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_child_supervisors.find(name);
    if (it == m_child_supervisors.end()) {
        return false;
    }

    // 停止子监督器
    it->second->stop();

    // 移除子监督器
    m_child_supervisors.erase(it);
    return true;
}

supervisor_ptr default_supervisor::get_child(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_child_supervisors.find(name);
    return (it != m_child_supervisors.end()) ? it->second : nullptr;
}

bool default_supervisor::is_healthy() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 检查所有服务是否健康
    for (const auto& [name, service] : m_services) {
        if (!service->is_healthy()) {
            return false;
        }
    }

    // 检查所有子监督器是否健康
    for (const auto& [name, child] : m_child_supervisors) {
        if (!child->is_healthy()) {
            return false;
        }
    }

    return true;
}

bool default_supervisor::restart_all_services()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 检查重启策略
    auto now     = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_last_restart_time);

    if (elapsed > std::chrono::seconds(m_restart_window_seconds)) {
        // 重置重启计数
        m_restart_count     = 0;
        m_last_restart_time = now;
    }

    if (m_restart_count >= m_max_restarts) {
        wlog("supervisor '${name}' restart count over limit", ("name", m_name));
        return false;
    }

    // 停止所有服务
    for (auto& [name, service] : m_services) {
        service->stop();
    }

    // 启动所有服务
    for (auto& pair : m_services) {
        if (!pair.second->start()) {
            auto& name = pair.first;
            wlog("restart service '${name}' failed", ("name", name));
            return false;
        }
    }

    m_restart_count++;
    return true;
}

bool default_supervisor::restart_dependent_services(const std::string& service_name)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 构建依赖图
    auto get_dependencies = [](const service_base_ptr& service) -> std::vector<std::string> {
        return service->get_dependencies();
    };

    try {
        auto graph =
            core::internal::dependency_sorter::build_dependency_graph<service_base_ptr>(m_services, get_dependencies);

        // 获取所有受影响的服务（直接和间接依赖）
        std::unordered_set<std::string> affected_services;
        collect_dependent_services(graph, service_name, affected_services);

        // 按依赖顺序重启服务
        auto restart_order = core::internal::dependency_sorter::sort_for_startup(graph);

        bool success = true;
        // 只重启受影响的服务
        for (const auto& name : restart_order) {
            if (affected_services.find(name) != affected_services.end()) {
                if (!restart_one_service(name)) {
                    wlog("restart dependent service '${name}' failed", ("name", name));
                    success = false;
                }
            }
        }

        return success;
    } catch (const mc::parse_error_exception& e) {
        elog("service dependency cycle: ${error}", ("error", e.what()));
        return false;
    }
}

// 收集所有依赖于指定服务的服务（包括间接依赖）
void default_supervisor::collect_dependent_services(const std::unordered_map<std::string, dependency_node>& graph,
                                                    const std::string&               service_name,
                                                    std::unordered_set<std::string>& affected_services)
{
    if (affected_services.find(service_name) != affected_services.end()) {
        return; // 已处理过此服务
    }

    affected_services.insert(service_name);

    auto it = graph.find(service_name);
    if (it != graph.end()) {
        for (const auto& dependent : it->second.dependents) {
            collect_dependent_services(graph, dependent, affected_services);
        }
    }
}

bool default_supervisor::restart_one_service(const std::string& name)
{
    auto it = m_services.find(name);
    if (it == m_services.end()) {
        return false;
    }

    auto& service = it->second;

    // 重启服务
    if (!service->stop()) {
        wlog("restart service '${name}' failed", ("name", name));
        return false;
    }

    if (!service->start()) {
        wlog("restart service '${name}' failed", ("name", name));
        return false;
    }

    return true;
}

void default_supervisor::handle_service_crash(const std::string& name)
{
    auto it = m_services.find(name);
    if (it == m_services.end()) {
        return;
    }

    auto& service      = it->second;
    auto& restart_info = m_restart_infos[name];

    // 增加重启次数
    restart_info.restart_count++;

    // 如果重启次数超过阈值，不再重启
    if (m_max_restarts > 0 && restart_info.restart_count > m_max_restarts) {
        wlog("supervisor '${name}' restart count over limit", ("name", m_name));
        return;
    }

    // 根据策略重启服务
    switch (m_strategy) {
        case config::supervisor_strategy::one_for_one:
            restart_one_service(name);
            break;
        case config::supervisor_strategy::one_for_all:
            restart_all_services();
            break;
        case config::supervisor_strategy::rest_for_one:
            restart_dependent_services(name);
            break;
    }
}

// 获取策略名称
std::string default_supervisor::get_strategy_name(config::supervisor_strategy strategy)
{
    switch (strategy) {
        case config::supervisor_strategy::one_for_one:
            return "one_for_one";
        case config::supervisor_strategy::one_for_all:
            return "one_for_all";
        case config::supervisor_strategy::rest_for_one:
            return "rest_for_one";
        default:
            return "unknown";
    }
}

// 获取服务停止顺序
std::vector<std::string> default_supervisor::get_service_stop_order(bool already_locked) const
{
    if (!already_locked) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return get_service_stop_order_internal();
    }
    return get_service_stop_order_internal();
}

std::vector<std::string> default_supervisor::get_service_stop_order_internal() const
{
    // 构建服务依赖关系图的lambda函数
    auto get_dependencies = [](const service_base_ptr& service) -> std::vector<std::string> {
        return service->get_dependencies();
    };

    // 使用dependency_sorter构建依赖图
    auto graph =
        core::internal::dependency_sorter::build_dependency_graph<service_base_ptr>(m_services, get_dependencies);

    // 获取停止顺序（按照被依赖关系的拓扑排序）
    try {
        return core::internal::dependency_sorter::sort_for_shutdown(graph);
    } catch (const mc::parse_error_exception& e) {
        elog("service dependency cycle: ${error}", ("error", e.what()));
        // 如果存在循环依赖，返回空列表
        return {};
    }
}

// 启动单个服务
bool default_supervisor::start_one_service(const std::string& name)
{
    try {
        ilog("start service: ${name}", ("name", name));
        if (!m_services[name]->start()) {
            wlog("start service '${name}' failed", ("name", name));
            return false;
        }
        ilog("service start success: ${name}", ("name", name));
        return true;
    } catch (const std::exception& e) {
        elog("start service '${name}' failed: ${error}", ("name", name)("error", e.what()));
        return false;
    }
}

// 停止单个服务
bool default_supervisor::stop_one_service(const std::string& name)
{
    auto it = m_services.find(name);
    if (it == m_services.end()) {
        return true; // 服务不存在视为成功
    }

    try {
        ilog("stop service: ${name}", ("name", name));
        if (!it->second->stop()) {
            wlog("stop service '${name}' failed", ("name", name));
            return false;
        }
        ilog("service stop success: ${name}", ("name", name));
        return true;
    } catch (const std::exception& e) {
        elog("stop service '${name}' failed: ${error}", ("name", name)("error", e.what()));
        return false;
    }
}

// 停止单个子监督器
bool default_supervisor::stop_one_child_supervisor(const std::string& name)
{
    auto it = m_child_supervisors.find(name);
    if (it == m_child_supervisors.end()) {
        return true; // 子监督器不存在视为成功
    }

    try {
        ilog("stop child supervisor: ${name}", ("name", name));
        if (!it->second->stop()) {
            wlog("stop child supervisor '${name}' failed", ("name", name));
            return false;
        }
        ilog("stop child supervisor success: ${name}", ("name", name));
        return true;
    } catch (const std::exception& e) {
        elog("stop child supervisor '${name}' failed: ${error}", ("name", name)("error", e.what()));
        return false;
    }
}

} // namespace mc::core