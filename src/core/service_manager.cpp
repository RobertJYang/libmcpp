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
    stop_services();
}

// 注册服务类型
bool service_manager::register_service(const std::string& type, 
                                     std::function<std::shared_ptr<service>()> factory,
                                     std::function<void(po::options_description&, po::options_description&)> register_options) {
    if (m_service_types.find(type) != m_service_types.end()) {
        elog("错误: 服务类型 '${type}' 已注册", ("type", type));
        return false;
    }
    
    if (!factory) {
        elog("错误: 服务类型 '${type}' 的工厂函数为空", ("type", type));
        return false;
    }
    
    service_type_info info;
    info.factory = factory;
    info.register_options = register_options ? register_options : [](po::options_description&, po::options_description&) {};
    
    m_service_types[type] = std::move(info);
    return true;
}

// 创建服务
std::shared_ptr<service> service_manager::create_service(const std::string& type, const service_config& config) {
    // 查找服务类型的工厂
    auto it = m_service_types.find(type);
    if (it == m_service_types.end()) {
        elog("错误: 未知的服务类型 '${type}'", ("type", type));
        return nullptr;
    }
    
    // 创建服务实例
    auto service = it->second.factory();
    if (!service) {
        elog("错误: 创建服务类型 '${type}' 的实例失败", ("type", type));
        return nullptr;
    }
    
    // 初始化服务
    if (!service->init(config)) {
        elog("错误: 初始化服务 '${name}' 失败", ("name", config.name));
        return nullptr;
    }
    
    try {
        // 注册服务
        m_services[config.name] = service;
        return service;
    } catch (const std::exception& e) {
        elog("错误: 创建服务 '${name}' 失败: ${error}", ("name", config.name)("error", e.what()));
        return nullptr;
    }
}

// 获取服务
std::shared_ptr<service> service_manager::get_service(const std::string& name) const {
    auto it = m_services.find(name);
    if (it != m_services.end()) {
        return it->second;
    }
    return nullptr;
}

// 收集配置选项
void service_manager::collect_options(po::options_description& cli_opts, po::options_description& cfg_opts) const {
    for (const auto& [type, info] : m_service_types) {
        po::options_description type_cli_opts(type + " 命令行选项");
        po::options_description type_cfg_opts(type + " 配置文件选项");
        
        // 调用服务类型的注册选项函数
        info.register_options(type_cli_opts, type_cfg_opts);
        
        if (!type_cli_opts.options().empty()) {
            cli_opts.add(type_cli_opts);
        }
        if (!type_cfg_opts.options().empty()) {
            cfg_opts.add(type_cfg_opts);
            cli_opts.add(type_cfg_opts);
        }
    }
}

// 启动所有服务
bool service_manager::start_services() {
    bool success = true;
    for (auto& [name, service] : m_services) {
        try {
            if (!service->start()) {
                elog("启动服务 '${name}' 失败", ("name", name));
                success = false;
            }
        } catch (const std::exception& e) {
            elog("启动服务 '${name}' 失败: ${error}", ("name", name)("error", e.what()));
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
            }
        } catch (const std::exception& e) {
            elog("停止服务 '${name}' 失败: ${error}", ("name", name)("error", e.what()));
            success = false;
        }
    }
    return success;
}

} // namespace mc 