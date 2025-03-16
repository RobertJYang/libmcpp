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
    supervisor_config config;
    config.name = "root";
    config.strategy = supervisor_strategy::one_for_one;
    config.max_restarts = 10;
    config.restart_period = std::chrono::seconds(5);
    
    m_root_supervisor = std::make_shared<default_supervisor>();
    if (!m_root_supervisor->init(config)) {
        elog("错误: 初始化根监督器失败");
        return false;
    }
    
    m_supervisors["root"] = m_root_supervisor;
    return true;
}

// 创建监督器
supervisor_ptr supervisor_manager::create_supervisor(const supervisor_config& config) {
    // 检查监督器是否已存在
    if (m_supervisors.find(config.name) != m_supervisors.end()) {
        elog("错误: 监督器 '${name}' 已存在", ("name", config.name));
        return nullptr;
    }
    
    try {
        // 创建监督器
        auto supervisor = std::make_shared<default_supervisor>();
        if (!supervisor->init(config)) {
            elog("错误: 初始化监督器 '${name}' 失败", ("name", config.name));
            return nullptr;
        }
        
        // 添加到监督器列表
        m_supervisors[config.name] = supervisor;
        return supervisor;
    } catch (const std::exception& e) {
        elog("错误: 创建监督器 '${name}' 失败: ${error}", ("name", config.name)("error", e.what()));
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

} // namespace mc 