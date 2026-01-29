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

#ifndef MC_DBUS_L_SD_BUS_H
#define MC_DBUS_L_SD_BUS_H

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace mc::dbus::lua {

// sd_bus metatable 名称
constexpr const char* SD_BUS_METATABLE = "dbus.sd_bus";

// sd_bus.open_user(start_now, is_blocking) - 创建新的 sd_bus 用户总线连接
int sd_bus_open_user(lua_State* L);

// 注册 sd_bus 模块
void register_sd_bus_module(lua_State* L);

// 注册 sd_bus metatable
void register_sd_bus_metatable(lua_State* L);

} // namespace mc::dbus::lua

#endif // MC_DBUS_L_SD_BUS_H
