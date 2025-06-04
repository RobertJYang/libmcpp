/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_DBUS_SIGNAL_H
#define MC_DBUS_SIGNAL_H

#include <mc/dbus/connection.h>
#include <mc/dbus/message.h>
#include <mc/engine/base.h>

namespace mc::dbus {

void emit_interfaces_added(connection& conn, const engine::abstract_object& obj);

void emit_interfaces_removed(connection& conn, const engine::abstract_object& obj);

void emit_properties_changed(connection& conn, engine::abstract_object& obj,
                             const engine::property_base& prop, const variant& value);

} // namespace mc::dbus

#endif