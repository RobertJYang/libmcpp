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

#ifndef MC_LUACLIB_MDB_ACCESS_H
#define MC_LUACLIB_MDB_ACCESS_H

#include <memory>
#include <string>

#include "mc/mdb/mdb_access.h"
#include "mc/mdb/proxy_object.h"
#include "../utils/variant_utils.h"
#include <mc/dbus/connection.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace mc::mdb::lua {

constexpr const char* PROXY_OBJECT_METATABLE = "lmdb.mdb_access.proxy_object";

// 用于存储 proxy_object 的包装器
struct proxy_object_wrapper {
    std::shared_ptr<proxy_object> obj_ptr;

    explicit proxy_object_wrapper(proxy_object* obj)
        : obj_ptr(std::shared_ptr<proxy_object>(obj, [](proxy_object*) {
          }))
    {
    }

    explicit proxy_object_wrapper(std::shared_ptr<proxy_object> obj)
        : obj_ptr(std::move(obj))
    {
    }

    proxy_object& get()
    {
        return *obj_ptr;
    }
};

// 创建 proxy_object userdata 并推入 Lua 栈
inline int push_proxy_object(lua_State* L, proxy_object* obj)
{
    void* userdata = lua_newuserdata(L, sizeof(proxy_object_wrapper));
    new (userdata) proxy_object_wrapper(obj);

    luaL_getmetatable(L, PROXY_OBJECT_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

// 创建 proxy_object userdata 并推入 Lua 栈（使用 shared_ptr）
inline int push_proxy_object(lua_State* L, std::shared_ptr<proxy_object> obj)
{
    void* userdata = lua_newuserdata(L, sizeof(proxy_object_wrapper));
    new (userdata) proxy_object_wrapper(std::move(obj));

    luaL_getmetatable(L, PROXY_OBJECT_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

// 从 Lua 栈获取 proxy_object
inline proxy_object_wrapper* check_proxy_object(lua_State* L, int index = 1)
{
    return static_cast<proxy_object_wrapper*>(
        luaL_checkudata(L, index, PROXY_OBJECT_METATABLE));
}

// 注册 proxy_object metatable
void register_proxy_object_metatable(lua_State* L);

/**
 * @brief 注册 mdb_access 模块的所有函数到 Lua 栈顶的 table 中
 * @param L Lua 状态机
 */
void register_mdb_access_functions(lua_State* L);

} // namespace mc::mdb::lua

#endif // MC_LUACLIB_MDB_ACCESS_H
