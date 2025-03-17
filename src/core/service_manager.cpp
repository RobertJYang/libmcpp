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

// 拓扑排序，返回服务名称列表
std::vector<std::string> service_manager::topological_sort(const std::unordered_map<std::string, service_node>& graph) {
    std::vector<std::string> result;
    std::queue<std::string> queue;
    
    // 计算每个节点的入度
    std::unordered_map<std::string, int> in_degrees;
    for (const auto& [name, node] : graph) {
        in_degrees[name] = node.in_degree;
        if (node.in_degree == 0) {
            queue.push(name);
        }
    }
    
    // 拓扑排序
    while (!queue.empty()) {
        std::string name = queue.front();
        queue.pop();
        result.push_back(name);
        
        // 更新依赖此服务的服务的入度
        const auto& node = graph.at(name);
        for (const auto& dependent : node.dependents) {
            in_degrees[dependent]--;
            if (in_degrees[dependent] == 0) {
                queue.push(dependent);
            }
        }
    }
    
    // 检查是否有循环依赖
    if (result.size() != graph.size()) {
        MC_THROW(mc::parse_error_exception, "服务依赖图中存在循环依赖");
    }
    
    return result;
}

// 从配置初始化服务
bool service_manager::initialize_from_configs(
    const std::vector<config::service_config>& configs,
    const std::unordered_map<std::string, std::shared_ptr<supervisor>>& supervisors,
    service_factory& factory) {
    
    // 构建服务依赖图
    std::unordered_map<std::string, service_node> graph;
    
    // 打印已注册的服务类型（只打印一次）
    auto service_types = factory.get_service_types();
    ilog("已注册的服务类型: ${types}", ("types", mc::string::join(service_types, ", ")));
    
    // 第一步：创建所有服务节点
    for (const auto& svc_config : configs) {
        service_node node;
        node.name = svc_config.meta.name;
        node.type = svc_config.type;
        node.supervisor = "main";  // 默认监督器
        node.config = svc_config;
        
        // 检查服务类型是否存在
        dlog("当前服务类型: ${type}", ("type", node.type));
        if (!factory.has_service(node.type)) {
            elog("未知服务类型: ${type}", ("type", node.type));
            return false;
        }
        
        // 获取监督器名称
        if (svc_config.meta.labels && svc_config.meta.labels->contains("supervisor")) {
            node.supervisor = svc_config.meta.labels->at("supervisor").as<std::string>();
        }
        
        // 添加依赖关系
        for (const auto& dep : svc_config.dependencies) {
            node.dependencies.insert(dep);
            node.in_degree++;
        }
        
        graph[node.name] = node;
    }
    
    // 第二步：构建依赖关系
    for (const auto& [name, node] : graph) {
        for (const auto& dep : node.dependencies) {
            if (graph.find(dep) != graph.end()) {
                graph[dep].dependents.insert(name);
            } else {
                elog("服务 '${name}' 依赖未知服务 '${dep}'", ("name", name)("dep", dep));
                return false;
            }
        }
    }
    
    // 第三步：拓扑排序
    auto sorted_services = topological_sort(graph);
    
    // 第四步：按照排序后的顺序初始化服务
    for (const auto& name : sorted_services) {
        const auto& node = graph[name];
        
        // 创建服务实例
        auto service = factory.create_service(node.type, node.name, node.config.properties);
        if (!service) {
            elog("创建服务 '${name}' 失败", ("name", name));
            return false;
        }
        
        // 设置监督器
        auto it = supervisors.find(node.supervisor);
        if (it == supervisors.end()) {
            elog("未知监督器 '${supervisor}'", ("supervisor", node.supervisor));
            return false;
        }
        service->set_supervisor(it->second);
        
        // 添加服务
        if (!add_service(name, service)) {
            elog("添加服务 '${name}' 失败", ("name", name));
            return false;
        }
    }
    
    return true;
}

} // namespace mc 