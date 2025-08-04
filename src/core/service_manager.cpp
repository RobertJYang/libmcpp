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

#include "./include/dependency_sorter.h"

#include <mc/core/config_schema.h>
#include <mc/core/service_factory.h>
#include <mc/core/service_manager.h>
#include <mc/exception.h>
#include <mc/log.h>

namespace mc::core {

service_manager::service_manager() {
}

service_manager::~service_manager() {
    cleanup_services();
}

bool service_manager::add_service(const std::string& name, service_base_ptr service_instance) {
    if (!service_instance) {
        elog("error: try to add null service instance '${name}'", ("name", name));
        return false;
    }

    if (m_services.find(name) != m_services.end()) {
        elog("error: service '${name}' already exists", ("name", name));
        return false;
    }

    m_services[name] = std::move(service_instance);
    ilog("service '${name}' added", ("name", name));
    return true;
}

// 获取服务实例
service_base_ptr service_manager::get_service(const std::string& name) const {
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
        wlog("service '${name}' not found, cannot remove", ("name", name));
        return false;
    }

    // 停止服务
    try {
        if (it->second->get_state() != service_state::stopped) {
            if (!it->second->stop()) {
                wlog("stop service '${name}' failed, but will remove it", ("name", name));
            }
        }
    } catch (const std::exception& e) {
        wlog("stop service '${name}' failed: ${error}, but will remove it",
             ("name", name)("error", e.what()));
    }

    // 清理资源
    try {
        it->second->cleanup();
    } catch (const std::exception& e) {
        wlog("cleanup service '${name}' failed: ${error}", ("name", name)("error", e.what()));
    }

    m_services.erase(it);
    ilog("service '${name}' removed", ("name", name));
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
            ilog("starting service '${name}'...", ("name", name));
            if (!m_services[name]->start()) {
                elog("start service '${name}' failed", ("name", name));
                success = false;
            } else {
                dlog("service '${name}' started", ("name", name));
            }
        } catch (const std::exception& e) {
            elog("start service '${name}' failed: ${error}", ("name", name)("error", e.what()));
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
                elog("stop service '${name}' failed", ("name", name));
                success = false;
            } else {
                ilog("service '${name}' stopped", ("name", name));
            }
        } catch (const std::exception& e) {
            elog("stop service '${name}' failed: ${error}", ("name", name)("error", e.what()));
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
            ilog("service '${name}' cleaned up", ("name", name));
        } catch (const std::exception& e) {
            elog("cleanup service '${name}' failed: ${error}", ("name", name)("error", e.what()));
        }
    }

    m_services.clear();
}

// 拓扑排序，返回服务名称列表
std::vector<std::string>
service_manager::topological_sort(const std::unordered_map<std::string, service_node>& graph) {
    // 转换数据结构以适配dependency_sorter
    std::unordered_map<std::string, core::internal::dependency_sorter::dependency_node>
        dependency_graph;

    for (const auto& [name, node] : graph) {
        core::internal::dependency_sorter::dependency_node dep_node;
        dep_node.name          = name;
        dep_node.dependencies  = node.dependencies;
        dep_node.dependents    = node.dependents;
        dep_node.in_degree     = node.in_degree;
        dependency_graph[name] = dep_node;
    }

    try {
        // 使用dependency_sorter进行拓扑排序，获取启动顺序
        return core::internal::dependency_sorter::sort_for_startup(dependency_graph);
    } catch (const mc::parse_error_exception& e) {
        // 重新抛出异常
        MC_THROW(mc::parse_error_exception, "服务依赖图中存在循环依赖: ${error}",
                 ("error", e.what()));
    }
}

// 构建服务依赖图
std::unordered_map<std::string, service_node>
service_manager::build_dependency_graph(const std::vector<config::service_config>& configs) {
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
    auto dependency_graph =
        core::internal::dependency_sorter::build_dependency_graph(config_map, get_dependencies);

    // 转换为 service_node 格式
    std::unordered_map<std::string, service_node> graph;
    for (const auto& [name, dep_node] : dependency_graph) {
        service_node node;
        const auto&  config = config_map[name];
        node.name           = name;
        node.type           = config.type;
        node.supervisor     = config.meta.labels->at("supervisor").as<std::string>();
        node.config         = config;
        node.dependents     = dep_node.dependents;
        node.dependencies =
            std::unordered_set<std::string>(config.dependencies.begin(), config.dependencies.end());
        node.in_degree = dep_node.in_degree;

        dlog("add service node '${name}', dependencies count: ${deps}",
             ("name", name)("deps", node.dependencies.size()));

        for (const auto& dep : node.dependencies) {
            dlog("establish dependency: '${from}' depends on '${to}'", ("from", name)("to", dep));
        }

        graph[name] = std::move(node);
    }

    return graph;
}

// 创建单个服务实例
bool service_manager::create_service_instance(const std::string& name, config_manager& config_mgr,
                                              supervisor_manager& supervisor_mgr,
                                              service_factory&    factory) {
    // 查找对应的配置
    auto config = config_mgr.get_config<config::service_config>(name);
    if (!config) {
        elog("error: cannot find service '${name}' config", ("name", name));
        return false;
    }

    // 查找监督器
    auto supervisor =
        supervisor_mgr.get_supervisor(config->meta.labels->at("supervisor").as<std::string>());
    if (!supervisor) {
        elog("error: cannot find service '${name}' supervisor", ("name", name));
        return false;
    }

    // 创建服务
    try {
        dlog("start to create service '${name}', type: ${type}",
             ("name", name)("type", config->type));

        auto service = factory.create_service(config->type, name, config->properties);
        if (!service) {
            elog("error: create service '${name}' failed", ("name", name));
            return false;
        }

        // 添加到服务列表
        m_services[name] = service;

        ilog("create service '${name}' success, type: ${type}, supervisor: ${supervisor}",
             ("name", name)("type", config->type)(
                 "supervisor", config->meta.labels->at("supervisor").as<std::string>()));

        return true;
    } catch (const std::exception& e) {
        elog("error: create service '${name}' failed: ${error}", ("name", name)("error", e.what()));
        return false;
    }
}

// 从配置初始化服务
bool service_manager::initialize_from_configs(config_manager&     config_mgr,
                                              supervisor_manager& supervisor_mgr,
                                              service_factory&    factory) {
    // 清理现有服务
    m_services.clear();
    m_service_start_order.clear();

    // 获取服务配置
    const auto& configs = config_mgr.get_configs<config::service_config>();
    dlog("found ${count} service configs", ("count", configs.size()));

    // 构建服务依赖图
    auto graph = build_dependency_graph(configs);

    try {
        // 使用已有的拓扑排序函数
        m_service_start_order = topological_sort(graph);
        dlog("service start order: ${order}", ("order", m_service_start_order));
    } catch (const mc::parse_error_exception& e) {
        elog("error: service dependency cycle: ${error}", ("error", e.what()));
        return false;
    }

    // 按照拓扑排序的顺序创建服务
    bool   success       = true;
    size_t success_count = 0;

    for (const auto& name : m_service_start_order) {
        if (create_service_instance(name, config_mgr, supervisor_mgr, factory)) {
            success_count++;
        } else {
            success = false;
        }
    }

    if (success) {
        ilog("all services initialized successfully, ${count} services", ("count", success_count));
    } else {
        wlog("some services initialized failed, ${success} success, ${failed} failed",
             ("success", success_count)("failed", m_service_start_order.size() - success_count));
    }

    return success;
}

} // namespace mc