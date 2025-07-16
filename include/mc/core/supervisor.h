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

#include <mc/core/config_schema.h>
#include <mc/core/service.h>

#include <memory>

namespace mc::core {
class supervisor;
using supervisor_ptr = std::shared_ptr<supervisor>;

/**
 * @brief 监督器接口类
 */
class MC_API supervisor {
public:
    MC_API virtual ~supervisor() = default;

    // 生命周期方法
    MC_API virtual bool init(const config::supervisor_config& config) = 0; // 初始化监督器
    MC_API virtual bool start()                                       = 0; // 启动监督器
    MC_API virtual bool stop()                                        = 0; // 停止监督器
    MC_API virtual void cleanup()                                     = 0; // 清理资源

    // 服务管理
    MC_API virtual bool             add_service(service_base_ptr service)      = 0; // 添加服务
    MC_API virtual bool             remove_service(const std::string& name)    = 0; // 移除服务
    MC_API virtual service_base_ptr get_service(const std::string& name) const = 0; // 获取服务

    // 子监督器管理
    MC_API virtual bool           add_child(supervisor_ptr child)          = 0; // 添加子监督器
    MC_API virtual bool           remove_child(const std::string& name)    = 0; // 移除子监督器
    MC_API virtual supervisor_ptr get_child(const std::string& name) const = 0; // 获取子监督器

    // 状态查询
    MC_API virtual bool                             is_healthy() const = 0; // 检查监督器健康状态
    MC_API virtual const config::supervisor_config& get_config() const = 0; // 获取监督器配置

    // 重启服务
    MC_API virtual bool restart_all_services()                                      = 0; // 重启所有服务
    MC_API virtual bool restart_dependent_services(const std::string& service_name) = 0; // 重启依赖服务

    // 获取监督器名称
    MC_API virtual const std::string& name() const = 0;
};

/**
 * @brief 基础监督器类，提供通用功能实现
 */
class MC_API supervisor_base : public supervisor {
public:
    MC_API supervisor_base()           = default;
    MC_API ~supervisor_base() override = default;

    MC_API const config::supervisor_config& get_config() const override;

protected:
    MC_API void set_config(const config::supervisor_config& config);

    MC_API virtual void handle_service_failure(const service_base_ptr& failed_service);
    MC_API virtual bool restart_service(const service_base_ptr& service);

private:
    config::supervisor_config m_config;
};

} // namespace mc::core

#endif // MC_CORE_SUPERVISOR_H