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

#ifndef MC_DBUS_L_DBUS_COMMON_H
#define MC_DBUS_L_DBUS_COMMON_H

#include <mc/dbus/connection.h>
#include <mc/dbus/enums.h>
#include <mc/dbus/error.h>
#include <mc/runtime.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <memory>

#include "l_dbus_connection.h"

namespace mc::dbus::lua {

// io_context 包装器
struct io_context_wrapper {
    std::shared_ptr<mc::io_context> ctx;
    bool                            should_delete;

    io_context_wrapper(std::shared_ptr<mc::io_context> c, bool del = true)
        : ctx(std::move(c)), should_delete(del) {
    }
};

// DBus 连接包装器
struct dbus_wrapper {
    mc::dbus::connection            conn;
    std::shared_ptr<mc::io_context> io_ctx;

    dbus_wrapper(mc::dbus::connection c, std::shared_ptr<mc::io_context> ctx)
        : conn(std::move(c)), io_ctx(std::move(ctx)) {
    }
};

// 辅助函数：从 Lua 栈获取 io_context
inline std::shared_ptr<mc::io_context> get_io_context(lua_State* L, int index,
                                                      const char* metatable_name) {
    if (lua_isuserdata(L, index)) {
        auto* wrapper = static_cast<io_context_wrapper*>(luaL_checkudata(L, index, metatable_name));
        return wrapper->ctx;
    }
    return nullptr;
}

// 辅助函数：转换总线类型
inline mc::dbus::bus_type to_bus_type(const char* type) {
    if (strcmp(type, "session") == 0) {
        return mc::dbus::bus_type::session;
    } else if (strcmp(type, "system") == 0) {
        return mc::dbus::bus_type::system;
    } else if (strcmp(type, "starter") == 0) {
        return mc::dbus::bus_type::starter;
    }
    return mc::dbus::bus_type::system;
}

// 打开 D-Bus 总线连接
inline mc::dbus::connection open_bus_connection(mc::io_context& executor, mc::dbus::bus_type type) {
    mc::dbus::error err;
    DBusBusType     dbus_type;

    switch (type) {
    case mc::dbus::bus_type::session:
        dbus_type = DBUS_BUS_SESSION;
        break;
    case mc::dbus::bus_type::system:
        dbus_type = DBUS_BUS_SYSTEM;
        break;
    case mc::dbus::bus_type::starter:
        dbus_type = DBUS_BUS_STARTER;
        break;
    default:
        MC_THROW(mc::invalid_argument_exception, "Unsupported bus type");
    }

    dbus_error_init(&err);
    DBusConnection* conn = dbus_bus_get_private(dbus_type, &err);

    if (!err.is_set() && conn) {
        dbus_connection_set_exit_on_disconnect(conn, false);
        return mc::dbus::connection(executor, conn, false);
    }

    std::string error_msg = err.message ? std::string(err.message) : "Unknown error";
    dbus_error_free(&err);
    MC_THROW(mc::system_exception, "DBus connection failed: ${error}", ("error", error_msg));
}

// 打开 D-Bus 地址连接
inline mc::dbus::connection open_address_connection(mc::io_context& executor, std::string_view address) {
    mc::dbus::error err;
    dbus_error_init(&err);
    DBusConnection* raw_conn = dbus_connection_open_private(address.data(), &err);

    if (!err.is_set() && raw_conn) {
        dbus_connection_set_exit_on_disconnect(raw_conn, false);
        return mc::dbus::connection(executor, raw_conn, false);
    }

    std::string error_msg = err.message ? std::string(err.message) : "Unknown error";
    dbus_error_free(&err);
    MC_THROW(mc::system_exception, "DBus connection failed: ${error}", ("error", error_msg));
}

// 通用方法实现

// shutdown 方法（静态）
inline int dbus_shutdown(lua_State* L) {
    try {
        ::dbus_shutdown();
        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "Failed to shutdown dbus: %s", e.what());
    }
}

// start 方法
inline int dbus_start(lua_State* L) {
    try {
        auto* wrapper = reinterpret_cast<dbus_wrapper*>(lua_touserdata(L, 1));
        bool  result  = wrapper->conn.start();
        lua_pushboolean(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "Failed to start connection: %s", e.what());
    }
}

// request_name 方法
// 返回两个参数：(success: bool, error: string|nil)
inline int dbus_request_name(lua_State* L) {
    try {
        auto*       wrapper = reinterpret_cast<dbus_wrapper*>(lua_touserdata(L, 1));
        const char* name    = luaL_checkstring(L, 2);
        uint32_t    flags   = luaL_optinteger(L, 3, 0);

        auto [result, err_opt] = wrapper->conn.request_name(name, flags);

        lua_pushboolean(L, result);

        if (result) {
            lua_pushnil(L); // 成功时第二个参数为 nil
        } else {
            // 获取真实的错误信息
            std::string error_msg;
            if (err_opt.has_value()) {
                const auto& err = err_opt.value();
                if (err.name) {
                    error_msg = err.name;
                }
                if (err.message) {
                    if (!error_msg.empty()) {
                        error_msg += " : ";
                    }
                    error_msg += err.message;
                }
            }
            if (error_msg.empty()) {
                error_msg = "请求名称失败";
            }
            lua_pushstring(L, error_msg.c_str());
        }
        return 2;
    } catch (const std::exception& e) {
        lua_pushboolean(L, false);   // 异常时第一个参数为 false
        lua_pushstring(L, e.what()); // 异常时第二个参数为错误信息
        return 2;
    }
}

// close 方法
inline int dbus_close(lua_State* L) {
    try {
        auto* wrapper = reinterpret_cast<dbus_wrapper*>(lua_touserdata(L, 1));
        wrapper->conn.disconnect();
        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "Failed to close connection: %s", e.what());
    }
}

// flush 方法
inline int dbus_flush(lua_State* L) {
    try {
        auto* wrapper = reinterpret_cast<dbus_wrapper*>(lua_touserdata(L, 1));
        wrapper->conn.flush();
        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "Failed to flush connection: %s", e.what());
    }
}

// dispatch 方法
inline int dbus_dispatch(lua_State* L) {
    try {
        auto* wrapper = reinterpret_cast<dbus_wrapper*>(lua_touserdata(L, 1));
        wrapper->conn.dispatch();
        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "Failed to dispatch messages: %s", e.what());
    }
}

// __index 元方法（处理 conn 属性）
inline int dbus_index(lua_State* L) {
    // 检查是否是方法调用
    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2); // 复制 key
    lua_rawget(L, -2);   // 查找方法表

    if (!lua_isnil(L, -1)) {
        return 1; // 找到方法，返回
    }

    lua_pop(L, 2); // 弹出 nil 和 metatable

    // 检查是否是属性访问
    const char* key = lua_tostring(L, 2);
    if (key) {
        try {
            auto* wrapper = reinterpret_cast<dbus_wrapper*>(lua_touserdata(L, 1));

            // conn 属性：返回 connection 对象
            if (strcmp(key, "conn") == 0) {
                // 创建 connection 的拷贝并推入 Lua 栈
                return push_connection(L, wrapper->conn);
            }
        } catch (const std::exception& e) {
            return luaL_error(L, "Failed to access property '%s': %s", key, e.what());
        }
    }

    lua_pushnil(L);
    return 1;
}

// __gc 元方法
inline int dbus_gc(lua_State* L) {
    auto* wrapper = reinterpret_cast<dbus_wrapper*>(lua_touserdata(L, 1));
    wrapper->~dbus_wrapper();
    return 0;
}

// 通用的 dbus.xxx.new(arg) 实现
inline int dbus_new_impl(lua_State* L, const char* metatable_name, const char* io_name) {
    try {
        // 创建 io_context
        auto io_ctx = std::make_shared<mc::io_context>(1, io_name);

        mc::dbus::connection conn;

        if (lua_isnil(L, 1) || lua_isnoneornil(L, 1)) {
            // 参数为 nil：使用 dbus_bus_get_private 连接到会话总线
            conn = open_bus_connection(*io_ctx, mc::dbus::bus_type::session);
        } else if (lua_isstring(L, 1)) {
            // 参数为字符串：使用 dbus_connection_open_private
            const char* address = lua_tostring(L, 1);
            conn                = open_address_connection(*io_ctx, address);
        } else if (lua_isuserdata(L, 1)) {
            // 参数为 userdata：使用已有的 DBus 连接
            auto* raw_conn = *static_cast<DBusConnection**>(lua_touserdata(L, 1));
            conn           = mc::dbus::connection(*io_ctx, raw_conn, true);
        } else {
            return luaL_error(L, "Invalid argument type: expected nil, string, or userdata");
        }

        // 创建 userdata
        auto* wrapper = static_cast<dbus_wrapper*>(lua_newuserdata(L, sizeof(dbus_wrapper)));
        new (wrapper) dbus_wrapper(std::move(conn), io_ctx);

        // 设置 metatable
        luaL_getmetatable(L, metatable_name);
        lua_setmetatable(L, -2);

        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "Failed to create dbus: %s", e.what());
    }
}

// 通用的 dbus.xxx.open_user() 实现
inline int dbus_open_user_impl(lua_State*                                 L,
                               std::shared_ptr<mc::io_context>&           static_io_ctx,
                               const char*                                metatable_name,
                               std::function<void(mc::dbus::connection&)> post_connect = nullptr) {
    try {
        // 创建连接
        auto conn = open_bus_connection(*static_io_ctx, mc::dbus::bus_type::session);

        // 执行连接后的操作（如果有）
        if (post_connect) {
            post_connect(conn);
        }

        // 创建 wrapper
        void* userdata = lua_newuserdata(L, sizeof(dbus_wrapper));
        new (userdata) dbus_wrapper(std::move(conn), static_io_ctx);

        // 设置 metatable
        luaL_getmetatable(L, metatable_name);
        lua_setmetatable(L, -2);

        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "Failed to open user session: %s", e.what());
    }
}

// 通用的 register_metatable 实现
inline void register_metatable_impl(lua_State*      L,
                                    const char*     metatable_name,
                                    const luaL_Reg* methods,
                                    lua_CFunction   index_func,
                                    lua_CFunction   gc_func) {
    // 创建 metatable
    luaL_newmetatable(L, metatable_name);

    // 注册方法到 metatable
    luaL_setfuncs(L, methods, 0);

    // 设置 __index 元方法
    lua_pushcfunction(L, index_func);
    lua_setfield(L, -2, "__index");

    // 设置 __gc
    lua_pushcfunction(L, gc_func);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1); // 弹出 metatable
}

} // namespace mc::dbus::lua

#endif // MC_DBUS_L_DBUS_COMMON_H
