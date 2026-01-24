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

#ifndef MC_MDB_L_PRIVILEGE_H
#define MC_MDB_L_PRIVILEGE_H

extern "C" {
#include <lua.h>
}

namespace mc::mdb::lua {

/**
 * @brief 注册 privilege 常量到 Lua
 * @param L Lua 状态机
 */
void register_privilege_constants(lua_State* L);

/**
 * @brief 注册 privilege 函数到 Lua
 * @param L Lua 状态机
 */
void register_privilege_functions(lua_State* L);

} // namespace mc::mdb::lua

#endif // MC_MDB_L_PRIVILEGE_H
