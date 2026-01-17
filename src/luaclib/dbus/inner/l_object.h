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

class dynamic_object : public mc::core::object{
public:
    void set_property(std::string intf_name, std::string prop_name, const mc::variant& value);
    mc::variant get_property(std::string intf_name, std::string prop_name) const;
    void set_object_path(std::string_view path);
    void add_interface(mc::shared_ptr<dynamic_interface>);
    mc::shared_ptr<dynamic_interface> get_interface(std::string intf_name) const;

private:
    std::string m_path;
    std::map<std::string, mc::shared_ptr<dynamic_interface>> m_interfaces;
};

// 注册 object 模块的 metatable
void register_object_metatable(lua_State* L);

int new_object_class(lua_State* L);

} // namespace mc::dbus::lua

#endif // MC_DBUS_L_OBJECT_H

