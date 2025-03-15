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

#ifndef MC_CORE_DEFAULT_SUPERVISOR_H
#define MC_CORE_DEFAULT_SUPERVISOR_H

#include "mc/core/supervisor.h"
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace mc {

/**
 * @brief 默认监督器实现
 */
class default_supervisor : public supervisor_base<default_supervisor> {
public:
    default_supervisor();
    ~default_supervisor() override;

    // 生命周期方法
    bool init(const supervisor_config& config) override;
    bool start() override;
    bool stop() override;
    void cleanup() override;

    // 服务管理
    bool add_service(service_ptr service) override;
    bool remove_service(const std::string& name) override;
    service_ptr get_service(const std::string& name) const override;

    // 子监督器管理
    bool add_child(supervisor_ptr child) override;
    bool remove_child(const std::string& name) override;
    supervisor_ptr get_child(const std::string& name) const override;

    // 状态查询
    bool is_healthy() const override;

    // 重启服务
    bool restart_all_services() override;
    bool restart_dependent_services(const std::string& service_name) override;

private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, service_ptr> m_services;
    std::unordered_map<std::string, supervisor_ptr> m_children;
    supervisor_config m_config;
    int m_restart_count{0};
    std::chrono::steady_clock::time_point m_last_restart_time;
};

} // namespace mc

#endif // MC_CORE_DEFAULT_SUPERVISOR_H 