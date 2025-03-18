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
    return m_service_start_order;
}

// 收集配置选项
void service_manager::collect_options(po::options_description& cli_opts, po::options_description& cfg_opts) const {
    // 该方法现在为空，因为服务类型注册已移至service_factory
}

// 启动所有服务
bool service_manager::start_services() {
    bool success = true;
    // 按照拓扑排序的顺序启动服务
    for (const auto& name : m_service_start_order) {
        try {
            if (!m_services[name]->start()) {
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
    // 按照启动顺序的反序停止服务
    for (auto it = m_service_start_order.rbegin(); it != m_service_start_order.rend(); ++it) {
        const auto& name = *it;
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
    std::vector<std::string> result;
    std::queue<std::string> queue;
    
    // 计算每个节点的入度（依赖的数量）
    std::unordered_map<std::string, int> in_degrees;
    for (const auto& [name, node] : graph) {
        in_degrees[name] = node.dependencies.size();  // 入度是依赖的数量
        if (node.dependencies.empty()) {  // 没有依赖的服务先启动
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
    config_manager& config_mgr,
    supervisor_manager& supervisor_mgr,
    service_factory& factory) {
    
    // 清理现有服务
    m_services.clear();
    m_service_start_order.clear();
    
    // 获取服务配置
    const auto& configs = config_mgr.get_configs<config::service_config>();
    
    // 构建服务依赖图
    std::unordered_map<std::string, service_node> graph;
    
    // 初始化图
    for (const auto& config : configs) {
        service_node node;
        node.dependencies = std::unordered_set<std::string>(config.dependencies.begin(), config.dependencies.end());
        graph[config.meta.name] = node;
    }
    
    // 建立依赖关系
    for (const auto& config : configs) {
        for (const auto& dep : config.dependencies) {
            // 增加被依赖服务的依赖者
            graph[dep].dependents.insert(config.meta.name);
        }
    }
    
    try {
        // 使用已有的拓扑排序函数
        m_service_start_order = topological_sort(graph);
    } catch (const mc::parse_error_exception& e) {
        elog("服务依赖图中存在循环依赖: ${error}", ("error", e.what()));
        return false;
    }
    
    // 按照拓扑排序的顺序创建服务
    bool success = true;
    for (const auto& name : m_service_start_order) {
        // 查找对应的配置
        auto config = config_mgr.get_config<config::service_config>(name);
        if (!config) {
            elog("找不到服务 '${name}' 的配置", ("name", name));
            success = false;
            continue;
        }
        
        // 查找监督器
        auto supervisor = supervisor_mgr.get_supervisor(config->meta.labels->at("supervisor").as<std::string>());
        if (!supervisor) {
            elog("找不到服务 '${name}' 的监督器", ("name", name));
            success = false;
            continue;
        }
        
        // 创建服务
        try {
            auto service = factory.create_service(config->type, name, config->properties);
            if (!service) {
                elog("创建服务 '${name}' 失败", ("name", name));
                success = false;
                continue;
            }
            
            // 设置监督器
            service->set_supervisor(supervisor);
            
            // 添加到服务列表
            m_services[name] = service;
            
            dlog("创建服务 '${name}' 成功", ("name", name));
        } catch (const std::exception& e) {
            elog("创建服务 '${name}' 异常: ${error}", ("name", name)("error", e.what()));
            success = false;
        }
    }
    
    return success;
}

} // namespace mc 