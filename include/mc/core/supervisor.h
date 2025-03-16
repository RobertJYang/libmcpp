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

#ifndef MC_CORE_SUPERVISOR_H
#define MC_CORE_SUPERVISOR_H

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include "mc/core/service.h"

namespace mc {

/**
 * @brief 监督策略枚举
 */
enum class supervisor_strategy {
    one_for_one,    // 只重启失败的服务
    one_for_all,    // 重启所有服务
    rest_for_one    // 重启失败服务及其依赖服务
};

/**
 * @brief 监督器配置结构
 */
struct supervisor_config {
    std::string name;                     // 监督器名称
    supervisor_strategy strategy;          // 监督策略
    int max_restarts{3};                  // 最大重启次数
    std::chrono::seconds restart_period{5}; // 重启周期
};

// 前向声明
class supervisor;
using supervisor_ptr = std::shared_ptr<supervisor>;

/**
 * @brief 监督器接口类
 */
class supervisor {
public:
    virtual ~supervisor() = default;

    // 生命周期方法
    virtual bool init(const supervisor_config& config) = 0;  // 初始化监督器
    virtual bool start() = 0;                               // 启动监督器
    virtual bool stop() = 0;                                // 停止监督器
    virtual void cleanup() = 0;                             // 清理资源

    // 服务管理
    virtual bool add_service(service_ptr service) = 0;     // 添加服务
    virtual bool remove_service(const std::string& name) = 0;     // 移除服务
    virtual service_ptr get_service(const std::string& name) const = 0; // 获取服务

    // 子监督器管理
    virtual bool add_child(supervisor_ptr child) = 0;     // 添加子监督器
    virtual bool remove_child(const std::string& name) = 0;                   // 移除子监督器
    virtual supervisor_ptr get_child(const std::string& name) const = 0; // 获取子监督器

    // 状态查询
    virtual bool is_healthy() const = 0;                    // 检查监督器健康状态
    virtual const supervisor_config& get_config() const = 0; // 获取监督器配置

    // 重启服务
    virtual bool restart_all_services() = 0;                // 重启所有服务
    virtual bool restart_dependent_services(const std::string& service_name) = 0; // 重启依赖服务
};

/**
 * @brief 基础监督器类，提供通用功能实现
 */
template <typename Impl>
class supervisor_base : public supervisor {
public:
    supervisor_base() = default;
    ~supervisor_base() override = default;

    // 获取监督器配置
    const supervisor_config& get_config() const override {
        return m_config;
    }

protected:
    // 设置监督器配置
    void set_config(const supervisor_config& config) {
        m_config = config;
    }

    // 处理服务失败
    virtual void handle_service_failure(const service_ptr& failed_service) {
        switch (m_config.strategy) {
            case supervisor_strategy::one_for_one:
                restart_service(failed_service);
                break;
            case supervisor_strategy::one_for_all:
                restart_all_services();
                break;
            case supervisor_strategy::rest_for_one:
                restart_dependent_services(failed_service->name());
                break;
        }
    }

    // 重启服务
    virtual bool restart_service(const service_ptr& service) {
        if (!service) {
            return false;
        }
        
        service->stop();
        return service->start();
    }

private:
    supervisor_config m_config; // 监督器配置
};

} // namespace mc

#endif // MC_CORE_SUPERVISOR_H 