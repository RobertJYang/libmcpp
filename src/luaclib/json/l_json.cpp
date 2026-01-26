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

#include "utils/variant_utils.h"
#include <climits>
#include <cmath>
#include <string>
#include <vector>

#include <mc/dict/dict.h>
#include <mc/exception.h>
#include <mc/filesystem.h>
#include <mc/json_wrapper.h>
#include <json_api.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace mc {
namespace json_wrapper {
namespace lua {

constexpr const char* JSON_VALUE_METATABLE = "json.value";

// 注意：l_json.cpp 中只能使用 C++ 接口（JsonValue, JsonArray, JsonObject），不能直接调用 C 接口

// 全局标志位：控制空 table 是否编码为 JSON 对象
// false: 空 table 编码为 JSON 数组 []（默认）
// true:  空 table 编码为 JSON 对象 {}
static bool g_encode_empty_table_as_object = false;

namespace {
// 辅助函数：获取 Lua table 的数组部分大小
lua_Integer get_lua_table_length(::lua_State* L, int index) {
    lua_len(L, index);
    lua_Integer len = lua_tointeger(L, -1);
    lua_pop(L, 1);
    return len;
}

// 辅助函数：判断 Lua number 是否为整数
bool is_lua_integer(::lua_State* L, int index) {
    if (!lua_isnumber(L, index)) {
        return false;
    }
    lua_Number val = lua_tonumber(L, index);
    // 检查是否为整数
    return std::floor(val) == val && val >= static_cast<lua_Number>(INT64_MIN) &&
           val <= static_cast<lua_Number>(INT64_MAX);
}

// 辅助函数：判断 Lua table 是否为数组
bool is_lua_array(::lua_State* L, int index) {
    if (!lua_istable(L, index)) {
        return false;
    }

    // 规范化索引
    int abs_index = (index < 0) ? lua_gettop(L) + index + 1 : index;

    // 获取 table 的大小
    lua_Integer array_size = get_lua_table_length(L, abs_index);

    if (array_size == 0) {
        // 空 table，检查是否有非整数键
        lua_pushnil(L);
        if (lua_next(L, abs_index) != 0) {
            lua_pop(L, 2); // 弹出键和值
            return false;  // 有键但不是数组索引
        }
        // 空 table 根据 g_encode_empty_table_as_object 标志决定
        // 如果 g_encode_empty_table_as_object 为 true，则不视为数组（返回 false）
        // 如果为 false，则视为数组（返回 true）
        return !g_encode_empty_table_as_object;
    }

    // 检查从 1 到 array_size 的键是否都存在且值不为 nil
    for (lua_Integer i = 1; i <= array_size; ++i) {
        lua_rawgeti(L, abs_index, static_cast<int>(i));
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            return false; // 缺少某个索引
        }
        lua_pop(L, 1);
    }

    // 检查是否有其他键（非 1 到 array_size 的整数键）
    lua_pushnil(L);
    while (lua_next(L, abs_index) != 0) {
        if (is_lua_integer(L, -2)) {
            lua_Number  key_num = lua_tonumber(L, -2);
            lua_Integer key     = static_cast<lua_Integer>(key_num);
            if (key < 1 || key > array_size) {
                lua_pop(L, 2); // 弹出键和值
                return false;  // 有超出范围的整数键
            }
        } else {
            lua_pop(L, 2); // 弹出键和值
            return false;  // 有非整数键
        }
        lua_pop(L, 1); // 弹出值，保留键用于下一次迭代
    }

    return true;
}
} // namespace

// 用于存储 JsonValue 的包装器
struct json_value_wrapper {
    JsonValue value;

    json_value_wrapper() = default;
    explicit json_value_wrapper(JsonValue&& v)
        : value(std::move(v)) {
    }
};

// 强制检查并获取 json_value userdata
// 如果类型不匹配，会抛出 Lua 错误（用于必须确保类型的场景，如元方法）
inline json_value_wrapper* check_json_value(lua_State* L, int index) {
    return static_cast<json_value_wrapper*>(luaL_checkudata(L, index, JSON_VALUE_METATABLE));
}

// 尝试获取 json_value userdata（可选检查）
// 如果类型不匹配，返回 nullptr（用于可选场景，如 get_json_value_from_lua）
inline json_value_wrapper* test_json_value(lua_State* L, int index) {
    return static_cast<json_value_wrapper*>(luaL_testudata(L, index, JSON_VALUE_METATABLE));
}

// 创建 json_value userdata 并推入 Lua 栈
inline int push_json_value(lua_State* L, JsonValue&& value) {
    void* userdata = lua_newuserdata(L, sizeof(json_value_wrapper));
    new (userdata) json_value_wrapper(std::move(value));

    luaL_getmetatable(L, JSON_VALUE_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

static Json *json_array_new(lua_State* L, int idx);
static Json *json_object_new(lua_State* L, int idx);
static Json *json_new(lua_State* L, int index)
{   Json *json;
    int idx = (index < 0) ? lua_gettop(L) + index + 1 : index;
    int type = lua_type(L, idx);
    switch(type) {
        case LUA_TNIL: {
            JsonNullCreate(&json);
            return json;
        }
        case LUA_TBOOLEAN: {
            bool val = lua_toboolean(L, idx) != 0;
            JsonBoolCreate(val, &json);
            return json;
        }
        case LUA_TNUMBER: {
            if (lua_isinteger(L, idx)) {
                int64_t val = static_cast<int64_t>(lua_tointeger(L, idx));
                JsonIntegerCreate(val, &json);
                return json;
            } else {
                double val = lua_tonumber(L, idx);
                JsonDoubleCreate(val, &json);
                return json;
            }
        }
        case LUA_TSTRING: {
            size_t      len = 0;
            const char* str = lua_tolstring(L, idx, &len);
            JsonStringCreateWithLen(str, len, &json);
            return json;
        }
        case LUA_TTABLE: {
            if (lua_rawlen(L, idx) > 0) {
                return json_array_new(L, idx);
            } else {
                return json_object_new(L, idx);
            }
        }
        case LUA_TLIGHTUSERDATA: {
            if (lua_touserdata(L, idx) == nullptr) {
                JsonNullCreate(&json);
                return json;
            } else {
                luaL_error(L, "json: cannot convert lightuserdata to json");
                return nullptr;
            }
        }
        default: {
            luaL_error(L, "json: cannot convert %s to json", lua_typename(L, type));
            return nullptr;
        }
    }
}

static Json *json_array_new(lua_State* L, int idx)
{
    size_t len = lua_rawlen(L, idx);
    Json *object;
    if (JsonArrayCreate(&object) != JSON_OK) {
        luaL_error(L, "json: cannot create json array");
    }
    uint32_t array_size = 0;
    for (size_t i = 0; i < len; i++) {
        if (JsonArraySizeGet(object, &array_size) != JSON_OK) {
            luaL_error(L, "json: cannot get json array size");
        }
        lua_rawgeti(L, idx, i + 1);
        JsonArrayItemInsert(object, array_size, json_new(L, -1));
        lua_pop(L, 1);
    }

    return object;
}

static bool is_empty_object(lua_State* L, int idx)
{
    lua_pushnil(L);
    if (lua_next(L, idx) == 0) {
        return true;
    } else {
        lua_pop(L, 2);
        return false;
    }
}

static Json *json_object_new(lua_State* L, int idx)
{
    if (is_empty_object(L, idx) && !g_encode_empty_table_as_object) {
        return json_array_new(L, idx);
    }
    Json *object;
    if (JsonObjectCreate(&object) != JSON_OK) {
        luaL_error(L, "json: cannot create json object");
    } 
    lua_pushnil(L);
    while (lua_next(L, idx) != 0) { 
        const char* key_c = luaL_checkstring(L, -2);
        JsonItemAddToObject(key_c, json_new(L, -1), object);
        lua_pop(L, 1);
    }
    return object;
}


// 直接从 Lua 值转换为 JsonValue（避免 variant 中间转换，只使用 C++ 接口）
static JsonValue build_json_from_lua(lua_State* L, int index) {
    int abs_index = (index < 0) ? lua_gettop(L) + index + 1 : index;
    return JsonValue(json_new(L, abs_index));
}

static void json_object_push(lua_State* L, Json* val);
static void json_array_push(lua_State* L, Json* val);
static void json_value_push(lua_State* L, Json* val) {
    JsonType type = JsonTypeGet(val);
    switch (type) {
        case JSONTYPE_QUOTE: {
            Json *tmp;
            if (JsonObjectQuoteGet(val, &tmp) != JSON_OK) {
                luaL_error(L, "json: cannot get json quote");
            }
            val = tmp;
            break;
        }
        case JSONTYPE_NULL: {
            lua_pushlightuserdata(L, nullptr);
            break;
        }
        case JSONTYPE_TRUE: {
            lua_pushboolean(L, true);
            break;
        }
        case JSONTYPE_FALSE: {
            lua_pushboolean(L, false);
            break;
        }
        case JSONTYPE_NUMBER: {
            if (JsonIsInteger(val)) {
                int64_t value;
                if (JsonItemIntegerValueGet(val, &value) != JSON_OK) {
                    luaL_error(L, "json: cannot get json integer value");
                }
                lua_pushinteger(L, value);
            } else {
                double value;
                if (JsonItemDoubleValueGet(val, &value) != JSON_OK) {
                    luaL_error(L, "json: cannot get json double value");
                }
                lua_pushnumber(L, value);
            }
            break;
        }
        case JSONTYPE_OBJECT: {
            json_object_push(L, val);
            break;
        }
        case JSONTYPE_ARRAY: {
            json_array_push(L, val);
            break;
        }
        case JSONTYPE_STRING: {
            char* value = nullptr;
            uint32_t size = 0;
            if (JsonItemStringValueGet(val, &value) != JSON_OK || JsonItemStringValueLenGet(val, &size) != JSON_OK) {
                luaL_error(L, "json: cannot get json string value");
            }
            lua_pushlstring(L, value, size);
            break;
        }
        default:
            luaL_error(L, "json: cannot convert json type %d to lua", type);
    }
}

static void json_object_push(lua_State* L, Json* val) {
    lua_createtable(L, 0, 0);
    Json *child;
    uint32_t ret = JsonItemFirstChild(val, &child);
    char *key = nullptr;
    while (ret == JSON_OK && child != nullptr) {
        if (JsonItemKeyGet(child, &key) == JSON_OK) {
            json_value_push(L, child);
            lua_setfield(L, -2, key);
        } else {
            luaL_error(L, "json: cannot get json object key");
        }
        if (JsonItemNextSibling(child, &child) != JSON_OK) {
            break;
        }
    }
}

static void json_array_push(lua_State* L, Json* val) {
    uint32_t len = 0;
    if (JsonArraySizeGet(val, &len) != JSON_OK) {
        luaL_error(L, "json: cannot get json object size");
    }
    lua_createtable(L, len, 0);
    for (size_t i = 0; i < len; i++) {
        Json *item;
        if (JsonArrayItemGet(val, i, &item) != JSON_OK) {
            luaL_error(L, "json: cannot get json object item");
        }
        json_value_push(L, item);
        lua_rawseti(L, -2, i + 1);
    }
}

// 直接从 JsonValue 转换为 Lua table（避免 variant 中间转换，只使用 C++ 接口）
static void json_value_to_lua(lua_State* L, const JsonValue& value) {
    json_value_push(L, value.get_raw());
}

// 从 Lua 值转换为 JsonValue
// 如果是 userdata，直接使用；否则从 Lua 直接转换
inline JsonValue get_json_value_from_lua(lua_State* L, int index) {
    // 先尝试作为 userdata
    json_value_wrapper* wrapper = test_json_value(L, index);
    if (wrapper != nullptr) {
        // 直接返回拷贝（JsonValue 的拷贝构造函数会管理引用计数）
        return wrapper->value;
    }

    // 否则直接从 Lua 值转换为 JsonValue（只使用 C++ 接口）
    return build_json_from_lua(L, index);
}

// 将 Lua 值转换为 JsonValue 并推入栈
inline int lua_value_to_json_value(lua_State* L, int index) {
    json_value_wrapper* wrapper = test_json_value(L, index);
    if (wrapper != nullptr) {
        // 已经是 JsonValue，直接推入
        lua_pushvalue(L, index);
        return 1;
    }

    // 直接从 Lua 值转换为 JsonValue（只使用 C++ 接口）
    JsonValue json_val = build_json_from_lua(L, index);
    return push_json_value(L, std::move(json_val));
}

// __gc 元方法
static int json_value_gc(lua_State* L) {
    auto* wrapper = check_json_value(L, 1);
    wrapper->~json_value_wrapper();
    return 0;
}

// __index 元方法 - 支持 obj[key] 和 obj.method() 访问
static int json_value_index(lua_State* L) {
    try {
        auto* wrapper = check_json_value(L, 1);

        // 获取索引
        int key_type = lua_type(L, 2);

        if (key_type == LUA_TSTRING) {
            const char* key = lua_tostring(L, 2);

            // 先检查是否是方法名
            lua_getmetatable(L, 1);
            lua_getfield(L, -1, key);
            if (!lua_isnil(L, -1)) {
                return 1; // 返回方法
            }
            lua_pop(L, 2); // 弹出 nil 和 metatable

            // 如果是对象，通过键访问
            if (wrapper->value.is_object()) {
                JsonObject obj = wrapper->value.as_object();
                if (obj.has(key)) {
                    JsonValue val = obj.get(key);
                    return push_json_value(L, std::move(val));
                }
                lua_pushnil(L);
                return 1;
            }
        } else if (key_type == LUA_TNUMBER) {
            // 如果是数组，通过索引访问
            if (wrapper->value.is_array()) {
                lua_Integer idx   = lua_tointeger(L, 2);
                JsonArray   arr   = wrapper->value.as_array();
                uint32_t    size  = arr.size();
                uint32_t    index = static_cast<uint32_t>(idx - 1); // Lua 索引从 1 开始

                if (index < size) {
                    JsonValue val = arr.at(index);
                    return push_json_value(L, std::move(val));
                }
                lua_pushnil(L);
                return 1;
            }
        }

        lua_pushnil(L);
        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "__index failed: %s", e.what());
    }
}

// __newindex 元方法 - 支持 obj[key] = value 设置
static int json_value_newindex(lua_State* L) {
    try {
        auto* wrapper = check_json_value(L, 1);

        int key_type = lua_type(L, 2);

        if (key_type == LUA_TSTRING && wrapper->value.is_object()) {
            const char* key = lua_tostring(L, 2);
            JsonValue   val = get_json_value_from_lua(L, 3);

            JsonObject obj = wrapper->value.as_object();
            obj.set(key, val);
            return 0;
        } else if (key_type == LUA_TNUMBER && wrapper->value.is_array()) {
            lua_Integer idx   = lua_tointeger(L, 2);
            uint32_t    index = static_cast<uint32_t>(idx - 1); // Lua 索引从 1 开始
            JsonValue   val   = get_json_value_from_lua(L, 3);

            JsonArray arr  = wrapper->value.as_array();
            uint32_t  size = arr.size();

            if (index < size) {
                // 替换已存在的元素
                arr.set(index, val);
            } else if (index == size) {
                // 在末尾添加新元素
                arr.push_back(val);
            } else {
                // 索引超出范围，需要先扩展数组
                // 先添加 null 值填充到 index-1，然后添加新值
                for (uint32_t i = size; i < index; ++i) {
                    JsonValue null_val = JsonValue::make_null();
                    arr.push_back(null_val);
                }
                arr.push_back(val);
            }
            return 0;
        }

        return luaL_error(L, "__newindex: invalid operation");
    } catch (const mc::exception& e) {
        return luaL_error(L, "__newindex failed: %s", e.what());
    }
}

// __len 元方法 - 支持 #obj 获取长度
static int json_value_len(lua_State* L) {
    try {
        auto* wrapper = check_json_value(L, 1);

        if (wrapper->value.is_array()) {
            JsonArray arr = wrapper->value.as_array();
            lua_pushinteger(L, static_cast<lua_Integer>(arr.size()));
            return 1;
        } else if (wrapper->value.is_object()) {
            JsonObject obj = wrapper->value.as_object();
            lua_pushinteger(L, static_cast<lua_Integer>(obj.size()));
            return 1;
        }

        lua_pushinteger(L, 0);
        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "__len failed: %s", e.what());
    }
}

// __pairs 元方法 - 支持 for k, v in pairs(obj) 遍历对象
static int json_value_pairs_iter(lua_State* L) {
    try {
        auto* wrapper = check_json_value(L, 1);

        if (!wrapper->value.is_object()) {
            return 0;
        }

        JsonObject obj = wrapper->value.as_object();

        // 第二个参数是上一个键（如果有）
        const char* prev_key = nullptr;
        if (!lua_isnil(L, 2)) {
            prev_key = lua_tostring(L, 2);
        }

        // 遍历找到下一个键
        std::string next_key;
        JsonValue   next_value;
        bool        has_next = false;

        if (prev_key == nullptr) {
            // 第一次调用，返回第一个键
            auto it = obj.begin();
            if (it != obj.end()) {
                auto kv    = *it;
                next_key   = kv.key;
                next_value = kv.value;
                has_next   = true;
            }
        } else {
            // 查找上一个键，返回下一个键
            bool found_prev = false;
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                auto kv = *it;
                if (found_prev) {
                    // 已经找到上一个键，返回当前键
                    next_key   = kv.key;
                    next_value = kv.value;
                    has_next   = true;
                    break;
                }
                // 查找上一个键
                if (kv.key == prev_key) {
                    found_prev = true;
                    // 继续循环，下一次迭代会返回下一个键
                }
            }
        }

        if (has_next) {
            lua_pushstring(L, next_key.c_str());
            // 直接使用 JsonValue，避免 variant 转换
            return push_json_value(L, std::move(next_value)) + 1;
        }

        return 0;
    } catch (const mc::exception& e) {
        return luaL_error(L, "pairs iterator failed: %s", e.what());
    }
}

static int json_value_pairs(lua_State* L) {
    // 返回迭代器函数、状态和初始值
    lua_pushcfunction(L, json_value_pairs_iter);
    lua_pushvalue(L, 1); // state: json object
    lua_pushnil(L);      // initial key
    return 3;
}

// 注册 json.value metatable
static void register_json_value_metatable(lua_State* L) {
    // 创建 metatable
    luaL_newmetatable(L, JSON_VALUE_METATABLE);

    // 设置元方法
    lua_pushcfunction(L, json_value_index);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, json_value_newindex);
    lua_setfield(L, -2, "__newindex");

    lua_pushcfunction(L, json_value_len);
    lua_setfield(L, -2, "__len");

    lua_pushcfunction(L, json_value_pairs);
    lua_setfield(L, -2, "__pairs");

    lua_pushcfunction(L, json_value_gc);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1); // 弹出 metatable
}

// 获取 JSON 对象的所有键（按顺序）
static int l_json_object_get_keys(lua_State* L) {
    try {
        int nargs = lua_gettop(L);
        if (nargs < 1) {
            return luaL_error(L, "json_object_get_keys requires 1 argument: json_value");
        }

        JsonValue json_val = get_json_value_from_lua(L, 1);

        if (!json_val.is_object()) {
            return luaL_error(L, "argument must be a JSON object");
        }

        JsonObject obj = json_val.as_object();

        // 创建 Lua table 存储键（按顺序）
        lua_newtable(L);
        int index = 1;
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            auto kv = *it;
            lua_pushstring(L, kv.key.c_str());
            lua_rawseti(L, -2, index++);
        }

        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "json_object_get_keys failed: %s", e.what());
    }
}

// 判断两个 JSON 对象是否相等
static int l_json_object_is_equal(lua_State* L) {
    try {
        int nargs = lua_gettop(L);
        if (nargs < 2) {
            return luaL_error(L, "json_object_is_equal requires 2 arguments: json1, json2");
        }

        JsonValue json1 = get_json_value_from_lua(L, 1);
        JsonValue json2 = get_json_value_from_lua(L, 2);

        bool is_equal = (json1 == json2);
        lua_pushboolean(L, is_equal ? 1 : 0);
        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "json_object_is_equal failed: %s", e.what());
    }
}

// 将 JSON 对象转换为 Lua table
// 注意：只接受 userdata（json_object）作为参数，如果传入普通的 Lua table 会报错
// 直接从 JsonValue 转换为 Lua table，避免 variant 中间转换
static int l_json_object_to_table(lua_State* L) {
    try {
        int nargs = lua_gettop(L);
        if (nargs < 1) {
            return luaL_error(L, "json_object_to_table requires 1 argument: json_value");
        }

        // 检查参数类型：只接受 userdata（json_object）
        // check_json_value 使用 luaL_checkudata，如果不是 userdata 或类型不匹配会抛出错误
        json_value_wrapper* wrapper = check_json_value(L, 1);

        // 直接从 JsonValue 转换为 Lua table，递归转换所有嵌套对象
        // 确保返回的 table 中不包含 userdata
        json_value_to_lua(L, wrapper->value);
        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "json_object_to_table failed: %s", e.what());
    }
}

// 从 Lua table 创建 JSON 对象（直接从 Lua 转换，避免 variant 中间层）
static int l_json_object_from_table(lua_State* L) {
    try {
        int nargs = lua_gettop(L);
        if (nargs < 1) {
            return luaL_error(L, "json_object_from_table requires 1 argument: table");
        }

        // 直接从 Lua 值转换为 JsonValue，避免 variant 中间转换（只使用 C++ 接口）
        JsonValue json_val = build_json_from_lua(L, 1);
        return push_json_value(L, std::move(json_val));
    } catch (const mc::exception& e) {
        return luaL_error(L, "json_object_from_table failed: %s", e.what());
    }
}

// 深拷贝 JsonValue（避免 variant 中间转换，只使用 C++ 接口）
static JsonValue copy_json_value(const JsonValue& value) {
    if (value.is_null()) {
        return JsonValue::make_null();
    } else if (value.is_bool()) {
        return JsonValue::make_bool(value.as_bool());
    } else if (value.is_int()) {
        return JsonValue::make_int(value.as_int());
    } else if (value.is_double()) {
        return JsonValue::make_double(value.as_double());
    } else if (value.is_string()) {
        return JsonValue::make_string(value.as_string());
    } else if (value.is_array()) {
        JsonValue arr_val = JsonValue::make_array();
        JsonArray arr     = arr_val.as_array();
        JsonArray src_arr = value.as_array();

        for (uint32_t i = 0; i < src_arr.size(); ++i) {
            JsonValue child = copy_json_value(src_arr.at(i));
            arr.push_back(child);
        }
        return arr_val;
    } else if (value.is_object()) {
        JsonValue  obj_val = JsonValue::make_object();
        JsonObject obj     = obj_val.as_object();
        JsonObject src_obj = value.as_object();

        // 收集所有键值对，保持原始顺序
        std::vector<std::pair<std::string, JsonValue>> entries;

        for (const auto& pair : src_obj) {
            JsonValue copied_value = copy_json_value(pair.value);
            entries.push_back({pair.key, copied_value});
        }

        // 按照原始顺序插入到 JSON 对象中
        for (const auto& entry : entries) {
            obj.set(entry.first, entry.second);
        }

        return obj_val;
    } else {
        MC_THROW(mc::parse_error_exception, "Unsupported JSON type for copy");
    }
}

// 复制 JSON 对象（深拷贝）
static int l_json_object_copy(lua_State* L) {
    try {
        int nargs = lua_gettop(L);
        if (nargs < 1) {
            return luaL_error(L, "json_object_copy requires 1 argument: json_value");
        }

        JsonValue json_val = get_json_value_from_lua(L, 1);
        JsonValue copied   = copy_json_value(json_val);
        return push_json_value(L, std::move(copied));
    } catch (const mc::exception& e) {
        return luaL_error(L, "json_object_copy failed: %s", e.what());
    }
}

// 判断是否为 JSON 对象类型
static int l_json_object_is_object(lua_State* L) {
    try {
        int nargs = lua_gettop(L);
        if (nargs < 1) {
            return luaL_error(L, "json_object_is_object requires 1 argument: json_value");
        }

        JsonValue json_val  = get_json_value_from_lua(L, 1);
        bool      is_object = json_val.is_object();
        lua_pushboolean(L, is_object ? 1 : 0);
        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "json_object_is_object failed: %s", e.what());
    }
}

// 判断是否为 JSON 数组类型
static int l_json_object_is_array(lua_State* L) {
    try {
        int nargs = lua_gettop(L);
        if (nargs < 1) {
            return luaL_error(L, "json_object_is_array requires 1 argument: json_value");
        }

        JsonValue json_val = get_json_value_from_lua(L, 1);
        bool      is_array = json_val.is_array();
        lua_pushboolean(L, is_array ? 1 : 0);
        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "json_object_is_array failed: %s", e.what());
    }
}

// 创建新的 JSON 对象
static int l_json_object_new_object(lua_State* L) {
    try {
        JsonValue obj = JsonValue::make_object();
        return push_json_value(L, std::move(obj));
    } catch (const mc::exception& e) {
        return luaL_error(L, "json_object_new_object failed: %s", e.what());
    }
}

// 创建新的 JSON 数组
static int l_json_object_new_array(lua_State* L) {
    try {
        JsonValue arr = JsonValue::make_array();
        return push_json_value(L, std::move(arr));
    } catch (const mc::exception& e) {
        return luaL_error(L, "json_object_new_array failed: %s", e.what());
    }
}

// 获取底层 Json* 指针（作为 lightuserdata）
static int l_to_raw(lua_State* L) {
    try {
        int nargs = lua_gettop(L);
        if (nargs < 1) {
            return luaL_error(L, "to_raw requires 1 argument: json_value");
        }

        JsonValue json_val = get_json_value_from_lua(L, 1);
        Json*     raw_ptr  = json_val.get_raw();
        lua_pushlightuserdata(L, static_cast<void*>(raw_ptr));
        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "to_raw failed: %s", e.what());
    }
}

// 从底层 Json* 指针创建 JsonValue
static int l_new_from_raw(lua_State* L) {
    try {
        int nargs = lua_gettop(L);
        if (nargs < 1) {
            return luaL_error(L, "new_from_raw requires 1 argument: raw_pointer");
        }

        if (!lua_islightuserdata(L, 1)) {
            return luaL_error(L, "argument must be a lightuserdata");
        }

        Json* raw_ptr = static_cast<Json*>(lua_touserdata(L, 1));
        if (raw_ptr == nullptr) {
            return luaL_error(L, "null pointer provided");
        }

        JsonValue json_val = JsonValue::new_from_raw(raw_ptr);
        return push_json_value(L, std::move(json_val));
    } catch (const mc::exception& e) {
        return luaL_error(L, "new_from_raw failed: %s", e.what());
    }
}

// Lua 接口：将 Lua 值编码为 JSON 字符串
// 使用 C++ 接口（JsonValue）进行转换，然后使用 json_encode 进行序列化
static int lencode(lua_State* L) {
    try {
        int nargs = lua_gettop(L);
        if (nargs < 1) {
            return luaL_error(L, "encode requires 1 argument: value");
        }

        // 使用 build_json_from_lua 将 Lua 值转换为 JsonValue
        JsonValue json_val = build_json_from_lua(L, 1);

        // 直接使用 json_encode 进行序列化（紧凑格式）
        std::string json_str = mc::json_wrapper::json_encode(json_val, false);

        // 将结果推入 Lua 栈
        lua_pushlstring(L, json_str.data(), json_str.size());

        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "encode failed: %s", e.what());
    }
}

// Lua 接口：将 JSON 字符串解码为 Lua 值
static int ldecode(lua_State* L) {
    try {
        size_t      json_len = 0;
        const char* json_str = luaL_checklstring(L, 1, &json_len);

        JsonValue json_val = mc::json_wrapper::json_decode_raw(json_str);

        // 将 JsonValue 对象转换为 Lua 值并推入栈（递归转换所有嵌套对象）
        json_value_to_lua(L, json_val);

        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "decode failed: %s", e.what());
    }
}

// Lua 接口：将 JSON 字符串解码为 JsonValue 对象
// 接受 JSON 字符串，返回 JsonValue userdata
static int ljson_object_ordered_decode(lua_State* L) {
    try {
        size_t      json_len = 0;
        const char* json_str = luaL_checklstring(L, 1, &json_len);

        JsonValue json_val = mc::json_wrapper::json_decode_raw(json_str);

        // 将 JsonValue 对象推入 Lua 栈（作为 userdata）
        return push_json_value(L, std::move(json_val));
    } catch (const mc::exception& e) {
        return luaL_error(L, "json_object_ordered_decode failed: %s", e.what());
    }
}

// Lua 接口：将 JsonValue 对象编码为 JSON 字符串
// 接受 JsonValue userdata，将其编码为 JSON 字符串（紧凑格式）
static int ljson_object_ordered_encode(lua_State* L) {
    try {
        int nargs = lua_gettop(L);
        if (nargs < 1) {
            return luaL_error(L, "json_object_ordered_encode requires 1 argument: json_value");
        }

        // 从 Lua 栈获取 JsonValue
        JsonValue json_val = get_json_value_from_lua(L, 1);

        // 直接使用 json_encode 进行序列化（紧凑格式）
        std::string json_str = mc::json_wrapper::json_encode(json_val, false);

        // 将结果推入 Lua 栈
        lua_pushlstring(L, json_str.data(), json_str.size());

        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "json_object_ordered_encode failed: %s", e.what());
    }
}

// Lua 接口：将 JsonValue 对象编码为格式化的 JSON 字符串
// 接受 JsonValue userdata，将其编码为格式化的 JSON 字符串（带缩进）
static int ljson_object_ordered_encode_pretty(lua_State* L) {
    try {
        int nargs = lua_gettop(L);
        if (nargs < 1) {
            return luaL_error(L, "json_object_ordered_encode_pretty requires 1 argument: json_value");
        }

        // 从 Lua 栈获取 JsonValue
        JsonValue json_val = get_json_value_from_lua(L, 1);

        // 直接使用 json_encode 进行序列化（格式化输出）
        std::string json_str = mc::json_wrapper::json_encode(json_val, true);

        // 将结果推入 Lua 栈
        lua_pushlstring(L, json_str.data(), json_str.size());

        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "json_object_ordered_encode_pretty failed: %s", e.what());
    }
}

// Lua 接口：将 Lua 值编码为 JSON 字符串并写入文件
static int ldump(lua_State* L) {
    try {
        int nargs = lua_gettop(L);
        if (nargs < 2) {
            return luaL_error(L, "dump requires 2 arguments: value, file_path");
        }

        // 使用 build_json_from_lua 将 Lua 值转换为 JsonValue（只使用 C++ 接口）
        JsonValue json_val = build_json_from_lua(L, 1);

        // 从 Lua 栈获取文件路径
        if (!lua_isstring(L, 2)) {
            return luaL_error(L, "dump argument 2 must be string (file_path)");
        }

        size_t      path_len = 0;
        const char* path_str = lua_tolstring(L, 2, &path_len);
        mc::filesystem::path file_path(std::string(path_str, path_len));

        // 使用 json_wrapper::dump(JsonValue&, bool) 保存到文件（紧凑格式）
        bool success = mc::json_wrapper::dump(json_val, file_path, false);

        // 将结果推入 Lua 栈
        lua_pushboolean(L, success ? 1 : 0);

        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "dump failed: %s", e.what());
    }
}

// Lua 接口：将 Lua 值编码为格式化的 JSON 字符串并写入文件
static int ldump_pretty(lua_State* L) {
    try {
        int nargs = lua_gettop(L);
        if (nargs < 2) {
            return luaL_error(L, "dump_pretty requires 2 arguments: value, file_path");
        }

        // 使用 build_json_from_lua 将 Lua 值转换为 JsonValue（只使用 C++ 接口）
        JsonValue json_val = build_json_from_lua(L, 1);

        // 从 Lua 栈获取文件路径
        if (!lua_isstring(L, 2)) {
            return luaL_error(L, "dump_pretty argument 2 must be string (file_path)");
        }

        size_t      path_len = 0;
        const char* path_str = lua_tolstring(L, 2, &path_len);
        mc::filesystem::path file_path(std::string(path_str, path_len));

        // 使用 json_wrapper::dump(JsonValue&, bool) 保存到文件（格式化输出）
        bool success = mc::json_wrapper::dump(json_val, file_path, true);

        // 将结果推入 Lua 栈
        lua_pushboolean(L, success ? 1 : 0);

        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "dump_pretty failed: %s", e.what());
    }
}

// Lua 接口：将 JsonValue 对象序列化到文件
// 接受 JsonValue userdata，将其序列化到文件（紧凑格式）
static int ljson_object_dump(lua_State* L) {
    try {
        int nargs = lua_gettop(L);
        if (nargs < 2) {
            return luaL_error(L, "json_object_dump requires 2 arguments: json_value, file_path");
        }

        // 从 Lua 栈获取 JsonValue
        JsonValue json_val = get_json_value_from_lua(L, 1);

        // 从 Lua 栈获取文件路径
        if (!lua_isstring(L, 2)) {
            return luaL_error(L, "json_object_dump argument 2 must be string (file_path)");
        }

        size_t      path_len = 0;
        const char* path_str = lua_tolstring(L, 2, &path_len);
        mc::filesystem::path file_path(std::string(path_str, path_len));

        // 使用 json_wrapper::dump(JsonValue&, bool) 保存到文件（紧凑格式）
        bool success = mc::json_wrapper::dump(json_val, file_path, false);

        // 将结果推入 Lua 栈
        lua_pushboolean(L, success ? 1 : 0);

        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "json_object_dump failed: %s", e.what());
    }
}

// Lua 接口：将 JsonValue 对象序列化为格式化的 JSON 输出到文件
// 接受 JsonValue userdata，将其序列化到文件（格式化输出）
static int ljson_object_dump_pretty(lua_State* L) {
    try {
        int nargs = lua_gettop(L);
        if (nargs < 2) {
            return luaL_error(L, "json_object_dump_pretty requires 2 arguments: json_value, file_path");
        }

        // 从 Lua 栈获取 JsonValue
        JsonValue json_val = get_json_value_from_lua(L, 1);

        // 从 Lua 栈获取文件路径
        if (!lua_isstring(L, 2)) {
            return luaL_error(L, "json_object_dump_pretty argument 2 must be string (file_path)");
        }

        size_t      path_len = 0;
        const char* path_str = lua_tolstring(L, 2, &path_len);
        mc::filesystem::path file_path(std::string(path_str, path_len));

        // 使用 json_wrapper::dump(JsonValue&, bool) 保存到文件（格式化输出）
        bool success = mc::json_wrapper::dump(json_val, file_path, true);

        // 将结果推入 Lua 栈
        lua_pushboolean(L, success ? 1 : 0);

        return 1;
    } catch (const mc::exception& e) {
        return luaL_error(L, "json_object_dump_pretty failed: %s", e.what());
    }
}

// Lua 接口：设置空 table 编码为 JSON 对象还是数组
// true: 空 table 编码为 {}，false: 空 table 编码为 []（默认）
static int lencode_empty_table_as_object(lua_State* L) {
    try {
        int nargs = lua_gettop(L);
        if (nargs < 1) {
            return luaL_error(L, "encode_empty_table_as_object requires 1 argument: boolean");
        }

        // 检查参数类型
        if (!lua_isboolean(L, 1)) {
            return luaL_error(L, "encode_empty_table_as_object argument 1 must be boolean");
        }

        // 设置全局标志位
        g_encode_empty_table_as_object = (lua_toboolean(L, 1) != 0);

        return 0;  // 无返回值
    } catch (const mc::exception& e) {
        return luaL_error(L, "encode_empty_table_as_object failed: %s", e.what());
    }
}

static const luaL_Reg methods[] = {
    {"encode", lencode},
    {"decode", ldecode},
    {"dump", ldump},
    {"dump_pretty", ldump_pretty},
    {"encode_empty_table_as_object", lencode_empty_table_as_object},
    {"json_object_ordered_decode", ljson_object_ordered_decode},
    {"json_object_ordered_encode", ljson_object_ordered_encode},
    {"json_object_ordered_encode_pretty", ljson_object_ordered_encode_pretty},
    {"json_object_dump", ljson_object_dump},
    {"json_object_dump_pretty", ljson_object_dump_pretty},
    {"json_object_get_keys", l_json_object_get_keys},
    {"json_object_is_equal", l_json_object_is_equal},
    {"json_object_to_table", l_json_object_to_table},
    {"json_object_from_table", l_json_object_from_table},
    {"json_object_copy", l_json_object_copy},
    {"json_object_is_object", l_json_object_is_object},
    {"json_object_is_array", l_json_object_is_array},
    {"json_object_new_object", l_json_object_new_object},
    {"json_object_new_array", l_json_object_new_array},
    {"to_raw", l_to_raw},
    {"new_from_raw", l_new_from_raw},
    {nullptr, nullptr}};

} // namespace lua
} // namespace json_wrapper
} // namespace mc

extern "C" {

__attribute__((visibility("default"))) int luaopen_ljson(lua_State* L) {
    using namespace mc::json_wrapper::lua;

    // 注册 metatable
    register_json_value_metatable(L);

    // 创建模块表
    luaL_checkversion(L);
    luaL_newlib(L, methods);

    return 1;
}

} // extern "C"
