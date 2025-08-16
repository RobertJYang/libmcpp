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

#include "./include/default_supervisor.h"
#include <mc/core/supervisor_manager.h>
#include <mc/log.h>

#include <iostream>

namespace mc::core {

supervisor_manager::supervisor_manager() {
}

supervisor_manager::~supervisor_manager() {
    stop_supervisors();
}

bool supervisor_manager::init() {
    // 创建根监督器
    config::supervisor_config config;
    config.api_version  = "v1";
    config.kind         = "Supervisor";
    config.meta.name    = "main"; // 使用 main 作为默认监督器名称
    config.strategy     = config::supervisor_strategy::one_for_one;
    config.max_restarts = 10;

    m_root_supervisor = std::make_shared<default_supervisor>();
    if (!m_root_supervisor->init(config)) {
        elog("error: init root supervisor failed");
        return false;
    }

    m_supervisors["main"] = m_root_supervisor; // 使用 main 作为默认监督器名称
    ilog("init root supervisor success: ${name}", ("name", config.meta.name));
    return true;
}

supervisor_ptr supervisor_manager::create_supervisor(const config::supervisor_config& config) {
    if (m_supervisors.find(config.meta.name) != m_supervisors.end()) {
        elog("error: supervisor '${name}' already exists", ("name", config.meta.name));
        return nullptr;
    }

    try {
        auto supervisor = std::make_shared<default_supervisor>();
        if (!supervisor->init(config)) {
            elog("error: init supervisor '${name}' failed", ("name", config.meta.name));
            return nullptr;
        }

        m_supervisors[config.meta.name] = supervisor;
        return supervisor;
    } catch (const std::exception& e) {
        elog("error: create supervisor '${name}' failed: ${error}",
             ("name", config.meta.name)("error", e.what()));
        return nullptr;
    }
}

supervisor_ptr supervisor_manager::get_supervisor(const std::string& name) const {
    auto it = m_supervisors.find(name);
    if (it != m_supervisors.end()) {
        return it->second;
    }
    return nullptr;
}

supervisor_ptr supervisor_manager::get_root_supervisor() const {
    return m_root_supervisor;
}

bool supervisor_manager::add_supervisor(const std::string& name, supervisor_ptr supervisor) {
    if (!supervisor) {
        elog("error: supervisor pointer is null");
        return false;
    }

    if (m_supervisors.find(name) != m_supervisors.end()) {
        elog("error: supervisor '${name}' already exists", ("name", name));
        return false;
    }

    m_supervisors[name] = supervisor;
    return true;
}

bool supervisor_manager::start_supervisors() {
    bool success = true;

    for (auto& pair : m_supervisors) {
        const auto& name       = pair.first;
        auto&       supervisor = pair.second;
        try {
            if (!supervisor->start()) {
                elog("error: start supervisor '${name}' failed", ("name", name));
                success = false;
            }
        } catch (const std::exception& e) {
            elog("error: start supervisor '${name}' failed: ${error}",
                 ("name", name)("error", e.what()));
            success = false;
        }
    }

    return success;
}

bool supervisor_manager::stop_supervisors() {
    bool success = true;

    for (auto& pair : m_supervisors) {
        const auto& name       = pair.first;
        auto&       supervisor = pair.second;
        try {
            if (!supervisor->stop()) {
                elog("error: stop supervisor '${name}' failed", ("name", name));
                success = false;
            }
        } catch (const std::exception& e) {
            elog("error: stop supervisor '${name}' failed: ${error}",
                 ("name", name)("error", e.what()));
            success = false;
        }
    }

    return success;
}

bool supervisor_manager::initialize_from_configs(
    const std::vector<config::supervisor_config>& configs) {
    for (const auto& sup_config : configs) {
        auto supervisor = std::make_shared<default_supervisor>();
        if (!supervisor->init(sup_config)) {
            elog("error: init supervisor '${name}' failed", ("name", sup_config.meta.name));
            continue;
        }
        add_supervisor(sup_config.meta.name, supervisor);
        dlog("add supervisor: ${name}, strategy: ${strategy}, max restarts: ${max_restarts}",
             ("name", sup_config.meta.name)("strategy", static_cast<int>(sup_config.strategy))(
                 "max_restarts", sup_config.max_restarts));
    }

    return true;
}

} // namespace mc::core