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
 * @file supervisor_manager.h
 * @brief 监督器管理器，负责监督器的创建和管理
 */
#ifndef MC_SUPERVISOR_MANAGER_H
#define MC_SUPERVISOR_MANAGER_H

#include <mc/core/supervisor.h>

namespace mc::core {

/**
 * @brief 监督器管理器类
 */
class supervisor_manager {
public:
    // 构造和析构
    supervisor_manager();
    ~supervisor_manager();

    // 初始化
    bool init();

    // 监督器创建和获取
    supervisor_ptr create_supervisor(const config::supervisor_config& config);
    supervisor_ptr get_supervisor(const std::string& name) const;
    supervisor_ptr get_root_supervisor() const;
    bool           add_supervisor(const std::string& name, supervisor_ptr supervisor);

    // 监督器生命周期管理
    bool start_supervisors();
    bool stop_supervisors();

    // 从配置初始化监督器
    bool initialize_from_configs(const std::vector<config::supervisor_config>& configs);

private:
    using supervisor_map = std::unordered_map<std::string, supervisor_ptr>;

    supervisor_ptr m_root_supervisor; // 根监督器
    supervisor_map m_supervisors;     // 监督器映射表
};

} // namespace mc::core

#endif // MC_SUPERVISOR_MANAGER_H