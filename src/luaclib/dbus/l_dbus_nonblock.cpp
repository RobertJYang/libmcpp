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

#include "l_dbus_nonblock.h"
#include "../utils/variant_utils.h"
#include "l_dbus_common.h"
#include <mc/dbus/message.h>
#include <mc/dbus/sd_bus.h>
#include <boost/asio/post.hpp>
#include <thread>

namespace mc::dbus::lua {

constexpr const char* NON_BLOCKING_DBUS_METATABLE = "dbus.nonblock";

// 静态 io_context（延迟初始化）
static std::shared_ptr<mc::io_context>& get_static_io_context() {
    static auto s_io_context = std::make_shared<mc::io_context>(1, "lua-dbus-nonblock");
    return s_io_context;
}

// dbus.nonblock.shutdown()
int dbus_nonblock_shutdown(lua_State* L) {
    return dbus_shutdown(L);
}

// dbus.nonblock.open_user([start_now])
int dbus_nonblock_open_user(lua_State* L) {
    // 获取 start_now 参数（默认 true）
    bool start_now = true;
    if (lua_isboolean(L, 1)) {
        start_now = lua_toboolean(L, 1);
    }

    // 如果需要自动启动，使用 post_connect 回调
    auto post_connect = start_now ? std::function<void(mc::dbus::connection&)>([](mc::dbus::connection& conn) {
        conn.start();
    })
                                  : std::function<void(mc::dbus::connection&)>(nullptr);

    return dbus_open_user_impl(L, get_static_io_context(), NON_BLOCKING_DBUS_METATABLE, post_connect);
}

// dbus.nonblock.new(arg)
int dbus_nonblock_new(lua_State* L) {
    return dbus_new_impl(L, NON_BLOCKING_DBUS_METATABLE, "lua-dbus-nonblock");
}

// conn:start()
int dbus_nonblock_start(lua_State* L) {
    return dbus_start(L);
}

// conn:request_name(name, flags)
int dbus_nonblock_request_name(lua_State* L) {
    return dbus_request_name(L);
}

// __index 元方法
int dbus_nonblock_index(lua_State* L) {
    return dbus_index(L);
}

// conn:close()
int dbus_nonblock_close(lua_State* L) {
    return dbus_close(L);
}

// conn:flush()
int dbus_nonblock_flush(lua_State* L) {
    return dbus_flush(L);
}

// conn:dispatch()
int dbus_nonblock_dispatch(lua_State* L) {
    return dbus_dispatch(L);
}

// __gc 元方法
int dbus_nonblock_gc(lua_State* L) {
    return dbus_gc(L);
}

int dbus_nonblock_async_call(lua_State* L) {
    try {
        auto*       wrapper      = static_cast<dbus_wrapper*>(luaL_checkudata(L, 1, NON_BLOCKING_DBUS_METATABLE));
        luaL_checktype(L, 2, LUA_TFUNCTION); // 回调函数
        std::string service_name = luaL_checkstring(L, 3);
        std::string path         = luaL_checkstring(L, 4);
        std::string interface    = luaL_checkstring(L, 5);
        std::string method       = luaL_checkstring(L, 6);
        std::string signature    = luaL_optstring(L, 7, "");
        variants    args;
        int         top = lua_gettop(L);
        if (top >= 8) {
            args = mc::lua::lua_to_variants(L, 8);
        }

        mc::dbus::sd_bus bus(wrapper->conn, false);
        auto [error, result] = bus.pcall({service_name, path, interface, method, signature, std::move(args)});

        // 获取回调函数
        lua_pushvalue(L, 2); // 复制回调函数到栈顶

        if (error.has_value()) {
            // 失败：调用回调 ok=false, error
            lua_pushboolean(L, false);
            lua_pushstring(L, error.value().c_str());

            if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                return luaL_error(L, "method result callback failed: %s", err ? err : "unknown error");
            }
        } else {
            // 成功：调用回调 ok=true, result...
            lua_pushboolean(L, true);
            int ret_count = mc::lua::variants_to_lua(L, result);

            // 调用回调函数：1个固定参数 + ret_count个返回值
            if (lua_pcall(L, 1 + ret_count, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                return luaL_error(L, "method result callback failed: %s", err ? err : "unknown error");
            }
        }

        return 0;
    } catch (const std::exception& e) {
        return luaL_error(L, "call method failed: %s", e.what());
    }
}

int dbus_nonblock_async_timeout_call(lua_State* L) {
    try {
        auto*       wrapper      = static_cast<dbus_wrapper*>(luaL_checkudata(L, 1, NON_BLOCKING_DBUS_METATABLE));
        int         timeout_ms   = luaL_checkinteger(L, 2);
        luaL_checktype(L, 3, LUA_TFUNCTION); // 回调函数
        std::string service_name = luaL_checkstring(L, 4);
        std::string path         = luaL_checkstring(L, 5);
        std::string interface    = luaL_checkstring(L, 6);
        std::string method       = luaL_checkstring(L, 7);
        std::string signature    = luaL_optstring(L, 8, "");
        variants    args;
        int         top = lua_gettop(L);
        if (top >= 9) {
            args = mc::lua::lua_to_variants(L, 9);
        }

        mc::dbus::sd_bus bus(wrapper->conn, false);
        auto [error, result] =
            bus.timeout_pcall(mc::milliseconds(timeout_ms), {service_name, path, interface, method, signature, std::move(args)});

        // 获取回调函数
        lua_pushvalue(L, 3); // 复制回调函数到栈顶

        if (error.has_value()) {
            // 失败：调用回调 ok=false, error
            lua_pushboolean(L, false);
            lua_pushstring(L, error.value().c_str());

            if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                return luaL_error(L, "method result callback failed: %s", err ? err : "unknown error");
            }
        } else {
            // 成功：调用回调 ok=true, result...
            lua_pushboolean(L, true);
            int ret_count = mc::lua::variants_to_lua(L, result);

            // 调用回调函数：1个固定参数 + ret_count个返回值
            if (lua_pcall(L, 1 + ret_count, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                return luaL_error(L, "method result callback failed: %s", err ? err : "unknown error");
            }
        }

        return 0;
    } catch (const std::exception& e) {
        return luaL_error(L, "call method failed: %s", e.what());
    }
}

int dbus_nonblock_async_shm_timeout_call(lua_State* L) {
    try {
        auto*       wrapper      = static_cast<dbus_wrapper*>(luaL_checkudata(L, 1, NON_BLOCKING_DBUS_METATABLE));
        int         timeout_ms   = luaL_checkinteger(L, 2);
        luaL_checktype(L, 3, LUA_TFUNCTION); // 回调函数
        std::string service_name = luaL_checkstring(L, 4);
        std::string path         = luaL_checkstring(L, 5);
        std::string interface    = luaL_checkstring(L, 6);
        std::string method       = luaL_checkstring(L, 7);
        std::string signature    = luaL_optstring(L, 8, "");
        variants    args;
        int         top = lua_gettop(L);
        if (top >= 9) {
            args = mc::lua::lua_to_variants(L, 9);
        }

        mc::dbus::sd_bus bus(wrapper->conn, false);

        // 获取回调函数
        lua_pushvalue(L, 3); // 复制回调函数到栈顶

        std::optional<variants> result;
        try {
            result = bus.shm_timeout_call(mc::milliseconds(timeout_ms), {service_name, path, interface, method, signature, std::move(args)});
        } catch (const std::exception& e) {
            // 捕获 shm_timeout_call 抛出的异常，传递给回调
            lua_pushboolean(L, false);
            lua_pushstring(L, e.what());

            if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                return luaL_error(L, "method result callback failed: %s", err ? err : "unknown error");
            }
            // 返回 true 给 Lua，表示共享内存传输成功，即使方法调用失败
            lua_pushboolean(L, true);
            return 1;
        }

        if (result.has_value()) {
            // 成功：调用回调 ok=true, result...
            lua_pushboolean(L, true);
            int ret_count = mc::lua::variants_to_lua(L, result.value());

            // 调用回调函数：1个固定参数 + ret_count个返回值
            if (lua_pcall(L, 1 + ret_count, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                return luaL_error(L, "method result callback failed: %s", err ? err : "unknown error");
            }
            // 返回 true 给 Lua，表示共享内存传输成功
            lua_pushboolean(L, true);
        } else {
            // 返回 false 给 Lua，表示共享内存传输失败
            lua_pushboolean(L, false);
        }
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "call method failed: %s", e.what());
    }
}

// 注册 nonblock 模块的方法表
const luaL_Reg dbus_nonblock_methods[] = {{"start", dbus_nonblock_start},
                                          {"request_name", dbus_nonblock_request_name},
                                          {"close", dbus_nonblock_close},
                                          {"flush", dbus_nonblock_flush},
                                          {"dispatch", dbus_nonblock_dispatch},
                                          {"async_call", dbus_nonblock_async_call},
                                          {"async_timeout_call", dbus_nonblock_async_timeout_call},
                                          {"async_shm_timeout_call", dbus_nonblock_async_shm_timeout_call},
                                          {"dispatch", dbus_nonblock_dispatch},
                                          {nullptr, nullptr}};

// 注册 nonblock 模块的 metatable
void register_nonblock_metatable(lua_State* L) {
    register_metatable_impl(L, NON_BLOCKING_DBUS_METATABLE, dbus_nonblock_methods, dbus_nonblock_index,
                            dbus_nonblock_gc);
}

} // namespace mc::dbus::lua
