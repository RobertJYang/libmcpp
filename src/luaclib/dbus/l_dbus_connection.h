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

#ifndef MC_DBUS_L_DBUS_CONNECTION_H
#define MC_DBUS_L_DBUS_CONNECTION_H

#include "l_dbus_error.h"
#include "l_dbus_message.h"
#include <mc/dbus/connection.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace mc::dbus::lua {

constexpr const char* CONNECTION_METATABLE = "dbus.connection";

// 用于存储 connection 的包装器
// 注意：存储指针而不是对象本身，避免拷贝构造问题
struct connection_wrapper {
    mc::dbus::connection* conn_ptr;
    bool                  owns;

    explicit connection_wrapper(mc::dbus::connection* ptr, bool take_ownership = false)
        : conn_ptr(ptr), owns(take_ownership) {
    }

    mc::dbus::connection& get() {
        return *conn_ptr;
    }
};

// 创建 connection userdata 并推入 Lua 栈
inline int push_connection(lua_State* L, mc::dbus::connection& conn) {
    void* userdata = lua_newuserdata(L, sizeof(connection_wrapper));
    // 不拥有 connection，只是引用
    new (userdata) connection_wrapper(&conn, false);

    luaL_getmetatable(L, CONNECTION_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

// 从 Lua 栈获取 connection
inline connection_wrapper* check_connection(lua_State* L, int index = 1) {
    return static_cast<connection_wrapper*>(luaL_checkudata(L, index, CONNECTION_METATABLE));
}

// conn:close()
inline int connection_close(lua_State* L) {
    auto wrapper = check_connection(L);
    try {
        wrapper->get().disconnect();
        return 0;
    } catch (const std::exception& e) {
        return luaL_error(L, "close failed: %s", e.what());
    }
}

// conn:flush()
inline int connection_flush(lua_State* L) {
    auto wrapper = check_connection(L);
    try {
        wrapper->get().flush();
        return 0;
    } catch (const std::exception& e) {
        return luaL_error(L, "flush failed: %s", e.what());
    }
}

// conn:dispatch()
inline int connection_dispatch(lua_State* L) {
    auto wrapper = check_connection(L);
    try {
        wrapper->get().dispatch();
        return 0;
    } catch (const std::exception& e) {
        return luaL_error(L, "dispatch failed: %s", e.what());
    }
}

// conn:request_name(name, [flags])
// 返回两个参数：(success: bool, error: string|nil)
inline int connection_request_name(lua_State* L) {
    auto wrapper = check_connection(L);
    try {
        const char* name = luaL_checkstring(L, 2);
        uint32_t    flags =
            lua_isnumber(L, 3) ? static_cast<uint32_t>(lua_tointeger(L, 3)) : 0;

        auto [result, err_opt] = wrapper->get().request_name(name, flags);
        lua_pushboolean(L, result);

        if (result) {
            return 1;
        }

        // 获取错误对象并返回
        if (err_opt.has_value()) {
            push_error(L, err_opt.value());
        } else {
            error err;
            err.set_error(error_names::failed, "Unknown error");
            push_error(L, err);
        }
        return 2;
    } catch (const std::exception& e) {
        return luaL_error(L, "request_name failed: %s", e.what());
    }
}

// conn:is_connected()（实际调用 get_is_connected）
inline int connection_is_connected(lua_State* L) {
    auto wrapper = check_connection(L);
    try {
        bool result = wrapper->get().get_is_connected();
        lua_pushboolean(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "is_connected failed: %s", e.what());
    }
}

// conn:unique_name()
inline int connection_unique_name(lua_State* L) {
    auto wrapper = check_connection(L);
    try {
        auto name = wrapper->get().get_unique_name();
        if (name.empty()) {
            lua_pushnil(L);
        } else {
            lua_pushlstring(L, name.data(), name.size());
        }
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "unique_name failed: %s", e.what());
    }
}

// conn:set_unique_name(name)
inline int connection_set_unique_name(lua_State* L) {
    auto wrapper = check_connection(L);
    try {
        const char* name = luaL_checkstring(L, 2);
        wrapper->get().set_unique_name(name);
        return 0;
    } catch (const std::exception& e) {
        return luaL_error(L, "set_unique_name failed: %s", e.what());
    }
}

// conn:set_matchs(matchs)
// matchs 是一个字符串数组
inline int connection_set_matchs(lua_State* L) {
    auto wrapper = check_connection(L);
    try {
        // TODO: 实现 set_matchs 功能
        // 暂时为空实现
        return 0;
    } catch (const std::exception& e) {
        return luaL_error(L, "set_matchs failed: %s", e.what());
    }
}

// conn:register_object_fallback(path, handler)
inline int connection_register_object_fallback(lua_State* L) {
    auto wrapper = check_connection(L);
    try {
        // TODO: 实现 register_object_fallback 功能
        // 暂时为空实现
        return 0;
    } catch (const std::exception& e) {
        return luaL_error(L, "register_object_fallback failed: %s", e.what());
    }
}

// conn:register_object_path(path, handler)
inline int connection_register_object_path(lua_State* L) {
    auto wrapper = check_connection(L);
    try {
        // TODO: 实现 register_object_path 功能
        // 暂时为空实现
        return 0;
    } catch (const std::exception& e) {
        return luaL_error(L, "register_object_path failed: %s", e.what());
    }
}

// conn:unregister_object_path(path)
inline int connection_unregister_object_path(lua_State* L) {
    auto wrapper = check_connection(L);
    try {
        // TODO: 实现 unregister_object_path 功能
        // 暂时为空实现
        return 0;
    } catch (const std::exception& e) {
        return luaL_error(L, "unregister_object_path failed: %s", e.what());
    }
}

// conn:to_raw(add_ref)
// 获取底层 DBusConnection* 的 lightuserdata
// add_ref: 布尔值，是否增加引用计数（可选，默认false）
inline int connection_to_raw(lua_State* L) {
    auto wrapper = check_connection(L);
    try {
        // 获取 add_ref 参数（可选，默认 false）
        bool add_ref = false;
        if (lua_gettop(L) >= 2 && lua_isboolean(L, 2)) {
            add_ref = lua_toboolean(L, 2);
        }

        // 获取原始 DBusConnection 指针
        DBusConnection* raw_conn = wrapper->get().get_connection();
        if (!raw_conn) {
            return luaL_error(L, "connection is not initialized");
        }

        // 如果需要，增加引用计数
        if (add_ref) {
            dbus_connection_ref(raw_conn);
        }

        // 返回 lightuserdata
        lua_pushlightuserdata(L, raw_conn);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "to_raw failed: %s", e.what());
    }
}

// conn:send_with_reply_and_block(msg, timeout)
// 发送消息并阻塞等待回复
// 如果失败，通过 ensure_ok 抛出 lua_error
inline int connection_send_with_reply_and_block(lua_State* L) {
    auto wrapper = check_connection(L);
    try {
        // 参数1: message对象（userdata）
        if (!lua_isuserdata(L, 2)) {
            return luaL_error(L, "argument 1 must be a message");
        }

        // 获取 message wrapper
        auto* msg_wrapper = static_cast<message_wrapper*>(
            luaL_checkudata(L, 2, "dbus.message"));

        if (!msg_wrapper) {
            return luaL_error(L, "invalid message object");
        }

        // 参数2: timeout（可选，不传或传 nil 则使用 C++ 的默认值）
        // 需要复制 message，因为 C++ 方法会移动它
        mc::dbus::message msg_copy(msg_wrapper->msg.get_dbus_message(), true);

        mc::dbus::message reply;
        if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) {
            // 有 timeout 参数且不是 nil，使用指定值
            int timeout_ms = luaL_checkinteger(L, 3);
            reply          = wrapper->get().send_with_reply_and_block(
                std::move(msg_copy), mc::milliseconds(timeout_ms));
        } else {
            // 没有 timeout 参数或参数是 nil，使用 C++ 的默认参数
            reply = wrapper->get().send_with_reply_and_block(std::move(msg_copy));
        }

        // 返回回复消息的 lightuserdata
        lua_pushlightuserdata(L, reply.get_dbus_message());
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "send_with_reply_and_block failed: %s", e.what());
    }
}

// __index 元方法
inline int connection_index(lua_State* L) {
    // 从方法表中查找
    luaL_getmetatable(L, CONNECTION_METATABLE);
    lua_pushvalue(L, 2); // key
    lua_rawget(L, -2);

    if (!lua_isnil(L, -1)) {
        return 1;
    }

    // 没找到，返回 nil
    lua_pop(L, 1); // 弹出 nil
    return 1;      // 栈上已经有 nil 了
}

// __gc 元方法
inline int connection_gc(lua_State* L) {
    auto wrapper = check_connection(L);
    // 如果拥有 connection，需要删除
    if (wrapper->owns && wrapper->conn_ptr) {
        delete wrapper->conn_ptr;
    }
    wrapper->~connection_wrapper();
    return 0;
}

// 注册 connection metatable
inline void register_connection_metatable(lua_State* L) {
    // 创建 metatable
    luaL_newmetatable(L, CONNECTION_METATABLE);

    // 设置方法
    static const luaL_Reg methods[] = {{"close", connection_close},
                                       {"flush", connection_flush},
                                       {"dispatch", connection_dispatch},
                                       {"request_name", connection_request_name},
                                       {"is_connected", connection_is_connected},
                                       {"unique_name", connection_unique_name},
                                       {"set_unique_name", connection_set_unique_name},
                                       {"set_matchs", connection_set_matchs},
                                       {"register_object_fallback", connection_register_object_fallback},
                                       {"register_object_path", connection_register_object_path},
                                       {"unregister_object_path", connection_unregister_object_path},
                                       {"to_raw", connection_to_raw},
                                       {"send_with_reply_and_block", connection_send_with_reply_and_block},
                                       {nullptr, nullptr}};

    luaL_setfuncs(L, methods, 0);

    // 设置 __index
    lua_pushcfunction(L, connection_index);
    lua_setfield(L, -2, "__index");

    // 设置 __gc
    lua_pushcfunction(L, connection_gc);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1); // 弹出 metatable
}

} // namespace mc::dbus::lua

#endif // MC_DBUS_L_DBUS_CONNECTION_H
