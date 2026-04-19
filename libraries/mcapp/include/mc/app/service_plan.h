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

#ifndef MC_APP_SERVICE_PLAN_H
#define MC_APP_SERVICE_PLAN_H

#include <mc/array.h>
#include <mc/dict.h>
#include <mc/string.h>
#include <mc/string_view.h>

namespace mc::app {

enum class service_execution_model { inherit, io_strand, work_strand };

struct application_definition {
    mc::string            name;
    mc::string            config_file;
    mc::array<mc::string> modules;
    std::size_t           io_threads{2};
    std::size_t           work_threads{2};
};

struct service_definition {
    mc::string              name;
    mc::string              path;
    mc::string              type;
    bool                    enabled{true};
    service_execution_model execution_model{service_execution_model::inherit};
    mc::dict                properties;
};

struct service_plan {
    const service_definition* find_service(mc::string_view service_name) const
    {
        for (const auto& definition : services) {
            if (definition.name == service_name) {
                return &definition;
            }
        }
        return nullptr;
    }

    application_definition        application;
    mc::array<service_definition> services;
};

} // namespace mc::app

#endif // MC_APP_SERVICE_PLAN_H
