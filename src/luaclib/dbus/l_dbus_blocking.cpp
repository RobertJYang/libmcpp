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

#include "l_dbus_blocking.h"
#include "l_dbus_common.h"
#include <mc/log.h>

namespace mc::dbus::lua {

constexpr const char* BLOCKING_DBUS_METATABLE = "dbus.blocking";

// 静态 io_context（延迟初始化）
static std::shared_ptr<mc::io_context>& get_static_io_context() {
    static auto s_io_context = std::make_shared<mc::io_context>(1, "lua-dbus-blocking");
    return s_io_context;
}

// dbus.blocking.shutdown()
int dbus_blocking_shutdown(lua_State* L) {
    return dbus_shutdown(L);
}

// dbus.blocking.open_user()
int dbus_blocking_open_user(lua_State* L) {
    return dbus_open_user_impl(L, get_static_io_context(), BLOCKING_DBUS_METATABLE);
}

// dbus.blocking.new(arg)
int dbus_blocking_new(lua_State* L) {
    return dbus_new_impl(L, BLOCKING_DBUS_METATABLE, "lua-dbus-blocking");
}

// conn:request_name(name, flags)
int dbus_blocking_request_name(lua_State* L) {
    return dbus_request_name(L);
}

// __index 元方法
int dbus_blocking_index(lua_State* L) {
    return dbus_index(L);
}

// conn:close()
int dbus_blocking_close(lua_State* L) {
    return dbus_close(L);
}

// conn:flush()
int dbus_blocking_flush(lua_State* L) {
    return dbus_flush(L);
}

// conn:dispatch()
int dbus_blocking_dispatch(lua_State* L) {
    return dbus_dispatch(L);
}

// conn:run_once(timeout_ms)
int dbus_blocking_run_once(lua_State* L) {
    try {
        auto* wrapper    = static_cast<dbus_wrapper*>(luaL_checkudata(L, 1, BLOCKING_DBUS_METATABLE));
        int   timeout_ms = luaL_checkinteger(L, 2);

        if (!wrapper->conn.is_connected()) {
            lua_pushboolean(L, false);
            return 1;
        }

        DBusConnection* conn = wrapper->conn.get_connection();

        // 阻塞等待消息，带超时
        dbus_connection_read_write(conn, timeout_ms);

        // 取出一条消息
        DBusMessage* msg = dbus_connection_pop_message(conn);
        if (!msg) {
            lua_pushboolean(L, false);
            return 1;
        }

        // 分发消息（通过内部的filter处理）
        wrapper->conn.dispatch();

        // 释放消息
        dbus_message_unref(msg);

        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "Failed to run once: %s", e.what());
    }
}

// conn:run_until(callback, total_timeout_ms, step_ms)
int dbus_blocking_run_until(lua_State* L) {
    try {
        auto* wrapper = static_cast<dbus_wrapper*>(luaL_checkudata(L, 1, BLOCKING_DBUS_METATABLE));
        luaL_checktype(L, 2, LUA_TFUNCTION); // 回调函数
        int total_timeout_ms = luaL_checkinteger(L, 3);
        int step_ms          = 100;
        if (!lua_isnil(L, 4)) {
            step_ms = luaL_checkinteger(L, 4);
        }

        if (!wrapper->conn.is_connected()) {
            lua_pushboolean(L, false);
            return 1;
        }

        // 创建回调引用
        lua_pushvalue(L, 2); // 复制回调函数到栈顶
        int callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);

        auto start_time = std::chrono::steady_clock::now();
        auto timeout    = std::chrono::milliseconds(total_timeout_ms);
        bool result     = false;

        while (true) {
            // 执行一次消息循环
            DBusConnection* conn = wrapper->conn.get_connection();
            dbus_connection_read_write(conn, step_ms);
            DBusMessage* msg = dbus_connection_pop_message(conn);
            if (msg) {
                wrapper->conn.dispatch();
                dbus_message_unref(msg);
            }

            // 检查回调条件
            lua_rawgeti(L, LUA_REGISTRYINDEX, callback_ref);
            if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
                const char* error = lua_tostring(L, -1);
                elog("Lua回调执行失败: ${error}", ("error", error ? error : "unknown"));
                lua_pop(L, 1);
                break;
            }

            bool cb_result = lua_toboolean(L, -1);
            lua_pop(L, 1);

            if (cb_result) {
                result = true;
                break;
            }

            // 检查总超时时间
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (elapsed >= timeout) {
                break;
            }
        }

        // 释放回调引用
        luaL_unref(L, LUA_REGISTRYINDEX, callback_ref);

        lua_pushboolean(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "Failed to run until: %s", e.what());
    }
}

// __gc 元方法
int dbus_blocking_gc(lua_State* L) {
    return dbus_gc(L);
}

// 注册 blocking 模块的方法表
const luaL_Reg dbus_blocking_methods[] = {{"request_name", dbus_blocking_request_name},
                                          {"close", dbus_blocking_close},
                                          {"flush", dbus_blocking_flush},
                                          {"dispatch", dbus_blocking_dispatch},
                                          {"run_once", dbus_blocking_run_once},
                                          {"run_until", dbus_blocking_run_until},
                                          {nullptr, nullptr}};

// 注册 blocking 模块的 metatable
void register_blocking_metatable(lua_State* L) {
    register_metatable_impl(L, BLOCKING_DBUS_METATABLE, dbus_blocking_methods, dbus_blocking_index,
                            dbus_blocking_gc);
}

} // namespace mc::dbus::lua
