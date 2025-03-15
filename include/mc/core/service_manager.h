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
 * @file service_manager.h
 * @brief 服务管理器，负责服务的注册、创建和管理
 */
#ifndef MC_SERVICE_MANAGER_H
#define MC_SERVICE_MANAGER_H

#include "mc/core/service.h"
#include <boost/program_options.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace mc {

namespace po = boost::program_options;

/**
 * @brief 服务类型信息结构体
 */
struct service_type_info {
    std::function<std::shared_ptr<service>()> factory;                                      // 服务工厂函数
    std::function<void(po::options_description&, po::options_description&)> register_options;  // 配置选项注册函数
};

/**
 * @brief 服务管理器类
 */
class service_manager {
public:
    // 构造和析构
    service_manager();
    ~service_manager();

    // 服务类型注册
    bool register_service(const std::string& type, 
                         std::function<std::shared_ptr<service>()> factory,
                         std::function<void(po::options_description&, po::options_description&)> register_options);

    // 服务创建和获取
    std::shared_ptr<service> create_service(const std::string& type, const service_config& config);
    std::shared_ptr<service> get_service(const std::string& name) const;

    // 配置选项收集
    void collect_options(po::options_description& cli_opts, po::options_description& cfg_opts) const;

    // 服务生命周期管理
    bool start_services();
    bool stop_services();

private:
    // 成员变量
    std::unordered_map<std::string, service_type_info> m_service_types;  // 服务类型注册表
    std::unordered_map<std::string, std::shared_ptr<service>> m_services;  // 服务实例表
};

} // namespace mc

#endif // MC_SERVICE_MANAGER_H 