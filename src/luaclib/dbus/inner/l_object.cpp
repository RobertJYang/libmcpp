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

#include "l_object.h"
#include "l_interface.h"
#include "../../utils/variant_utils.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace mc::dbus::lua {

void dynamic_object::set_object_path(std::string_view path)
{
    m_path = path;
}

void dynamic_object::set_property(std::string intf_name, std::string prop_name, const mc::variant& value)
{
    auto it = m_interfaces.find(intf_name);
    if (it == m_interfaces.end()) {
        MC_THROW(mc::exception, "interface not found");
    }
    it->second->set_property(prop_name, value);
}

mc::variant dynamic_object::get_property(std::string intf_name, std::string prop_name) const
{
    auto it = m_interfaces.find(intf_name);
    if (it == m_interfaces.end()) {
        MC_THROW(mc::exception, "interface not found");
    }
    return it->second->get_property(prop_name);
}

void dynamic_object::add_interface(mc::shared_ptr<dynamic_interface> interface)
{
    m_interfaces.emplace(interface->get_name(), interface);
}

mc::shared_ptr<dynamic_interface> dynamic_object::get_interface(std::string intf_name) const
{
    auto it = m_interfaces.find(intf_name);
    if (it == m_interfaces.end()) {
        return nullptr;
    }
    return it->second;
}

struct l_object {
    l_object() {
        impl = mc::make_shared<dynamic_object>();
    }

    mc::shared_ptr<dynamic_object> impl;
};

static int l_set_property(lua_State* L) {
    l_object* object     = reinterpret_cast<l_object*>(luaL_checkudata(L, 1, OBJECT_METATABLE));
    const char*  intf_name          = luaL_checkstring(L, 2);
    const char*  prop_name          = luaL_checkstring(L, 3);
    try {
        object->impl->set_property(intf_name, prop_name, mc::lua::lua_to_variant(L, 4));
    } catch (const std::exception& e) {
        return luaL_error(L, "set property failed: %s", e.what());
    }
    return 0;
}

static int l_get_property(lua_State* L) {
    l_object* object = reinterpret_cast<l_object*>(luaL_checkudata(L, 1, OBJECT_METATABLE));
    const char*  intf_name          = luaL_checkstring(L, 2);
    const char*  prop_name          = luaL_checkstring(L, 3);
    try {
        mc::lua::variant_to_lua(L, object->impl->get_property(intf_name, prop_name));
    } catch (const std::exception& e) {
        return luaL_error(L, "get property failed: %s", e.what());
    }
    return 1;
}

int new_object_class(lua_State* L) {
    const char*  path      = luaL_checkstring(L, 1);
    LDBusError error;
    if (!dbus_validate_path(path, &error)) {
        return luaL_error(L, "invalid path");
    }
    void*       ud   = lua_newuserdata(L, sizeof(l_object));
    l_object *obj = new (ud) l_object();
    obj->impl->set_object_path(path);
    luaL_getmetatable(L, OBJECT_METATABLE);
    lua_setmetatable(L, -2);
    return 1;
}

static int l_register_interface(lua_State* L) {
    l_object* object = reinterpret_cast<l_object*>(luaL_checkudata(L, 1, OBJECT_METATABLE));
    l_interface* interface = reinterpret_cast<l_interface*>(luaL_checkudata(L, 2, INTERFACE_METATABLE));
    object->impl->add_interface(interface->impl);
    return 0;
}

static int l_get_interface(lua_State* L) {
    l_object* object = reinterpret_cast<l_object*>(luaL_checkudata(L, 1, OBJECT_METATABLE));
    const char*  intf_name          = luaL_checkstring(L, 2);
    auto intf = object->impl->get_interface(intf_name);
    void*       ud   = lua_newuserdata(L, sizeof(l_interface));
    new (ud) l_interface(intf);
    luaL_getmetatable(L, INTERFACE_METATABLE);
    lua_setmetatable(L, -2);
    return 1;
}

static int l_gc_func(lua_State* L) {
    l_object* object = reinterpret_cast<l_object*>(luaL_checkudata(L, 1, OBJECT_METATABLE));
    object->~l_object();
    return 0;
}

// 注册 object 模块的方法表
const luaL_Reg dbus_object_methods[] = {{"set_property", l_set_property},
                                          {"get_property", l_get_property},
                                          {"register_interface", l_register_interface},
                                          {"get_interface", l_get_interface},
                                          {nullptr, nullptr}};

// 注册 object 模块的 metatable
void register_object_metatable(lua_State* L) {
    luaL_newmetatable(L, OBJECT_METATABLE);
    luaL_setfuncs(L, dbus_object_methods, 0);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
        // 设置 __gc
    lua_pushcfunction(L, l_gc_func);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1); // 弹出 metatable
}

} // namespace mc::dbus::lua