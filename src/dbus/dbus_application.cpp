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

#include <mc/app/dbus_application.h>

#include <mc/dbus/service_protocol.h>

namespace mc::app {

void configure_dbus_protocol(application& app, mc::dbus::sd_bus& bus)
{
    app.set_protocol_factory([&bus](const service_definition& definition) {
        MC_UNUSED(definition);
        return mc::make_shared<mc::dbus::dbus_service_protocol>(bus);
    });
}

} // namespace mc::app
