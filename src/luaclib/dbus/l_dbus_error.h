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

#ifndef MC_DBUS_L_DBUS_ERROR_H
#define MC_DBUS_L_DBUS_ERROR_H

#include <mc/dbus/error.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace mc::dbus::lua {

constexpr const char* ERROR_METATABLE = "dbus.error";

// 用于存储 error 的包装器
struct error_wrapper {
    mc::dbus::error err;

    error_wrapper() = default;
    explicit error_wrapper(const mc::dbus::error& e) : err(e)
    {}
    explicit error_wrapper(mc::dbus::error&& e) : err(std::move(e))
    {}
};

// 检查并获取 error userdata
inline error_wrapper* check_error(lua_State* L, int index = 1)
{
    return static_cast<error_wrapper*>(luaL_checkudata(L, index, ERROR_METATABLE));
}

// 创建 error userdata 并推入 Lua 栈
inline int push_error(lua_State* L, const mc::dbus::error& err)
{
    void* userdata = lua_newuserdata(L, sizeof(error_wrapper));
    new (userdata) error_wrapper(err);

    luaL_getmetatable(L, ERROR_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

// 创建 error userdata (移动语义) 并推入 Lua 栈
inline int push_error(lua_State* L, mc::dbus::error&& err)
{
    void* userdata = lua_newuserdata(L, sizeof(error_wrapper));
    new (userdata) error_wrapper(std::move(err));

    luaL_getmetatable(L, ERROR_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

// err:is_set()
// 检查错误是否已设置
inline int error_is_set(lua_State* L)
{
    auto wrapper = check_error(L);
    lua_pushboolean(L, wrapper->err.is_set());
    return 1;
}

// __tostring 元方法
// 格式化为 "name : message"
inline int error_tostring(lua_State* L)
{
    auto wrapper = check_error(L);
    if (!wrapper->err.is_set()) {
        lua_pushstring(L, "no error");
        return 1;
    }

    const char* name    = wrapper->err.name ? wrapper->err.name : "unknown";
    const char* message = wrapper->err.message ? wrapper->err.message : "";

    lua_pushfstring(L, "%s : %s", name, message);
    return 1;
}

// err:ensure_ok()
// 如果错误已设置，则抛出 lua_error
inline int error_ensure_ok(lua_State* L)
{
    auto wrapper = check_error(L);
    if (wrapper->err.is_set()) {
        // 复用 error_tostring 获取格式化的错误信息
        error_tostring(L); // 将格式化的字符串推入栈
        const char* error_msg = lua_tostring(L, -1);
        return luaL_error(L, "%s", error_msg);
    }
    return 0;
}

// __gc 元方法
inline int error_gc(lua_State* L)
{
    auto wrapper = check_error(L);
    wrapper->~error_wrapper();
    return 0;
}

// __index 元方法
inline int error_index(lua_State* L)
{
    // 从方法表中查找
    luaL_getmetatable(L, ERROR_METATABLE);
    lua_pushvalue(L, 2); // key
    lua_rawget(L, -2);

    if (!lua_isnil(L, -1)) {
        return 1;
    }

    // 支持直接访问属性
    const char* key = lua_tostring(L, 2);
    if (key == nullptr) {
        return 0;
    }

    auto wrapper = check_error(L, 1);

    if (std::strcmp(key, "name") == 0) {
        if (wrapper->err.is_set() && wrapper->err.name) {
            lua_pushstring(L, wrapper->err.name);
        } else {
            lua_pushnil(L);
        }
        return 1;
    }

    if (std::strcmp(key, "message") == 0) {
        if (wrapper->err.is_set() && wrapper->err.message) {
            lua_pushstring(L, wrapper->err.message);
        } else {
            lua_pushnil(L);
        }
        return 1;
    }

    return 0;
}

// 注册 error metatable
inline void register_error_metatable(lua_State* L)
{
    // 创建 metatable
    luaL_newmetatable(L, ERROR_METATABLE);

    // 设置方法
    static const luaL_Reg methods[] = {{"is_set", error_is_set}, {"ensure_ok", error_ensure_ok}, {nullptr, nullptr}};

    luaL_setfuncs(L, methods, 0);

    // 设置 __index
    lua_pushcfunction(L, error_index);
    lua_setfield(L, -2, "__index");

    // 设置 __tostring
    lua_pushcfunction(L, error_tostring);
    lua_setfield(L, -2, "__tostring");

    // 设置 __gc
    lua_pushcfunction(L, error_gc);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1); // 弹出 metatable
}

} // namespace mc::dbus::lua

#endif // MC_DBUS_L_DBUS_ERROR_H
