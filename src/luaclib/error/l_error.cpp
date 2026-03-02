/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "l_error.h"
#include "../utils/variant_utils.h"
#include <mc/error.h>
#include <mc/error_engine.h>
#include <mc/error_message_converter.h>
#include <mc/json.h>
#include <mc/log.h>

namespace mc::lua::error {

/**
 * @brief 将 variant 数组转换为 dict
 *
 * 将 variants 数组转换为 dict，键从 1 开始（符合 Lua 数组索引习惯）
 *
 * @param arr variant 数组
 * @return 转换后的 dict
 */
static mc::dict array_to_dict(const mc::variants& arr)
{
    mc::dict result;
    for (size_t i = 0; i < arr.size(); ++i) {
        result.insert(i + 1, arr[i]); // Lua 数组索引从 1 开始
    }
    return result;
}

/**
 * @brief 从 Lua 栈中读取 dict
 *
 * 将 Lua 栈中指定索引的值转换为 mc::dict。
 * 支持两种类型的 Lua table：
 *   1. 键值对类型（key-value）→ 转换为 dict 的 object 类型
 *   2. 数组类型（array）→ 转换为 dict 的 integer 索引类型
 *
 * 该函数使用 mc::lua::lua_to_variant 进行转换，自动处理嵌套 table 结构。
 *
 * @param L Lua 状态机
 * @param index Lua 栈中的索引（支持正索引和负索引）
 * @return 转换后的 dict
 */
static mc::dict check_dict(lua_State* L, int index)
{
    // 使用 lua_to_variant 将 Lua 值转换为 variant
    // 该函数内部已处理嵌套 table 的递归转换
    mc::variant var = mc::lua::lua_to_variant(L, index);
    mc::dict    result;

    // 根据类型进行转换
    if (var.is_object()) {
        // 键值对类型 table，直接获取
        result = var.get_object();
    } else if (var.is_array()) {
        // 数组类型 table，转换为从 1 开始索引的 dict
        result = array_to_dict(var.get_array());
    }
    return result;
}

/**
 * @brief 将 dict 推入 Lua 栈
 */
static void push_dict(lua_State* L, const mc::dict& dict)
{
    lua_newtable(L);

    for (mc::dict::const_iterator it = dict.begin(); it != dict.end(); ++it) {
        const mc::dict_types::entry& entry = *it;
        const mc::variant&           key   = entry.key;
        const mc::variant&           value = entry.value;

        // 将 key 转换为字符串
        if (key.get_type() == mc::type_id::string_type) {
            lua_pushstring(L, key.as<std::string>().c_str());
        } else {
            lua_pushstring(L, mc::to_string(key).c_str());
        }

        // 根据 variant 类型推入对应的 Lua 值
        mc::type_id value_type = value.get_type();
        if (value_type == mc::type_id::bool_type) {
            lua_pushboolean(L, value.as<bool>());
        } else if (value_type == mc::type_id::int64_type || value_type == mc::type_id::int32_type ||
                   value_type == mc::type_id::int16_type || value_type == mc::type_id::int8_type) {
            lua_pushinteger(L, static_cast<lua_Integer>(value.as<int64_t>()));
        } else if (value_type == mc::type_id::double_type) {
            lua_pushnumber(L, value.as<double>());
        } else if (value_type == mc::type_id::string_type) {
            lua_pushstring(L, value.as<std::string>().c_str());
        } else {
            // 其他类型转换为字符串
            lua_pushstring(L, mc::to_string(value).c_str());
        }

        lua_settable(L, -3);
    }
}

/**
 * @brief 转换 format 字符串中的占位符
 *
 * 将 %s, %d, %1, %2 等占位符转换为 {n} 格式，以便 format_dict 处理
 * 转换规则：
 *   %1, %2, %3... → {0}, {1}, {2}...
 *   %s, %d, %f... → {0}, {1}, {2}...
 *
 * @param format 原始 format 字符串
 * @return 转换后的 format 字符串
 */
static std::string convert_format_placeholders(std::string_view format)
{
    std::string result;
    size_t      pos       = 0;
    size_t      arg_index = 0;

    while (pos < format.length()) {
        size_t percent_pos = format.find('%', pos);
        if (percent_pos == std::string::npos) {
            result += format.substr(pos);
            break;
        }

        result += format.substr(pos, percent_pos - pos);

        if (percent_pos + 1 < format.length()) {
            char spec = format[percent_pos + 1];
            if (spec == '%') {
                result += "%%";
                pos = percent_pos + 2;
            } else if (spec >= '1' && spec <= '9') {
                // %1, %2, %3... → {n-1}
                result += "{" + std::to_string(spec - '0' - 1) + "}";
                pos = percent_pos + 2;
            } else if (spec == 's' || spec == 'd' || spec == 'i' || spec == 'f' ||
                       spec == 'x' || spec == 'X' || spec == 'c' || spec == 'p' ||
                       spec == 'u' || spec == 'l' || spec == 'L') {
                // %s, %d, %f 等占位符 → {n}
                // 处理 %ld, %lld 等形式
                size_t skip = 2;
                if (percent_pos + 2 < format.length() &&
                    (format[percent_pos + 2] == 'd' ||
                     format[percent_pos + 2] == 'i' ||
                     format[percent_pos + 2] == 'u' ||
                     format[percent_pos + 2] == 'x' ||
                     format[percent_pos + 2] == 'X')) {
                    skip = 3;
                }
                result += "{" + std::to_string(arg_index) + "}";
                arg_index++;
                pos = percent_pos + skip;
            } else {
                // 未知占位符，保留原样
                result += '%';
                pos = percent_pos + 1;
            }
        } else {
            result += '%';
            pos = percent_pos + 1;
        }
    }

    return result;
}

/**
 * @brief 将 standard_error_message 推入 Lua 栈
 */
static void push_standard_message(lua_State* L, const mc::standard_error_message& msg)
{
    lua_newtable(L);

    lua_pushstring(L, "message_id");
    lua_pushstring(L, msg.message_id.c_str());
    lua_settable(L, -3);

    lua_pushstring(L, "message_name");
    lua_pushstring(L, msg.message_name.c_str());
    lua_settable(L, -3);

    lua_pushstring(L, "message");
    lua_pushstring(L, msg.message.c_str());
    lua_settable(L, -3);

    lua_pushstring(L, "severity");
    lua_pushstring(L, msg.severity.c_str());
    lua_settable(L, -3);

    lua_pushstring(L, "registry_prefix");
    lua_pushstring(L, msg.registry_prefix.c_str());
    lua_settable(L, -3);

    lua_pushstring(L, "registry_version");
    lua_pushstring(L, msg.registry_version.c_str());
    lua_settable(L, -3);

    lua_pushstring(L, "http_status_code");
    lua_pushinteger(L, msg.http_status_code);
    lua_settable(L, -3);

    if (!msg.resolution.empty()) {
        lua_pushstring(L, "resolution");
        lua_pushstring(L, msg.resolution.c_str());
        lua_settable(L, -3);
    }

    lua_pushstring(L, "message_args");
    push_dict(L, msg.message_args);
    lua_settable(L, -3);
}

// ============================================================================
// 错误相关函数实现
// ============================================================================

int error_new(lua_State* L)
{
    const char* name   = luaL_checkstring(L, 1);
    const char* format = luaL_optstring(L, 2, "");

    mc::dict args;
    if (lua_istable(L, 3)) {
        args = check_dict(L, 3);
    }

    auto err = mc::make_shared<mc::error>(name, format);
    err->set_args(args);

    return push_error(L, std::move(err));
}

int error_new_message_error(lua_State* L)
{
    // 检查参数是否是 table
    if (!lua_istable(L, 1)) {
        lua_pushstring(L, "new_message_error: expected table as first argument");
        lua_error(L);
        return 0;
    }

    // 从 table 中提取字段
    std::string name;
    std::string format;
    std::string registry_prefix;
    mc::dict    args;

    // 提取 name（复制到 std::string）
    lua_getfield(L, 1, "name");
    if (lua_isstring(L, -1)) {
        name = lua_tostring(L, -1);
    }
    lua_pop(L, 1);

    // 提取 format（复制到 std::string）
    lua_getfield(L, 1, "format");
    if (lua_isstring(L, -1)) {
        format = lua_tostring(L, -1);
        // 转换 format 中的占位符
        format = convert_format_placeholders(format);
    }
    lua_pop(L, 1);

    // 提取 registry_prefix
    lua_getfield(L, 1, "registry_prefix");
    if (lua_isstring(L, -1)) {
        registry_prefix = lua_tostring(L, -1);
    }
    lua_pop(L, 1);

    // 提取 params
    lua_getfield(L, 1, "params");
    if (lua_istable(L, -1)) {
        // params 在栈顶，获取其绝对索引
        int params_index = lua_gettop(L);
        args             = check_dict(L, params_index);
    }
    lua_pop(L, 1);

    // 使用 error_with_owner 创建 error 对象，持有 name 和 format 的所有权
    auto err = mc::make_shared<mc::error_with_owner>(std::move(name), std::move(format));
    err->set_args(args);

    // 设置 registry_prefix
    if (!registry_prefix.empty()) {
        err->set_registry_prefix(registry_prefix);
    }

    // 将 error 对象推入栈
    push_error(L, std::move(err));

    // 现在处理额外的字段（非 name, format, params, registry_prefix）
    // 简化实现：先收集额外字段到一个临时 table，然后设置 uservalue

    // 创建临时 table 存储额外字段
    lua_newtable(L); // 临时 table 在索引 2（原 table 在 1）
    int temp_table = lua_gettop(L);

    lua_pushnil(L); // 第一个 key
    while (lua_next(L, 1) != 0) {
        // key 在 -2，value 在 -1
        if (lua_isstring(L, -2)) {
            std::string key = lua_tostring(L, -2);

            // 只复制额外字段（排除 name, format, params, registry_prefix）
            if (key != "name" && key != "format" && key != "params" && key != "registry_prefix") {
                lua_pushvalue(L, -2); // key
                lua_pushvalue(L, -2); // value
                lua_rawset(L, temp_table);
            }
        }

        lua_pop(L, 1); // 移除 value
    }

    // 设置 uservalue（临时 table 在 temp_table，error userdata 在栈顶）
    lua_insert(L, temp_table); // 将临时 table 移到 error userdata 下方
    lua_setuservalue(L, -2);   // 设置 error userdata 的 uservalue

    return 1; // 返回 error 对象
}

int error_tostring(lua_State* L)
{
    auto wrapper = check_error(L);

    if (!wrapper->err) {
        lua_pushstring(L, "no error");
        return 1;
    }

    std::string str = wrapper->err->to_string();
    lua_pushstring(L, str.c_str());
    return 1;
}

int error_args(lua_State* L)
{
    auto wrapper = check_error(L);

    if (!wrapper->err) {
        lua_newtable(L);
        return 1;
    }

    push_dict(L, wrapper->err->get_args());
    return 1;
}

int error_traceback(lua_State* L)
{
    auto wrapper = check_error(L);

    if (!wrapper->err) {
        lua_pushnil(L);
        return 1;
    }

    // 调用 C++ traceback 方法生成调用栈
    wrapper->err->traceback();

    // 返回 traceback 字符串
    const std::string& tb = wrapper->err->get_traceback();
    lua_pushstring(L, tb.c_str());
    return 1;
}

int error_post_process(lua_State* L)
{
    auto wrapper = check_error(L);

    if (!wrapper->err) {
        lua_pushnil(L);
        return 1;
    }

    // 检查参数类型并分发到对应的实现
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) {
        if (lua_istable(L, 2)) {
            // dict 参数
            mc::dict param_struct = check_dict(L, 2);
            wrapper->err->post_process(param_struct);
        } else if (lua_isstring(L, 2)) {
            // string 参数
            const char* param_struct = lua_tostring(L, 2);
            wrapper->err->post_process(std::string(param_struct));
        }
    }

    // 返回 error 对象以支持链式调用
    lua_pushvalue(L, 1);
    return 1;
}

int error_raise(lua_State* L)
{
    auto wrapper = check_error(L);

    if (!wrapper->err) {
        lua_pushnil(L);
        return 1;
    }

    // 调用 C++ raise 方法抛出异常，需要捕获并转换为 Lua 错误
    try {
        wrapper->err->raise();
        // 永远不会执行到这里（raise() 标记为 [[noreturn]]）
        return 0;
    } catch (const mc::error_exception& e) {
        // 捕获 error_exception 并转换为 Lua 错误
        lua_pushstring(L, e.what());
        lua_error(L); // 这会抛出 Lua 错误，不会返回
        return 0;     // 永远不会执行到这里
    } catch (const std::exception& e) {
        // 捕获其他异常并转换为 Lua 错误
        lua_pushstring(L, e.what());
        lua_error(L); // 这会抛出 Lua 错误，不会返回
        return 0;     // 永远不会执行到这里
    }
}

int error_encode(lua_State* L)
{
    auto wrapper = check_error(L);

    if (!wrapper->err) {
        lua_pushnil(L);
        return 1;
    }

    try {
        // 调用 C++ encode 方法，使用默认选项
        std::string json_str = wrapper->err->encode();
        lua_pushstring(L, json_str.c_str());
        return 1;
    } catch (const std::exception& e) {
        lua_pushnil(L);
        lua_pushstring(L, e.what());
        return 2;
    }
}

int error_gc(lua_State* L)
{
    auto wrapper = check_error(L);
    wrapper->~error_wrapper();
    return 0;
}

int error_index(lua_State* L)
{
    // 获取 key
    const char* key = lua_tostring(L, 2);
    if (key == nullptr) {
        lua_pushnil(L);
        return 1;
    }

    auto wrapper = check_error(L, 1);

    if (!wrapper->err) {
        lua_pushnil(L);
        return 1;
    }

    // 优先处理属性访问（通过点号 . 访问）
    // 这样 err.name 返回字符串，而不是函数
    if (std::strcmp(key, "name") == 0) {
        std::string name = std::string(wrapper->err->get_name());
        lua_pushstring(L, name.c_str());
        return 1;
    }

    if (std::strcmp(key, "message") == 0) {
        std::string msg = wrapper->err->get_message();
        lua_pushstring(L, msg.c_str());
        return 1;
    }

    // params 属性（即 args 的别名）
    if (std::strcmp(key, "params") == 0) {
        push_dict(L, wrapper->err->get_args());
        return 1;
    }

    // args_with_index 属性
    if (std::strcmp(key, "args_with_index") == 0) {
        push_dict(L, wrapper->err->get_args_with_index());
        return 1;
    }

    // registry_prefix 属性
    if (std::strcmp(key, "registry_prefix") == 0) {
        std::string prefix = std::string(wrapper->err->get_registry_prefix());
        lua_pushstring(L, prefix.c_str());
        return 1;
    }

    // 检查 uservalue 中是否有该键（支持读取通过 __newindex 设置的值）
    if (lua_getuservalue(L, 1) != LUA_TNIL) {
        // uservalue 是一个 table
        lua_pushvalue(L, 2); // key
        lua_rawget(L, -2);
        if (!lua_isnil(L, -1)) {
            // 找到了，返回值
            return 1;
        }
        lua_pop(L, 1); // 移除 nil
    }
    lua_pop(L, 1); // 移除 uservalue

    // 不是 name, message, params, args_with_index 属性，也不是 uservalue 中的键
    // 从方法表中查找方法
    luaL_getmetatable(L, ERROR_METATABLE);
    lua_pushvalue(L, 2); // key
    lua_rawget(L, -2);

    // 找到方法就返回，否则返回 nil
    if (lua_isnil(L, -1)) {
        lua_pop(L, 2); // 清理 metatable 和 nil
        lua_pushnil(L);
        return 1;
    }

    return 1; // 返回找到的方法（如 err:tostring()）
}

int error_newindex(lua_State* L)
{
    // 获取 key
    const char* key = lua_tostring(L, 2);
    if (key == nullptr) {
        return 0;
    }

    auto wrapper = check_error(L, 1);

    if (!wrapper->err) {
        return 0;
    }

    // 处理特殊属性
    if (std::strcmp(key, "name") == 0) {
        // 允许修改 name
        if (lua_isstring(L, 3)) {
            wrapper->err->set_name(lua_tostring(L, 3));
        }
        return 0;
    }

    if (std::strcmp(key, "registry_prefix") == 0) {
        // 允许修改 registry_prefix
        if (lua_isstring(L, 3)) {
            wrapper->err->set_registry_prefix(lua_tostring(L, 3));
        }
        return 0;
    }

    if (std::strcmp(key, "message") == 0) {
        // message 是只读属性，不允许修改
        // 因为 message 是根据 name 和 args 动态生成的
        return 0;
    }

    // 获取或创建 uservalue table
    if (lua_getuservalue(L, 1) == LUA_TNIL) {
        // uservalue 不存在，创建一个新的 table
        lua_pop(L, 1); // 移除 nil
        lua_newtable(L);
        lua_pushvalue(L, -1);   // 复制 table
        lua_setuservalue(L, 1); // 设置为 uservalue
    }

    // 现在 uservalue table 在栈顶 (-1)
    // key 在索引 2，value 在索引 3
    lua_pushvalue(L, 2); // key
    lua_pushvalue(L, 3); // value
    lua_rawset(L, -3);   // uservalue[key] = value

    return 0;
}

void register_error_metatable(lua_State* L)
{
    luaL_newmetatable(L, ERROR_METATABLE);

    static const luaL_Reg methods[] = {{"tostring", error_tostring},
                                       {"args", error_args},
                                       {"traceback", error_traceback},
                                       {"raise", error_raise},
                                       {"post_process", error_post_process},
                                       {"encode", error_encode},
                                       {nullptr, nullptr}};

    luaL_setfuncs(L, methods, 0);

    lua_pushcfunction(L, error_index);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, error_newindex);
    lua_setfield(L, -2, "__newindex");

    lua_pushcfunction(L, error_tostring);
    lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, error_gc);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1);
}

// ============================================================================
// 错误消息转换器相关函数实现
// ============================================================================

int converter_load_registries(lua_State* L)
{
    const char* base_path   = luaL_checkstring(L, 1);
    const char* custom_path = luaL_checkstring(L, 2);

    try {
        mc::error_message_converter::get_instance().load_registries(base_path, custom_path);
        lua_pushboolean(L, 1);
        return 1;
    } catch (const std::exception& e) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, e.what());
        return 2;
    }
}

int converter_load_registries_from_string(lua_State* L)
{
    const char* base_json   = luaL_checkstring(L, 1);
    const char* custom_json = luaL_checkstring(L, 2);

    try {
        mc::error_message_converter::get_instance().load_registries_from_string(base_json,
                                                                                custom_json);
        lua_pushboolean(L, 1);
        return 1;
    } catch (const std::exception& e) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, e.what());
        return 2;
    }
}

int converter_convert(lua_State* L)
{
    auto wrapper = check_error(L, 1);

    if (!wrapper->err) {
        lua_pushnil(L);
        return 1;
    }

    mc::standard_error_message std_msg =
        mc::error_message_converter::get_instance().convert(*wrapper->err);

    push_standard_message(L, std_msg);
    return 1;
}

int converter_convert_to_dict(lua_State* L)
{
    auto wrapper = check_error(L, 1);

    if (!wrapper->err) {
        lua_pushnil(L);
        return 1;
    }

    mc::dict dict = mc::error_message_converter::get_instance().convert_to_dict(*wrapper->err);

    push_dict(L, dict);
    return 1;
}

int converter_gc(lua_State* L)
{
    (void)L;
    return 0;
}

int converter_index(lua_State* L)
{
    luaL_getmetatable(L, ERROR_CONVERTER_METATABLE);
    lua_pushvalue(L, 2); // key
    lua_rawget(L, -2);

    if (!lua_isnil(L, -1)) {
        return 1;
    }

    return 0;
}

void register_converter_metatable(lua_State* L)
{
    luaL_newmetatable(L, ERROR_CONVERTER_METATABLE);

    static const luaL_Reg methods[] = {{"load_registries", converter_load_registries},
                                       {"load_registries_from_string",
                                        converter_load_registries_from_string},
                                       {"convert", converter_convert},
                                       {"convert_to_dict", converter_convert_to_dict},
                                       {nullptr, nullptr}};

    luaL_setfuncs(L, methods, 0);

    lua_pushcfunction(L, converter_index);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, converter_gc);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1);
}

// ============================================================================
// Lua API 兼容性层（Migration from Lua to C++）
// ============================================================================

/**
 * @brief new_error(name, fmt, params, ...)
 * 创建新的错误对象，支持格式化参数
 *
 * 注意：此函数会将 format 中的 %s, %d, %1, %2 等占位符转换为 {n} 格式
 * 以便使用 format_dict 进行格式化
 */
int compat_new_error(lua_State* L)
{
    const char* name   = luaL_checkstring(L, 1);
    const char* format = luaL_optstring(L, 2, "");

    mc::dict args;
    bool     need_convert = false; // 是否需要转换 format

    if (lua_istable(L, 3)) {
        args = check_dict(L, 3);
    } else {
        // 如果 params 不是 table，收集可变参数，使用数字键 (0, 1, 2...) 以支持 Redfish %1, %2 格式
        need_convert  = true; // 需要转换 format
        int arg_index = 0;
        int n         = lua_gettop(L);
        for (int i = 3; i <= n; ++i) {
            if (lua_isboolean(L, i)) {
                args[arg_index++] = static_cast<bool>(lua_toboolean(L, i));
            } else if (lua_isinteger(L, i)) {
                args[arg_index++] = static_cast<int64_t>(lua_tointeger(L, i));
            } else if (lua_isnumber(L, i)) {
                args[arg_index++] = lua_tonumber(L, i);
            } else if (lua_isstring(L, i)) {
                args[arg_index++] = std::string(lua_tostring(L, i));
            }
        }
    }

    // 转换 format 中的占位符
    std::string converted_format = need_convert ? convert_format_placeholders(format) : std::string(format);

    auto err = mc::make_shared<mc::error>(name, converted_format);
    err->set_args(args);

    return push_error(L, std::move(err));
}

/**
 * @brief raise_error(name, message, params, ...)
 * 抛出错误，通过 new_error 创建错误对象然后抛出
 */
int compat_raise_error(lua_State* L)
{
    const char* name   = luaL_checkstring(L, 1);
    const char* format = luaL_optstring(L, 2, "");

    mc::dict args;
    if (lua_istable(L, 3)) {
        args = check_dict(L, 3);
    } else {
        // 收集可变参数，使用数字键 (0, 1, 2...) 以支持 Redfish %1, %2 格式
        int arg_index = 0;
        int n         = lua_gettop(L);
        for (int i = 3; i <= n; ++i) {
            if (lua_isboolean(L, i)) {
                args[arg_index++] = static_cast<bool>(lua_toboolean(L, i));
            } else if (lua_isinteger(L, i)) {
                args[arg_index++] = static_cast<int64_t>(lua_tointeger(L, i));
            } else if (lua_isnumber(L, i)) {
                args[arg_index++] = lua_tonumber(L, i);
            } else if (lua_isstring(L, i)) {
                args[arg_index++] = std::string(lua_tostring(L, i));
            }
        }
    }

    auto err = mc::make_shared<mc::error>(name, format);
    err->set_args(args);

    // 将错误对象推入栈然后抛出
    push_error(L, err);
    lua_error(L); // 这会引发 longjmp，不会返回

    return 0; // 永远不会执行到这里
}

/**
 * @brief print_log(severity, format, ...)
 * 增强的日志打印方法，自动包含调用栈信息（文件名和行号）
 *
 * 日志级别参照 DLOG_LEVEL_E 枚举：
 *   0 - DLOG_ERROR  (错误)
 *   1 - DLOG_WARN   (警告)
 *   2 - DLOG_NOTICE (注意)
 *   3 - DLOG_INFO   (信息)
 *   4 - DLOG_DEBUG  (调试)
 *
 * 使用方式: print_log(level, "message: %s %d", arg1, arg2)
 * 类似于 C 的 printf，可变参数按顺序填充 format 中的占位符
 */
int compat_print_log(lua_State* L)
{
    // 参数: severity, format, ...
    int severity = luaL_checkinteger(L, 1);

    // 校验日志级别范围：DLOG_ERROR(0) 到 DLOG_DEBUG(4) 之间
    constexpr int DLOG_ERROR = 0;
    constexpr int DLOG_DEBUG = 4;

    if (severity < DLOG_ERROR || severity > DLOG_DEBUG) {
        // 超出范围，直接返回
        return 0;
    }

    const char* format = luaL_optstring(L, 2, "");

    // 捕获调用栈信息：从第3层开始取到第5层
    // 层级说明：
    //   1 = 当前 compat_print_log 函数
    //   2 = 调用 compat_print_log 的代码
    //   3 = 实际调用点（我们想要的）
    std::vector<std::string> file_names;
    std::vector<int>         line_nums;

    for (int level = 3; level <= 5; ++level) {
        lua_Debug ar;
        if (lua_getstack(L, level, &ar)) {
            lua_getinfo(L, "Sl", &ar);

            // 提取文件名（只保留 xxx.lua 格式）
            std::string file = ar.short_src;
            size_t      pos  = file.find_last_of('/');
            if (pos != std::string::npos) {
                file = file.substr(pos + 1);
            }

            file_names.push_back(file);
            line_nums.push_back(ar.currentline);
        } else {
            // 如果没有更多层，用占位符填充
            file_names.push_back("?");
            line_nums.push_back(0);
        }
    }

    // 构建格式化字符串：file3:line3 > file2:line2 > file1:line1: format
    std::ostringstream location_format;
    location_format << file_names[2] << ":" << line_nums[2] << " > "
                    << file_names[1] << ":" << line_nums[1] << " > "
                    << file_names[0] << ":" << line_nums[0] << ": "
                    << format;

    std::string format_str = location_format.str();

    // 将 DLOG_LEVEL_E 映射到 mc::log::level
    // 0(DLOG_ERROR) -> error, 1(DLOG_WARN) -> warn, 2(DLOG_NOTICE) -> notice,
    // 3(DLOG_INFO) -> info, 4(DLOG_DEBUG) -> debug
    mc::log::level log_level;
    switch (severity) {
    case 0:
        log_level = mc::log::level::error;
        break;
    case 1:
        log_level = mc::log::level::warn;
        break;
    case 2:
        log_level = mc::log::level::notice;
        break;
    case 3:
        log_level = mc::log::level::info;
        break;
    case 4:
        log_level = mc::log::level::debug;
        break;
    default:
        log_level = mc::log::level::info;
        break;
    }

    // 处理可变参数并格式化消息
    std::string formatted_msg;
    int         n_args = lua_gettop(L);

    if (n_args > 2) {
        // 收集可变参数到 dict（使用数字索引 0, 1, 2, ...）
        mc::dict args;
        for (int i = 3; i <= n_args; ++i) {
            if (lua_isboolean(L, i)) {
                args[i - 3] = static_cast<bool>(lua_toboolean(L, i));
            } else if (lua_isinteger(L, i)) {
                args[i - 3] = static_cast<int64_t>(lua_tointeger(L, i));
            } else if (lua_isnumber(L, i)) {
                args[i - 3] = lua_tonumber(L, i);
            } else if (lua_isstring(L, i)) {
                args[i - 3] = std::string(lua_tostring(L, i));
            }
        }

        // 使用 convert_format_placeholders 转换占位符
        std::string format_template = convert_format_placeholders(format_str);

        // 使用 format_dict 格式化消息
        formatted_msg = mc::fmt::format_dict(format_template, args);
    } else {
        formatted_msg = format_str;
    }

    // 创建日志消息
    mc::log::message log_msg(log_level, formatted_msg);

    // 获取 logger 并记录日志
    mc::log::logger::get().log(log_msg);

    return 0;
}

/**
 * @brief print_trace(backtrace_level, err_data)
 * 打印错误调用栈跟踪信息
 */
int compat_print_trace(lua_State* L)
{
    // 参数: backtrace_level, err_data
    // backtrace_level 在这个实现中没有使用，但保留参数以兼容接口
    // int backtrace_level = luaL_optinteger(L, 1, 0);

    // 获取错误对象（第二个参数）
    if (lua_gettop(L) < 2) {
        // 没有提供错误对象
        return 0;
    }

    auto wrapper = check_error(L, 2);
    if (!wrapper || !wrapper->err) {
        return 0;
    }

    // 调用 error:traceback() 生成调用栈
    wrapper->err->traceback();

    // 获取 traceback 字符串
    const std::string& tb = wrapper->err->get_traceback();

    // 使用 notice 级别打印
    nlog("[${traceback}]", ("traceback", tb));

    return 0;
}

} // namespace mc::lua::error

// ============================================================================
// 库打开函数（C 链接，必须在外部定义）
// ============================================================================

extern "C" {
int luaopen_mc_error(lua_State* L)
{
    // 创建错误模块表
    lua_newtable(L);

    // 注册 error metatable
    mc::lua::error::register_error_metatable(L);

    // 注册 error_converter metatable
    mc::lua::error::register_converter_metatable(L);

    // 添加错误模块函数
    static const luaL_Reg functions[] = {
        // 错误创建函数
        {"new", mc::lua::error::error_new},
        {"new_message_error", mc::lua::error::error_new_message_error},

        // 错误消息转换器函数
        {"load_registries", mc::lua::error::converter_load_registries},
        {"load_registries_from_string", mc::lua::error::converter_load_registries_from_string},
        {"convert", mc::lua::error::converter_convert},
        {"convert_to_dict", mc::lua::error::converter_convert_to_dict},

        // Lua API 兼容性层函数
        {"new_error", mc::lua::error::compat_new_error},
        {"raise_error", mc::lua::error::compat_raise_error},
        {"print_log", mc::lua::error::compat_print_log},
        {"print_trace", mc::lua::error::compat_print_trace},

        {nullptr, nullptr}};

    luaL_setfuncs(L, functions, 0);

    // 返回模块表
    return 1;
}
}
