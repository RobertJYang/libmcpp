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

/**
 * @brief 发射接口添加信号
 * @param conn [in/out] DBus连接对象
 * @param obj [in] 抽象对象
 * @constraint 用于通知DBus总线某个对象添加了新接口
 */
MC_API void emit_interfaces_added(connection& conn, const engine::abstract_object& obj);

/**
 * @brief 发射接口移除信号
 * @param conn [in/out] DBus连接对象
 * @param obj [in] 抽象对象
 * @constraint 用于通知DBus总线某个对象移除了接口
 */
MC_API void emit_interfaces_removed(connection& conn, const engine::abstract_object& obj);

/**
 * @brief 发射属性变更信号
 * @param conn [in/out] DBus连接对象
 * @param obj [in/out] 抽象对象
 * @param prop [in] 属性基类引用
 * @param value [in] 新属性值
 * @constraint 用于通知DBus总线某个对象的属性发生了变化
 */
MC_API void emit_properties_changed(connection& conn, engine::abstract_object& obj,
                                    const engine::property_base& prop, const variant& value);

} // namespace mc::dbus

#endif