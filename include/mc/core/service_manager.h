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

/**
 * @file service_manager.h
 * @brief 服务管理器，负责服务实例的管理
 */
#ifndef MC_SERVICE_MANAGER_H
#define MC_SERVICE_MANAGER_H

#include <boost/program_options.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "mc/core/config_schema.h"
#include "mc/core/service.h"
#include "mc/core/service_factory.h"
#include "mc/core/supervisor.h"

namespace mc {

namespace po = boost::program_options;

// 服务依赖图节点
struct service_node {
    std::string name;
    std::string type;
    std::string supervisor;
    config::service_config config;
    std::unordered_set<std::string> dependents;  // 依赖此服务的服务
    std::unordered_set<std::string> dependencies; // 此服务依赖的服务
    int in_degree = 0;  // 入度，用于拓扑排序
};

/**
 * @brief 服务管理器类
 */
class service_manager {
public:
    // 构造函数
    service_manager() = default;
    
    // 析构函数
    ~service_manager();
    
    // 从配置初始化服务
    bool initialize_from_configs(
        const std::vector<config::service_config>& configs,
        const std::unordered_map<std::string, std::shared_ptr<supervisor>>& supervisors,
        service_factory& factory);
    
    // 获取服务
    std::shared_ptr<service> get_service(const std::string& name) const;
    
    // 清理服务
    void cleanup_services();

    // 添加服务
    bool add_service(const std::string& name, std::shared_ptr<service> service_instance);

    // 移除服务
    bool remove_service(const std::string& name);

    // 获取所有服务名称
    std::vector<std::string> get_service_names() const;

    // 收集命令行选项
    void collect_options(po::options_description& cli_opts, po::options_description& cfg_opts) const;

    // 启动所有服务
    bool start_services();

    // 停止所有服务
    bool stop_services();

private:
    // 拓扑排序，返回服务名称列表
    std::vector<std::string> topological_sort(const std::unordered_map<std::string, service_node>& graph);
    
    // 成员变量
    std::unordered_map<std::string, std::shared_ptr<service>> m_services;
};

} // namespace mc

#endif // MC_SERVICE_MANAGER_H 