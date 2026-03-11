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
 * @file variant_convert.cpp
 * @brief variant 与 Lua 互转的性能测试接口实现
 */

#include "variant_convert.h"

#include <chrono>

#include "../utils/variant_utils.h"
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/variant.h>

// Lua 头文件
extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace mc {
namespace lua {
namespace dft {

namespace {

/**
 * @brief 处理单个参数的转换（lua_to_variant / variant_to_lua）
 * @param L Lua 状态机
 * @return 返回值数量（3个）
 */
int convert_single_variant(::lua_State* L)
{
    // 开始计时：lua_to_variant
    auto    start_lua_to_variant = std::chrono::high_resolution_clock::now();
    variant v                    = mc::lua::lua_to_variant(L, 1);
    auto    end_lua_to_variant   = std::chrono::high_resolution_clock::now();
    auto    lua_to_variant_time  = std::chrono::duration_cast<std::chrono::microseconds>(
                                   end_lua_to_variant - start_lua_to_variant)
                                   .count();

    // lua_to_variant 只读取不修改栈，立即移除原始参数
    lua_remove(L, 1);
    // 栈：[]

    // 开始计时：variant_to_lua
    auto start_variant_to_lua = std::chrono::high_resolution_clock::now();
    mc::lua::variant_to_lua(L, v);
    auto end_variant_to_lua  = std::chrono::high_resolution_clock::now();
    auto variant_to_lua_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                   end_variant_to_lua - start_variant_to_lua)
                                   .count();

    // 栈：[转换后的值]
    // 推送两个耗时数值
    lua_pushnumber(L, static_cast<lua_Number>(lua_to_variant_time));
    lua_pushnumber(L, static_cast<lua_Number>(variant_to_lua_time));
    // 栈：[转换后的值] [time1] [time2]

    // 调整顺序为：[time1] [time2] [转换后的值]
    lua_insert(L, -3); // time2 移到转换值前面
    lua_insert(L, -3); // time1 移到最前面
    // 栈：[time1] [time2] [转换后的值]

    return 3;
}

/**
 * @brief 处理多个参数的转换（lua_to_variants / variants_to_lua）
 * @param L Lua 状态机
 * @param arg_count 参数数量
 * @return 返回值数量（3个）
 */
int convert_multiple_variants(::lua_State* L, int arg_count)
{
    // 开始计时：lua_to_variants
    auto     start_lua_to_variants = std::chrono::high_resolution_clock::now();
    variants vs                    = mc::lua::lua_to_variants(L, 1, arg_count);
    auto     end_lua_to_variants   = std::chrono::high_resolution_clock::now();
    auto     lua_to_variants_time  = std::chrono::duration_cast<std::chrono::microseconds>(
                                    end_lua_to_variants - start_lua_to_variants)
                                    .count();

    // lua_to_variants 只读取不修改栈，立即移除所有原始参数
    for (int i = 0; i < arg_count; ++i) {
        lua_remove(L, 1);
    }
    // 栈：[]

    // 开始计时：variants_to_lua
    auto start_variants_to_lua = std::chrono::high_resolution_clock::now();
    int  pushed_count          = mc::lua::variants_to_lua(L, vs);
    auto end_variants_to_lua   = std::chrono::high_resolution_clock::now();
    auto variants_to_lua_time  = std::chrono::duration_cast<std::chrono::microseconds>(
                                    end_variants_to_lua - start_variants_to_lua)
                                    .count();

    // 将转换后的多个值收集到一个数组 table 中
    // 栈：[转换后的值1] [转换后的值2] ...
    lua_createtable(L, pushed_count, 0);
    // 栈：[转换后的值1] [转换后的值2] ... [新数组]

    // 将所有转换后的值移到数组中
    for (int i = pushed_count; i > 0; --i) {
        lua_insert(L, -2);     // 将值插入到数组前面
        lua_rawseti(L, -2, i); // 设置 array[i] = 值
    }
    // 栈：[数组]

    // 推送两个耗时数值
    lua_pushnumber(L, static_cast<lua_Number>(lua_to_variants_time));
    lua_pushnumber(L, static_cast<lua_Number>(variants_to_lua_time));
    // 栈：[数组] [time1] [time2]

    // 调整顺序为：[time1] [time2] [数组]
    lua_insert(L, -3); // time2 移到最前
    lua_insert(L, -3); // time1 移到最前
    // 栈：[time1] [time2] [数组]

    return 3;
}

} // namespace

} // namespace dft
} // namespace lua
} // namespace mc

// Lua C 模块导出函数必须使用 C 链接
extern "C" {

int convert_between_variant_lua(::lua_State* L)
{
    if (!L) {
        return 0;
    }

    try {
        // 检查参数数量
        int arg_count = lua_gettop(L);
        if (arg_count < 1) {
            luaL_error(L, "需要至少1个参数");
            return 0;
        }

        // 根据参数数量选择转换方式
        if (arg_count == 1) {
            // 单个参数：使用 lua_to_variant / variant_to_lua
            return mc::lua::dft::convert_single_variant(L);
        } else {
            // 多个参数：使用 lua_to_variants / variants_to_lua
            return mc::lua::dft::convert_multiple_variants(L, arg_count);
        }
    } catch (const mc::exception& e) {
        luaL_error(L, "convert_between_variant_lua 失败: %s", e.what());
        return 0;
    } catch (const std::exception& e) {
        luaL_error(L, "convert_between_variant_lua 失败: %s", e.what());
        return 0;
    }
}

} // extern "C"
