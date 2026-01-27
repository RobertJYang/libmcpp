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

#include "l_mdb.h"
#include "l_mdb_service.h"
#include "l_privilege.h"
#include "l_mdb_access.h"

extern "C" {
#include <lualib.h>
}

extern "C" {

// lmdb 模块加载函数
__attribute__((visibility("default"))) int luaopen_lmdb(lua_State* L) {
    using namespace mc::mdb::lua;

    // 创建主模块表
    lua_newtable(L);

    // ===== mdb_service 子模块 =====
    lua_newtable(L);
    register_mdb_service_functions(L);
    lua_setfield(L, -2, "mdb_service");

    // ===== privilege 子模块 =====
    lua_newtable(L);
    register_privilege_functions(L);
    register_privilege_constants(L);
    lua_setfield(L, -2, "privilege");

    // ===== mdb_access 子模块 =====
    lua_newtable(L);
    register_mdb_access_functions(L);
    lua_setfield(L, -2, "mdb_access");

    return 1;
}

} // extern "C"
