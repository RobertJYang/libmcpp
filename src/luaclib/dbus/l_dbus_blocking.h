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

#ifndef MC_DBUS_L_DBUS_BLOCKING_H
#define MC_DBUS_L_DBUS_BLOCKING_H

extern "C" {
#include <lua.h>
}

namespace mc::dbus::lua {

// 注册 blocking 模块的 metatable
void register_blocking_metatable(lua_State* L);

// blocking 模块的函数
int dbus_blocking_new(lua_State* L);
int dbus_blocking_open_user(lua_State* L);
int dbus_blocking_shutdown(lua_State* L);

} // namespace mc::dbus::lua

#endif // MC_DBUS_L_DBUS_BLOCKING_H
