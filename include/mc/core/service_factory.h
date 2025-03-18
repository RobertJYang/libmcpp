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
 * @file service_factory.h
 * @brief 服务工厂，负责服务的注册和创建
 */
#ifndef MC_CORE_SERVICE_FACTORY_H
#define MC_CORE_SERVICE_FACTORY_H

#include "mc/core/service.h"
#include <boost/program_options.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace mc {

namespace po          = boost::program_options;
using service_creator = std::function<service_ptr(std::string&& name, mc::dict&& args)>;

/**
 * @brief 服务工厂类
 */
class service_factory {
public:
    struct service_options {
        po::options_description cli;
        po::options_description cfg;
    };

    // 构造函数
    service_factory() : m_opts(std::make_unique<service_options>()) {
    }

    // 虚析构函数
    virtual ~service_factory() = default;

    /**
     * @brief 注册服务类型
     * @tparam ServiceType 服务类型
     * @param type_name 服务类型名称
     */
    template <typename ServiceType>
    void register_service(const std::string& service_name) {
        m_creators[service_name] = [](std::string&& object_name, mc::dict&& args) {
            auto service = std::make_shared<ServiceType>(std::forward<std::string>(object_name));
            if (service->init(std::forward<mc::dict>(args))) {
                return std::static_pointer_cast<mc::service, ServiceType>(service);
            }
            return service_ptr();
        };

        // 注册服务选项（如果有）
        ServiceType::register_options(m_opts->cli, m_opts->cfg);
    }

    /**
     * @brief 创建服务实例
     * @param service_name 服务类型
     * @param object_name 对象名称
     * @param args 服务配置
     * @return 服务实例
     */
    virtual service_ptr create_service(const std::string& service_name, std::string object_name,
                               mc::dict args) {
        auto it = m_creators.find(service_name);
        if (it == m_creators.end()) {
            return service_ptr();
        }

        return it->second(std::forward<std::string>(object_name), std::forward<mc::dict>(args));
    }

    /**
     * @brief 检查是否存在指定类型的服务
     * @param service_name 服务类型名称
     * @return 如果存在返回true，否则返回false
     */
    bool has_service(const std::string& service_name) const {
        return m_creators.find(service_name) != m_creators.end();
    }

    /**
     * @brief 获取所有注册的服务类型
     * @return 服务类型名称列表
     */
    std::vector<std::string> get_service_types() const {
        std::vector<std::string> types;
        for (const auto& [type, _] : m_creators) {
            types.push_back(type);
        }
        return types;
    }

    std::unique_ptr<service_options>& get_service_options() {
        return m_opts;
    }

private:
    std::unordered_map<std::string, service_creator> m_creators;
    std::unique_ptr<service_options>                 m_opts;
};

} // namespace mc

#endif // MC_CORE_SERVICE_FACTORY_H