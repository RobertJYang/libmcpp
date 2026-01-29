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

#include "../utils/variant_utils.h"
#include <mc/dbus/shm/shm_tree.h>
#include <mc/exception.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace mc {
namespace dbus {
namespace lua {

// MDB 查询函数

static int l_get_mdb_object(lua_State* L) {
    try {
        int nargs = lua_gettop(L);
        if (nargs < 2) {
            return luaL_error(L, "get_mdb_object requires 2 arguments: path, interfaces");
        }

        const char*      path_cstr = luaL_checkstring(L, 1);
        std::string_view path(path_cstr);
        variant          interfaces_var = mc::lua::lua_to_variant(L, 2);

        variants interfaces;
        if (interfaces_var.is_array()) {
            interfaces = interfaces_var.get_array();
        } else {
            return luaL_error(L, "interfaces must be an array");
        }

        auto result = shm_tree::get_mdb_object(path, interfaces);
        if (result.has_value()) {
            mc::lua::variant_to_lua(L, variant(result.value()));
            return 1;
        } else {
            lua_pushnil(L);
            return 1;
        }
    } catch (const mc::exception& e) {
        return luaL_error(L, "get_mdb_object failed: %s", e.what());
    }
}

static int l_get_mdb_sub_paths(lua_State* L) {
    try {
        int nargs = lua_gettop(L);
        if (nargs < 3) {
            return luaL_error(L, "get_mdb_sub_paths requires at least 3 arguments");
        }

        const char*      path_cstr = luaL_checkstring(L, 1);
        std::string_view path(path_cstr);
        uint32_t         depth          = static_cast<uint32_t>(luaL_checkinteger(L, 2));
        variant          interfaces_var = mc::lua::lua_to_variant(L, 3);
        uint32_t         skip           = (nargs >= 4) ? static_cast<uint32_t>(luaL_optinteger(L, 4, 0)) : 0;
        uint32_t         top            = (nargs >= 5) ? static_cast<uint32_t>(luaL_optinteger(L, 5, 0)) : 0;

        // ignore_case 是可选参数，如果提供了则必须严格检查类型
        bool ignore_case = false;
        if (nargs >= 6) {
            if (!lua_isboolean(L, 6)) {
                return luaL_error(L, "ignore_case must be a boolean");
            }
            ignore_case = lua_toboolean(L, 6) != 0;
        }

        variants interfaces;
        if (interfaces_var.is_array()) {
            interfaces = interfaces_var.get_array();
        } else {
            return luaL_error(L, "interfaces must be an array");
        }

        auto result = shm_tree::get_mdb_sub_paths(path, depth, interfaces, skip, top, ignore_case);
        if (result.has_value()) {
            mc::lua::variant_to_lua(L, variant(result.value()));
            return 1;
        } else {
            lua_pushnil(L);
            return 1;
        }
    } catch (const mc::exception& e) {
        return luaL_error(L, "get_mdb_sub_paths failed: %s", e.what());
    }
}

static int l_is_valid_mdb_path(lua_State* L) {
    try {
        int nargs = lua_gettop(L);
        if (nargs < 2) {
            return luaL_error(L, "is_valid_mdb_path requires 2 arguments: path, ignore_case");
        }

        const char*      path_cstr = luaL_checkstring(L, 1);
        std::string_view path(path_cstr);

        // 严格检查 ignore_case 必须是布尔类型
        if (!lua_isboolean(L, 2)) {
            return luaL_error(L, "ignore_case must be a boolean");
        }
        bool ignore_case = lua_toboolean(L, 2) != 0;

        bool result = shm_tree::is_valid_mdb_path(path, ignore_case);
        lua_pushboolean(L, result ? 1 : 0);
        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "is_valid_mdb_path failed: %s", e.what());
    }
}

static int l_get_mdb_path(lua_State* L) {
    try {
        int nargs = lua_gettop(L);
        if (nargs < 3) {
            return luaL_error(L, "get_mdb_path requires 3 arguments: interface_name, filter_json, ignore_case");
        }

        const char*      interface_cstr = luaL_checkstring(L, 1);
        std::string_view interface_name(interface_cstr);

        // filter_json 是 JSON 字符串
        const char*      filter_json_cstr = luaL_checkstring(L, 2);
        std::string_view filter_json(filter_json_cstr);

        // 严格检查 ignore_case 必须是布尔类型
        if (!lua_isboolean(L, 3)) {
            return luaL_error(L, "ignore_case must be a boolean");
        }
        bool ignore_case = lua_toboolean(L, 3) != 0;

        variants result = shm_tree::get_mdb_path(interface_name, filter_json, ignore_case);
        mc::lua::variants_to_lua(L, result);
        return static_cast<int>(result.size());
    } catch (const mc::exception& e) {
        return luaL_error(L, "get_mdb_path failed: %s", e.what());
    }
}

static int l_get_mdb_interface_owners(lua_State* L) {
    try {
        const char*      interface_cstr = luaL_checkstring(L, 1);
        std::string_view interface_name(interface_cstr);

        variants result = shm_tree::get_mdb_interface_owners(interface_name);
        mc::lua::variant_to_lua(L, variant(result));
        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "get_mdb_interface_owners failed: %s", e.what());
    }
}

static int l_get_mdb_service_name(lua_State* L) {
    try {
        const char*      sender_cstr = luaL_checkstring(L, 1);
        std::string_view sender(sender_cstr);

        std::string_view result = shm_tree::get_mdb_service_name(sender);
        std::string      result_str(result);
        lua_pushlstring(L, result_str.data(), result_str.size());
        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "get_mdb_service_name failed: %s", e.what());
    }
}

static int l_get_mdb_sub_objects(lua_State* L) {
    try {
        const char*      path_cstr = luaL_checkstring(L, 1);
        std::string_view path(path_cstr);
        uint32_t         depth          = static_cast<uint32_t>(luaL_checkinteger(L, 2));
        variant          interfaces_var = mc::lua::lua_to_variant(L, 3);

        variants interfaces;
        if (interfaces_var.is_array()) {
            interfaces = interfaces_var.get_array();
        } else {
            return luaL_error(L, "interfaces must be an array");
        }

        auto result = shm_tree::get_mdb_sub_objects(path, depth, interfaces);
        if (result.has_value()) {
            mc::lua::variant_to_lua(L, variant(result.value()));
            return 1;
        } else {
            lua_pushnil(L);
            return 1;
        }
    } catch (const mc::exception& e) {
        return luaL_error(L, "get_mdb_sub_objects failed: %s", e.what());
    }
}

static int l_get_mdb_parent_objects(lua_State* L) {
    try {
        const char*      path_cstr = luaL_checkstring(L, 1);
        std::string_view path(path_cstr);
        variant          interfaces_var = mc::lua::lua_to_variant(L, 2);

        variants interfaces;
        if (interfaces_var.is_array()) {
            interfaces = interfaces_var.get_array();
        } else {
            return luaL_error(L, "interfaces must be an array");
        }

        auto result = shm_tree::get_mdb_parent_objects(path, interfaces);
        if (result.has_value()) {
            mc::lua::variant_to_lua(L, variant(result.value()));
            return 1;
        } else {
            lua_pushnil(L);
            return 1;
        }
    } catch (const mc::exception& e) {
        return luaL_error(L, "get_mdb_parent_objects failed: %s", e.what());
    }
}

static int l_get_mdb_service_names(lua_State* L) {
    try {
        variants result = shm_tree::get_mdb_service_names();
        mc::lua::variant_to_lua(L, variant(result));
        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "get_mdb_service_names failed: %s", e.what());
    }
}

static int l_get_mdb_classes(lua_State* L) {
    try {
        const char*      service_cstr = luaL_checkstring(L, 1);
        std::string_view service_name(service_cstr);

        variants result = shm_tree::get_mdb_classes(service_name);
        mc::lua::variant_to_lua(L, variant(result));
        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "get_mdb_classes failed: %s", e.what());
    }
}

static int l_get_mdb_object_list(lua_State* L) {
    try {
        const char*      class_cstr = luaL_checkstring(L, 1);
        std::string_view class_name(class_cstr);

        variants result = shm_tree::get_mdb_object_list(class_name);
        mc::lua::variant_to_lua(L, variant(result));
        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "get_mdb_object_list failed: %s", e.what());
    }
}

static int l_get_mdb_object_owner(lua_State* L) {
    try {
        const char*      object_cstr = luaL_checkstring(L, 1);
        std::string_view object_name(object_cstr);

        variants result = shm_tree::get_mdb_object_owner(object_name);
        mc::lua::variant_to_lua(L, variant(result));
        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "get_mdb_object_owner failed: %s", e.what());
    }
}

static int l_get_mdb_matched_objects(lua_State* L) {
    try {
        const char*      service_cstr = luaL_checkstring(L, 1);
        std::string_view service_name(service_cstr);
        const char*      pattern_cstr = luaL_checkstring(L, 2);
        // Lua 正则转义字符为'%s' c++为'\'
        std::string iface_pattern_str(pattern_cstr);
        std::replace(iface_pattern_str.begin(), iface_pattern_str.end(), '%', '\\');
        std::string_view iface_pattern(iface_pattern_str);

        variants result = shm_tree::get_mdb_matched_objects(service_name, iface_pattern);
        mc::lua::variant_to_lua(L, variant(result));
        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "get_mdb_matched_objects failed: %s", e.what());
    }
}

// SHM 属性访问函数

static int l_call_shm_get_property(lua_State* L) {
    try {
        const char*      service_cstr = luaL_checkstring(L, 1);
        std::string_view service_name(service_cstr);
        const char*      path_cstr = luaL_checkstring(L, 2);
        std::string_view path(path_cstr);
        const char*      interface_cstr = luaL_checkstring(L, 3);
        std::string_view interface_name(interface_cstr);
        const char*      property_cstr = luaL_checkstring(L, 4);
        std::string_view property_name(property_cstr);

        auto result = shm_tree::call_shm_get_property(service_name, path, interface_name, property_name);
        if (result.has_value()) {
            mc::lua::variant_to_lua(L, result.value());
            return 1;
        } else {
            lua_pushnil(L);
            return 1;
        }
    } catch (const mc::exception& e) {
        return luaL_error(L, "call_shm_get_property failed: %s", e.what());
    }
}

static int l_call_shm_get_all_properties(lua_State* L) {
    try {
        const char*      service_cstr = luaL_checkstring(L, 1);
        std::string_view service_name(service_cstr);
        const char*      path_cstr = luaL_checkstring(L, 2);
        std::string_view path(path_cstr);
        const char*      interface_cstr = luaL_checkstring(L, 3);
        std::string_view interface_name(interface_cstr);

        auto result = shm_tree::call_shm_get_all_properties(service_name, path, interface_name);
        if (result.has_value()) {
            mc::lua::variant_to_lua(L, variant(result.value()));
            return 1;
        } else {
            lua_pushnil(L);
            return 1;
        }
    } catch (const mc::exception& e) {
        return luaL_error(L, "call_shm_get_all_properties failed: %s", e.what());
    }
}

static int l_call_shm_get_properties_by_names(lua_State* L) {
    try {
        const char*      service_cstr = luaL_checkstring(L, 1);
        std::string_view service_name(service_cstr);
        const char*      path_cstr = luaL_checkstring(L, 2);
        std::string_view path(path_cstr);
        const char*      interface_cstr = luaL_checkstring(L, 3);
        std::string_view interface_name(interface_cstr);
        variant          prop_names_var = mc::lua::lua_to_variant(L, 4);

        variants prop_names;
        if (prop_names_var.is_array()) {
            prop_names = prop_names_var.get_array();
        } else {
            return luaL_error(L, "prop_names must be an array");
        }

        auto result = shm_tree::call_shm_get_properties_by_names(service_name, path, interface_name, prop_names);
        if (result.has_value()) {
            mc::lua::variants_to_lua(L, result.value());
            return static_cast<int>(result.value().size());
        } else {
            lua_pushnil(L);
            return 1;
        }
    } catch (const mc::exception& e) {
        return luaL_error(L, "call_shm_get_properties_by_names failed: %s", e.what());
    }
}

static const luaL_Reg methods[] = {
    // MDB 查询函数
    {"get_mdb_object", l_get_mdb_object},
    {"get_mdb_sub_paths", l_get_mdb_sub_paths},
    {"is_valid_mdb_path", l_is_valid_mdb_path},
    {"get_mdb_path", l_get_mdb_path},
    {"get_mdb_interface_owners", l_get_mdb_interface_owners},
    {"get_mdb_service_name", l_get_mdb_service_name},
    {"get_mdb_sub_objects", l_get_mdb_sub_objects},
    {"get_mdb_parent_objects", l_get_mdb_parent_objects},
    {"get_mdb_service_names", l_get_mdb_service_names},
    {"get_mdb_classes", l_get_mdb_classes},
    {"get_mdb_object_list", l_get_mdb_object_list},
    {"get_mdb_object_owner", l_get_mdb_object_owner},
    {"get_mdb_matched_objects", l_get_mdb_matched_objects},
    // SHM 属性访问函数
    {"call_shm_get_property", l_call_shm_get_property},
    {"call_shm_get_all_properties", l_call_shm_get_all_properties},
    {"call_shm_get_properties_by_names", l_call_shm_get_properties_by_names},
    {nullptr, nullptr}};

} // namespace lua
} // namespace dbus
} // namespace mc

extern "C" {

__attribute__((visibility("default"))) int luaopen_lshm_tree(lua_State* L) {
    using namespace mc::dbus::lua;
    luaL_checkversion(L);
    luaL_newlib(L, methods);
    return 1;
}

} // extern "C"
