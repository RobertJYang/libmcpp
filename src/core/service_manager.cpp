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
#include "mc/core/service_factory.h"
#include "mc/core/config_schema.h"
#include "core/include/dependency_sorter.h"
#include <iostream>
#include <mc/log.h>
#include <mutex>
#include <queue>

namespace mc {

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

// 获取所有服务名称
std::vector<std::string> service_manager::get_service_names() const {
    return m_service_start_order;
}

// 启动所有服务
bool service_manager::start_services() {
    bool success = true;
    // 按照拓扑排序的顺序启动服务
    for (const auto& name : m_service_start_order) {
        try {
            ilog("正在启动服务 '${name}'...", ("name", name));
            if (!m_services[name]->start()) {
                elog("启动服务 '${name}' 失败", ("name", name));
                success = false;
            } else {
                dlog("服务 '${name}' 启动完成", ("name", name));
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
    // 按照启动顺序的反序停止服务
    for (auto it = m_service_start_order.rbegin(); it != m_service_start_order.rend(); ++it) {
        const auto& name = *it;
        if (m_services.count(name) == 0) {
            continue;
        }
        try {
            if (!m_services[name]->stop()) {
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

// 拓扑排序，返回服务名称列表
std::vector<std::string> service_manager::topological_sort(const std::unordered_map<std::string, service_node>& graph) {
    // 转换数据结构以适配dependency_sorter
    std::unordered_map<std::string, core::internal::dependency_sorter::dependency_node> dependency_graph;
    
    for (const auto& [name, node] : graph) {
        core::internal::dependency_sorter::dependency_node dep_node;
        dep_node.name = name;
        dep_node.dependencies = node.dependencies;
        dep_node.dependents = node.dependents;
        dep_node.in_degree = node.in_degree;
        dependency_graph[name] = dep_node;
    }
    
    try {
        // 使用dependency_sorter进行拓扑排序，获取启动顺序
        return core::internal::dependency_sorter::sort_for_startup(dependency_graph);
    } catch (const mc::parse_error_exception& e) {
        // 重新抛出异常
        MC_THROW(mc::parse_error_exception, "服务依赖图中存在循环依赖: ${error}", ("error", e.what()));
    }
}

// 构建服务依赖图
std::unordered_map<std::string, service_node> service_manager::build_dependency_graph(
    const std::vector<config::service_config>& configs) {
    
    // 首先将 vector 转换为 map
    std::unordered_map<std::string, config::service_config> config_map;
    for (const auto& config : configs) {
        config_map[config.meta.name] = config;
    }
    
    // 构造获取依赖的 lambda 函数
    std::function<std::vector<std::string>(const config::service_config&)> get_dependencies = 
        [](const config::service_config& config) -> std::vector<std::string> {
            return config.dependencies;
        };
    
    // 使用 dependency_sorter 构建依赖图
    auto dependency_graph = core::internal::dependency_sorter::build_dependency_graph(
        config_map, get_dependencies);
    
    // 转换为 service_node 格式
    std::unordered_map<std::string, service_node> graph;
    for (const auto& [name, dep_node] : dependency_graph) {
        service_node node;
        const auto& config = config_map[name];
        node.name = name;
        node.type = config.type;
        node.supervisor = config.meta.labels->at("supervisor").as<std::string>();
        node.config = config;
        node.dependents = dep_node.dependents;
        node.dependencies = std::unordered_set<std::string>(
            config.dependencies.begin(), config.dependencies.end());
        node.in_degree = dep_node.in_degree;
        
        dlog("添加服务节点 '${name}'，依赖数量: ${deps}", 
            ("name", name)("deps", node.dependencies.size()));
            
        for (const auto& dep : node.dependencies) {
            dlog("建立依赖关系: '${from}' 依赖 '${to}'", 
                ("from", name)("to", dep));
        }
        
        graph[name] = std::move(node);
    }
    
    return graph;
}

// 创建单个服务实例
bool service_manager::create_service_instance(
    const std::string& name,
    config_manager& config_mgr,
    supervisor_manager& supervisor_mgr,
    service_factory& factory) {
    
    // 查找对应的配置
    auto config = config_mgr.get_config<config::service_config>(name);
    if (!config) {
        elog("找不到服务 '${name}' 的配置", ("name", name));
        return false;
    }
    
    // 查找监督器
    auto supervisor = supervisor_mgr.get_supervisor(config->meta.labels->at("supervisor").as<std::string>());
    if (!supervisor) {
        elog("找不到服务 '${name}' 的监督器", ("name", name));
        return false;
    }
    
    // 创建服务
    try {
        dlog("开始创建服务 '${name}'，类型: ${type}", 
            ("name", name)("type", config->type));
            
        auto service = factory.create_service(config->type, name, config->properties);
        if (!service) {
            elog("创建服务 '${name}' 失败", ("name", name));
            return false;
        }
        
        // 设置监督器
        service->set_supervisor(supervisor);
        
        // 添加到服务列表
        m_services[name] = service;
        
        ilog("创建服务 '${name}' 成功，类型: ${type}, 监督器: ${supervisor}", 
            ("name", name)("type", config->type)
            ("supervisor", config->meta.labels->at("supervisor").as<std::string>()));
            
        return true;
    } catch (const std::exception& e) {
        elog("创建服务 '${name}' 异常: ${error}", ("name", name)("error", e.what()));
        return false;
    }
}

// 从配置初始化服务
bool service_manager::initialize_from_configs(
    config_manager& config_mgr,
    supervisor_manager& supervisor_mgr,
    service_factory& factory) {
    
    // 清理现有服务
    m_services.clear();
    m_service_start_order.clear();
    
    // 获取服务配置
    const auto& configs = config_mgr.get_configs<config::service_config>();
    dlog("发现 ${count} 个服务配置", ("count", configs.size()));
    
    // 构建服务依赖图
    auto graph = build_dependency_graph(configs);
    
    try {
        // 使用已有的拓扑排序函数
        m_service_start_order = topological_sort(graph);
        dlog("服务启动顺序: ${order}", ("order", m_service_start_order));
    } catch (const mc::parse_error_exception& e) {
        elog("服务依赖图中存在循环依赖: ${error}", ("error", e.what()));
        return false;
    }
    
    // 按照拓扑排序的顺序创建服务
    bool success = true;
    size_t success_count = 0;
    
    for (const auto& name : m_service_start_order) {
        if (create_service_instance(name, config_mgr, supervisor_mgr, factory)) {
            success_count++;
        } else {
            success = false;
        }
    }
    
    if (success) {
        ilog("所有服务初始化成功，共 ${count} 个服务", ("count", success_count));
    } else {
        wlog("部分服务初始化失败，成功 ${success} 个，失败 ${failed} 个",
            ("success", success_count)
            ("failed", m_service_start_order.size() - success_count));
    }
    
    return success;
}

} // namespace mc 