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

#ifndef MC_MDB_L_MDB_SERVICE_H
#define MC_MDB_L_MDB_SERVICE_H

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace mc::mdb::lua {

/**
 * @brief 注册 mdb_service 模块的所有函数到 Lua 栈顶的 table 中
 * @param L Lua 状态机
 */
void register_mdb_service_functions(lua_State* L);

} // namespace mc::mdb::lua

#endif // MC_MDB_L_MDB_SERVICE_H
