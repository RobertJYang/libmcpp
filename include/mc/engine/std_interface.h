/**
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_ENGINE_STD_INTERFACE_H
#define MC_ENGINE_STD_INTERFACE_H

#include <mc/dict.h>
#include <mc/engine/base.h>
#include <mc/engine/interface.h>
#include <mc/engine/macro.h>
#include <mc/engine/path.h>
#include <mc/signal_slot.h>
#include <mc/variant.h>

namespace mc::engine {
constexpr std::string_view properties_interface_name     = "org.freedesktop.DBus.Properties";
constexpr std::string_view introspectable_interface_name = "org.freedesktop.DBus.Introspectable";
constexpr std::string_view peer_interface_name           = "org.freedesktop.DBus.Peer";
constexpr std::string_view object_manager_interface_name = "org.freedesktop.DBus.ObjectManager";
constexpr std::string_view common_properties_name        = "bmc.kepler.Object.Properties"; // 通用属性接口

/*
 <interface name="org.freedesktop.DBus.Properties">
<method name="Get">
  <arg type="s" name="interface_name" direction="in"/>
  <arg type="s" name="property_name" direction="in"/>
  <arg type="v" name="value" direction="out"/>
</method>
<method name="GetAll">
  <arg type="s" name="interface_name" direction="in"/>
  <arg type="a{sv}" name="properties" direction="out"/>
</method>
<method name="Set">
  <arg type="s" name="interface_name" direction="in"/>
  <arg type="s" name="property_name" direction="in"/>
  <arg type="v" name="value" direction="in"/>
</method>
<signal name="PropertiesChanged">
  <arg type="s" name="interface_name"/>
  <arg type="a{sv}" name="changed_properties"/>
  <arg type="as" name="invalidated_properties"/>
</signal>
</interface>
*/
struct MC_API properties_interface : public mc::engine::interface<properties_interface> {
    MC_INTERFACE(properties_interface_name)

    ~properties_interface() override = default;

    mc::variant get(std::string_view interface_name, std::string_view property_name) const;
    mc::dict    get_all(std::string_view interface_name) const;
    void        set(std::string_view interface_name, std::string_view property_name, const mc::variant& value);

    using properties_changed_signal = mc::signal<void(std::string_view, mc::dict, std::vector<std::string>)>;
    properties_changed_signal properties_changed;

    static properties_interface& get_instance();
};

/*
<interface name="org.freedesktop.DBus.Introspectable">
    <method name="Introspect">
      <arg type="s" name="xml_data" direction="out"/>
    </method>
  </interface>
*/
struct MC_API introspectable_interface : public mc::engine::interface<introspectable_interface> {
    MC_INTERFACE(introspectable_interface_name)

    ~introspectable_interface() override = default;

    std::string introspect() const;

    static introspectable_interface& get_instance();
};

/*
<interface name="org.freedesktop.DBus.Peer">
    <method name="Ping"/>
    <method name="GetMachineId">
      <arg type="s" name="machine_uuid" direction="out"/>
    </method>
  </interface>
*/
struct MC_API peer_interface : public mc::engine::interface<peer_interface> {
    MC_INTERFACE(peer_interface_name)

    ~peer_interface() override = default;

    void        ping() const;
    std::string get_machine_id() const;

    static peer_interface& get_instance();
};

/*
<interface name="org.freedesktop.DBus.ObjectManager">
    <method name="GetManagedObjects">
      <arg name="objects" type="a{oa{sa{sv}}}" direction="out"/>
    </method>
    <signal name="InterfacesAdded">
      <arg name="object" type="o"/>
      <arg name="interfaces" type="a{sa{sv}}"/>
    </signal>
    <signal name="InterfacesRemoved">
      <arg name="object" type="o"/>
      <arg name="interfaces" type="as"/>
    </signal>
  </interface>
*/
struct MC_API object_manager_interface : public mc::engine::interface<object_manager_interface> {
    MC_INTERFACE(object_manager_interface_name)

    ~object_manager_interface() override = default;

    using interfaces_type = std::map<std::string_view, mc::dict>;
    using objects_type    = std::map<mc::dbus::path, interfaces_type>;
    objects_type get_managed_objects() const;

    using interfaces_added_signal = mc::signal<void(mc::dbus::path, interfaces_type)>;
    interfaces_added_signal interfaces_added;

    using interfaces_removed_signal = mc::signal<void(mc::dbus::path, std::vector<std::string_view>)>;
    interfaces_removed_signal interfaces_removed;

    static object_manager_interface& get_instance();
};

struct MC_API common_properties_interface : public mc::engine::interface<common_properties_interface> {
    MC_INTERFACE(common_properties_name)

    ~common_properties_interface() override = default;
    std::string_view                                           m_parent_path;
    std::string_view                                           m_object_name;
    std::string_view                                           m_class_name;
    std::tuple<uint8_t, std::string, std::string, std::string> m_object_identifier;

    static mc::variant get(std::string_view property_name);
    static mc::dict    get_all();
    static mc::variant get_with_context(std::map<std::string, std::string> context, std::string_view interface_name,
                                        std::string_view property_name);
    static void        set_with_context(std::map<std::string, std::string> context, std::string_view interface_name,
                                        std::string_view property_name, const mc::variant& value);
    static mc::dict get_all_with_context(std::map<std::string, std::string> context, std::string_view interface_name);
    static std::string get_property_detail(std::map<std::string, std::string> context, std::string_view interface_name,
                                           std::string_view property_name);
    static std::string get_private_properties(std::map<std::string, std::string> context);

    static common_properties_interface& get_instance();
};

class MC_API standard_interfaces {
public:
    static constexpr std::string_view common_prefix          = "org.freedesktop.DBus.";
    static constexpr std::string_view properties_name        = "Properties";
    static constexpr std::string_view introspectable_name    = "Introspectable";
    static constexpr std::string_view peer_name              = "Peer";
    static constexpr std::string_view object_manager_name    = "ObjectManager";
    static constexpr std::string_view common_properties_name = "bmc.kepler.Object.Properties";
    static invoke_result invoke(abstract_object* object, std::string_view method_name, const mc::variants& args,
                                std::string_view interface_name);
};

} // namespace mc::engine

#endif // MC_ENGINE_STD_INTERFACE_H