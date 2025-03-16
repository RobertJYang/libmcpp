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

namespace mc {

default_supervisor::default_supervisor() : m_started(false) {}
default_supervisor::~default_supervisor() = default;

bool default_supervisor::init(const supervisor_config& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    set_config(config);
    m_restart_count = 0;
    m_last_restart_time = std::chrono::steady_clock::now();
    return true;
}

bool default_supervisor::start() {
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
        if (!service->start()) {
            wlog("启动服务 '${name}' 失败", ("name", name));
            success = false;
        }
    }
    
    m_started = true;
    return success;
}

bool default_supervisor::stop() {
    if (!m_started) {
        return true;
    }
    
    bool success = true;
    
    // 先停止服务
    for (auto it = m_services.begin(); it != m_services.end(); ++it) {
        try {
            if (!it->second->stop()) {
                wlog("停止服务 '${name}' 失败", ("name", it->first));
                success = false;
            }
        } catch (...) {
            wlog("停止服务 '${name}' 失败", ("name", it->first));
            success = false;
        }
    }
    
    // 再停止子监督器
    for (auto it = m_child_supervisors.begin(); it != m_child_supervisors.end(); ++it) {
        try {
            if (!it->second->stop()) {
                wlog("停止子监督器 '${name}' 失败", ("name", it->first));
                success = false;
            }
        } catch (...) {
            wlog("停止子监督器 '${name}' 失败", ("name", it->first));
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
    
    if (elapsed > m_config.restart_period) {
        // 重置重启计数
        m_restart_count = 0;
        m_last_restart_time = now;
    }
    
    if (m_restart_count >= m_config.max_restarts) {
        wlog("监督器 '${name}' 重启次数超过限制", ("name", m_config.name));
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
    auto it = m_services.find(service_name);
    if (it == m_services.end()) {
        return false;
    }
    
    bool success = true;
    
    // 先重启当前服务
    if (!restart_one_service(service_name)) {
        wlog("重启服务 '${name}' 失败", ("name", service_name));
        success = false;
    }
    
    // 找出依赖当前服务的其他服务并重启
    for (auto& [name, service] : m_services) {
        if (name == service_name) {
            continue;
        }
        
        // 检查service的依赖列表中是否包含service_name
        const auto& deps = service->get_config().dependencies;
        if (std::find(deps.begin(), deps.end(), service_name) != deps.end()) {
            if (!restart_one_service(name)) {
                wlog("重启依赖服务 '${name}' 失败", ("name", service->get_config().name));
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
    if (m_config.max_restarts > 0 && restart_info.restart_count > m_config.max_restarts) {
        wlog("监督器 '${name}' 重启次数超过限制", ("name", m_config.name));
        return;
    }
    
    // 根据策略重启服务
    switch (m_config.strategy) {
    case supervisor_strategy::one_for_one:
        restart_one_service(name);
        break;
    case supervisor_strategy::one_for_all:
        restart_all_services();
        break;
    case supervisor_strategy::rest_for_one:
        restart_dependent_services(name);
        break;
    }
}

} // namespace mc 