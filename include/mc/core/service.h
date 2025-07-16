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
 * @file service.h
 * @brief 服务基类定义
 */
#ifndef MC_CORE_SERVICE_H
#define MC_CORE_SERVICE_H

#include <boost/program_options.hpp>

#include <mc/core/object.h>
#include <mc/dict.h>

#include <memory>
#include <string>
#include <vector>

namespace mc::core {
namespace po = boost::program_options;

/**
 * @brief 服务状态枚举
 */
enum class service_state {
    stopped,  // 已停止
    starting, // 正在启动
    running,  // 运行中
    stopping, // 正在停止
    failed    // 失败
};

/**
 * @brief 服务配置结构
 */
struct service_config {
    std::string              name;         // 服务名称
    std::string              type;         // 服务类型
    std::vector<std::string> dependencies; // 服务依赖
    dict                     properties;   // 服务属性配置
};

/**
 * @brief 服务接口类
 */
class abstract_service : public mc::core::object {
public:
    virtual ~abstract_service() = default;

    virtual bool init(dict args) = 0;
    virtual bool start()         = 0;
    virtual bool stop()          = 0;
    virtual void cleanup()       = 0;

    virtual const std::string& name() const       = 0;
    virtual service_state      get_state() const  = 0;
    virtual bool               is_healthy() const = 0;

    virtual const service_config&           get_config() const       = 0;
    virtual const std::vector<std::string>& get_dependencies() const = 0;
};

/**
 * @brief 基础服务类，提供通用功能实现
 */
class MC_API service_base : public abstract_service {
public:
    MC_API explicit service_base(std::string name = "");
    MC_API ~service_base() override;

    MC_API void  set_name(std::string name);
    MC_API const std::string& name() const override;

    MC_API service_state get_state() const override;

    MC_API const service_config& get_config() const override;

    MC_API const std::vector<std::string>& get_dependencies() const override;

    bool init(dict args) override {
        return true;
    }

    bool start() override {
        return true;
    }

    bool stop() override {
        return true;
    }

    void cleanup() override {
    }

    bool is_healthy() const override {
        return true;
    }

protected:
    void set_state(service_state state);
    void set_dependencies(const std::vector<std::string>& dependencies);

    struct impl;
    std::unique_ptr<impl> m_base_impl;
};

using service_base_ptr = mc::shared_ptr<abstract_service>;

} // namespace mc::core

#endif // MC_CORE_SERVICE_H