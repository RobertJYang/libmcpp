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

#include "l_sd_bus.h"
#include "../utils/variant_utils.h"
#include "l_dbus_common.h"
#include "l_dbus_message.h"
#include "l_skynet_syms.h"
#include "inner/l_object.h"
#include <mc/dbus/bus_mode_impl.h>
#include <mc/dbus/message.h>
#include <mc/dbus/sd_bus.h>
#include <mc/dbus/shm/harbor.h>
#include <mc/log.h>

namespace mc::dbus::lua {

// sd_bus 包装器 - 使用 shared_ptr 管理生命周期
struct sd_bus_wrapper {
    std::shared_ptr<mc::dbus::sd_bus> bus;     // 使用 shared_ptr 避免悬挂指针
    std::shared_ptr<mc::io_context>   io_ctx;  // 共享的 io_context

    sd_bus_wrapper(std::shared_ptr<mc::dbus::sd_bus> b, std::shared_ptr<mc::io_context> ctx)
        : bus(std::move(b)), io_ctx(std::move(ctx)) {
    }
};

// 辅助函数：获取 sd_bus_wrapper
static sd_bus_wrapper* get_sd_bus_wrapper(lua_State* L, int index = 1) {
    return static_cast<sd_bus_wrapper*>(luaL_checkudata(L, index, SD_BUS_METATABLE));
}

// sd_bus.open_user(start_now, is_blocking)
// 创建一个新的 sd_bus 用户总线连接
// 参数1: start_now - 是否立即启动连接（默认 false）
// 参数2: is_blocking - 是否使用阻塞模式（默认 false）
int sd_bus_open_user(lua_State* L) {
    try {
        bool start_now   = false;
        bool is_blocking = false;

        // 解析参数
        if (lua_gettop(L) >= 1 && lua_isboolean(L, 1)) {
            start_now = lua_toboolean(L, 1);
        }
        if (lua_gettop(L) >= 2 && lua_isboolean(L, 2)) {
            is_blocking = lua_toboolean(L, 2);
        }

        // 创建共享的 io_context
        const char* io_name = is_blocking ? "lua-sd-bus-blocking" : "lua-sd-bus-nonblocking";
        auto        io_ctx  = std::make_shared<mc::io_context>(1, io_name);

        // 使用 shared_ptr 创建 bus，避免生命周期问题
        auto bus = std::make_shared<mc::dbus::sd_bus>(start_now, is_blocking);

        auto* wrapper = static_cast<sd_bus_wrapper*>(lua_newuserdata(L, sizeof(sd_bus_wrapper)));
        new (wrapper) sd_bus_wrapper(bus, io_ctx);

        luaL_getmetatable(L, SD_BUS_METATABLE);
        lua_setmetatable(L, -2);

        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "Failed to open sd_bus: %s", e.what());
    }
}

// bus:request_name(name, flags)
static int sd_bus_request_name(lua_State* L) {
    try {
        auto*       wrapper = get_sd_bus_wrapper(L);
        const char* name    = luaL_checkstring(L, 2);
        uint32_t    flags   = luaL_optinteger(L, 3, 0);

        wrapper->bus->request_name(name, flags);
        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception& e) {
        lua_pushboolean(L, false);
        lua_pushstring(L, e.what());
        return 2;
    }
}

// bus:add_match(callback_id, member, interface, is_path_namespace, path, sender, type, destination)
static int sd_bus_add_match(lua_State* L) {
    try {
        auto* wrapper     = get_sd_bus_wrapper(L);
        int   callback_id = luaL_checkinteger(L, 2);

        // 解析 match rule 参数
        const char* member    = luaL_checkstring(L, 3);
        const char* interface = luaL_checkstring(L, 4);
        auto        rule      = mc::dbus::match_rule::new_signal(member, interface);

        int n = lua_gettop(L);
        if (n >= 6 && lua_isboolean(L, 5) && lua_isstring(L, 6)) {
            bool        is_path_namespace = lua_toboolean(L, 5);
            const char* path              = lua_tostring(L, 6);
            if (is_path_namespace) {
                rule.with_path_namespace(path);
            } else {
                rule.with_path(path);
            }
        }
        if (n >= 7 && lua_isstring(L, 7)) {
            rule.with_sender(lua_tostring(L, 7));
        }

        // 使用 skynet 回调
        uint64_t rule_id = 0;
        rule_id          = wrapper->bus->add_match(rule, [callback_id, &rule_id](mc::dbus::message& msg) mutable {
            auto skynet = SkynetSyms::get_instance();
            if (!skynet.skynet_send) {
                return;
            }
            dbus_message_ref(msg.get_dbus_message());
            constexpr std::size_t N = ((sizeof(std::uintptr_t) - 2) * 8);

            std::uintptr_t data      = reinterpret_cast<std::uintptr_t>(msg.get_dbus_message());
            uint32_t       data_high = data >> N;
            uint32_t       source    = (data_high << 16) | callback_id;
            std::size_t    sz        = data & ~(std::size_t(0xFFFF) << N);
            skynet.skynet_send_with_priority(nullptr, source, rule_id,
                                                      PTYPE_SIGNAL_PROCESS | PTYPE_TAG_DONTCOPY, 0, 0, sz, 0);
        });

        if (rule_id == 0) {
            lua_pushboolean(L, false);
            lua_pushstring(L, "add_match failed");
            return 2;
        }

        lua_pushboolean(L, true);
        lua_pushinteger(L, rule_id);
        return 2;
    } catch (const std::exception& e) {
        lua_pushboolean(L, false);
        lua_pushstring(L, e.what());
        return 2;
    }
}

// bus:remove_match(id)
static int sd_bus_remove_match(lua_State* L) {
    try {
        auto*    wrapper = get_sd_bus_wrapper(L);
        uint64_t id      = luaL_checkinteger(L, 2);
        wrapper->bus->remove_match(id);
        lua_pushboolean(L, true);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "Failed to remove_match: %s", e.what());
    }
}

// __gc 元方法
static int sd_bus_gc(lua_State* L) {
    auto* wrapper = static_cast<sd_bus_wrapper*>(lua_touserdata(L, 1));
    wrapper->~sd_bus_wrapper();
    return 0;
}

// __index 元方法
static int sd_bus_index(lua_State* L) {
    // 检查是否是方法调用
    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);

    if (!lua_isnil(L, -1)) {
        return 1;
    }

    lua_pop(L, 2);

    // 检查是否是属性访问
    const char* key = lua_tostring(L, 2);
    if (key && strcmp(key, "bus") == 0) {
        // 访问 bus 属性：返回 dbus.blocking 或 dbus.nonblock 对象
        try {
            auto* sd_wrapper = get_sd_bus_wrapper(L, 1);

            // 创建借用模式的 dbus_wrapper
            void*         userdata  = lua_newuserdata(L, sizeof(dbus_wrapper));
            dbus_wrapper* dbus_wrap = new (userdata)
                dbus_wrapper(sd_wrapper->bus->get_bus_mode_impl(), sd_wrapper->io_ctx);

            // 设置对应的 metatable (blocking 或 nonblocking)
            if (sd_wrapper->bus->is_blocking()) {
                luaL_getmetatable(L, "dbus.blocking");
            } else {
                luaL_getmetatable(L, "dbus.nonblock");
            }
            lua_setmetatable(L, -2);

            return 1;
        } catch (const std::exception& e) {
            return luaL_error(L, "Failed to access bus property: %s", e.what());
        }
    }

    lua_pushnil(L);
    return 1;
}

static int sd_bus_register_object(lua_State* L) {
    try {
        auto* wrapper = get_sd_bus_wrapper(L);
        auto* object = reinterpret_cast<l_object*>(luaL_checkudata(L, 2, OBJECT_METATABLE));
        wrapper->bus->register_object(object->impl);
        return 0;
    } catch (const std::exception& e) {
        return luaL_error(L, "Failed to register object: %s", e.what());
    }
}

static int sd_bus_unregister_object(lua_State* L) {
    try {
        auto* wrapper = get_sd_bus_wrapper(L);
        const char* path = luaL_checkstring(L, 2);
        wrapper->bus->unregister_object(path);
        return 0;
    } catch (const std::exception& e) {
        return luaL_error(L, "Failed to unregister object: %s", e.what());
    }
}

// ==================== sd_bus 方法 ====================

// 方法表
static const luaL_Reg sd_bus_methods[] = {
    {"request_name", sd_bus_request_name},
    {"add_match", sd_bus_add_match},
    {"remove_match", sd_bus_remove_match},
    {"register_object", sd_bus_register_object},
    {"unregister_object", sd_bus_unregister_object},
    {nullptr, nullptr}};

// 模块函数表
static const luaL_Reg sd_bus_module_funcs[] = {
    {"open_user", sd_bus_open_user},
    {nullptr, nullptr}};

void register_sd_bus_metatable(lua_State* L) {
    // 注册 sd_bus metatable
    luaL_newmetatable(L, SD_BUS_METATABLE);

    luaL_setfuncs(L, sd_bus_methods, 0);

    lua_pushcfunction(L, sd_bus_index);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, sd_bus_gc);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1);
}

void register_sd_bus_module(lua_State* L) {
    register_sd_bus_metatable(L);

    luaL_newlib(L, sd_bus_module_funcs);
    lua_setglobal(L, "sd_bus");
}

} // namespace mc::dbus::lua
