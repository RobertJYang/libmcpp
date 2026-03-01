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

#ifndef MC_LUACLIB_ERROR_L_ERROR_H
#define MC_LUACLIB_ERROR_L_ERROR_H

#include <mc/error.h>
#include <mc/error_engine.h>
#include <mc/error_message_converter.h>
#include <mc/error_message_parser.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace mc::lua::error {

// ============================================================================
// 错误包装器
// ============================================================================

struct error_wrapper {
    mc::error_ptr err;

    error_wrapper() = default;
    explicit error_wrapper(mc::error_ptr e)
        : err(e) {
    }
    explicit error_wrapper(mc::error* e)
        : err(e) {
    }
};

// ============================================================================
// Metatable 名称
// ============================================================================

constexpr const char* ERROR_METATABLE           = "mc.error";
constexpr const char* ERROR_CONVERTER_METATABLE = "mc.error_converter";

// ============================================================================
// 错误相关函数
// ============================================================================

/**
 * @brief 检查并获取 error userdata
 */
inline error_wrapper* check_error(lua_State* L, int index = 1) {
    return static_cast<error_wrapper*>(luaL_checkudata(L, index, ERROR_METATABLE));
}

/**
 * @brief 创建 error userdata 并推入 Lua 栈
 */
inline int push_error(lua_State* L, const mc::error_ptr& err) {
    void* userdata = lua_newuserdata(L, sizeof(error_wrapper));
    new (userdata) error_wrapper(err);

    luaL_getmetatable(L, ERROR_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

/**
 * @brief 创建 error userdata 并推入 Lua 栈（移动语义）
 */
inline int push_error(lua_State* L, mc::error_ptr&& err) {
    void* userdata = lua_newuserdata(L, sizeof(error_wrapper));
    new (userdata) error_wrapper(std::move(err));

    luaL_getmetatable(L, ERROR_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

// ============================================================================
// Lua C 函数声明
// ============================================================================

/**
 * @brief 创建新的错误对象
 * Lua: mc.error.new(name, format, args...)
 */
int error_new(lua_State* L);

/**
 * @brief 根据 Lua table 创建错误对象
 * Lua: mc.error.new_message_error(table)
 *
 * @param table 包含错误信息的 table，支持以下字段：
 *   - name: 错误名称
 *   - message/format: 错误格式（message 优先）
 *   - params/args: 错误参数（params 优先）
 *   - 其他字段: 保存到 uservalue
 * @return error 对象
 */
int error_new_message_error(lua_State* L);

/**
 * @brief 错误对象的 tostring
 * Lua: err:tostring()
 */
int error_tostring(lua_State* L);

/**
 * @brief 获取错误参数
 * Lua: err:args()
 */
int error_args(lua_State* L);

/**
 * @brief 获取错误调用栈
 * Lua: err:traceback()
 */
int error_traceback(lua_State* L);

/**
 * @brief 参数后处理
 * Lua: err:post_process(param_struct)
 *
 * @param param_struct 可以是 dict 或 string 类型
 */
int error_post_process(lua_State* L);

/**
 * @brief 抛出异常
 * Lua: err:raise()
 */
int error_raise(lua_State* L);

/**
 * @brief 序列化为 JSON 字符串
 * Lua: err:encode()
 *
 * @return JSON 字符串
 */
int error_encode(lua_State* L);

/**
 * @brief __gc 元方法
 */
int error_gc(lua_State* L);

/**
 * @brief __index 元方法
 */
int error_index(lua_State* L);

/**
 * @brief __newindex 元方法
 * 支持 err.xxx = yyy 语法自动设置 error 参数
 */
int error_newindex(lua_State* L);

// ============================================================================
// Lua API 兼容性层函数声明
// ============================================================================

/**
 * @brief 创建新的错误对象，支持格式化参数
 * Lua: mc.error.new_error(name, fmt, params, ...)
 */
int compat_new_error(lua_State* L);

/**
 * @brief 抛出错误
 * Lua: mc.error.raise_error(name, message, params, ...)
 */
int compat_raise_error(lua_State* L);

/**
 * @brief 增强的日志打印，自动包含调用栈信息
 * Lua: mc.error.print_log(severity, format, ...)
 */
int compat_print_log(lua_State* L);

/**
 * @brief 打印错误调用栈跟踪
 * Lua: mc.error.print_trace(backtrace_level, err_data)
 */
int compat_print_trace(lua_State* L);

// ============================================================================
// 错误消息转换器相关函数声明
// ============================================================================

/**
 * @brief 加载错误定义文件
 * Lua: mc.error_converter.load_registries(base_path, custom_path)
 */
int converter_load_registries(lua_State* L);

/**
 * @brief 从字符串加载错误定义
 * Lua: mc.error_converter.load_registries_from_string(base_json, custom_json)
 */
int converter_load_registries_from_string(lua_State* L);

/**
 * @brief 转换错误为标准消息格式
 * Lua: mc.error_converter.convert(err)
 */
int converter_convert(lua_State* L);

/**
 * @brief 转换错误为 dict 格式
 * Lua: mc.error_converter.convert_to_dict(err)
 */
int converter_convert_to_dict(lua_State* L);

/**
 * @brief __gc 元方法
 */
int converter_gc(lua_State* L);

/**
 * @brief __index 元方法
 */
int converter_index(lua_State* L);

// ============================================================================
// 注册函数
// ============================================================================

/**
 * @brief 注册 error metatable
 */
void register_error_metatable(lua_State* L);

/**
 * @brief 注册 error_converter metatable
 */
void register_converter_metatable(lua_State* L);

} // namespace mc::lua::error

// ============================================================================
// 库打开函数（C 链接）
// ============================================================================

extern "C" {
/**
 * @brief 打开 error 库
 * Lua: require 'mc_error'
 */
__attribute__((visibility("default"))) int luaopen_mc_error(lua_State* L);
}

#endif // MC_LUACLIB_ERROR_L_ERROR_H
