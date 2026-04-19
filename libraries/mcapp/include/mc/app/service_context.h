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

#ifndef MC_APP_SERVICE_CONTEXT_H
#define MC_APP_SERVICE_CONTEXT_H

#include <mc/common.h>
#include <mc/memory.h>
#include <mc/runtime/runtime_context.h>

#include <mc/app/service_plan.h>

namespace mc::app {

class application;
class service;

using service_ptr = mc::shared_ptr<service>;

class MC_API service_context {
public:
    service_context() = default;
    service_context(application& app, const service_plan& plan, const service_definition& definition);

    explicit operator bool() const noexcept
    {
        return m_app != nullptr && m_plan != nullptr && m_definition != nullptr;
    }

    application&                  app() const;
    mc::runtime::runtime_context& runtime() const;
    const service_plan&           plan() const;
    const application_definition& application_config() const;
    const service_definition&     definition() const;
    service_ptr                   find_service(mc::string_view service_name) const;
    mc::runtime::any_executor     get_default_executor() const;
    mc::runtime::any_executor     create_io_strand() const;
    mc::runtime::any_executor     create_work_strand() const;
    void                          quit(int exit_code = 0) const;

private:
    application*              m_app{nullptr};
    const service_plan*       m_plan{nullptr};
    const service_definition* m_definition{nullptr};
};

} // namespace mc::app

#endif // MC_APP_SERVICE_CONTEXT_H
