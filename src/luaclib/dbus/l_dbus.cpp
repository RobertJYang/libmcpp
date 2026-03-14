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

#include "inner/l_interface.h"
#include "inner/l_object.h"
#include "l_dbus_blocking.h"
#include "l_dbus_connection.h"
#include "l_dbus_error.h"
#include "l_dbus_message.h"
#include "l_dbus_nonblock.h"
#include "l_sd_bus.h"
#include <mc/log.h>

extern "C" {
#include <lualib.h>
}

#include <cstdlib>

extern "C" {
// 设置环境变量
static int ldbus_setenv(lua_State* L)
{
    const char* name      = luaL_checkstring(L, 1);
    const char* value     = luaL_checkstring(L, 2);
    int         overwrite = lua_toboolean(L, 3); // 默认 false

    int result = ::setenv(name, value, overwrite ? 1 : 0);
    if (result == 0) {
        lua_pushboolean(L, 1);
        return 1;
    } else {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Failed to set environment variable");
        return 2;
    }
}

// ldbus 模块加载函数
__attribute__((visibility("default"))) int luaopen_ldbus(lua_State* L)
{
    using namespace mc::dbus::lua;

    // 注册所有 metatable
    register_error_metatable(L);
    register_message_metatable(L);
    register_connection_metatable(L);
    register_blocking_metatable(L);
    register_nonblock_metatable(L);
    register_sd_bus_metatable(L);
    register_interface_metatable(L);
    register_object_metatable(L);

    // 创建主模块表
    lua_newtable(L);

    // ===== 工具函数 =====
    lua_pushcfunction(L, ldbus_setenv);
    lua_setfield(L, -2, "setenv");

    // ===== message 子表 =====
    lua_newtable(L);
    lua_pushcfunction(L, message_new_method_call);
    lua_setfield(L, -2, "new_method_call");
    lua_setfield(L, -2, "message");

    // ===== blocking 子表 =====
    lua_newtable(L);
    lua_pushcfunction(L, dbus_blocking_new);
    lua_setfield(L, -2, "new");
    lua_pushcfunction(L, dbus_blocking_open_user);
    lua_setfield(L, -2, "open_user");
    lua_pushcfunction(L, dbus_blocking_shutdown);
    lua_setfield(L, -2, "shutdown");
    lua_setfield(L, -2, "blocking");

    // ===== nonblock 子表 =====
    lua_newtable(L);
    lua_pushcfunction(L, dbus_nonblock_new);
    lua_setfield(L, -2, "new");
    lua_pushcfunction(L, dbus_nonblock_open_user);
    lua_setfield(L, -2, "open_user");
    lua_pushcfunction(L, dbus_nonblock_shutdown);
    lua_setfield(L, -2, "shutdown");
    lua_setfield(L, -2, "nonblock");

    // ===== interface 子表 =====
    lua_newtable(L);
    lua_pushcfunction(L, new_interface_class);
    lua_setfield(L, -2, "new");
    lua_setfield(L, -2, "interface");

    // ===== object 子表 =====
    lua_newtable(L);
    lua_pushcfunction(L, new_object_class);
    lua_setfield(L, -2, "new");
    lua_setfield(L, -2, "object");

    // ===== sd_bus 子表 =====
    lua_newtable(L);
    lua_pushcfunction(L, sd_bus_open_user);
    lua_setfield(L, -2, "open_user");
    lua_setfield(L, -2, "sd_bus");

    return 1;
}

} // extern "C"
