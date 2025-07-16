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
#include <mc/core/plugin_manager.h>
#include <mc/core/service.h>
#include <mc/core/service_factory.h>
#include <mc/core/service_manager.h>
#include <mc/core/supervisor_manager.h>
#include <mc/signal_slot.h>
#include <mc/singleton.h>

#include <memory>
#include <string>
#include <unordered_map>

namespace mc::core {

/**
 * @brief 应用程序类
 *
 * 继承自 service_base 类，使其成为服务树的根节点
 */
class MC_API application {
public:
    static MC_API application& instance();
    static MC_API void         reset_for_test();

    application(const application&)            = delete;
    application& operator=(const application&) = delete;
    application(application&&)                 = delete;
    application& operator=(application&&)      = delete;

    MC_API ~application();

    MC_API void  set_version(const std::string& version);
    MC_API const std::string& version() const;

    MC_API plugin_manager&     get_plugin_manager();
    MC_API service_factory&    get_service_factory();
    MC_API service_manager&    get_service_manager();
    MC_API config_manager&     get_config_manager();
    MC_API supervisor_manager& get_supervisor_manager();

    MC_API bool initialize();
    MC_API bool initialize(int argc, char** argv);
    MC_API bool start();
    MC_API bool stop();
    MC_API bool is_stopped() const;

    MC_API void exec();

private:
    application();

    struct impl;
    std::unique_ptr<impl> m_impl;
};

inline application& app() {
    return mc::core::application::instance();
}
} // namespace mc::core

namespace mc {

inline mc::core::application& app() {
    return mc::core::app();
}

} // namespace mc

#endif // MC_APPLICATION_H