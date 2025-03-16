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
 * @brief 服务管理器，负责服务实例的管理
 */
#ifndef MC_SERVICE_MANAGER_H
#define MC_SERVICE_MANAGER_H

#include "mc/core/service.h"
#include <boost/program_options.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace mc {

namespace po = boost::program_options;

/**
 * @brief 服务管理器类
 */
class service_manager {
public:
    // 构造和析构
    service_manager();
    ~service_manager();

    // 服务实例管理
    bool add_service(const std::string& name, std::shared_ptr<service> service_instance);
    std::shared_ptr<service> get_service(const std::string& name) const;
    bool remove_service(const std::string& name);
    
    // 获取所有服务名
    std::vector<std::string> get_service_names() const;

    // 配置选项收集
    void collect_options(po::options_description& cli_opts, po::options_description& cfg_opts) const;

    // 服务生命周期管理
    bool start_services();
    bool stop_services();
    void cleanup_services();

private:
    // 成员变量
    std::unordered_map<std::string, std::shared_ptr<service>> m_services;  // 服务实例表
};

} // namespace mc

#endif // MC_SERVICE_MANAGER_H 