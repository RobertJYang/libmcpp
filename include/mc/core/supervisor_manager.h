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

#include "mc/core/supervisor.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace mc {

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
    std::shared_ptr<supervisor> create_supervisor(const supervisor_config& config);
    std::shared_ptr<supervisor> get_supervisor(const std::string& name) const;
    std::shared_ptr<supervisor> get_root_supervisor() const;

    // 监督器生命周期管理
    bool start_supervisors();
    bool stop_supervisors();

private:
    // 成员变量
    std::shared_ptr<supervisor> m_root_supervisor;                          // 根监督器
    std::unordered_map<std::string, std::shared_ptr<supervisor>> m_supervisors;  // 监督器映射表
};

} // namespace mc

#endif // MC_SUPERVISOR_MANAGER_H 