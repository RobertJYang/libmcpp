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
    supervisor_config root_config;
    root_config.name = "root";
    root_config.strategy = supervisor_strategy::one_for_one;
    root_config.max_restarts = 3;
    root_config.restart_period = std::chrono::seconds(5);

    m_root_supervisor = std::make_shared<default_supervisor>();
    if (!m_root_supervisor->init(root_config)) {
        std::cerr << "错误: 初始化根监督器失败" << std::endl;
        return false;
    }
    
    m_supervisors["root"] = m_root_supervisor;
    return true;
}

// 创建监督器
std::shared_ptr<supervisor> supervisor_manager::create_supervisor(const supervisor_config& config) {
    if (m_supervisors.find(config.name) != m_supervisors.end()) {
        std::cerr << "错误: 监督器 '" << config.name << "' 已存在" << std::endl;
        return nullptr;
    }
    
    try {
        auto supervisor = std::make_shared<default_supervisor>();
        if (!supervisor->init(config)) {
            std::cerr << "错误: 初始化监督器 '" << config.name << "' 失败" << std::endl;
            return nullptr;
        }
        
        m_supervisors[config.name] = supervisor;
        return supervisor;
    } catch (const std::exception& e) {
        std::cerr << "错误: 创建监督器 '" << config.name << "' 失败: " << e.what() << std::endl;
        return nullptr;
    }
}

// 获取监督器
std::shared_ptr<supervisor> supervisor_manager::get_supervisor(const std::string& name) const {
    auto it = m_supervisors.find(name);
    if (it != m_supervisors.end()) {
        return it->second;
    }
    return nullptr;
}

// 获取根监督器
std::shared_ptr<supervisor> supervisor_manager::get_root_supervisor() const {
    return m_root_supervisor;
}

// 启动所有监督器
bool supervisor_manager::start_supervisors() {
    bool success = true;
    for (auto& [name, supervisor] : m_supervisors) {
        try {
            if (!supervisor->start()) {
                std::cerr << "启动监督器 '" << name << "' 失败" << std::endl;
                success = false;
            }
        } catch (const std::exception& e) {
            std::cerr << "启动监督器 '" << name << "' 失败: " << e.what() << std::endl;
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
                std::cerr << "停止监督器 '" << name << "' 失败" << std::endl;
                success = false;
            }
        } catch (const std::exception& e) {
            std::cerr << "停止监督器 '" << name << "' 失败: " << e.what() << std::endl;
            success = false;
        }
    }
    return success;
}

} // namespace mc 