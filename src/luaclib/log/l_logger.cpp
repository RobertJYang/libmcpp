/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <mc/fmt/format_dict.h>
#include <mc/log/log_level.h>
#include <mc/log/log_manager.h>
#include <mc/log/log_message.h>
#include <mc/log/logger.h>

#include <lua.hpp>

#include <string>

namespace mc {
namespace log {

// logger 对象的 lua 元表名称
static const char* LOGGER_METATABLE = "mc.log.logger";

namespace {
// 从 Lua 调用栈中提取 file/line/function，填充日志上下文
context get_lua_call_context(lua_State* L, int stack_level) {
    lua_Debug ar;
    if (lua_getstack(L, stack_level, &ar) == 0) {
        return context("", "", 0);
    }
    if (lua_getinfo(L, "Sln", &ar) == 0) {
        return context("", "", 0);
    }

    std::string file;
    if (ar.source != nullptr) {
        // source 可能以 '@' 开头表示文件路径；否则是字符串 chunk
        file = (ar.source[0] == '@') ? (ar.source + 1) : ar.source;
    }

    std::string func = (ar.name != nullptr) ? ar.name : "";
    uint32_t    line = (ar.currentline > 0) ? static_cast<uint32_t>(ar.currentline) : 0;
    return context(std::move(file), std::move(func), line);
}
} // namespace

// 注册日志级别常量到当前 table（此时栈顶应为目标 table）
static void register_log_levels(lua_State* L) {
    lua_pushinteger(L, static_cast<int>(level::all));
    lua_setfield(L, -2, "ALL");

    lua_pushinteger(L, static_cast<int>(level::trace));
    lua_setfield(L, -2, "TRACE");

    lua_pushinteger(L, static_cast<int>(level::debug));
    lua_setfield(L, -2, "DEBUG");

    lua_pushinteger(L, static_cast<int>(level::info));
    lua_setfield(L, -2, "INFO");

    lua_pushinteger(L, static_cast<int>(level::notice));
    lua_setfield(L, -2, "NOTICE");

    lua_pushinteger(L, static_cast<int>(level::warn));
    lua_setfield(L, -2, "WARN");

    lua_pushinteger(L, static_cast<int>(level::error));
    lua_setfield(L, -2, "ERROR");

    lua_pushinteger(L, static_cast<int>(level::fatal));
    lua_setfield(L, -2, "FATAL");

    lua_pushinteger(L, static_cast<int>(level::off));
    lua_setfield(L, -2, "OFF");

    // 运行日志级别常量（RLOG_* 为 running 接口专用别名，不含 trace、fatal）
    lua_pushinteger(L, static_cast<int>(level::debug));
    lua_setfield(L, -2, "RLOG_DEBUG");
    lua_pushinteger(L, static_cast<int>(level::info));
    lua_setfield(L, -2, "RLOG_INFO");
    lua_pushinteger(L, static_cast<int>(level::notice));
    lua_setfield(L, -2, "RLOG_NOTICE");
    lua_pushinteger(L, static_cast<int>(level::warn));
    lua_setfield(L, -2, "RLOG_WARN");
    lua_pushinteger(L, static_cast<int>(level::error));
    lua_setfield(L, -2, "RLOG_ERROR");

    // 维护日志级别常量（MLOG_* 为 maintenance 接口专用别名，不含 trace、fatal）
    lua_pushinteger(L, static_cast<int>(level::debug));
    lua_setfield(L, -2, "MLOG_DEBUG");
    lua_pushinteger(L, static_cast<int>(level::info));
    lua_setfield(L, -2, "MLOG_INFO");
    lua_pushinteger(L, static_cast<int>(level::notice));
    lua_setfield(L, -2, "MLOG_NOTICE");
    lua_pushinteger(L, static_cast<int>(level::warn));
    lua_setfield(L, -2, "MLOG_WARN");
    lua_pushinteger(L, static_cast<int>(level::error));
    lua_setfield(L, -2, "MLOG_ERROR");
}

// 从 lua 栈获取 logger 对象
static logger* lua_check_logger(lua_State* L, int index) {
    void* ud = luaL_checkudata(L, index, LOGGER_METATABLE);
    luaL_argcheck(L, ud != nullptr, index, "logger expected");
    return static_cast<logger*>(ud);
}

// 创建 logger 对象并推入 lua 栈
static void lua_push_logger(lua_State* L, const logger& log) {
    void* ud = lua_newuserdata(L, sizeof(logger));
    new (ud) logger(log);
    luaL_getmetatable(L, LOGGER_METATABLE);
    lua_setmetatable(L, -2);
}

// logger:get_name()
static int lua_logger_get_name(lua_State* L) {
    logger*            log  = lua_check_logger(L, 1);
    const std::string& name = log->get_name();
    lua_pushstring(L, name.c_str());
    return 1;
}

// logger:set_name(name)
static int lua_logger_set_name(lua_State* L) {
    logger*     log  = lua_check_logger(L, 1);
    const char* name = luaL_checkstring(L, 2);
    log->set_name(name);
    return 0;
}

// logger:get_level()
static int lua_logger_get_level(lua_State* L) {
    logger* log = lua_check_logger(L, 1);
    level   lvl = log->get_level();
    lua_pushinteger(L, static_cast<int>(lvl));
    return 1;
}

// logger:set_level(level)
static int lua_logger_set_level(lua_State* L) {
    logger* log = lua_check_logger(L, 1);
    int     lvl = static_cast<int>(luaL_checkinteger(L, 2));
    log->set_level(static_cast<level>(lvl));
    lua_pushvalue(L, 1); // 返回 self，支持链式调用
    return 1;
}

// logger:is_enabled(level)
static int lua_logger_is_enabled(lua_State* L) {
    logger* log     = lua_check_logger(L, 1);
    int     lvl     = static_cast<int>(luaL_checkinteger(L, 2));
    bool    enabled = log->is_enabled(static_cast<level>(lvl));
    lua_pushboolean(L, enabled);
    return 1;
}

// logger:system(system_id)
// 如果传入 system_id，则设置 system_id；如果未传入参数，则重置为未设置状态（-1）
static int lua_logger_system(lua_State* L) {
    logger* log = lua_check_logger(L, 1);
    int     system_id;
    // 检查是否传入了 system_id 参数
    if (lua_isnoneornil(L, 2)) {
        // 未传入参数，重置为未设置状态
        system_id = -1;
    } else {
        // 传入了参数，使用传入的值
        system_id = static_cast<int>(luaL_checkinteger(L, 2));
    }
    // 注意：logger 的拷贝会共享 impl（shared_ptr），直接修改会污染全局状态。
    // 这里返回一个克隆 logger，仅用于本次链式调用，避免影响原 logger。
    logger new_log = log->clone();
    new_log.system(system_id);
    lua_push_logger(L, new_log);
    return 1;
}

// logger:period(period_s)
static int lua_logger_period(lua_State* L) {
    logger* log      = lua_check_logger(L, 1);
    int     period_s = static_cast<int>(luaL_checkinteger(L, 2));

    // 检查参数有效性：period_s 不能为负数
    if (period_s < 0) {
        luaL_error(L, "Invalid log period value");
    }

    // 返回一个克隆 logger，仅用于本次链式调用，避免影响原 logger 的周期配置
    logger new_log = log->clone();
    new_log.period(period_s);
    lua_push_logger(L, new_log);
    return 1;
}

// logger:condition(cond)
// 设置 condition，控制是否输出日志；cond 为 false 时不打印，为 true 时打印
// 返回克隆 logger 用于链式调用，仿照 system/period
static int lua_logger_condition(lua_State* L) {
    logger* log  = lua_check_logger(L, 1);
    bool    cond = lua_toboolean(L, 2) != 0;

    logger new_log = log->clone();
    new_log.condition(cond);
    lua_push_logger(L, new_log);
    return 1;
}

// 通用的日志记录函数，根据级别记录日志
// 接口：log:warn(module_name, message, ...) 或 log:warn(message, ...)
// 如果第一个参数和第二个参数都是字符串，则第一个参数作为 module_name（可选）
// 否则，第一个参数作为消息内容
// limit_rate：true 限流，false 不限流（_easy）
static int lua_logger_log_impl(lua_State* L, level lvl, bool limit_rate) {
    logger* log   = lua_check_logger(L, 1);
    int     nargs = lua_gettop(L);

    // 构建参数字典
    mc::dict    args;
    const char* msg       = nullptr;
    int         msg_index = 2; // 默认消息内容在索引2

    // 检查第一个参数（索引2）是否是 module_name
    // 如果参数数量 >= 3 且第一个和第二个参数都是字符串，则第一个参数作为 module_name
    if (nargs >= 3 && lua_type(L, 2) == LUA_TSTRING && lua_type(L, 3) == LUA_TSTRING) {
        const char* module_name = lua_tostring(L, 2);
        args["module_name"]     = std::string(module_name);
        msg_index               = 3; // 消息内容在索引3
    }

    // 获取消息内容
    if (msg_index > nargs) {
        return luaL_error(L, "missing message argument");
    }
    msg = luaL_checkstring(L, msg_index);

    // 从消息内容之后开始解析键值对参数
    int start_index = msg_index + 1;
    for (int i = start_index; i <= nargs; i++) {
        // 支持键值对参数，格式：key, value, key, value, ...
        if (i + 1 <= nargs && lua_type(L, i) == LUA_TSTRING) {
            const char* key = lua_tostring(L, i);
            if (lua_isnumber(L, i + 1)) {
                args[key] = lua_tonumber(L, i + 1);
            } else if (lua_isstring(L, i + 1)) {
                args[key] = std::string(lua_tostring(L, i + 1));
            } else if (lua_isboolean(L, i + 1)) {
                args[key] = lua_toboolean(L, i + 1) != 0;
            }
            i++; // 跳过 value
        }
    }

    // 创建日志消息：从 Lua 调用栈提取 file/line/function
    // 说明：level=1 通常是 Lua 调用 log:warn(...) 的那一行
    context ctx = get_lua_call_context(L, 1);
    message log_msg(lvl, ctx, msg, args);
    log_msg.set_limit(limit_rate);
    log->log(log_msg);
    return 0;
}

// logger:raise(fmt, ...)
// 按日志的 ${key} 格式模板进行格式化后抛出 Lua 异常
static int lua_logger_raise(lua_State* L) {
    logger*     log = lua_check_logger(L, 1);
    const char* fmt = luaL_checkstring(L, 2);

    // 构建参数 dict（与 lua_logger_log_impl 保持一致：key, value 成对）
    mc::dict args;
    int      nargs = lua_gettop(L);
    for (int i = 3; i <= nargs; i++) {
        if (i + 1 <= nargs && lua_type(L, i) == LUA_TSTRING) {
            const char* key = lua_tostring(L, i);
            if (lua_isnumber(L, i + 1)) {
                args[key] = lua_tonumber(L, i + 1);
            } else if (lua_isstring(L, i + 1)) {
                args[key] = std::string(lua_tostring(L, i + 1));
            } else if (lua_isboolean(L, i + 1)) {
                args[key] = lua_toboolean(L, i + 1) != 0;
            }
            i++; // 跳过 value
        }
    }

    // 调用 C++ 接口，捕获异常并转换为 Lua 错误
    try {
        log->raise(fmt, args);
    } catch (const mc::exception& e) {
        // 将 C++ 异常转换为 Lua 错误
        return luaL_error(L, "%s", e.what());
    } catch (const std::exception& e) {
        // 捕获其他标准异常
        return luaL_error(L, "%s", e.what());
    }
    // 不会到达这里，因为 raise 是 [[noreturn]]
    return 0;
}

// logger:log(level, message, ...)
static int lua_logger_log(lua_State* L) {
    logger*     log = lua_check_logger(L, 1);
    int         lvl = static_cast<int>(luaL_checkinteger(L, 2));
    const char* msg = luaL_checkstring(L, 3);

    // 构建参数字典
    mc::dict args;
    int      nargs = lua_gettop(L);
    for (int i = 4; i <= nargs; i++) {
        // 支持键值对参数，格式：key, value, key, value, ...
        if (i + 1 <= nargs && lua_type(L, i) == LUA_TSTRING) {
            const char* key = lua_tostring(L, i);
            if (lua_isnumber(L, i + 1)) {
                args[key] = lua_tonumber(L, i + 1);
            } else if (lua_isstring(L, i + 1)) {
                args[key] = std::string(lua_tostring(L, i + 1));
            } else if (lua_isboolean(L, i + 1)) {
                args[key] = lua_toboolean(L, i + 1) != 0;
            }
            i++; // 跳过 value
        }
    }

    // 创建日志消息：从 Lua 调用栈提取 file/line/function
    context ctx = get_lua_call_context(L, 1);
    message log_msg(static_cast<level>(lvl), ctx, msg, args);
    log->log(log_msg);
    return 0;
}

// logger:trace(message, ...)
static int lua_logger_trace(lua_State* L) {
    return lua_logger_log_impl(L, level::trace, true);
}

// logger:debug(message, ...)
static int lua_logger_debug(lua_State* L) {
    return lua_logger_log_impl(L, level::debug, true);
}

// logger:info(message, ...)
static int lua_logger_info(lua_State* L) {
    return lua_logger_log_impl(L, level::info, true);
}

// logger:notice(message, ...)
static int lua_logger_notice(lua_State* L) {
    return lua_logger_log_impl(L, level::notice, true);
}

// logger:warn(message, ...)
static int lua_logger_warn(lua_State* L) {
    return lua_logger_log_impl(L, level::warn, true);
}

// logger:error(message, ...)
static int lua_logger_error(lua_State* L) {
    return lua_logger_log_impl(L, level::error, true);
}

// logger:fatal(message, ...)
static int lua_logger_fatal(lua_State* L) {
    return lua_logger_log_impl(L, level::fatal, true);
}

// logger:trace_easy(message, ...) 不限流
static int lua_logger_trace_easy(lua_State* L) {
    return lua_logger_log_impl(L, level::trace, false);
}

// logger:debug_easy(message, ...) 不限流
static int lua_logger_debug_easy(lua_State* L) {
    return lua_logger_log_impl(L, level::debug, false);
}

// logger:info_easy(message, ...) 不限流
static int lua_logger_info_easy(lua_State* L) {
    return lua_logger_log_impl(L, level::info, false);
}

// logger:notice_easy(message, ...) 不限流
static int lua_logger_notice_easy(lua_State* L) {
    return lua_logger_log_impl(L, level::notice, false);
}

// logger:warn_easy(message, ...) 不限流
static int lua_logger_warn_easy(lua_State* L) {
    return lua_logger_log_impl(L, level::warn, false);
}

// logger:error_easy(message, ...) 不限流
static int lua_logger_error_easy(lua_State* L) {
    return lua_logger_log_impl(L, level::error, false);
}

// logger:fatal_easy(message, ...) 不限流
static int lua_logger_fatal_easy(lua_State* L) {
    return lua_logger_log_impl(L, level::fatal, false);
}

// logger:mdbctl_log(fmt) 输出到 mdbctl 终端（使用 mdbctl_logger，仅输出到 socket，不含 console/file）
static int lua_logger_mdbctl_log(lua_State* L) {
    lua_check_logger(L, 1);
    const char* fmt = luaL_checkstring(L, 2);
    context     ctx = get_lua_call_context(L, 1);
    message     log_msg(level::debug, ctx, fmt, mc::dict());
    log_msg.set_category(log_category::mdbctl);
    log_manager::instance().get_logger(MC_LOG_MDBCTL_LOGGER).log(log_msg);
}

// 解析 initiator 表，参考 Lua parse_initiator：键为 Interface、UserName、ClientIp，
// 值可能为带 :value() 方法的对象（如 variant），空字符串用 'Unknown' 代替
// 返回是否解析成功；成功时填充 interface_name、username、client_addr
static bool parse_initiator(lua_State* L, int table_idx, std::string& interface_name,
                            std::string& username, std::string& client_addr) {
    static const char* caller_info[] = {"Interface", "UserName", "ClientIp"};
    std::string*       out_fields[]  = {&interface_name, &username, &client_addr};

    for (size_t i = 0; i < 3; i++) {
        lua_getfield(L, table_idx, caller_info[i]);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            return false;
        }
        // 若值为带 :value() 方法的对象，则调用获取实际字符串
        lua_getfield(L, -1, "value");
        if (!lua_isnil(L, -1)) {
            lua_pushvalue(L, -2); // self
            lua_call(L, 1, 1);
        }
        const char* s  = lua_isnil(L, -1) ? lua_tostring(L, -2) : lua_tostring(L, -1);
        *out_fields[i] = (s != nullptr && s[0] != '\0') ? s : "Unknown";
        lua_pop(L, 2); // 弹出 field_value 与 value() 返回值或 nil
    }
    return true;
}

// logger:operation(initiator, executor, fmt, key1, value1, ...)
// 操作日志接口，对应 C++ operation_log
// initiator: Lua 表，包含 Interface、UserName、ClientIp（值可为带 :value() 的对象，空字符串用 'Unknown'）
// executor: module_name
// fmt: 日志内容，支持 ${key} 占位符，可跟键值对参数
static int lua_logger_operation(lua_State* L) {
    logger* log = lua_check_logger(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE); // initiator 必须是 table
    const char* executor = luaL_checkstring(L, 3);
    const char* fmt      = luaL_checkstring(L, 4);

    std::string interface_name;
    std::string username;
    std::string client_addr;
    if (!parse_initiator(L, 2, interface_name, username, client_addr)) {
        return luaL_error(L, "operation: initiator must contain Interface, UserName, ClientIp");
    }

    mc::dict args;
    args["interface_name"] = std::move(interface_name);
    args["username"]       = std::move(username);
    args["client_addr"]    = std::move(client_addr);
    args["module_name"]    = std::string(executor);

    int nargs = lua_gettop(L);
    for (int i = 5; i <= nargs; i++) {
        if (i + 1 <= nargs && lua_type(L, i) == LUA_TSTRING) {
            const char* key = lua_tostring(L, i);
            if (lua_isnumber(L, i + 1)) {
                args[key] = lua_tonumber(L, i + 1);
            } else if (lua_isstring(L, i + 1)) {
                args[key] = std::string(lua_tostring(L, i + 1));
            } else if (lua_isboolean(L, i + 1)) {
                args[key] = lua_toboolean(L, i + 1) != 0;
            }
            i++;
        }
    }

    context ctx = get_lua_call_context(L, 1);
    message log_msg(level::all, ctx, fmt, args);
    log_msg.set_category(log_category::operation);
    log->log(log_msg);
    return 0;
}

// logger:running(level, message)
// 运行日志接口，格式：时间、级别、日志内容，如 2026-02-04 15:20:32 WARN : test running log
static int lua_logger_running(lua_State* L) {
    logger*     log = lua_check_logger(L, 1);
    int         lvl = static_cast<int>(luaL_checkinteger(L, 2));
    const char* msg = luaL_checkstring(L, 3);

    context ctx = get_lua_call_context(L, 1);
    message log_msg(static_cast<level>(lvl), ctx, msg, mc::dict());
    log_msg.set_category(log_category::running);
    log->log(log_msg);
    return 0;
}

// logger:security(message)
// 安全日志接口，格式：时间、级别、日志内容，仅 message 参数，使用 info 级别
static int lua_logger_security(lua_State* L) {
    logger*     log = lua_check_logger(L, 1);
    const char* msg = luaL_checkstring(L, 2);

    context ctx = get_lua_call_context(L, 1);
    message log_msg(level::info, ctx, msg, mc::dict());
    log_msg.set_category(log_category::security);
    log->log(log_msg);
    return 0;
}

// logger:maintenance(level, error_code, message) 或 logger:maintenance(level, message)
// 维护日志接口，有错误码时格式：时间、级别、错误码、日志内容；无错误码时：时间、级别、日志内容
static int lua_logger_maintenance(lua_State* L) {
    logger*     log   = lua_check_logger(L, 1);
    int         lvl   = static_cast<int>(luaL_checkinteger(L, 2));
    int         nargs = lua_gettop(L);
    const char* msg;
    mc::dict    args;

    if (nargs >= 4) {
        const char* error_code = luaL_checkstring(L, 3);
        msg                    = luaL_checkstring(L, 4);
        args["error_code"]     = std::string(error_code);
    } else {
        msg = luaL_checkstring(L, 3);
    }

    context ctx = get_lua_call_context(L, 1);
    message log_msg(static_cast<level>(lvl), ctx, msg, args);
    log_msg.set_category(log_category::maintenance);
    log->log(log_msg);
    return 0;
}

// 南向硬件流日志通用实现：级别 + 类别 hw_stream，参数约定与 lua_logger_log_impl 一致
// 接口：log:hw_stream_xxx(module_name, message, ...) 或 log:hw_stream_xxx(message, ...)
static int lua_logger_hw_stream_impl(lua_State* L, level lvl) {
    logger* log   = lua_check_logger(L, 1);
    int     nargs = lua_gettop(L);

    mc::dict    args;
    const char* msg       = nullptr;
    int         msg_index = 2;

    if (nargs >= 3 && lua_type(L, 2) == LUA_TSTRING && lua_type(L, 3) == LUA_TSTRING) {
        const char* module_name = lua_tostring(L, 2);
        args["module_name"]     = std::string(module_name);
        msg_index               = 3;
    }

    if (msg_index > nargs) {
        return luaL_error(L, "missing message argument");
    }
    msg = luaL_checkstring(L, msg_index);

    int start_index = msg_index + 1;
    for (int i = start_index; i <= nargs; i++) {
        if (i + 1 <= nargs && lua_type(L, i) == LUA_TSTRING) {
            const char* key = lua_tostring(L, i);
            if (lua_isnumber(L, i + 1)) {
                args[key] = lua_tonumber(L, i + 1);
            } else if (lua_isstring(L, i + 1)) {
                args[key] = std::string(lua_tostring(L, i + 1));
            } else if (lua_isboolean(L, i + 1)) {
                args[key] = lua_toboolean(L, i + 1) != 0;
            }
            i++;
        }
    }

    context ctx = get_lua_call_context(L, 1);
    message log_msg(lvl, ctx, msg, args);
    log_msg.set_category(log_category::hw_stream);
    log_msg.set_limit(true);
    log->log(log_msg);
    return 0;
}

static int lua_logger_hw_stream_info(lua_State* L) {
    return lua_logger_hw_stream_impl(L, level::info);
}
static int lua_logger_hw_stream_warn(lua_State* L) {
    return lua_logger_hw_stream_impl(L, level::warn);
}
static int lua_logger_hw_stream_notice(lua_State* L) {
    return lua_logger_hw_stream_impl(L, level::notice);
}
static int lua_logger_hw_stream_error(lua_State* L) {
    return lua_logger_hw_stream_impl(L, level::error);
}

// mc 流日志通用实现：级别 + 类别 mc_stream，参数约定与 hw_stream 一致，输出到 LOG_LOCAL5
static int lua_logger_mc_stream_impl(lua_State* L, level lvl) {
    logger* log   = lua_check_logger(L, 1);
    int     nargs = lua_gettop(L);

    mc::dict    args;
    const char* msg       = nullptr;
    int         msg_index = 2;

    if (nargs >= 3 && lua_type(L, 2) == LUA_TSTRING && lua_type(L, 3) == LUA_TSTRING) {
        const char* module_name = lua_tostring(L, 2);
        args["module_name"]     = std::string(module_name);
        msg_index               = 3;
    }

    if (msg_index > nargs) {
        return luaL_error(L, "missing message argument");
    }
    msg = luaL_checkstring(L, msg_index);

    int start_index = msg_index + 1;
    for (int i = start_index; i <= nargs; i++) {
        if (i + 1 <= nargs && lua_type(L, i) == LUA_TSTRING) {
            const char* key = lua_tostring(L, i);
            if (lua_isnumber(L, i + 1)) {
                args[key] = lua_tonumber(L, i + 1);
            } else if (lua_isstring(L, i + 1)) {
                args[key] = std::string(lua_tostring(L, i + 1));
            } else if (lua_isboolean(L, i + 1)) {
                args[key] = lua_toboolean(L, i + 1) != 0;
            }
            i++;
        }
    }

    context ctx = get_lua_call_context(L, 1);
    message log_msg(lvl, ctx, msg, args);
    log_msg.set_category(log_category::mc_stream);
    log_msg.set_limit(true);
    log->log(log_msg);
    return 0;
}

static int lua_logger_mc_stream_info(lua_State* L) {
    return lua_logger_mc_stream_impl(L, level::info);
}
static int lua_logger_mc_stream_warn(lua_State* L) {
    return lua_logger_mc_stream_impl(L, level::warn);
}
static int lua_logger_mc_stream_notice(lua_State* L) {
    return lua_logger_mc_stream_impl(L, level::notice);
}
static int lua_logger_mc_stream_error(lua_State* L) {
    return lua_logger_mc_stream_impl(L, level::error);
}

// 串口 printf 日志通用实现：使用 Lua string.format 格式化后调用 log_serial_printf
// 接口：log:debug_printf(module_name, fmt, ...) 或 log:debug_printf(fmt, ...)
// 如果参数数量 >= 3 且第一个和第二个参数都是字符串，则第一个参数作为 module_name（可选）
static int lua_logger_printf_impl(lua_State* L, level lvl) {
    logger* log   = lua_check_logger(L, 1);
    int     nargs = lua_gettop(L);

    // 检查第一个参数（索引2）是否是 module_name
    // 如果参数数量 >= 3 且第一个和第二个参数都是字符串，则第一个参数作为 module_name
    int      fmt_index = 2; // 默认格式字符串在索引2
    mc::dict args;

    if (nargs >= 3 && lua_type(L, 2) == LUA_TSTRING && lua_type(L, 3) == LUA_TSTRING) {
        const char* module_name = lua_tostring(L, 2);
        args["module_name"]     = std::string(module_name);
        fmt_index               = 3; // 格式字符串在索引3
    }

    // 获取格式字符串
    if (fmt_index > nargs) {
        return luaL_error(L, "missing format argument");
    }

    // 调用 Lua string.format 进行格式化
    // 栈状态：[logger, module_name?, fmt, arg1, arg2, ...]
    // 需要调用 string.format(fmt, arg1, arg2, ...)
    lua_getglobal(L, "string");
    lua_getfield(L, -1, "format");
    lua_remove(L, -2); // 移除 "string" table

    // 将 format 函数插入到 fmt 之前
    // 栈状态：[logger, module_name?, fmt, arg1, arg2, ..., format_func]
    lua_insert(L, fmt_index);
    // 栈状态：[logger, module_name?, format_func, fmt, arg1, arg2, ...]

    // 计算 format 函数的参数数量：从 fmt 到栈顶的所有参数（包括 fmt）
    int format_nargs = nargs - fmt_index + 1; // fmt + 后续所有参数
    if (lua_pcall(L, format_nargs, 1, 0) != LUA_OK) {
        return lua_error(L);
    }

    // 获取格式化后的字符串
    const char* s = lua_tostring(L, -1);
    if (s == nullptr) {
        lua_pop(L, 1);
        return 0;
    }
    std::string formatted_msg(s);
    lua_pop(L, 1);

    context ctx = get_lua_call_context(L, 1);
    message log_msg(lvl, formatted_msg, ctx, args);
    log->log_serial_printf(lvl, log_msg);
    return 0;
}

static int lua_logger_debug_printf(lua_State* L) {
    return lua_logger_printf_impl(L, level::debug);
}

static int lua_logger_info_printf(lua_State* L) {
    return lua_logger_printf_impl(L, level::info);
}

static int lua_logger_notice_printf(lua_State* L) {
    return lua_logger_printf_impl(L, level::notice);
}

static int lua_logger_warn_printf(lua_State* L) {
    return lua_logger_printf_impl(L, level::warn);
}

static int lua_logger_error_printf(lua_State* L) {
    return lua_logger_printf_impl(L, level::error);
}

// logger:__gc()
static int lua_logger_gc(lua_State* L) {
    logger* log = lua_check_logger(L, 1);
    log->~logger();
    return 0;
}

// logger:__tostring()
static int lua_logger_tostring(lua_State* L) {
    logger*     log = lua_check_logger(L, 1);
    std::string str = "logger(" + log->get_name() + ")";
    lua_pushstring(L, str.c_str());
    return 1;
}

// __index 元方法
static int lua_logger_index(lua_State* L) {
    // 从元表中查找方法
    luaL_getmetatable(L, LOGGER_METATABLE);
    lua_pushvalue(L, 2); // key（要查找的方法名）
    lua_rawget(L, -2);   // 在元表中查找

    if (!lua_isnil(L, -1)) {
        // 找到方法，移除元表，返回方法
        lua_remove(L, -2); // 移除元表
        return 1;
    }

    // 没找到，移除元表和 nil，返回 nil
    lua_pop(L, 2); // 弹出 nil 和元表
    lua_pushnil(L);
    return 1;
}

// 注册 logger 元表
static void register_logger_metatable(lua_State* L) {
    luaL_newmetatable(L, LOGGER_METATABLE);

    // 设置元方法
    lua_pushstring(L, "__index");
    lua_pushcfunction(L, lua_logger_index);
    lua_settable(L, -3);

    lua_pushstring(L, "__gc");
    lua_pushcfunction(L, lua_logger_gc);
    lua_settable(L, -3);

    lua_pushstring(L, "__tostring");
    lua_pushcfunction(L, lua_logger_tostring);
    lua_settable(L, -3);

    // 设置方法
    lua_pushstring(L, "get_name");
    lua_pushcfunction(L, lua_logger_get_name);
    lua_settable(L, -3);

    lua_pushstring(L, "set_name");
    lua_pushcfunction(L, lua_logger_set_name);
    lua_settable(L, -3);

    lua_pushstring(L, "get_level");
    lua_pushcfunction(L, lua_logger_get_level);
    lua_settable(L, -3);

    lua_pushstring(L, "set_level");
    lua_pushcfunction(L, lua_logger_set_level);
    lua_settable(L, -3);

    lua_pushstring(L, "is_enabled");
    lua_pushcfunction(L, lua_logger_is_enabled);
    lua_settable(L, -3);

    lua_pushstring(L, "system");
    lua_pushcfunction(L, lua_logger_system);
    lua_settable(L, -3);

    lua_pushstring(L, "period");
    lua_pushcfunction(L, lua_logger_period);
    lua_settable(L, -3);

    lua_pushstring(L, "condition");
    lua_pushcfunction(L, lua_logger_condition);
    lua_settable(L, -3);

    lua_pushstring(L, "raise");
    lua_pushcfunction(L, lua_logger_raise);
    lua_settable(L, -3);

    lua_pushstring(L, "log");
    lua_pushcfunction(L, lua_logger_log);
    lua_settable(L, -3);

    // 添加便捷日志方法
    lua_pushstring(L, "trace");
    lua_pushcfunction(L, lua_logger_trace);
    lua_settable(L, -3);

    lua_pushstring(L, "debug");
    lua_pushcfunction(L, lua_logger_debug);
    lua_settable(L, -3);

    lua_pushstring(L, "info");
    lua_pushcfunction(L, lua_logger_info);
    lua_settable(L, -3);

    lua_pushstring(L, "notice");
    lua_pushcfunction(L, lua_logger_notice);
    lua_settable(L, -3);

    lua_pushstring(L, "warn");
    lua_pushcfunction(L, lua_logger_warn);
    lua_settable(L, -3);

    lua_pushstring(L, "error");
    lua_pushcfunction(L, lua_logger_error);
    lua_settable(L, -3);

    lua_pushstring(L, "fatal");
    lua_pushcfunction(L, lua_logger_fatal);
    lua_settable(L, -3);

    // 添加不限流（_easy）便捷日志方法
    lua_pushstring(L, "trace_easy");
    lua_pushcfunction(L, lua_logger_trace_easy);
    lua_settable(L, -3);

    lua_pushstring(L, "debug_easy");
    lua_pushcfunction(L, lua_logger_debug_easy);
    lua_settable(L, -3);

    lua_pushstring(L, "info_easy");
    lua_pushcfunction(L, lua_logger_info_easy);
    lua_settable(L, -3);

    lua_pushstring(L, "notice_easy");
    lua_pushcfunction(L, lua_logger_notice_easy);
    lua_settable(L, -3);

    lua_pushstring(L, "warn_easy");
    lua_pushcfunction(L, lua_logger_warn_easy);
    lua_settable(L, -3);

    lua_pushstring(L, "error_easy");
    lua_pushcfunction(L, lua_logger_error_easy);
    lua_settable(L, -3);

    lua_pushstring(L, "fatal_easy");
    lua_pushcfunction(L, lua_logger_fatal_easy);
    lua_settable(L, -3);

    // 操作日志方法（对应 C++ operation_log，类别 operation）
    lua_pushstring(L, "operation");
    lua_pushcfunction(L, lua_logger_operation);
    lua_settable(L, -3);

    // 运行日志方法（格式：时间、级别、日志内容）
    lua_pushstring(L, "running");
    lua_pushcfunction(L, lua_logger_running);
    lua_settable(L, -3);

    // 维护日志方法（格式：时间、级别、错误码、日志内容）
    lua_pushstring(L, "maintenance");
    lua_pushcfunction(L, lua_logger_maintenance);
    lua_settable(L, -3);

    // 安全日志方法（格式：时间、级别、日志内容）
    lua_pushstring(L, "security");
    lua_pushcfunction(L, lua_logger_security);
    lua_settable(L, -3);

    // 南向硬件流日志方法（输出到相关日志文件，格式：时间 模块名 级别: 文件名(行数): 日志文本）
    lua_pushstring(L, "hw_stream_info");
    lua_pushcfunction(L, lua_logger_hw_stream_info);
    lua_settable(L, -3);

    lua_pushstring(L, "hw_stream_warn");
    lua_pushcfunction(L, lua_logger_hw_stream_warn);
    lua_settable(L, -3);

    lua_pushstring(L, "hw_stream_notice");
    lua_pushcfunction(L, lua_logger_hw_stream_notice);
    lua_settable(L, -3);

    lua_pushstring(L, "hw_stream_error");
    lua_pushcfunction(L, lua_logger_hw_stream_error);
    lua_settable(L, -3);

    // mc 流日志方法（输出到 LOG_LOCAL5 对应日志文件，格式与 hw_stream 相同）
    lua_pushstring(L, "mc_stream_info");
    lua_pushcfunction(L, lua_logger_mc_stream_info);
    lua_settable(L, -3);

    lua_pushstring(L, "mc_stream_warn");
    lua_pushcfunction(L, lua_logger_mc_stream_warn);
    lua_settable(L, -3);

    lua_pushstring(L, "mc_stream_notice");
    lua_pushcfunction(L, lua_logger_mc_stream_notice);
    lua_settable(L, -3);

    lua_pushstring(L, "mc_stream_error");
    lua_pushcfunction(L, lua_logger_mc_stream_error);
    lua_settable(L, -3);

    // 串口 printf 方式日志（由 file_appender 承载，printf 输出日志内容）
    lua_pushstring(L, "debug_printf");
    lua_pushcfunction(L, lua_logger_debug_printf);
    lua_settable(L, -3);

    lua_pushstring(L, "info_printf");
    lua_pushcfunction(L, lua_logger_info_printf);
    lua_settable(L, -3);

    lua_pushstring(L, "notice_printf");
    lua_pushcfunction(L, lua_logger_notice_printf);
    lua_settable(L, -3);

    lua_pushstring(L, "warn_printf");
    lua_pushcfunction(L, lua_logger_warn_printf);
    lua_settable(L, -3);

    lua_pushstring(L, "error_printf");
    lua_pushcfunction(L, lua_logger_error_printf);
    lua_settable(L, -3);

    lua_pushstring(L, "mdbctl_log");
    lua_pushcfunction(L, lua_logger_mdbctl_log);
    lua_settable(L, -3);

    // 让 Lua 侧可以用 log.WARN / log.INFO 等常量（本质来自元表）
    register_log_levels(L);

    lua_pop(L, 1); // 弹出元表
}

extern "C" {
// 注册 mc.log 模块
__attribute__((visibility("default"))) int luaopen_mc_log(lua_State* L) {
    // 注册 logger 元表
    register_logger_metatable(L);

    // 直接返回默认 logger 对象
    logger log = logger::get();
    lua_push_logger(L, log);

    return 1;
}
}

} // namespace log
} // namespace mc
