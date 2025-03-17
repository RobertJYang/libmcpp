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

#include "mc/core/supervisor_manager.h"
#include "mc/core/default_supervisor.h"
#include <iostream>
#include <mc/log.h>

namespace mc {

// 构造函数
supervisor_manager::supervisor_manager() {
}

// 析构函数
supervisor_manager::~supervisor_manager() {
    stop_supervisors();
}

// 初始化
bool supervisor_manager::init() {
    // 创建根监督器
    config::supervisor_config config;
    config.api_version = "v1";
    config.kind = "Supervisor";
    config.meta.name = "main";  // 使用 main 作为默认监督器名称
    config.strategy = config::supervisor_strategy::one_for_one;
    config.max_restarts = 10;
    
    m_root_supervisor = std::make_shared<default_supervisor>();
    if (!m_root_supervisor->init(config)) {
        elog("错误: 初始化根监督器失败");
        return false;
    }
    
    m_supervisors["main"] = m_root_supervisor;  // 使用 main 作为默认监督器名称
    ilog("初始化根监督器成功: ${name}", ("name", config.meta.name));
    return true;
}

// 创建监督器
supervisor_ptr supervisor_manager::create_supervisor(const config::supervisor_config& config) {
    // 检查监督器是否已存在
    if (m_supervisors.find(config.meta.name) != m_supervisors.end()) {
        elog("错误: 监督器 '${name}' 已存在", ("name", config.meta.name));
        return nullptr;
    }
    
    try {
        // 创建监督器
        auto supervisor = std::make_shared<default_supervisor>();
        if (!supervisor->init(config)) {
            elog("错误: 初始化监督器 '${name}' 失败", ("name", config.meta.name));
            return nullptr;
        }
        
        // 添加到监督器列表
        m_supervisors[config.meta.name] = supervisor;
        return supervisor;
    } catch (const std::exception& e) {
        elog("错误: 创建监督器 '${name}' 失败: ${error}", ("name", config.meta.name)("error", e.what()));
        return nullptr;
    }
}

// 获取监督器
supervisor_ptr supervisor_manager::get_supervisor(const std::string& name) const {
    auto it = m_supervisors.find(name);
    if (it != m_supervisors.end()) {
        return it->second;
    }
    return nullptr;
}

// 获取根监督器
supervisor_ptr supervisor_manager::get_root_supervisor() const {
    return m_root_supervisor;
}

// 添加监督器
bool supervisor_manager::add_supervisor(const std::string& name, std::shared_ptr<supervisor> supervisor) {
    if (!supervisor) {
        elog("错误: 监督器指针为空");
        return false;
    }

    if (m_supervisors.find(name) != m_supervisors.end()) {
        elog("错误: 监督器 '${name}' 已存在", ("name", name));
        return false;
    }

    m_supervisors[name] = supervisor;
    return true;
}

// 启动所有监督器
bool supervisor_manager::start_supervisors() {
    bool success = true;
    
    for (auto& [name, supervisor] : m_supervisors) {
        try {
            if (!supervisor->start()) {
                elog("启动监督器 '${name}' 失败", ("name", name));
                success = false;
            }
        } catch (const std::exception& e) {
            elog("启动监督器 '${name}' 失败: ${error}", ("name", name)("error", e.what()));
            success = false;
        }
    }
    
    return success;
}

// 停止所有监督器
bool supervisor_manager::stop_supervisors() {
    bool success = true;
    
    for (auto& [name, supervisor] : m_supervisors) {
        try {
            if (!supervisor->stop()) {
                elog("停止监督器 '${name}' 失败", ("name", name));
                success = false;
            }
        } catch (const std::exception& e) {
            elog("停止监督器 '${name}' 失败: ${error}", ("name", name)("error", e.what()));
            success = false;
        }
    }
    
    return success;
}

bool supervisor_manager::initialize_from_configs(
    const std::vector<config::supervisor_config>& configs,
    std::unordered_map<std::string, std::shared_ptr<supervisor>>& supervisors_map) {
    
    for (const auto& sup_config : configs) {
        auto supervisor = std::make_shared<default_supervisor>();
        if (!supervisor->init(sup_config)) {
            elog("初始化监督器失败: ${name}", ("name", sup_config.meta.name));
            continue;
        }
        supervisors_map[sup_config.meta.name] = supervisor;
        add_supervisor(sup_config.meta.name, supervisor);
        dlog("添加监督器: ${name}, 策略: ${strategy}, 最大重启次数: ${max_restarts}",
             ("name", sup_config.meta.name)
             ("strategy", static_cast<int>(sup_config.strategy))
             ("max_restarts", sup_config.max_restarts));
    }
    
    return true;
}

} // namespace mc 