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

#ifndef MC_DBUS_L_INTERFACE_H
#define MC_DBUS_L_INTERFACE_H
#include <string>
#include <string_view>
#include <mc/variant.h>
#include <mc/core/object.h>
#include <mc/dict.h>
#include <dbus/dbus.h>

extern "C" {
#include <lua.h>
}

namespace mc::dbus::lua {

constexpr const char* INTERFACE_METATABLE = "dbus.interface";

struct method {
    lua_State *L;
    std::string name;
    std::string i_args;
    std::string o_args;
    int32_t cb_ref;
    uint8_t flags;
};

struct property {
    std::string name;
    std::string signatrue;
    mc::variant value;
    bool readonly;
    uint8_t flags;
};

struct signal {
    std::string name;
    std::string signatrue;
    uint8_t flags;
};

class LDBusError : public DBusError {
public:
    LDBusError() {
        dbus_error_init(this);
    }
    ~LDBusError()
    {
        dbus_error_free(this);
    }

    std::string to_string() const
    {
        return std::string(name ? name : "") + ": " + std::string(message ? message : "");
    }
};

class dynamic_interface : public mc::core::object{
public:
    dynamic_interface(std::string_view name);
    
    void add_property(std::string_view name, property prop);
    void add_method(std::string_view name, method m);
    void add_signal(std::string_view name, signal s);
    void set_property(std::string property_name, const mc::variant& value);
    mc::variant get_property(std::string property_name) const;
 	std::string_view get_name() const;

private:
    std::map<std::string, method> m_methods;
    std::string m_interface_name;
    std::map<std::string, property> m_properties;
    std::map<std::string, signal> m_signals;
};

struct l_interface {
    l_interface(std::string_view name) {
        impl = mc::make_shared<dynamic_interface>(name);
    }

    l_interface(mc::shared_ptr<dynamic_interface> intf) : impl(intf) {
    }

    mc::shared_ptr<dynamic_interface> impl;
};

// 注册 interface 模块的 metatable
void register_interface_metatable(lua_State* L);

int new_interface_class(lua_State* L);

} // namespace mc::dbus::lua

#endif // MC_DBUS_L_INTERFACE_H;

