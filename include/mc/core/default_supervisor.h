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
 * @file default_supervisor.h
 * @brief 默认监督器实现
 */
#ifndef MC_CORE_DEFAULT_SUPERVISOR_H
#define MC_CORE_DEFAULT_SUPERVISOR_H

#include "mc/core/supervisor.h"
#include "mc/core/service.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <memory>
#include <vector>

namespace mc {

/**
 * @brief 默认监督器实现
 * 
 * 负责管理服务的生命周期，包括启动、停止、重启和健康检查
 */
class default_supervisor : public supervisor, public std::enable_shared_from_this<default_supervisor> {
public:
    default_supervisor();
    ~default_supervisor() override;
    
    bool init(const config::supervisor_config& config) override;
    bool start() override;
    bool stop() override;
    void cleanup() override;
    
    bool add_service(service_ptr service) override;
    bool remove_service(const std::string& name) override;
    service_ptr get_service(const std::string& name) const override;
    
    bool add_child(supervisor_ptr child) override;
    bool remove_child(const std::string& name) override;
    supervisor_ptr get_child(const std::string& name) const override;
    
    const config::supervisor_config& get_config() const override { return m_config; }
    bool is_healthy() const override;
    
    std::string name() const override { return m_name; }
    
    // 获取服务停止顺序
    std::vector<std::string> get_service_stop_order() const;
    
protected:
    /**
     * @brief 重启所有服务
     * @return 是否成功重启所有服务
     */
    bool restart_all_services() override;
    
    /**
     * @brief 重启依赖于指定服务的所有服务
     * @param service_name 服务名称
     * @return 是否成功重启所有依赖服务
     */
    bool restart_dependent_services(const std::string& service_name) override;
    
    /**
     * @brief 重启单个服务
     * @param name 服务名称
     * @return 是否成功重启服务
     */
    bool restart_one_service(const std::string& name);
    
    /**
     * @brief 处理服务崩溃事件
     * @param name 服务名称
     */
    void handle_service_crash(const std::string& name);
    
    /**
     * @brief 获取策略名称
     * @param strategy 策略枚举值
     * @return 策略名称字符串
     */
    std::string get_strategy_name(config::supervisor_strategy strategy);
    
private:
    config::supervisor_config m_config;  // 监督器配置
    std::string m_name;                 // 监督器名称
    config::supervisor_strategy m_strategy;  // 监督策略
    int m_max_restarts;                // 最大重启次数
    
    mutable std::mutex m_mutex;         // 互斥锁
    std::atomic<bool> m_started;        // 是否已启动
    
    // 服务映射表 (name -> service)
    std::unordered_map<std::string, service_ptr> m_services;
    
    // 子监督器映射表 (name -> supervisor)
    std::unordered_map<std::string, supervisor_ptr> m_child_supervisors;
    
    // 重启信息
    struct restart_info {
        int restart_count = 0;
        std::chrono::steady_clock::time_point last_restart;
    };
    
    std::unordered_map<std::string, restart_info> m_restart_infos;
    int m_restart_count = 0;
    std::chrono::steady_clock::time_point m_last_restart_time;
    
    // 获取服务停止顺序（内部实现）
    std::vector<std::string> get_service_stop_order_internal() const;
    
    // 获取服务停止顺序
    std::vector<std::string> get_service_stop_order(bool already_locked = false) const;
};

} // namespace mc

#endif // MC_CORE_DEFAULT_SUPERVISOR_H 