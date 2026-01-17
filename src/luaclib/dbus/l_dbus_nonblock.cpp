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
#include "l_dbus_common.h"

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

// 注册 nonblock 模块的方法表
const luaL_Reg dbus_nonblock_methods[] = {{"start", dbus_nonblock_start},
                                          {"request_name", dbus_nonblock_request_name},
                                          {"close", dbus_nonblock_close},
                                          {"flush", dbus_nonblock_flush},
                                          {"dispatch", dbus_nonblock_dispatch},
                                          {nullptr, nullptr}};

// 注册 nonblock 模块的 metatable
void register_nonblock_metatable(lua_State* L) {
    register_metatable_impl(L, NON_BLOCKING_DBUS_METATABLE, dbus_nonblock_methods, dbus_nonblock_index,
                            dbus_nonblock_gc);
}

} // namespace mc::dbus::lua
