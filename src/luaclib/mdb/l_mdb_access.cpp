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

#include "l_mdb_access.h"
#include "../dbus/l_dbus_connection.h"
#include <mc/mdb_service.h>

#include "mc/mdb/mdb_validator.h"
#include <cstring>
#include <mc/dbus/sd_bus.h>
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/runtime.h>
#include <mc/variant.h>

extern "C" {
#include <dbus/dbus.h>
}

namespace mc::mdb::lua {

// 辅助函数：将方法返回值转换为字典格式并推送到 Lua 栈
// 如果成功转换，返回推送到栈上的值数量（1）；否则返回 0
static int convert_method_results_to_lua(lua_State* L, const method_info* minfo,
                                         const mc::variants& results) {
    if (!minfo) {
        return 0;
    }

    // 收集所有输出参数（direction == "out"）
    std::vector<std::pair<std::string, std::string>> out_args; // (name, type)
    for (const auto& arg : minfo->args) {
        if (arg.direction == "out") {
            std::string param_name = arg.name.empty() ? "" : arg.name;
            out_args.emplace_back(param_name, arg.type);
        }
    }

    // 如果有输出参数，将返回值组织成字典
    if (out_args.empty()) {
        return 0;
    }

    mc::dict result_dict;

    // 处理返回值，根据输出参数类型进行转换
    size_t result_idx = 0;
    for (size_t i = 0; i < out_args.size(); ++i) {
        const auto& [param_name, param_type] = out_args[i];
        mc::variant result_value;

        // 检查参数类型是否是数组类型（如 ay, au, ai 等）
        bool is_array_type = !param_type.empty() && param_type[0] == 'a';

        // 特殊情况：如果只有一个输出参数且类型是数组，且 results 有多个元素
        // 说明数组被展开了，需要重新组合
        if (is_array_type && out_args.size() == 1 && results.size() > 1) {
            // 将所有元素组合成数组
            mc::variants arr;
            arr.reserve(results.size());
            for (size_t j = 0; j < results.size(); ++j) {
                arr.push_back(results[j]);
            }
            result_value = mc::variant(arr);
        } else if (result_idx < results.size()) {
            result_value = results[result_idx];

            // 如果参数类型是数组类型，但返回值不是数组，需要包装成数组
            if (is_array_type && !result_value.is_array() && !result_value.is_blob()) {
                // 将单个值包装成数组
                mc::variants arr = {result_value};
                result_value     = mc::variant(arr);
            }
            // 如果参数类型是数组类型，且返回值是 blob，转换为数组
            else if (is_array_type && result_value.is_blob()) {
                const auto&  blob_data = result_value.get_blob();
                mc::variants arr;
                arr.reserve(blob_data.data.size());
                for (uint8_t byte : blob_data.data) {
                    arr.emplace_back(static_cast<uint8_t>(byte));
                }
                result_value = mc::variant(arr);
            }

            result_idx++;
        } else {
            // 返回值不足，使用空值
            result_value = mc::variant();
        }

        if (!param_name.empty()) {
            // 有参数名，使用参数名作为键
            result_dict.insert(mc::variant(param_name), result_value);
        } else {
            // 没有参数名，使用索引作为键（如 "1", "2"）
            result_dict.insert(mc::variant(static_cast<int64_t>(i + 1)), result_value);
        }
    }

    // 将字典转换为 Lua table 并返回
    mc::lua::variant_to_lua(L, mc::variant(result_dict));
    return 1;
}

static int proxy_object_method_closure(lua_State* L) {
    auto* wrapper = static_cast<proxy_object_wrapper*>(
        lua_touserdata(L, lua_upvalueindex(1)));
    const char* method_name = lua_tostring(L, lua_upvalueindex(2));

    try {
        auto& obj   = wrapper->get();
        int   start = 1;

        // 如果第一个参数是 userdata，则从第二个参数开始读取
        if (lua_type(L, 1) == LUA_TUSERDATA) {
            start = 2;
        }

        int         arg_count = lua_gettop(L) - (start - 1);
        mc::variant args_variant;
        if (arg_count > 0) {
            mc::variants args = mc::lua::lua_to_variants(L, start, arg_count);
            args_variant      = mc::variant(args);
        }
        mc::variants results = obj.call_method(method_name, args_variant);

        // 获取方法信息，构建返回值的字典
        const method_info* minfo = obj.get_method_info(method_name);
        if (convert_method_results_to_lua(L, minfo, results) > 0) {
            return 1;
        }

        // 如果没有方法信息或输出参数，使用原来的逻辑（返回多个值）
        return mc::lua::variants_to_lua(L, results);
    } catch (const mc::exception& e) {
        return luaL_error(L, "call method failed: %s", e.what());
    } catch (const std::exception& e) {
        return luaL_error(L, "call method failed: %s", e.what());
    }
}

// pcall 闭包函数：安全调用方法，返回 (ok, ret) 格式
static int proxy_object_pcall_closure(lua_State* L) {
    auto* wrapper = static_cast<proxy_object_wrapper*>(
        lua_touserdata(L, lua_upvalueindex(1)));

    try {
        auto& obj = wrapper->get();

        // 支持两种调用方式：
        // 1. obj:pcall(method, arg1, arg2) - 冒号语法，索引1是对象，索引2是方法名
        // 2. obj.pcall(method, arg1, arg2) - 点号语法，索引1是方法名
        int method_name_index = 1;
        if (lua_gettop(L) >= 1 && lua_isuserdata(L, 1)) {
            // 如果索引1是userdata，说明使用了冒号语法，方法名在索引2
            method_name_index = 2;
        }

        // 获取方法名参数
        if (lua_gettop(L) < method_name_index || !lua_isstring(L, method_name_index)) {
            return luaL_error(L, "argument %d must be a string (method name)", method_name_index);
        }
        const char* method_name = lua_tostring(L, method_name_index);

        // 检查是否是方法
        if (!obj.has_method(method_name)) {
            lua_pushboolean(L, 0); // ok = false
            lua_pushstring(L, ("method '" + std::string(method_name) + "' does not exist").c_str());
            return 2;
        }

        // 从 Lua 栈读取参数（从方法名之后开始）
        int         arg_start = method_name_index + 1;
        int         arg_count = lua_gettop(L) - method_name_index; // 总元素数 - 方法名索引 = 方法名及之后的所有元素数
        mc::variant args_variant;
        if (arg_count > 0) {
            // arg_count 包含了方法名，所以实际参数数量需要减1
            mc::variants args = mc::lua::lua_to_variants(L, arg_start, arg_count - 1);
            args_variant     = mc::variant(args);
        }

        // 使用 pcall 调用方法
        std::optional<std::string> error;
        auto [success, results] = obj.call_method_pcall(method_name, args_variant, error);

        if (!success) {
            // 调用失败，返回 (false, error_message)
            lua_pushboolean(L, 0); // ok = false
            lua_pushstring(L, error.has_value() ? error.value().c_str() : "unknown error");
            return 2;
        }

        // 调用成功，返回 (true, result)
        lua_pushboolean(L, 1); // ok = true

        // 获取方法信息，构建返回值的字典
        const method_info* minfo = obj.get_method_info(method_name);
        if (convert_method_results_to_lua(L, minfo, results) > 0) {
            return 2; // 返回 (ok, result_dict)
        }

        // 如果没有方法信息或输出参数，使用原来的逻辑（返回多个值）
        // 但 pcall 只返回一个值，所以将多个值包装成数组
        if (results.size() == 1) {
            mc::lua::variant_to_lua(L, results[0]);
        } else if (results.size() > 1) {
            mc::lua::variant_to_lua(L, mc::variant(results));
        } else {
            lua_pushnil(L);
        }

        return 2; // 返回 (ok, result)
    } catch (const mc::exception& e) {
        lua_pushboolean(L, 0); // ok = false
        lua_pushstring(L, e.what());
        return 2;
    } catch (const std::exception& e) {
        lua_pushboolean(L, 0); // ok = false
        lua_pushstring(L, e.what());
        return 2;
    }
}

// 辅助函数：从 CONNECTION_METATABLE userdata 创建临时 sd_bus
// 注意：传入 connection 的拷贝（shared_ptr 共享底层 connection_impl）
static std::shared_ptr<mc::dbus::sd_bus> create_sd_bus_from_lua(lua_State* L, int index) {
    auto* wrapper = mc::dbus::lua::check_connection(L, index);
    if (!wrapper || !wrapper->conn_ptr) {
        luaL_error(L, "无效的 connection 对象");
        return nullptr;
    }


    mc::dbus::connection conn = wrapper->get();

    return std::make_shared<mc::dbus::sd_bus>(conn, true);
}

// is_volatile 闭包函数
static int proxy_object_is_volatile_closure(lua_State* L) {
    auto* wrapper = static_cast<proxy_object_wrapper*>(
        lua_touserdata(L, lua_upvalueindex(1)));

    try {
        auto& obj = wrapper->get();

        // 支持两种调用方式：
        // 1. obj:is_volatile(prop_name) - 冒号语法，索引1是对象，索引2是属性名
        // 2. obj.is_volatile(prop_name) - 点号语法，索引1是属性名
        int prop_name_index = 1;
        if (lua_gettop(L) >= 1 && lua_isuserdata(L, 1)) {
            // 如果索引1是userdata，说明使用了冒号语法，属性名在索引2
            prop_name_index = 2;
        }

        // 获取属性名参数
        const char* prop_name = luaL_checkstring(L, prop_name_index);

        // 调用 is_volatile
        bool is_vol = obj.is_volatile(prop_name);

        // 返回布尔值
        lua_pushboolean(L, is_vol ? 1 : 0);
        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "is_volatile failed: %s", e.what());
    } catch (const std::exception& e) {
        return luaL_error(L, "is_volatile failed: %s", e.what());
    }
}

// get_all 闭包函数
static int proxy_object_get_all_closure(lua_State* L) {
    auto* wrapper = static_cast<proxy_object_wrapper*>(
        lua_touserdata(L, lua_upvalueindex(1)));

    try {
        auto& obj = wrapper->get();

        // 获取所有属性
        mc::dict props = obj.get_all_properties();

        // 转换为 Lua table
        lua_createtable(L, 0, static_cast<int>(props.size()));
        for (const auto& entry : props) {
            mc::lua::variant_to_lua(L, entry.key);
            mc::lua::variant_to_lua(L, entry.value);
            lua_settable(L, -3);
        }

        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "get_all failed: %s", e.what());
    } catch (const std::exception& e) {
        return luaL_error(L, "get_all failed: %s", e.what());
    }
}

// get_properties 闭包函数
static int proxy_object_get_properties_closure(lua_State* L) {
    auto* wrapper = static_cast<proxy_object_wrapper*>(
        lua_touserdata(L, lua_upvalueindex(1)));

    try {
        auto& obj = wrapper->get();

        // 支持两种调用方式：
        // 1. obj:get_properties({...}) - 冒号语法，索引1是对象，索引2是table
        // 2. obj.get_properties({...}) - 点号语法，索引1是table
        int table_index = 1;
        if (lua_gettop(L) >= 1 && lua_isuserdata(L, 1)) {
            // 如果索引1是userdata，说明使用了冒号语法，table在索引2
            table_index = 2;
        }

        // 检查table是否存在
        if (lua_gettop(L) < table_index || !lua_istable(L, table_index)) {
            return luaL_error(L, "argument %d must be a table", table_index);
        }

        // 读取属性名列表
        mc::variants prop_names;
        lua_pushnil(L);
        while (lua_next(L, table_index) != 0) {
            if (lua_isstring(L, -1)) {
                size_t      len;
                const char* name = lua_tolstring(L, -1, &len);
                prop_names.emplace_back(std::string(name, len));
            }
            lua_pop(L, 1);
        }

        // 调用代理对象的 get_properties 方法
        auto [props_dict, errs_dict] = obj.get_properties(prop_names);

        // 返回两个值：属性数据字典和错误信息字典
        mc::lua::variant_to_lua(L, mc::variant(props_dict));
        mc::lua::variant_to_lua(L, mc::variant(errs_dict));

        return 2;
    } catch (const mc::exception& e) {
        return luaL_error(L, "get_properties failed: %s", e.what());
    } catch (const std::exception& e) {
        return luaL_error(L, "get_properties failed: %s", e.what());
    }
}

// __index 元方法：读属性
static int proxy_object_index(lua_State* L) {
    auto        wrapper = check_proxy_object(L, 1);
    const char* key     = luaL_checkstring(L, 2);

    try {
        auto& obj = wrapper->get();

        // 检查是否是 is_volatile 方法
        if (strcmp(key, "is_volatile") == 0) {
            lua_pushlightuserdata(L, wrapper);
            lua_pushcclosure(L, proxy_object_is_volatile_closure, 1);
            return 1;
        }

        // 检查是否是 get_all 方法
        if (strcmp(key, "get_all") == 0) {
            lua_pushlightuserdata(L, wrapper);
            lua_pushcclosure(L, proxy_object_get_all_closure, 1);
            return 1;
        }

        // 检查是否是 get_properties 方法
        if (strcmp(key, "get_properties") == 0) {
            lua_pushlightuserdata(L, wrapper);
            lua_pushcclosure(L, proxy_object_get_properties_closure, 1);
            return 1;
        }

        // 检查是否是 pcall 方法
        if (strcmp(key, "pcall") == 0) {
            lua_pushlightuserdata(L, wrapper);
            lua_pushcclosure(L, proxy_object_pcall_closure, 1);
            return 1;
        }

        // 检查是否是属性
        if (obj.has_property(key)) {
            mc::variant value = obj.get_property(key);
            mc::lua::variant_to_lua(L, value);
            return 1;
        }

        // 检查是否是方法
        if (obj.has_method(key)) {
            lua_pushlightuserdata(L, wrapper);
            lua_pushstring(L, key);
            lua_pushcclosure(L, proxy_object_method_closure, 2);
            return 1;
        }

        // 都不是
        lua_pushnil(L);
        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "index operation failed: %s", e.what());
    } catch (const std::exception& e) {
        return luaL_error(L, "index operation failed: %s", e.what());
    }
}

// __newindex 元方法：写属性
static int proxy_object_newindex(lua_State* L) {
    auto        wrapper = check_proxy_object(L, 1);
    const char* key     = luaL_checkstring(L, 2);

    try {
        auto& obj = wrapper->get();

        // 检查是否是属性
        if (!obj.has_property(key)) {
            return luaL_error(L, "property '%s' does not exist", key);
        }

        // 获取属性信息用于校验
        const property_info* prop_info = obj.get_property_info(key);
        if (!prop_info) {
            return luaL_error(L, "property '%s' info not found", key);
        }

        // 从 Lua 栈读取值并转换为 variant
        mc::variant value = mc::lua::lua_to_variant(L, 3);

        // 使用 mdb_validator 校验
        mdb_validator::check(key, prop_info->type, value);

        // 设置属性
        obj.set_property(key, value);

        return 0;
    } catch (const mc::exception& e) {
        return luaL_error(L, "set property failed: %s", e.what());
    } catch (const std::exception& e) {
        return luaL_error(L, "set property failed: %s", e.what());
    }
}

// __call 元方法：方法调用
static int proxy_object_call(lua_State* L) {
    auto        wrapper     = check_proxy_object(L, 1);
    const char* method_name = luaL_checkstring(L, 2);

    try {
        auto& obj = wrapper->get();

        // 检查是否是方法
        if (!obj.has_method(method_name)) {
            return luaL_error(L, "method '%s' does not exist", method_name);
        }

        // 获取方法信息
        const method_info* minfo = obj.get_method_info(method_name);
        if (!minfo) {
            return luaL_error(L, "method '%s' info not found", method_name);
        }

        // 从 Lua 栈读取参数（从索引 3 开始，因为 1 是对象，2 是方法名）
        int         arg_count = lua_gettop(L) - 2;
        mc::variant args_variant;

        if (arg_count > 0) {
            mc::variants args = mc::lua::lua_to_variants(L, 3, arg_count);
            args_variant      = mc::variant(args);
        }

        // 调用方法（proxy_object 内部会进行校验）
        mc::variants results = obj.call_method(method_name, args_variant);

        // 获取方法信息，构建返回值的字典
        if (convert_method_results_to_lua(L, minfo, results) > 0) {
            return 1;
        }

        // 如果没有方法信息或输出参数，使用原来的逻辑（返回多个值）
        return mc::lua::variants_to_lua(L, results);
    } catch (const mc::exception& e) {
        return luaL_error(L, "call method failed: %s", e.what());
    } catch (const std::exception& e) {
        return luaL_error(L, "call method failed: %s", e.what());
    }
}

// __gc 元方法
static int proxy_object_gc(lua_State* L) {
    auto wrapper = check_proxy_object(L, 1);
    wrapper->~proxy_object_wrapper();
    return 0;
}

// 注册 proxy_object metatable
void register_proxy_object_metatable(lua_State* L) {
    // 创建 metatable
    luaL_newmetatable(L, PROXY_OBJECT_METATABLE);

    // 设置 __index
    lua_pushcfunction(L, proxy_object_index);
    lua_setfield(L, -2, "__index");

    // 设置 __newindex
    lua_pushcfunction(L, proxy_object_newindex);
    lua_setfield(L, -2, "__newindex");

    // 设置 __call
    lua_pushcfunction(L, proxy_object_call);
    lua_setfield(L, -2, "__call");

    // 设置 __gc
    lua_pushcfunction(L, proxy_object_gc);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1); // 弹出 metatable
}

// mdb_access.get_object(bus, path, interface)
static int mdb_access_get_object(lua_State* L) {
    try {
        // 参数1: bus (从 lightuserdata 创建 sd_bus)
        auto bus = create_sd_bus_from_lua(L, 1);
        if (!bus) {
            return luaL_error(L, "argument 1 must be a DBusConnection lightuserdata");
        }

        // 参数2: path (string)
        const char* path = luaL_checkstring(L, 2);

        // 参数3: interface (string)
        const char* interface = luaL_checkstring(L, 3);

        auto&                    obj_mgr = mdb_access::instance();
        std::shared_ptr<proxy_object> obj = obj_mgr.get_object(bus, path, interface);

        // 将代理对象推入 Lua 栈
        return push_proxy_object(L, obj);
    } catch (const mc::exception& e) {
        return luaL_error(L, "get_object failed: %s", e.what());
    } catch (const std::exception& e) {
        return luaL_error(L, "get_object failed: %s", e.what());
    }
}

// mdb_access.get_object_by_short_call(bus, path, interface)
static int mdb_access_get_object_by_short_call(lua_State* L) {
    try {
        // 参数1: bus (从 lightuserdata 创建 sd_bus)
        auto bus = create_sd_bus_from_lua(L, 1);
        if (!bus) {
            return luaL_error(L, "argument 1 must be a DBusConnection lightuserdata");
        }

        // 参数2: path (string)
        const char* path = luaL_checkstring(L, 2);

        // 参数3: interface (string)
        const char* interface = luaL_checkstring(L, 3);

        // 调用 mdb_access::get_object_by_short_call，返回 shared_ptr 管理对象生命周期
        // 注意：get_object_by_short_call 不缓存对象，对象生命周期由返回的 shared_ptr 管理
        auto&                    obj_mgr = mdb_access::instance();
        std::shared_ptr<proxy_object> obj = obj_mgr.get_object_by_short_call(bus.get(), path, interface);

        // 将代理对象推入 Lua 栈
        return push_proxy_object(L, obj);
    } catch (const mc::exception& e) {
        return luaL_error(L, "get_object_by_short_call failed: %s", e.what());
    } catch (const std::exception& e) {
        return luaL_error(L, "get_object_by_short_call failed: %s", e.what());
    }
}

// mdb_access.get_object_with_service(bus, service, path, interface)
static int mdb_access_get_object_with_service(lua_State* L) {
    try {
        // 参数1: bus (从 lightuserdata 创建 sd_bus)
        auto bus = create_sd_bus_from_lua(L, 1);
        if (!bus) {
            return luaL_error(L, "argument 1 must be a DBusConnection lightuserdata");
        }

        // 参数2: service (string)
        const char* service = luaL_checkstring(L, 2);

        // 参数3: path (string)
        const char* path = luaL_checkstring(L, 3);

        // 参数4: interface (string)
        const char* interface = luaL_checkstring(L, 4);

        auto&                    obj_mgr = mdb_access::instance();
        std::shared_ptr<proxy_object> obj = obj_mgr.get_object_with_service(bus, service, path, interface);

        // 将代理对象推入 Lua 栈
        return push_proxy_object(L, obj);
    } catch (const mc::exception& e) {
        return luaL_error(L, "get_object_with_service failed: %s", e.what());
    } catch (const std::exception& e) {
        return luaL_error(L, "get_object_with_service failed: %s", e.what());
    }
}

// mdb_access.get_all(bus, path, interface)
static int mdb_access_get_all(lua_State* L) {
    try {
        // 参数1: bus (从 lightuserdata 创建 sd_bus)
        auto bus = create_sd_bus_from_lua(L, 1);
        if (!bus) {
            return luaL_error(L, "argument 1 must be a DBusConnection lightuserdata");
        }

        // 参数2: path (string)
        const char* path = luaL_checkstring(L, 2);

        // 参数3: interface (string)
        const char* interface = luaL_checkstring(L, 3);

        // 调用 mdb_access 获取代理对象
        auto&                    obj_mgr = mdb_access::instance();
        std::shared_ptr<proxy_object> obj = obj_mgr.get_object(bus, path, interface);

        // 获取所有属性
        mc::dict props = obj->get_all_properties();

        // 转换为 Lua table
        lua_createtable(L, 0, static_cast<int>(props.size()));
        for (const auto& entry : props) {
            mc::lua::variant_to_lua(L, entry.key);
            mc::lua::variant_to_lua(L, entry.value);
            lua_settable(L, -3);
        }

        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "get_all failed: %s", e.what());
    } catch (const std::exception& e) {
        return luaL_error(L, "get_all failed: %s", e.what());
    }
}

// mdb_access.get_properties(bus, path, interface, props)
static int mdb_access_get_properties(lua_State* L) {
    try {
        // 参数1: bus (从 lightuserdata 创建 sd_bus)
        auto bus = create_sd_bus_from_lua(L, 1);
        if (!bus) {
            return luaL_error(L, "argument 1 must be a DBusConnection lightuserdata");
        }

        // 参数2: path (string)
        const char* path = luaL_checkstring(L, 2);

        // 参数3: interface (string)
        const char* interface = luaL_checkstring(L, 3);

        // 参数4: props (table of strings)
        if (!lua_istable(L, 4)) {
            return luaL_error(L, "argument 4 must be a table");
        }

        // 调用 mdb_access 获取代理对象
        auto&                    obj_mgr = mdb_access::instance();
        std::shared_ptr<proxy_object> obj = obj_mgr.get_object(bus, path, interface);

        // 读取属性名列表
        mc::variants prop_names;
        lua_pushnil(L);
        while (lua_next(L, 4) != 0) {
            if (lua_isstring(L, -1)) {
                size_t      len;
                const char* name = lua_tolstring(L, -1, &len);
                prop_names.emplace_back(std::string(name, len));
            }
            lua_pop(L, 1);
        }

        // 调用代理对象的 get_properties 方法
        auto [props_dict, errs_dict] = obj->get_properties(prop_names);

        // 返回两个值：属性数据字典和错误信息字典
        mc::lua::variant_to_lua(L, mc::variant(props_dict));
        mc::lua::variant_to_lua(L, mc::variant(errs_dict));

        return 2;
    } catch (const mc::exception& e) {
        return luaL_error(L, "get_properties failed: %s", e.what());
    } catch (const std::exception& e) {
        return luaL_error(L, "get_properties failed: %s", e.what());
    }
}

// Objects 元表的 fold 方法实现
// 对应 Lua: function Objects:fold(f, initial_acc)
static int objects_fold(lua_State* L) {
    // self 是 obj_list table（索引1）
    luaL_checktype(L, 1, LUA_TTABLE);

    // f 是累积函数（索引2）
    luaL_checktype(L, 2, LUA_TFUNCTION);

    // initial_acc 是初始累积值（索引3，可选）
    // 如果没有提供或为 nil，默认为空表 {}
    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) {
        lua_pushvalue(L, 3);  // 复制 initial_acc
    } else {
        lua_newtable(L);  // 创建空表作为默认值
    }
    // acc_val 现在在栈顶，记录其位置
    int acc_val_index = lua_gettop(L);

    // 遍历 table
    lua_pushnil(L);  // 第一个 key
    while (lua_next(L, 1) != 0) {
        lua_pushvalue(L, 2);           // f
        lua_pushvalue(L, -2);          // obj (value)
        lua_pushvalue(L, acc_val_index);  // acc_val

        int call_result = lua_pcall(L, 2, 2, 0);
        if (call_result != LUA_OK) {
            // 调用出错，错误信息在栈顶
            // lua_error 会传播错误并清理栈
            return lua_error(L);
        }

        lua_replace(L, acc_val_index);

        // 检查 continue_flag
        bool should_continue = true;
        if (lua_isboolean(L, -1)) {
            should_continue = lua_toboolean(L, -1) != 0;
        } else if (!lua_isnil(L, -1)) {
            should_continue = true;
        }
        lua_pop(L, 1);  // 弹出 continue_flag

        lua_pop(L, 1);  // 弹出 value

        if (!should_continue) {
            // continue_flag == false，提前退出
            lua_pop(L, 1);  // 弹出 key
            break;
        }
    }

    // 清理栈，只保留 acc_val
    lua_settop(L, acc_val_index);
    return 1;
}

// __index 元方法：用于访问 fold 方法
static int objects_index(lua_State* L) {
    // 参数1: table (obj_list)
    // 参数2: key (例如 "fold")
    const char* key = luaL_checkstring(L, 2);
    
    // 如果访问的是 "fold"，返回 fold 函数
    if (strcmp(key, "fold") == 0) {
        lua_pushcfunction(L, objects_fold);
        return 1;
    }
    
    // 其他情况返回 nil（让 Lua 使用默认行为）
    lua_pushnil(L);
    return 1;
}

// 注册 Objects 元表
static void register_objects_metatable(lua_State* L) {
    // 创建 Objects 元表
    lua_newtable(L);  // Objects 元表

    // 设置 __index 元方法（用于访问 fold 方法）
    lua_pushcfunction(L, objects_index);
    lua_setfield(L, -2, "__index");

    // 注册 fold 方法（直接放在元表中，也可以通过 __index 访问）
    lua_pushcfunction(L, objects_fold);
    lua_setfield(L, -2, "fold");

    lua_setfield(L, LUA_REGISTRYINDEX, "lmdb.mdb_access.Objects");
}

// 获取 Objects 元表（如果不存在则创建）
static void get_objects_metatable(lua_State* L) {
    // 尝试从注册表中获取
    lua_getfield(L, LUA_REGISTRYINDEX, "lmdb.mdb_access.Objects");
    if (lua_isnil(L, -1)) {
        // 不存在，创建并注册
        lua_pop(L, 1);  // 弹出 nil
        register_objects_metatable(L);
    }
}

// mdb_access.get_sub_objects(bus, path, interface, depth)
static int mdb_access_get_sub_objects(lua_State* L) {
    try {
        // 参数1: bus (从 lightuserdata 创建 sd_bus)
        auto bus = create_sd_bus_from_lua(L, 1);
        if (!bus) {
            return luaL_error(L, "argument 1 must be a DBusConnection lightuserdata");
        }

        // 参数2: path (string)
        const char* path = luaL_checkstring(L, 2);

        // 参数3: interface (string)
        const char* interface = luaL_checkstring(L, 3);

        // 参数4: depth (integer, 可选，默认1)
        int32_t depth = 1;
        if (lua_gettop(L) >= 4 && !lua_isnil(L, 4)) {
            if (lua_isnumber(L, 4)) {
                depth = static_cast<int32_t>(lua_tointeger(L, 4));
            } else {
                return luaL_error(L, "argument 4 (depth) must be a number");
            }
        }

        // 调用 mdb_access C++ 方法获取子对象
        auto& obj_mgr     = mdb_access::instance();
        auto  sub_objects = obj_mgr.get_sub_objects(bus, path, interface, depth);

        // 创建 Lua table，以路径为键，对象为值
        lua_createtable(L, 0, static_cast<int>(sub_objects.size()));
        for (const auto& [sub_path, obj] : sub_objects) {
            // 将路径作为键
            lua_pushlstring(L, sub_path.data(), sub_path.size());
            // 将对象作为值
            push_proxy_object(L, obj);
            lua_settable(L, -3);
        }
        // 现在 obj_list table 在栈顶

        // 获取 Objects 元表并设置
        get_objects_metatable(L);  // Objects 元表在栈顶
        lua_setmetatable(L, -2);    // setmetatable(obj_list, Objects)

        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "get_sub_objects failed: %s", e.what());
    } catch (const std::exception& e) {
        return luaL_error(L, "get_sub_objects failed: %s", e.what());
    }
}

// 模块函数注册表
static const luaL_Reg mdb_access_funcs[] = {
    {"get_object", mdb_access_get_object},
    {"get_object_by_short_call", mdb_access_get_object_by_short_call},
    {"get_object_with_service", mdb_access_get_object_with_service},
    {"get_all", mdb_access_get_all},
    {"get_properties", mdb_access_get_properties},
    {"get_sub_objects", mdb_access_get_sub_objects},
    {nullptr, nullptr}};

// 注册 mdb_access 模块的所有函数到 Lua 栈顶的 table 中
void register_mdb_access_functions(lua_State* L) {
    // 注册 proxy_object metatable
    register_proxy_object_metatable(L);

    // 注册函数到栈顶 table
    luaL_setfuncs(L, mdb_access_funcs, 0);
}

} // namespace mc::mdb::lua
