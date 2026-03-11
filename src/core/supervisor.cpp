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

#include <mc/core/service.h>
#include <mc/core/supervisor.h>

namespace mc::core {
const config::supervisor_config& supervisor_base::get_config() const
{
    return m_config;
}

void supervisor_base::set_config(const config::supervisor_config& config)
{
    m_config = config;
}

void supervisor_base::handle_service_failure(const service_base_ptr& failed_service)
{
    switch (m_config.strategy) {
    case config::supervisor_strategy::one_for_one:
        restart_service(failed_service);
        break;
    case config::supervisor_strategy::one_for_all:
        restart_all_services();
        break;
    case config::supervisor_strategy::rest_for_one:
        restart_dependent_services(failed_service->name());
        break;
    }
}

bool supervisor_base::restart_service(const service_base_ptr& service)
{
    if (!service) {
        return false;
    }

    service->stop();
    return service->start();
}

} // namespace mc::core
