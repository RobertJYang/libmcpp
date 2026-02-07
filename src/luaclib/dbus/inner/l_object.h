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

#ifndef MC_DBUS_L_OBJECT_H
#define MC_DBUS_L_OBJECT_H
#include "l_interface.h"

extern "C" {
#include <lua.h>
}

namespace mc::dbus::lua {

constexpr const char* OBJECT_METATABLE = "dbus.object";

struct l_object {
    l_object() {
        impl = mc::make_shared<mc::dbus::dynamic_object>();
    }

    mc::shared_ptr<mc::dbus::dynamic_object> impl;
};

// 注册 object 模块的 metatable
void register_object_metatable(lua_State* L);

int new_object_class(lua_State* L);

} // namespace mc::dbus::lua

#endif // MC_DBUS_L_OBJECT_H
