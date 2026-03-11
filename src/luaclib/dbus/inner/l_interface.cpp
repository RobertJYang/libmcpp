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

#include "l_interface.h"
#include "../../utils/variant_utils.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace mc::dbus::lua {

// Valid D-Bus type codes according to the specification
static const std::unordered_set<char> VALID_TYPE_CODES = {
    'y',  // BYTE
    'b',  // BOOLEAN
    'n',  // INT16
    'q',  // UINT16
    'i',  // INT32
    'u',  // UINT32
    'x',  // INT64
    't',  // UINT64
    'd',  // DOUBLE
    's',  // STRING
    'o',  // OBJECT_PATH
    'g',  // SIGNATURE
    'a',  // ARRAY
    '(',  // STRUCT start
    ')',  // STRUCT end
    '{',  // DICT entry start
    '}',  // DICT entry end
    'v'   // VARIANT
};

/**
 * Validates a D-Bus signature string according to the D-Bus specification
 * @param signature The signature string to validate
 * @return true if the signature is valid, false otherwise
 */
bool validate_dbus_signature(const std::string_view signature)
{
    if (signature.empty()) {
        return true; // Empty signature is valid (no arguments)
    }

    size_t pos = 0;
    while (pos < signature.length()) {
        char type_code = signature[pos];

        // Check if the type code is valid
        if (VALID_TYPE_CODES.find(type_code) == VALID_TYPE_CODES.end()) {
            return false;
        }

        switch (type_code) {
            case 'a': // Array
                // After 'a' must come a type
                pos++;
                continue;

            case '(': // Struct start
                {
                    // Find matching ')' while handling nested structures
                    int depth = 1;
                    pos++; // Move past '('

                    while (pos < signature.length() && depth > 0) {
                        char c = signature[pos];
                        if (c == '(') {
                            depth++;
                        } else if (c == ')') {
                            depth--;
                        } else if (VALID_TYPE_CODES.find(c) == VALID_TYPE_CODES.end()) {
                            return false; // Invalid character inside struct
                        }

                        if (depth > 0) {
                            pos++;
                        }
                    }

                    if (depth != 0) {
                        return false; // Unmatched parenthesis
                    }
                }
                break;

            case '{': // Dict entry start
                // Must be followed by two types (key and value) and a '}'
                pos++;
                if (pos >= signature.length() || VALID_TYPE_CODES.find(signature[pos]) == VALID_TYPE_CODES.end()) {
                    return false; // Invalid key type
                }

                pos++;
                if (pos >= signature.length() || VALID_TYPE_CODES.find(signature[pos]) == VALID_TYPE_CODES.end()) {
                    return false; // Invalid value type
                }

                pos++;
                if (pos >= signature.length() || signature[pos] != '}') {
                    return false; // Missing closing brace
                }
                break;

            default:
                // Simple type, just move to next character
                break;
        }

        pos++;
    }

    return true;
}

static int l_set_property(lua_State* L)
{
    l_interface* interface     = reinterpret_cast<l_interface*>(luaL_checkudata(L, 1, INTERFACE_METATABLE));
    const char*  name          = luaL_checkstring(L, 2);
    try {
        interface->impl->set_property(name, mc::lua::lua_to_variant(L, 3));
    } catch (const std::exception& e) {
        return luaL_error(L, "set property failed: %s", e.what());
    }
    return 0;
}

static int l_get_property(lua_State* L)
{
    l_interface* interface = reinterpret_cast<l_interface*>(luaL_checkudata(L, 1, INTERFACE_METATABLE));
    const char*  name      = luaL_checkstring(L, 2);
    try {
        return mc::lua::variant_to_lua(L, interface->impl->get_property(name));
    } catch (const std::exception& e) {
        return luaL_error(L, "get property failed: %s", e.what());
    }
}

static int l_add_property(lua_State* L)
{
    l_interface* interface = reinterpret_cast<l_interface*>(luaL_checkudata(L, 1, INTERFACE_METATABLE));
    const char*  name      = luaL_checkstring(L, 2);
    LDBusError error;
    if (!dbus_validate_member(name, &error)) {
        luaL_error(L, "invalid property name");
    }
    const char*  signature      = luaL_checkstring(L, 3);
    if (!validate_dbus_signature(signature)) {
        luaL_error(L, "invalid property signature");
    }
    mc::variant default_value = mc::lua::lua_to_variant(L, 4);
    luaL_checktype(L, 5, LUA_TBOOLEAN);
    bool readonly = lua_toboolean(L, 5);
    int  flags      = luaL_checkinteger(L, 6);
    dynamic_property prop = {name, signature, default_value, readonly, static_cast<uint8_t>(flags), {}};
    interface->impl->add_property(name, std::move(prop));
    return 0;
}

static int l_add_method(lua_State* L)
{
    l_interface* interface = reinterpret_cast<l_interface*>(luaL_checkudata(L, 1, INTERFACE_METATABLE));
    const char*  name      = luaL_checkstring(L, 2);
    LDBusError error;
    if (!dbus_validate_member(name, &error)) {
        luaL_error(L, "invalid method name");
    }
    const char*  i_args      = luaL_checkstring(L, 3);
    if (!validate_dbus_signature(i_args)) {
        luaL_error(L, "invalid method input signature");
    }
    const char*  o_args      = luaL_checkstring(L, 4);
    if (!validate_dbus_signature(o_args)) {
        luaL_error(L, "invalid method output signature");
    }
    luaL_checktype(L, 5, LUA_TFUNCTION);
    lua_pushvalue(L, 5);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    int  flags      = luaL_checkinteger(L, 6);
    dynamic_method m = {name, i_args, o_args, ref, static_cast<uint8_t>(flags)};
    interface->impl->add_method(name, std::move(m));
    return 0;
}

static int l_add_signal(lua_State* L)
{
    l_interface* interface = reinterpret_cast<l_interface*>(luaL_checkudata(L, 1, INTERFACE_METATABLE));
    const char*  name      = luaL_checkstring(L, 2);
    LDBusError error;
    if (!dbus_validate_member(name, &error)) {
        luaL_error(L, "invalid signal name");
    }
    const char*  signature      = luaL_checkstring(L, 3);
    if (!validate_dbus_signature(signature)) {
        luaL_error(L, "invalid signal signature");
    }
    int  flags      = luaL_checkinteger(L, 4);
    dynamic_signal s = {name, signature, static_cast<uint8_t>(flags)};
    interface->impl->add_signal(name, std::move(s));
    return 0;
}

int new_interface_class(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    LDBusError error;
    if (!dbus_validate_interface(name, &error)) {
        return luaL_error(L, "invalid inteface name");
    }
    void*       ud   = lua_newuserdata(L, sizeof(l_interface));
    new (ud) l_interface(name);
    luaL_getmetatable(L, INTERFACE_METATABLE);
    lua_setmetatable(L, -2);
    return 1;
}

static int l_gc_func(lua_State* L)
{
    l_interface* interface = reinterpret_cast<l_interface*>(luaL_checkudata(L, 1, INTERFACE_METATABLE));
    interface->~l_interface();
    return 0;
}

static int get_name(lua_State* L)
{
    l_interface* interface = reinterpret_cast<l_interface*>(luaL_checkudata(L, 1, INTERFACE_METATABLE));
    lua_pushstring(L, interface->impl->get_name().data());
    return 1;
}

// 注册 in 模块的方法表
static const luaL_Reg dbus_interface_methods[] = {{"set_property", l_set_property},
                                                  {"get_property", l_get_property},
                                                  {"add_property", l_add_property},
                                                  {"add_method", l_add_method},
                                                  {"add_signal", l_add_signal},
                                                  {"get_name", get_name},
                                                  {nullptr, nullptr}};

// 注册 object 模块的 metatable
void register_interface_metatable(lua_State* L)
{
    luaL_newmetatable(L, INTERFACE_METATABLE);
    luaL_setfuncs(L, dbus_interface_methods, 0);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
        // 设置 __gc
    lua_pushcfunction(L, l_gc_func);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1); // 弹出 metatable
}

} // namespace mc::dbus::lua