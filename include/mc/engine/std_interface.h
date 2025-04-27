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

#include <mc/dbus/path.h>
#include <mc/dict.h>
#include <mc/engine/base.h>
#include <mc/engine/interface.h>
#include <mc/engine/macro.h>
#include <mc/signal_slot.h>
#include <mc/variant.h>

namespace mc::engine {
constexpr std::string_view properties_interface_name     = "org.freedesktop.DBus.Properties";
constexpr std::string_view introspectable_interface_name = "org.freedesktop.DBus.Introspectable";
constexpr std::string_view peer_interface_name           = "org.freedesktop.DBus.Peer";
constexpr std::string_view object_manager_interface_name = "org.freedesktop.DBus.ObjectManager";

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
struct properties_interface : public mc::engine::interface<properties_interface> {
    MC_INTERFACE(properties_interface_name)

    ~properties_interface() override = default;

    mc::variant get(std::string_view interface_name, std::string_view property_name) const;
    mc::dict    get_all(std::string_view interface_name) const;
    void        set(std::string_view interface_name, std::string_view property_name,
                    const mc::variant& value);

    using properties_changed_signal =
        mc::signal<void(std::string_view, mc::dict, std::vector<std::string>)>;
    properties_changed_signal properties_changed;

    object_base* object;
};

/*
<interface name="org.freedesktop.DBus.Introspectable">
    <method name="Introspect">
      <arg type="s" name="xml_data" direction="out"/>
    </method>
  </interface>
*/
struct introspectable_interface : public mc::engine::interface<introspectable_interface> {
    MC_INTERFACE(introspectable_interface_name)

    ~introspectable_interface() override = default;

    std::string introspect() const;

    object_base* object;
};

/*
<interface name="org.freedesktop.DBus.Peer">
    <method name="Ping"/>
    <method name="GetMachineId">
      <arg type="s" name="machine_uuid" direction="out"/>
    </method>
  </interface>
*/
struct peer_interface : public mc::engine::interface<peer_interface> {
    MC_INTERFACE(peer_interface_name)

    ~peer_interface() override = default;

    void        ping() const;
    std::string get_machine_id() const;

    object_base* object;
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
struct object_manager_interface : public mc::engine::interface<object_manager_interface> {
    MC_INTERFACE(object_manager_interface_name)

    ~object_manager_interface() override = default;

    using interfaces_type = std::map<std::string, mc::dict>;
    using objects_type    = std::map<mc::dbus::path, interfaces_type>;
    objects_type get_managed_objects() const;

    using interfaces_added_signal = mc::signal<void(mc::dbus::path, interfaces_type)>;
    interfaces_added_signal interfaces_added;

    using interfaces_removed_signal = mc::signal<void(mc::dbus::path, std::vector<std::string>)>;
    interfaces_removed_signal interfaces_removed;

    object_base* object;
};

class standard_interfaces {
public:
    static mc::variant invoke(object_base* object, std::string_view method_name,
                              const mc::variants& args, std::string_view interface_name) {
        if (interface_name == properties_interface_name) {
            static thread_local properties_interface properties;
            properties.object = object;
            return properties.invoke(method_name, args);
        } else if (interface_name == introspectable_interface_name) {
            static thread_local introspectable_interface introspectable;
            introspectable.object = object;
            return introspectable.invoke(method_name, args);
        } else if (interface_name == peer_interface_name) {
            static thread_local peer_interface peer;
            peer.object = object;
            return peer.invoke(method_name, args);
        } else if (interface_name == object_manager_interface_name) {
            static thread_local object_manager_interface object_manager;
            object_manager.object = object;
            return object_manager.invoke(method_name, args);
        }

        return {};
    }
};

} // namespace mc::engine

MC_REFLECT(mc::engine::properties_interface,
           ((get, "Get"))((get_all, "GetAll"))((set, "Set"))((properties_changed,
                                                              "PropertiesChanged")))
MC_REFLECT(mc::engine::introspectable_interface, ((introspect, "Introspect")))
MC_REFLECT(mc::engine::peer_interface, ((ping, "Ping"))((get_machine_id, "GetMachineId")))
MC_REFLECT(mc::engine::object_manager_interface,
           ((get_managed_objects, "GetManagedObjects"))((interfaces_added, "InterfacesAdded"))(
               (interfaces_removed, "InterfacesRemoved")))

#endif // MC_ENGINE_STD_INTERFACE_H