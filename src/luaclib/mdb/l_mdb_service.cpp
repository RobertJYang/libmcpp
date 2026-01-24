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
#include "../dbus/l_dbus_connection.h"
#include "../utils/variant_utils.h"

#include <mc/dbus/connection.h>
#include <mc/dbus/sd_bus.h>
#include <mc/log.h>
#include <mc/mdb_service.h>
#include <mc/runtime.h>

extern "C" {
#include <lualib.h>
}

namespace mc::mdb::lua {

// 辅助函数：从 CONNECTION_METATABLE userdata 创建临时 sd_bus
// 根据 connection 的来源（阻塞式或非阻塞式）创建相应类型的 sd_bus
// 注意：传入 connection 的拷贝（shared_ptr 共享底层 connection_impl）
static std::unique_ptr<mc::dbus::sd_bus> create_sd_bus_from_lua(lua_State* L, int index) {
    auto* wrapper = mc::dbus::lua::check_connection(L, index);
    if (!wrapper || !wrapper->conn_ptr) {
        luaL_error(L, "无效的 connection 对象");
        return nullptr;
    }

    // 获取 connection 的引用，然后拷贝（不使用 move）
    // connection 内部使用 shared_ptr，拷贝会增加引用计数
    mc::dbus::connection conn = wrapper->get();
    
    // 根据 connection_wrapper 中的 is_blocking 标志创建相应类型的 sd_bus
    // 阻塞式 bus 用于同步调用，非阻塞式 bus 用于异步调用和信号订阅
    bool is_blocking = wrapper->is_blocking;
    
    // 创建 sd_bus，构造函数会 move 这个拷贝
    return std::make_unique<mc::dbus::sd_bus>(conn, is_blocking);
}

// 辅助函数：从 Lua 栈获取数组参数（interfaces）
// 如果传入的是数组 table，提取其内容；如果是单个值，包装为数组
static mc::variants lua_to_interfaces_array(lua_State* L, int index) {
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
static int l_get_object(lua_State* L) {
    try {
        auto        bus        = create_sd_bus_from_lua(L, 1);
        const char* path       = luaL_checkstring(L, 2);
        auto        interfaces = lua_to_interfaces_array(L, 3);

        auto result = mc::mdb::service::get_object(bus.get(), path, interfaces);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_object failed: %s", e.what());
    }
}

// mdb_service.get_sub_objects(conn, path, depth, interfaces)
static int l_get_sub_objects(lua_State* L) {
    try {
        auto        bus        = create_sd_bus_from_lua(L, 1);
        const char* path       = luaL_checkstring(L, 2);
        int32_t     depth      = luaL_checkinteger(L, 3);
        auto        interfaces = lua_to_interfaces_array(L, 4);

        auto result = mc::mdb::service::get_sub_objects(bus.get(), path, depth, interfaces);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_sub_objects failed: %s", e.what());
    }
}

// mdb_service.get_sub_paths(conn, path, depth, interfaces)
static int l_get_sub_paths(lua_State* L) {
    try {
        auto        bus        = create_sd_bus_from_lua(L, 1);
        const char* path       = luaL_checkstring(L, 2);
        int32_t     depth      = luaL_checkinteger(L, 3);
        auto        interfaces = lua_to_interfaces_array(L, 4);

        auto result = mc::mdb::service::get_sub_paths(bus.get(), path, depth, interfaces);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_sub_paths failed: %s", e.what());
    }
}

// mdb_service.get_parent_objects(conn, path, interfaces)
static int l_get_parent_objects(lua_State* L) {
    try {
        auto        bus        = create_sd_bus_from_lua(L, 1);
        const char* path       = luaL_checkstring(L, 2);
        auto        interfaces = lua_to_interfaces_array(L, 3);

        auto result = mc::mdb::service::get_parent_objects(bus.get(), path, interfaces);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_parent_objects failed: %s", e.what());
    }
}

// mdb_service.get_service_name(conn, sender)
static int l_get_service_name(lua_State* L) {
    try {
        auto        bus    = create_sd_bus_from_lua(L, 1);
        const char* sender = luaL_checkstring(L, 2);

        auto result = mc::mdb::service::get_service_name(bus.get(), sender);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_service_name failed: %s", e.what());
    }
}

// mdb_service.get_service_names(conn)
static int l_get_service_names(lua_State* L) {
    try {
        auto bus = create_sd_bus_from_lua(L, 1);

        auto result = mc::mdb::service::get_service_names(bus.get());
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_service_names failed: %s", e.what());
    }
}

// mdb_service.get_path(conn, interface, filter, ignore_case, enable_cache)
static int l_get_path(lua_State* L) {
    try {
        auto        bus          = create_sd_bus_from_lua(L, 1);
        const char* interface    = luaL_checkstring(L, 2);
        const char* filter       = luaL_checkstring(L, 3);
        bool        ignore_case  = lua_toboolean(L, 4);
        bool        enable_cache = lua_toboolean(L, 5);

        auto result =
            mc::mdb::service::get_path(bus.get(), interface, filter, ignore_case, enable_cache);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_path failed: %s", e.what());
    }
}

// mdb_service.get_interface_owners(conn, interface)
static int l_get_interface_owners(lua_State* L) {
    try {
        auto        bus       = create_sd_bus_from_lua(L, 1);
        const char* interface = luaL_checkstring(L, 2);

        auto result = mc::mdb::service::get_interface_owners(bus.get(), interface);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_interface_owners failed: %s", e.what());
    }
}

// mdb_service.is_valid_path(conn, path, ignore_case)
static int l_is_valid_path(lua_State* L) {
    try {
        auto        bus         = create_sd_bus_from_lua(L, 1);
        const char* path        = luaL_checkstring(L, 2);
        bool        ignore_case = lua_toboolean(L, 3);

        auto result = mc::mdb::service::is_valid_path(bus.get(), path, ignore_case);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "is_valid_path failed: %s", e.what());
    }
}

// mdb_service.get_sub_paths_paging(conn, path, depth, interfaces, skip, top)
static int l_get_sub_paths_paging(lua_State* L) {
    try {
        auto        bus        = create_sd_bus_from_lua(L, 1);
        const char* path       = luaL_checkstring(L, 2);
        int32_t     depth      = luaL_checkinteger(L, 3);
        auto        interfaces = lua_to_interfaces_array(L, 4);
        int32_t     skip       = luaL_checkinteger(L, 5);
        int32_t     top        = luaL_checkinteger(L, 6);

        auto result =
            mc::mdb::service::get_sub_paths_paging(bus.get(), path, depth, interfaces, skip, top);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_sub_paths_paging failed: %s", e.what());
    }
}

// mdb_service.get_classes(conn, service)
static int l_get_classes(lua_State* L) {
    try {
        auto        bus     = create_sd_bus_from_lua(L, 1);
        const char* service = luaL_checkstring(L, 2);

        auto result = mc::mdb::service::get_classes(bus.get(), service);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_classes failed: %s", e.what());
    }
}

// mdb_service.get_object_list(conn, class_name)
static int l_get_object_list(lua_State* L) {
    try {
        auto        bus        = create_sd_bus_from_lua(L, 1);
        const char* class_name = luaL_checkstring(L, 2);

        auto result = mc::mdb::service::get_object_list(bus.get(), class_name);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_object_list failed: %s", e.what());
    }
}

// mdb_service.get_object_owner(conn, object_name)
static int l_get_object_owner(lua_State* L) {
    try {
        auto        bus         = create_sd_bus_from_lua(L, 1);
        const char* object_name = luaL_checkstring(L, 2);

        auto result = mc::mdb::service::get_object_owner(bus.get(), object_name);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_object_owner failed: %s", e.what());
    }
}

// mdb_service.get_matched_objects(conn, object_name, interface_pattern)
static int l_get_matched_objects(lua_State* L) {
    try {
        auto        bus               = create_sd_bus_from_lua(L, 1);
        const char* object_name       = luaL_checkstring(L, 2);
        const char* interface_pattern = luaL_checkstring(L, 3);

        auto result =
            mc::mdb::service::get_matched_objects(bus.get(), object_name, interface_pattern);
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_matched_objects failed: %s", e.what());
    }
}

// mdb_service.get_traced_object(conn)
static int l_get_traced_object(lua_State* L) {
    try {
        auto bus = create_sd_bus_from_lua(L, 1);

        auto result = mc::mdb::service::get_traced_object(bus.get());
        mc::lua::variant_to_lua(L, result);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_traced_object failed: %s", e.what());
    }
}

// mdb_service.clear_cache()
static int l_clear_cache(lua_State* L) {
    try {
        mc::mdb::service::clear_cache();
        return 0;
    } catch (const std::exception& e) {
        return luaL_error(L, "clear_cache failed: %s", e.what());
    }
}

// mdb_service.set_max_cache_size(max_size)
static int l_set_max_cache_size(lua_State* L) {
    try {
        size_t max_size = luaL_checkinteger(L, 1);
        mc::mdb::service::set_max_cache_size(max_size);
        return 0;
    } catch (const std::exception& e) {
        return luaL_error(L, "set_max_cache_size failed: %s", e.what());
    }
}

// mdb_service.get_max_cache_size()
static int l_get_max_cache_size(lua_State* L) {
    try {
        size_t max_size = mc::mdb::service::get_max_cache_size();
        lua_pushinteger(L, max_size);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_max_cache_size failed: %s", e.what());
    }
}

// mdb_service.get_cache_size()
static int l_get_cache_size(lua_State* L) {
    try {
        size_t cache_size = mc::mdb::service::get_cache_size();
        lua_pushinteger(L, cache_size);
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "get_cache_size failed: %s", e.what());
    }
}

// 注册 mdb_service 模块的所有函数
void register_mdb_service_functions(lua_State* L) {
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
