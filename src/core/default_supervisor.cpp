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

namespace mc {

default_supervisor::default_supervisor() = default;
default_supervisor::~default_supervisor() = default;

bool default_supervisor::init(const supervisor_config& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    set_config(config);
    m_restart_count = 0;
    m_last_restart_time = std::chrono::steady_clock::now();
    return true;
}

bool default_supervisor::start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 启动所有子监督器
    for (auto& [name, child] : m_children) {
        if (!child->start()) {
            std::cerr << "启动子监督器 '" << name << "' 失败" << std::endl;
            return false;
        }
    }
    
    // 启动所有服务
    for (auto& [name, service] : m_services) {
        if (!service->start()) {
            std::cerr << "启动服务 '" << name << "' 失败" << std::endl;
            return false;
        }
    }
    
    return true;
}

bool default_supervisor::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    bool success = true;
    
    // 停止所有服务（按启动顺序的逆序）
    std::vector<std::pair<std::string, service_ptr>> services_vec(m_services.begin(), m_services.end());
    for (auto it = services_vec.rbegin(); it != services_vec.rend(); ++it) {
        if (!it->second->stop()) {
            std::cerr << "停止服务 '" << it->first << "' 失败" << std::endl;
            success = false;
        }
    }
    
    // 停止所有子监督器（按启动顺序的逆序）
    std::vector<std::pair<std::string, supervisor_ptr>> children_vec(m_children.begin(), m_children.end());
    for (auto it = children_vec.rbegin(); it != children_vec.rend(); ++it) {
        if (!it->second->stop()) {
            std::cerr << "停止子监督器 '" << it->first << "' 失败" << std::endl;
            success = false;
        }
    }
    
    return success;
}

void default_supervisor::cleanup() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 清理所有服务
    for (auto& [name, service] : m_services) {
        service->cleanup();
    }
    
    // 清理所有子监督器
    for (auto& [name, child] : m_children) {
        child->cleanup();
    }
    
    m_services.clear();
    m_children.clear();
}

bool default_supervisor::add_service(service_ptr service) {
    if (!service) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    const auto& name = service->get_config().name;
    if (m_services.find(name) != m_services.end()) {
        return false;
    }
    
    m_services[name] = service;
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
    
    const auto& name = child->get_config().name;
    if (m_children.find(name) != m_children.end()) {
        return false;
    }
    
    m_children[name] = child;
    return true;
}

bool default_supervisor::remove_child(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_children.find(name);
    if (it == m_children.end()) {
        return false;
    }
    
    // 停止子监督器
    it->second->stop();
    
    // 移除子监督器
    m_children.erase(it);
    return true;
}

supervisor_ptr default_supervisor::get_child(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_children.find(name);
    return (it != m_children.end()) ? it->second : nullptr;
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
    for (const auto& [name, child] : m_children) {
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
    
    if (elapsed > m_config.restart_period) {
        // 重置重启计数
        m_restart_count = 0;
        m_last_restart_time = now;
    }
    
    if (m_restart_count >= m_config.max_restarts) {
        std::cerr << "监督器 '" << m_config.name << "' 重启次数超过限制" << std::endl;
        return false;
    }
    
    // 停止所有服务
    for (auto& [name, service] : m_services) {
        service->stop();
    }
    
    // 启动所有服务
    for (auto& [name, service] : m_services) {
        if (!service->start()) {
            std::cerr << "重启服务 '" << name << "' 失败" << std::endl;
            return false;
        }
    }
    
    m_restart_count++;
    return true;
}

bool default_supervisor::restart_dependent_services(const std::string& service_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 检查服务是否存在
    auto it = m_services.find(service_name);
    if (it == m_services.end()) {
        return false;
    }
    
    // 获取依赖于此服务的其他服务
    std::vector<service_ptr> dependent_services;
    for (auto& [name, service] : m_services) {
        const auto& deps = service->get_config().dependencies;
        if (std::find(deps.begin(), deps.end(), service_name) != deps.end()) {
            dependent_services.push_back(service);
        }
    }
    
    // 停止依赖服务
    for (auto& service : dependent_services) {
        service->stop();
    }
    
    // 重启主服务
    if (!it->second->stop() || !it->second->start()) {
        std::cerr << "重启服务 '" << service_name << "' 失败" << std::endl;
        return false;
    }
    
    // 启动依赖服务
    for (auto& service : dependent_services) {
        if (!service->start()) {
            std::cerr << "重启依赖服务 '" << service->get_config().name << "' 失败" << std::endl;
            return false;
        }
    }
    
    return true;
}

} // namespace mc 