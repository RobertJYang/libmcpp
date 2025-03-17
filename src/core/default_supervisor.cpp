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

#include "mc/core/default_supervisor.h"
#include "mc/core/application.h"
#include <algorithm>
#include <iostream>
#include <vector>
#include <mc/log.h>
#include <unordered_set>
#include <queue>

namespace mc {

default_supervisor::default_supervisor() : m_started(false) {}
default_supervisor::~default_supervisor() = default;

bool default_supervisor::init(const config::supervisor_config& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    m_restart_count = 0;
    m_last_restart_time = std::chrono::steady_clock::now();
    m_name = config.meta.name;
    m_strategy = config.strategy;
    m_max_restarts = config.max_restarts;
    
    ilog("初始化监督器: ${name}, 策略: ${strategy}, 最大重启次数: ${max_restarts}",
         ("name", m_name)
         ("strategy", get_strategy_name(m_strategy))
         ("max_restarts", m_max_restarts));
    
    return true;
}

bool default_supervisor::start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_started) {
        return true;
    }
    
    bool success = true;
    
    // 启动子监督器
    for (auto& [name, supervisor] : m_child_supervisors) {
        if (!supervisor->start()) {
            wlog("启动子监督器 '${name}' 失败", ("name", name));
            success = false;
        }
    }
    
    // 启动服务
    for (auto& [name, service] : m_services) {
        try {
            ilog("正在启动服务: ${name}", ("name", name));
            if (!service->start()) {
                wlog("启动服务 '${name}' 失败", ("name", name));
                success = false;
            } else {
                ilog("服务启动成功: ${name}", ("name", name));
            }
        } catch (const std::exception& e) {
            elog("启动服务 '${name}' 异常: ${error}", ("name", name)("error", e.what()));
            success = false;
        }
    }
    
    m_started = success;
    return success;
}

bool default_supervisor::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_started) {
        return true;
    }
    
    bool success = true;
    
    // 获取服务停止顺序
    auto stop_order = get_service_stop_order(true);  // 传入 true 表示已经持有锁
    
    // 按照顺序停止服务
    for (const auto& name : stop_order) {
        auto it = m_services.find(name);
        if (it != m_services.end()) {
            try {
                ilog("正在停止服务: ${name}", ("name", name));
                if (!it->second->stop()) {
                    wlog("停止服务 '${name}' 失败", ("name", name));
                    success = false;
                } else {
                    ilog("服务停止成功: ${name}", ("name", name));
                }
            } catch (const std::exception& e) {
                elog("停止服务 '${name}' 异常: ${error}", ("name", name)("error", e.what()));
                success = false;
            }
        }
    }
    
    // 再停止子监督器
    for (auto& [name, supervisor] : m_child_supervisors) {
        try {
            ilog("正在停止子监督器: ${name}", ("name", name));
            if (!supervisor->stop()) {
                wlog("停止子监督器 '${name}' 失败", ("name", name));
                success = false;
            } else {
                ilog("子监督器停止成功: ${name}", ("name", name));
            }
        } catch (const std::exception& e) {
            elog("停止子监督器 '${name}' 异常: ${error}", ("name", name)("error", e.what()));
            success = false;
        }
    }
    
    m_started = false;
    return success;
}

void default_supervisor::cleanup() {
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

bool default_supervisor::add_service(service_ptr service) {
    if (!service) {
        elog("添加服务失败: 服务指针为空");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    const auto& name = service->name();
    if (m_services.find(name) != m_services.end()) {
        elog("添加服务失败: 服务 '${name}' 已存在", ("name", name));
        return false;
    }
    
    // 设置监督器
    service->set_supervisor(shared_from_this());
    
    m_services[name] = service;
    ilog("添加服务成功: ${name}", ("name", name));
    
    // 打印监督器当前管理的所有服务
    std::string service_list;
    for (const auto& [name, _] : m_services) {
        if (!service_list.empty()) {
            service_list += ", ";
        }
        service_list += name;
    }
    ilog("监督器 ${name} 当前管理服务: ${services}", 
         ("name", m_name)("services", service_list));
    
    return true;
}

bool default_supervisor::remove_service(const std::string& name) {
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

service_ptr default_supervisor::get_service(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_services.find(name);
    return (it != m_services.end()) ? it->second : nullptr;
}

bool default_supervisor::add_child(supervisor_ptr child) {
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

bool default_supervisor::remove_child(const std::string& name) {
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

supervisor_ptr default_supervisor::get_child(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_child_supervisors.find(name);
    return (it != m_child_supervisors.end()) ? it->second : nullptr;
}

bool default_supervisor::is_healthy() const {
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

bool default_supervisor::restart_all_services() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 检查重启策略
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_last_restart_time);
    
    if (elapsed > std::chrono::seconds(5)) {
        // 重置重启计数
        m_restart_count = 0;
        m_last_restart_time = now;
    }
    
    if (m_restart_count >= m_max_restarts) {
        wlog("监督器 '${name}' 重启次数超过限制", ("name", m_name));
        return false;
    }
    
    // 停止所有服务
    for (auto& [name, service] : m_services) {
        service->stop();
    }
    
    // 启动所有服务
    for (auto& [name, service] : m_services) {
        if (!service->start()) {
            wlog("重启服务 '${name}' 失败", ("name", name));
            return false;
        }
    }
    
    m_restart_count++;
    return true;
}

bool default_supervisor::restart_dependent_services(const std::string& service_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    bool success = true;
    for (auto& [name, service] : m_services) {
        // 在此只简单检查服务本身是否依赖于失败的服务
        // 依赖关系可以在配置中指定，这里使用空列表代替(实际项目会通过配置获取)
        const std::vector<std::string> deps; // 使用空依赖列表
        
        // 如果服务依赖于失败的服务，则重启它
        if (std::find(deps.begin(), deps.end(), service_name) != deps.end()) {
            if (!restart_one_service(name)) {
                wlog("重启依赖服务 '${name}' 失败", ("name", name));
                success = false;
            }
        }
    }
    
    return success;
}

bool default_supervisor::restart_one_service(const std::string& name) {
    auto it = m_services.find(name);
    if (it == m_services.end()) {
        return false;
    }
    
    auto& service = it->second;
    
    // 重启服务
    if (!service->stop()) {
        wlog("重启服务 '${name}' 失败", ("name", name));
        return false;
    }
    
    if (!service->start()) {
        wlog("重启服务 '${name}' 失败", ("name", name));
        return false;
    }
    
    return true;
}

void default_supervisor::handle_service_crash(const std::string& name) {
    auto it = m_services.find(name);
    if (it == m_services.end()) {
        return;
    }
    
    auto& service = it->second;
    auto& restart_info = m_restart_infos[name];
    
    // 增加重启次数
    restart_info.restart_count++;
    
    // 如果重启次数超过阈值，不再重启
    if (m_max_restarts > 0 && restart_info.restart_count > m_max_restarts) {
        wlog("监督器 '${name}' 重启次数超过限制", ("name", m_name));
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
std::string default_supervisor::get_strategy_name(config::supervisor_strategy strategy) {
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
std::vector<std::string> default_supervisor::get_service_stop_order(bool already_locked) const {
    if (!already_locked) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return get_service_stop_order_internal();
    }
    return get_service_stop_order_internal();
}

std::vector<std::string> default_supervisor::get_service_stop_order_internal() const {
    // 构建服务依赖图
    std::unordered_map<std::string, service_node> graph;
    for (const auto& [name, service] : m_services) {
        service_node node;
        node.name = name;
        // 获取服务依赖
        auto dependencies = service->get_dependencies();
        for (const auto& dep : dependencies) {
            node.dependencies.insert(dep);
            node.in_degree++;
        }
        graph[name] = node;
    }
    
    // 构建依赖关系
    for (const auto& [name, node] : graph) {
        for (const auto& dep : node.dependencies) {
            if (graph.find(dep) != graph.end()) {
                graph[dep].dependents.insert(name);
            }
        }
    }
    
    // 拓扑排序
    std::vector<std::string> stop_order;
    std::queue<std::string> queue;
    std::unordered_map<std::string, int> in_degrees;
    
    // 计算入度
    for (const auto& [name, node] : graph) {
        in_degrees[name] = node.dependents.size();
        if (in_degrees[name] == 0) {
            queue.push(name);
        }
    }
    
    // 拓扑排序
    while (!queue.empty()) {
        std::string name = queue.front();
        queue.pop();
        stop_order.push_back(name);
        
        // 更新依赖此服务的服务的入度
        const auto& node = graph[name];
        for (const auto& dependent : node.dependents) {
            in_degrees[dependent]--;
            if (in_degrees[dependent] == 0) {
                queue.push(dependent);
            }
        }
    }
    
    // 检查是否有循环依赖
    if (stop_order.size() != graph.size()) {
        elog("服务依赖图中存在循环依赖");
    }
    
    return stop_order;
}

} // namespace mc 