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

/**
 * @file variant_utils.cpp
 * @brief variant 和 variants 与 Lua 堆栈之间的转换工具函数实现
 */
#include "variant_utils.h"

#include <climits>
#include <cmath>
#include <cstring>

#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/variant/variant_common.h>

// Lua 头文件
extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace mc {
namespace lua {

namespace {

/**
 * @brief 获取 Lua table 的数组部分大小
 * @param L Lua 状态机
 * @param index 堆栈索引
 * @return 数组大小
 */
lua_Integer get_lua_table_length(::lua_State* L, int index)
{
    // Lua 5.3 使用 lua_len（Lua 5.1 的 lua_objlen 已废弃）
    lua_len(L, index);
    lua_Integer len = lua_tointeger(L, -1);
    lua_pop(L, 1);
    return len;
}

/**
 * @brief 判断 Lua number 是否为整数（Lua 5.1 没有 lua_isinteger）
 * @param L Lua 状态机
 * @param index 堆栈索引
 * @return 如果是整数返回 true，否则返回 false
 */
bool is_lua_integer(::lua_State* L, int index)
{
    if (!lua_isnumber(L, index)) {
        return false;
    }
    lua_Number val = lua_tonumber(L, index);
    // 检查是否为整数
    return std::floor(val) == val && val >= static_cast<lua_Number>(INT64_MIN) &&
           val <= static_cast<lua_Number>(INT64_MAX);
}

/**
 * @brief 判断 Lua table 是否为数组（所有键都是连续的正整数，从 1 开始）
 * @param L Lua 状态机
 * @param index 堆栈索引
 * @return 如果是数组返回 true，否则返回 false
 */
bool is_lua_array(::lua_State* L, int index)
{
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
        return true; // 空 table 视为数组
    }

    // 检查从 1 到 array_size 的键是否都存在且值不为 nil
    for (lua_Integer i = 1; i <= array_size; ++i) {
        // Lua 5.1 使用 lua_rawgeti
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

int variant_to_lua(::lua_State* L, const variant& v)
{
    if (!L) {
        MC_THROW(mc::exception, "Lua state is null");
    }

    try {
        switch (v.get_type()) {
            case type_id::null_type:
                lua_pushnil(L);
                break;

            case type_id::bool_type:
                lua_pushboolean(L, v.as_bool() ? 1 : 0);
                break;

            case type_id::int8_type:
            case type_id::int16_type:
            case type_id::int32_type:
            case type_id::int64_type:
                lua_pushinteger(L, v.as_int64());
                break;

            case type_id::uint8_type:
            case type_id::uint16_type:
            case type_id::uint32_type:
            case type_id::uint64_type: {
                uint64_t val = v.as_uint64();
                // Lua 5.1 没有 LUA_MAXINTEGER，直接使用 number
                lua_pushnumber(L, static_cast<lua_Number>(val));
                break;
            }

            case type_id::double_type:
                lua_pushnumber(L, v.as_double());
                break;

            case type_id::string_type: {
                const std::string& str = v.get_string();
                lua_pushlstring(L, str.data(), str.size());
                break;
            }

            case type_id::array_type: {
                const variants& arr = v.get_array();
                lua_createtable(L, static_cast<int>(arr.size()), 0);
                for (size_t i = 0; i < arr.size(); ++i) {
                    variant_to_lua(L, arr[i]);
                    lua_rawseti(L, -2, static_cast<int>(i + 1)); // Lua 数组索引从 1 开始
                }
                break;
            }

            case type_id::object_type: {
                const dict& obj = v.get_object();
                lua_createtable(L, 0, static_cast<int>(obj.size()));
                for (const auto& entry : obj) {
                    variant_to_lua(L, entry.key);
                    variant_to_lua(L, entry.value);
                    lua_settable(L, -3);
                }
                break;
            }

            case type_id::blob_type: {
                const blob& b = v.get_blob();
                lua_pushlstring(L, b.data.data(), b.data.size());
                break;
            }

            case type_id::extension_type: {
                // extension 类型转换为字符串表示
                std::string str = v.as_string();
                lua_pushlstring(L, str.data(), str.size());
                break;
            }

            default:
                MC_THROW(mc::exception, "unsupported variant type: ${type}",
                         ("type", get_type_name_internal(v.get_type())));
        }

        return 1;
    } catch (const mc::exception& e) {
        MC_THROW(mc::exception, "failed to convert variant to Lua: ${error}", ("error", e.what()));
    }
}

int variants_to_lua(::lua_State* L, const variants& vs)
{
    if (!L) {
        MC_THROW(mc::exception, "Lua state is null");
    }

    try {
        for (size_t i = 0; i < vs.size(); ++i) {
            variant_to_lua(L, vs[i]);
        }
        return static_cast<int>(vs.size());
    } catch (const mc::exception& e) {
        MC_THROW(mc::exception, "failed to convert variants to Lua: ${error}", ("error", e.what()));
    }
}

variant lua_to_variant(::lua_State* L, int index)
{
    if (!L) {
        MC_THROW(mc::exception, "Lua state is null");
    }

    // 规范化索引（转换为正数）
    int abs_index = (index < 0) ? lua_gettop(L) + index + 1 : index;
    if (abs_index < 1 || abs_index > lua_gettop(L)) {
        MC_THROW(mc::exception, "invalid Lua stack index: ${index}", ("index", index));
    }

    try {
        int type = lua_type(L, index);

        switch (type) {
            case LUA_TNIL:
                return variant();

            case LUA_TBOOLEAN:
                return variant(lua_toboolean(L, index) != 0);

            case LUA_TNUMBER: {
                // Lua 5.1 只有 number 类型，需要判断是否为整数
                lua_Number val = lua_tonumber(L, index);
                // 检查是否为整数
                if (std::floor(val) == val && val >= static_cast<lua_Number>(INT64_MIN) &&
                    val <= static_cast<lua_Number>(INT64_MAX)) {
                    lua_Integer int_val = static_cast<lua_Integer>(val);
                    // 根据值的大小选择合适的整数类型
                    if (int_val >= INT8_MIN && int_val <= INT8_MAX) {
                        return variant(static_cast<int8_t>(int_val));
                    } else if (int_val >= INT16_MIN && int_val <= INT16_MAX) {
                        return variant(static_cast<int16_t>(int_val));
                    } else if (int_val >= INT32_MIN && int_val <= INT32_MAX) {
                        return variant(static_cast<int32_t>(int_val));
                    } else {
                        return variant(static_cast<int64_t>(int_val));
                    }
                } else {
                    return variant(static_cast<double>(val));
                }
            }

            case LUA_TSTRING: {
                size_t      len;
                const char* str = lua_tolstring(L, index, &len);
                return variant(std::string(str, len));
            }

            case LUA_TTABLE: {
                int abs_index = (index < 0) ? lua_gettop(L) + index + 1 : index;
                if (is_lua_array(L, index)) {
                    // 作为数组处理
                    variants    arr;
                    lua_Integer size = get_lua_table_length(L, abs_index);

                    arr.reserve(static_cast<size_t>(size));
                    for (lua_Integer i = 1; i <= size; ++i) {
                        // Lua 5.1 使用 lua_rawgeti
                        lua_rawgeti(L, abs_index, static_cast<int>(i));
                        arr.push_back(lua_to_variant(L, -1));
                        lua_pop(L, 1);
                    }
                    return variant(arr);
                } else {
                    // 作为字典处理
                    dict obj;
                    lua_pushnil(L);
                    while (lua_next(L, abs_index) != 0) {
                        variant key   = lua_to_variant(L, -2);
                        variant value = lua_to_variant(L, -1);
                        obj.insert(key, value);
                        lua_pop(L, 1); // 弹出值，保留键用于下一次迭代
                    }
                    return variant(obj);
                }
            }

            default:
                MC_THROW(mc::exception, "unsupported Lua type: ${type}", ("type", lua_typename(L, type)));
        }
    } catch (const mc::exception& e) {
        MC_THROW(mc::exception, "failed to convert Lua value to variant: ${error}", ("error", e.what()));
    }
}

variants lua_to_variants(::lua_State* L, int start_index, int count)
{
    if (!L) {
        MC_THROW(mc::exception, "Lua state is null");
    }

    // 规范化起始索引
    int abs_start = (start_index < 0) ? lua_gettop(L) + start_index + 1 : start_index;
    if (abs_start < 1 || abs_start > lua_gettop(L)) {
        MC_THROW(mc::exception, "invalid Lua stack start index: ${index}", ("index", start_index));
    }

    try {
        variants vs;

        if (count < 0) {
            // 读取到栈顶
            count = lua_gettop(L) - abs_start + 1;
        }

        if (count > 0) {
            int end_index = abs_start + count - 1;
            if (end_index > lua_gettop(L)) {
                MC_THROW(mc::exception, "invalid Lua stack count: ${count}, top is ${top}",
                         ("count", count)("top", lua_gettop(L)));
            }

            vs.reserve(static_cast<size_t>(count));
            for (int i = abs_start; i <= end_index; ++i) {
                vs.push_back(lua_to_variant(L, i));
            }
        }

        return vs;
    } catch (const mc::exception& e) {
        MC_THROW(mc::exception, "failed to convert Lua values to variants: ${error}", ("error", e.what()));
    }
}

variants lua_to_variants(::lua_State* L, int start_index)
{
    return lua_to_variants(L, start_index, -1);
}

} // namespace lua
} // namespace mc
