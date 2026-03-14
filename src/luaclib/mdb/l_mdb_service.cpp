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

#include "l_mdb_service.h"
#include "../dbus/l_sd_bus.h"
#include "../utils/variant_utils.h"

#include <mc/dbus/sd_bus.h>
#include <mc/log.h>
#include <mc/mdb_service.h>
#include <mc/runtime.h>

extern "C" {
#include <lualib.h>
}

namespace mc::mdb::lua {

// sd_bus wrapper 结构（来自 l_sd_bus.cpp）
struct sd_bus_wrapper {
    std::shared_ptr<mc::dbus::sd_bus> bus;
    std::shared_ptr<mc::io_context>   io_ctx;
};

// 辅助函数：从 sd_bus userdata 获取 sd_bus 裸指针
static mc::dbus::sd_bus* get_sd_bus_from_lua(lua_State* L, int index)
{
    auto* wrapper = static_cast<sd_bus_wrapper*>(luaL_checkudata(L, index, mc::dbus::lua::SD_BUS_METATABLE));
    if (!wrapper || !wrapper->bus) {
        luaL_error(L, "无效的 sd_bus 对象");
        return nullptr;
    }
    return wrapper->bus.get(); // 返回裸指针
}

// 辅助函数：从 Lua 栈获取数组参数（interfaces）
// 如果传入的是数组 table，提取其内容；如果是单个值，包装为数组
static mc::variants lua_to_interfaces_array(lua_State* L, int index)
{
    if (lua_isnil(L, index)) {
        return mc::variants(); // 空数组
    }

    // 先转换为 variant
    mc::variant v = mc::lua::lua_to_variant(L, index);
    // 如果已经是数组类型，直接返回其内容
    if (v.get_type() == mc::type_id::array_type) {
        return v.get_array();
    }

    // 如果是单个值，包装为数组
    mc::variants result;
    result.push_back(v);
    return result;
}

// mdb_service.get_object(conn, path, interfaces)
static int l_get_object(lua_State* L)
{
    try {
        auto*       bus        = get_sd_bus_from_lua(L, 1);
        const char* path       = luaL_checkstring(L, 2);
        auto        interfaces = lua_to_interfaces_array(L, 3);

        auto result = mc::mdb::service::get_object(bus, path, interfaces);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_object failed: %s", e.what());
    }
}

// mdb_service.get_sub_objects(conn, path, depth, interfaces)
static int l_get_sub_objects(lua_State* L)
{
    try {
        auto*       bus        = get_sd_bus_from_lua(L, 1);
        const char* path       = luaL_checkstring(L, 2);
        int32_t     depth      = luaL_checkinteger(L, 3);
        auto        interfaces = lua_to_interfaces_array(L, 4);

        auto result = mc::mdb::service::get_sub_objects(bus, path, depth, interfaces);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_sub_objects failed: %s", e.what());
    }
}

// mdb_service.get_sub_paths(conn, path, depth, interfaces)
static int l_get_sub_paths(lua_State* L)
{
    try {
        auto*       bus        = get_sd_bus_from_lua(L, 1);
        const char* path       = luaL_checkstring(L, 2);
        int32_t     depth      = luaL_checkinteger(L, 3);
        auto        interfaces = lua_to_interfaces_array(L, 4);

        auto result = mc::mdb::service::get_sub_paths(bus, path, depth, interfaces);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_sub_paths failed: %s", e.what());
    }
}

// mdb_service.get_parent_objects(conn, path, interfaces)
static int l_get_parent_objects(lua_State* L)
{
    try {
        auto*       bus        = get_sd_bus_from_lua(L, 1);
        const char* path       = luaL_checkstring(L, 2);
        auto        interfaces = lua_to_interfaces_array(L, 3);

        auto result = mc::mdb::service::get_parent_objects(bus, path, interfaces);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_parent_objects failed: %s", e.what());
    }
}

// mdb_service.get_service_name(conn, sender)
static int l_get_service_name(lua_State* L)
{
    try {
        auto* bus = get_sd_bus_from_lua(L, 1);

        // 严格检查参数类型
        if (!lua_isstring(L, 2)) {
            return luaL_error(L, "bad argument #2 to 'get_service_name' (string expected, got %s)",
                              lua_typename(L, lua_type(L, 2)));
        }
        const char* sender = lua_tostring(L, 2);

        auto result = mc::mdb::service::get_service_name(bus, sender);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_service_name failed: %s", e.what());
    }
}

// mdb_service.get_service_names(conn)
static int l_get_service_names(lua_State* L)
{
    try {
        auto* bus = get_sd_bus_from_lua(L, 1);

        auto result = mc::mdb::service::get_service_names(bus);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_service_names failed: %s", e.what());
    }
}

// mdb_service.get_path(conn, interface, filter, ignore_case, enable_cache)
static int l_get_path(lua_State* L)
{
    try {
        auto        bus          = get_sd_bus_from_lua(L, 1);
        const char* interface    = luaL_checkstring(L, 2);
        const char* filter       = luaL_checkstring(L, 3);
        bool        ignore_case  = lua_toboolean(L, 4);
        bool        enable_cache = lua_toboolean(L, 5);

        auto result = mc::mdb::service::get_path(bus, interface, filter, ignore_case, enable_cache);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_path failed: %s", e.what());
    }
}

// mdb_service.get_interface_owners(conn, interface)
static int l_get_interface_owners(lua_State* L)
{
    try {
        auto        bus       = get_sd_bus_from_lua(L, 1);
        const char* interface = luaL_checkstring(L, 2);

        auto result = mc::mdb::service::get_interface_owners(bus, interface);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_interface_owners failed: %s", e.what());
    }
}

// mdb_service.is_valid_path(conn, path, ignore_case)
static int l_is_valid_path(lua_State* L)
{
    try {
        auto        bus         = get_sd_bus_from_lua(L, 1);
        const char* path        = luaL_checkstring(L, 2);
        bool        ignore_case = lua_toboolean(L, 3);

        auto result = mc::mdb::service::is_valid_path(bus, path, ignore_case);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "is_valid_path failed: %s", e.what());
    }
}

// mdb_service.get_sub_paths_paging(conn, path, depth, interfaces, skip, top)
static int l_get_sub_paths_paging(lua_State* L)
{
    try {
        auto*       bus        = get_sd_bus_from_lua(L, 1);
        const char* path       = luaL_checkstring(L, 2);
        int32_t     depth      = luaL_checkinteger(L, 3);
        auto        interfaces = lua_to_interfaces_array(L, 4);
        int32_t     skip       = luaL_checkinteger(L, 5);
        int32_t     top        = luaL_checkinteger(L, 6);

        auto result = mc::mdb::service::get_sub_paths_paging(bus, path, depth, interfaces, skip, top);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_sub_paths_paging failed: %s", e.what());
    }
}

// mdb_service.get_classes(conn, service)
static int l_get_classes(lua_State* L)
{
    try {
        auto        bus     = get_sd_bus_from_lua(L, 1);
        const char* service = luaL_checkstring(L, 2);

        auto result = mc::mdb::service::get_classes(bus, service);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_classes failed: %s", e.what());
    }
}

// mdb_service.get_object_list(conn, class_name)
static int l_get_object_list(lua_State* L)
{
    try {
        auto*       bus        = get_sd_bus_from_lua(L, 1);
        const char* class_name = luaL_checkstring(L, 2);

        auto result = mc::mdb::service::get_object_list(bus, class_name);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_object_list failed: %s", e.what());
    }
}

// mdb_service.get_object_owner(conn, object_name)
static int l_get_object_owner(lua_State* L)
{
    try {
        auto        bus         = get_sd_bus_from_lua(L, 1);
        const char* object_name = luaL_checkstring(L, 2);

        auto result = mc::mdb::service::get_object_owner(bus, object_name);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_object_owner failed: %s", e.what());
    }
}

// mdb_service.get_matched_objects(conn, object_name, interface_pattern)
static int l_get_matched_objects(lua_State* L)
{
    try {
        auto        bus               = get_sd_bus_from_lua(L, 1);
        const char* object_name       = luaL_checkstring(L, 2);
        const char* interface_pattern = luaL_checkstring(L, 3);

        auto result = mc::mdb::service::get_matched_objects(bus, object_name, interface_pattern);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_matched_objects failed: %s", e.what());
    }
}

// mdb_service.get_traced_object(conn)
static int l_get_traced_object(lua_State* L)
{
    try {
        auto* bus = get_sd_bus_from_lua(L, 1);

        auto result = mc::mdb::service::get_traced_object(bus);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_traced_object failed: %s", e.what());
    }
}

// mdb_service.clear_cache()
static int l_clear_cache(lua_State* L)
{
    try {
        mc::mdb::service::clear_cache();
        return 0;
    } catch (const std::exception& e) {
        return luaL_error(L, "clear_cache failed: %s", e.what());
    }
}

// mdb_service.set_max_cache_size(max_size)
static int l_set_max_cache_size(lua_State* L)
{
    try {
        size_t max_size = luaL_checkinteger(L, 1);
        mc::mdb::service::set_max_cache_size(max_size);
        return 0;
    } catch (const std::exception& e) {
        return luaL_error(L, "set_max_cache_size failed: %s", e.what());
    }
}

// mdb_service.get_max_cache_size()
static int l_get_max_cache_size(lua_State* L)
{
    try {
        size_t max_size = mc::mdb::service::get_max_cache_size();
        lua_pushinteger(L, max_size);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_max_cache_size failed: %s", e.what());
    }
}

// mdb_service.get_cache_size()
static int l_get_cache_size(lua_State* L)
{
    try {
        size_t cache_size = mc::mdb::service::get_cache_size();
        lua_pushinteger(L, cache_size);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_cache_size failed: %s", e.what());
    }
}

// 注册 mdb_service 模块的所有函数
void register_mdb_service_functions(lua_State* L)
{
    static const luaL_Reg service_funcs[] = {{"get_object", l_get_object},
                                             {"get_sub_objects", l_get_sub_objects},
                                             {"get_sub_paths", l_get_sub_paths},
                                             {"get_parent_objects", l_get_parent_objects},
                                             {"get_service_name", l_get_service_name},
                                             {"get_service_names", l_get_service_names},
                                             {"get_path", l_get_path},
                                             {"get_interface_owners", l_get_interface_owners},
                                             {"is_valid_path", l_is_valid_path},
                                             {"get_sub_paths_paging", l_get_sub_paths_paging},
                                             {"get_classes", l_get_classes},
                                             {"get_object_list", l_get_object_list},
                                             {"get_object_owner", l_get_object_owner},
                                             {"get_matched_objects", l_get_matched_objects},
                                             {"get_traced_object", l_get_traced_object},
                                             {"clear_cache", l_clear_cache},
                                             {"set_max_cache_size", l_set_max_cache_size},
                                             {"get_max_cache_size", l_get_max_cache_size},
                                             {"get_cache_size", l_get_cache_size},
                                             {nullptr, nullptr}};

    luaL_setfuncs(L, service_funcs, 0);
}

} // namespace mc::mdb::lua
