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
 * @file module_entry.cpp
 * @brief dft 模块的 Lua C 模块入口
 */

extern "C" {
#include <lauxlib.h>
#include <lua.h>

// 声明模块内的各个功能函数
extern int convert_between_variant_lua(::lua_State* L);

// 模块函数注册表
static const luaL_Reg ldft_functions[] = {
    {"convert_between_variant_lua", convert_between_variant_lua},
    // 后续新增函数在此添加
    {nullptr, nullptr}};

/**
 * @brief ldft 模块的 Lua 入口函数
 * @param L Lua 状态机
 * @return 返回值数量（1个，模块表）
 */
__attribute__((visibility("default")))
int luaopen_ldft(::lua_State* L) {
    // 创建模块表并注册函数
    luaL_newlib(L, ldft_functions);
    return 1;
}

} // extern "C"
