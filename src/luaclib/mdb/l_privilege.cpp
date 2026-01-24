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

#include "l_privilege.h"
#include <mc/mdb/privilege.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <vector>

namespace mc::mdb::lua {

// privilege.get_privilege_str(privileges_table)
// 输入：Lua table，包含权限值数组
// 输出：字符串，权限值的按位或结果
static int l_get_privilege_str(lua_State* L) {
    try {
        // 检查参数是否为 table
        luaL_checktype(L, 1, LUA_TTABLE);

        // 从 table 中读取权限数组
        std::vector<uint32_t> privileges;
        lua_pushnil(L);
        while (lua_next(L, 1) != 0) {
            if (lua_isinteger(L, -1)) {
                privileges.push_back(static_cast<uint32_t>(lua_tointeger(L, -1)));
            }
            lua_pop(L, 1);
        }

        // 调用 C++ 函数
        std::string result = mc::mdb::privilege::get_privilege_str(privileges);

        // 返回结果
        lua_pushstring(L, result.c_str());
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_privilege_str failed: %s", e.what());
    }
}

// privilege.validate(expected_privilege)
// 输入：整数，期望的权限值
// 输出：无（成功时），或抛出异常
static int l_validate(lua_State* L) {
    try {
        // 检查参数
        uint32_t expected_privilege = static_cast<uint32_t>(luaL_checkinteger(L, 1));

        // 调用 C++ 函数
        mc::mdb::privilege::validate(expected_privilege);

        return 0;
    } catch (const std::exception& e) {
        return luaL_error(L, "validate failed: %s", e.what());
    }
}

// 注册 privilege 常量到 Lua
void register_privilege_constants(lua_State* L) {
    // 权限枚举常量
    lua_pushinteger(L, mc::mdb::privilege::privilege::read_only);
    lua_setfield(L, -2, "ReadOnly");

    lua_pushinteger(L, mc::mdb::privilege::privilege::diagnose_mgmt);
    lua_setfield(L, -2, "DiagnoseMgmt");

    lua_pushinteger(L, mc::mdb::privilege::privilege::security_mgmt);
    lua_setfield(L, -2, "SecurityMgmt");

    lua_pushinteger(L, mc::mdb::privilege::privilege::basic_setting);
    lua_setfield(L, -2, "BasicSetting");

    lua_pushinteger(L, mc::mdb::privilege::privilege::user_mgmt);
    lua_setfield(L, -2, "UserMgmt");

    lua_pushinteger(L, mc::mdb::privilege::privilege::power_mgmt);
    lua_setfield(L, -2, "PowerMgmt");

    lua_pushinteger(L, mc::mdb::privilege::privilege::vmm_mgmt);
    lua_setfield(L, -2, "VMMMgmt");

    lua_pushinteger(L, mc::mdb::privilege::privilege::kvm_mgmt);
    lua_setfield(L, -2, "KVMMgmt");

    lua_pushinteger(L, mc::mdb::privilege::privilege::configure_self);
    lua_setfield(L, -2, "ConfigureSelf");

    // 认证状态枚举常量
    lua_pushinteger(L, static_cast<int>(mc::mdb::privilege::auth_state::no_auth));
    lua_setfield(L, -2, "NoAuth");

    lua_pushinteger(L, static_cast<int>(mc::mdb::privilege::auth_state::auth_required));
    lua_setfield(L, -2, "AuthRequired");
}

// 注册 privilege 函数
void register_privilege_functions(lua_State* L) {
    static const luaL_Reg privilege_funcs[] = {
        {"get_privilege_str", l_get_privilege_str},
        {"validate", l_validate},
        {nullptr, nullptr}};

    luaL_setfuncs(L, privilege_funcs, 0);
}

} // namespace mc::mdb::lua
