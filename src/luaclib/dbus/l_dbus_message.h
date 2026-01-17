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

#ifndef MC_DBUS_L_DBUS_MESSAGE_H
#define MC_DBUS_L_DBUS_MESSAGE_H

#include <mc/dbus/message.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace mc::dbus::lua {

constexpr const char* MESSAGE_METATABLE = "dbus.message";

// 用于存储 message 的包装器
struct message_wrapper {
    mc::dbus::message msg;

    message_wrapper() = default;
    explicit message_wrapper(mc::dbus::message&& m)
        : msg(std::move(m)) {
    }
};

// 检查并获取 message userdata
inline message_wrapper* check_message(lua_State* L, int index = 1) {
    return static_cast<message_wrapper*>(luaL_checkudata(L, index, MESSAGE_METATABLE));
}

// 创建 message userdata 并推入 Lua 栈
inline int push_message(lua_State* L, mc::dbus::message&& msg) {
    void* userdata = lua_newuserdata(L, sizeof(message_wrapper));
    new (userdata) message_wrapper(std::move(msg));

    luaL_getmetatable(L, MESSAGE_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

// dbus.message.new_method_call(destination, path, interface, method)
// 创建方法调用消息
inline int message_new_method_call(lua_State* L) {
    const char* destination = luaL_checkstring(L, 1);
    const char* path        = luaL_checkstring(L, 2);
    const char* interface   = luaL_checkstring(L, 3);
    const char* method      = luaL_checkstring(L, 4);

    try {
        auto msg = mc::dbus::message::new_method_call(destination, path, interface, method);
        return push_message(L, std::move(msg));
    } catch (const std::exception& e) {
        return luaL_error(L, "new_method_call failed: %s", e.what());
    }
}

// __tostring 元方法
inline int message_tostring(lua_State* L) {
    auto wrapper = check_message(L);
    lua_pushstring(L, "dbus.message");
    return 1;
}

// __gc 元方法
inline int message_gc(lua_State* L) {
    auto wrapper = check_message(L);
    wrapper->~message_wrapper();
    return 0;
}

// 注册 message metatable
inline void register_message_metatable(lua_State* L) {
    // 创建 metatable
    luaL_newmetatable(L, MESSAGE_METATABLE);

    // 设置 __tostring
    lua_pushcfunction(L, message_tostring);
    lua_setfield(L, -2, "__tostring");

    // 设置 __gc
    lua_pushcfunction(L, message_gc);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1); // 弹出 metatable
}

} // namespace mc::dbus::lua

#endif // MC_DBUS_L_DBUS_MESSAGE_H
