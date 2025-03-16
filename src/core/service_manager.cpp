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

#include "mc/core/service_manager.h"
#include <iostream>
#include <mc/log.h>
#include <mutex>

namespace mc {

// 构造函数
service_manager::service_manager() {
}

// 析构函数
service_manager::~service_manager() {
    cleanup_services();
}

// 添加服务实例
bool service_manager::add_service(const std::string& name, std::shared_ptr<service> service_instance) {
    if (!service_instance) {
        elog("错误: 试图添加空的服务实例 '${name}'", ("name", name));
        return false;
    }
    
    if (m_services.find(name) != m_services.end()) {
        elog("错误: 服务 '${name}' 已经存在", ("name", name));
        return false;
    }
    
    m_services[name] = std::move(service_instance);
    ilog("服务 '${name}' 已添加", ("name", name));
    return true;
}

// 获取服务实例
std::shared_ptr<service> service_manager::get_service(const std::string& name) const {
    auto it = m_services.find(name);
    if (it != m_services.end()) {
        return it->second;
    }
    return nullptr;
}

// 移除服务实例
bool service_manager::remove_service(const std::string& name) {
    auto it = m_services.find(name);
    if (it == m_services.end()) {
        wlog("服务 '${name}' 不存在，无法移除", ("name", name));
        return false;
    }
    
    // 停止服务
    try {
        if (it->second->get_state() != service_state::stopped) {
            if (!it->second->stop()) {
                wlog("停止服务 '${name}' 失败，但会继续移除", ("name", name));
            }
        }
    } catch (const std::exception& e) {
        wlog("停止服务 '${name}' 异常: ${error}，但会继续移除", ("name", name)("error", e.what()));
    }
    
    // 清理资源
    try {
        it->second->cleanup();
    } catch (const std::exception& e) {
        wlog("清理服务 '${name}' 异常: ${error}", ("name", name)("error", e.what()));
    }
    
    m_services.erase(it);
    ilog("服务 '${name}' 已移除", ("name", name));
    return true;
}

// 获取所有服务名
std::vector<std::string> service_manager::get_service_names() const {
    std::vector<std::string> names;
    names.reserve(m_services.size());
    
    for (const auto& [name, _] : m_services) {
        names.push_back(name);
    }
    
    return names;
}

// 收集配置选项
void service_manager::collect_options(po::options_description& cli_opts, po::options_description& cfg_opts) const {
    // 该方法现在为空，因为服务类型注册已移至service_factory
}

// 启动所有服务
bool service_manager::start_services() {
    bool success = true;
    for (auto& [name, service] : m_services) {
        try {
            if (!service->start()) {
                elog("启动服务 '${name}' 失败", ("name", name));
                success = false;
            } else {
                ilog("服务 '${name}' 已启动", ("name", name));
            }
        } catch (const std::exception& e) {
            elog("启动服务 '${name}' 异常: ${error}", ("name", name)("error", e.what()));
            success = false;
        }
    }
    return success;
}

// 停止所有服务
bool service_manager::stop_services() {
    bool success = true;
    for (auto& [name, service] : m_services) {
        try {
            if (!service->stop()) {
                elog("停止服务 '${name}' 失败", ("name", name));
                success = false;
            } else {
                ilog("服务 '${name}' 已停止", ("name", name));
            }
        } catch (const std::exception& e) {
            elog("停止服务 '${name}' 异常: ${error}", ("name", name)("error", e.what()));
            success = false;
        }
    }
    return success;
}

// 清理所有服务
void service_manager::cleanup_services() {
    stop_services();
    
    for (auto& [name, service] : m_services) {
        try {
            service->cleanup();
            ilog("服务 '${name}' 已清理", ("name", name));
        } catch (const std::exception& e) {
            elog("清理服务 '${name}' 异常: ${error}", ("name", name)("error", e.what()));
        }
    }
    
    m_services.clear();
}

} // namespace mc 