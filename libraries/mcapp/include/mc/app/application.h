/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_APP_APPLICATION_H
#define MC_APP_APPLICATION_H

#include <mc/app/base_app.h>
#include <mc/app/service_registry.h>
#include <mc/array.h>
#include <mc/engine/service.h>
#include <mc/module.h>
#include <mc/small_function.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace mc::engine {
class endpoint_service;
} // namespace mc::engine

namespace mc::app {

class MC_API application : public base_app {
public:
    static application* get() noexcept;
    static application& instance();
    static void         reset_for_test();

    application(const application&)            = delete;
    application& operator=(const application&) = delete;
    application(application&&)                 = delete;
    application& operator=(application&&)      = delete;

    ~application() override;

    service_registry&       registry();
    const service_registry& registry() const;

    template <typename ServiceType>
    void register_service(mc::string              type_name,
                          service_execution_model execution_model    = service_execution_model::inherit,
                          bool                    enabled_by_default = false)
    {
        m_registry.register_service<ServiceType>(std::move(type_name), execution_model, enabled_by_default);
    }

    bool initialize(const app_options& options = {});
    bool start();
    bool stop();

    int  exec();
    void quit(int exit_code = 0);

    bool is_initialized() const noexcept;
    bool is_running() const noexcept;
    int  exit_code() const noexcept;

    const service_plan&   plan() const noexcept;
    service_ptr           root_service() const;
    service_ptr           get_service(mc::string_view service_name) const;
    mc::array<mc::string> get_service_names() const;

    application();

private:
    bool initialize_internal(const app_options& options);
    bool load_module(mc::string_view module_name);
    void clear_runtime_state();
    void clear_application_state();
    bool create_services();
    void assign_service_executor(const service_definition& definition, const service_ptr& service_instance);
    bool stop_services();

    bool bootstrap_engine_endpoint();
    void teardown_engine_endpoint();

    service_registry                             m_registry;
    service_plan                                 m_plan;
    service_ptr                                  m_root_service;
    std::unordered_map<std::string, service_ptr> m_services;
    std::vector<mc::string>                      m_service_order;
    std::vector<mc::module::module_ptr>          m_modules;
    bool                                         m_initialized{false};
    bool                                         m_running{false};
    bool                                         m_runtime_started{false};
    int                                          m_exit_code{0};

    std::unique_ptr<mc::engine::endpoint_service> m_endpoint_service;
};

} // namespace mc::app

#endif // MC_APP_APPLICATION_H
