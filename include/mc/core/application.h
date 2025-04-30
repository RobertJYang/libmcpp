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
 * @file application.h
 * @brief 应用程序类，作为核心类，协调各个管理器
 */
#ifndef MC_APPLICATION_H
#define MC_APPLICATION_H

#include <mc/core/config_manager.h>
#include <mc/core/object.h>
#include <mc/core/plugin_manager.h>
#include <mc/core/service_factory.h>
#include <mc/core/service_manager.h>
#include <mc/core/supervisor_manager.h>
#include <mc/signal_slot.h>
#include <mc/singleton.h>

#include <boost/asio.hpp>

#include <memory>
#include <string>
#include <unordered_map>

namespace mc::core {

/**
 * @brief 应用程序类
 *
 * 继承自 object 类，使其成为对象树的根节点
 */
class application : public core::object {
public:
    static application& instance() {
        return singleton<application>::instance_with_creator([]() {
            return new application();
        });
    }

    application(const application&)            = delete;
    application& operator=(const application&) = delete;
    application(application&&)                 = delete;
    application& operator=(application&&)      = delete;

    ~application() override;

    void destory() override {
        // 应用程序对象的生命周期由单例管理，这里什么都不用做
    }

    void               set_version(const std::string& version);
    const std::string& version() const;

    plugin_manager&     get_plugin_manager();
    service_factory&    get_service_factory();
    service_manager&    get_service_manager();
    config_manager&     get_config_manager();
    supervisor_manager& get_supervisor_manager();

    bool initialize();
    bool initialize(int argc, char** argv);
    void start();
    void exec();
    void stop();
    bool is_stopped() const;

private:
    application();

    struct impl;
    std::unique_ptr<impl> m_impl;
};

inline application& app() {
    return application::instance();
}

} // namespace mc::core

#endif // MC_APPLICATION_H