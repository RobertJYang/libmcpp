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

// 恢复 mcapp 作为 DBus Server 所需的 4 个标准接口：
//   - org.freedesktop.DBus.Peer
//   - org.freedesktop.DBus.Properties
//   - org.freedesktop.DBus.Introspectable
//   - org.freedesktop.DBus.ObjectManager
// 单例 + 反射 + object_call_stack 传递对象上下文；dispatcher 在对象反射前
// 优先尝试命中。bmc.kepler.Object.Properties (common_properties) 所依赖的
// service::timeout_call 等 API 已经在新 mcengine 中淘汰，本轮不恢复，后续
// 评估 ref_info/sync_info 迁移完成后再补。

#ifndef MC_ENGINE_STD_INTERFACE_H
#define MC_ENGINE_STD_INTERFACE_H

#include <mc/dict.h>
#include <mc/engine/base.h>
#include <mc/engine/interface.h>
#include <mc/engine/macro.h>
#include <mc/engine/path.h>
#include <mc/signal/signal.h>
#include <mc/string.h>
#include <mc/string_view.h>
#include <mc/variant.h>

#include <map>
#include <optional>
#include <vector>

namespace mc::engine {

constexpr mc::string_view properties_interface_name     = "org.freedesktop.DBus.Properties";
constexpr mc::string_view introspectable_interface_name = "org.freedesktop.DBus.Introspectable";
constexpr mc::string_view peer_interface_name           = "org.freedesktop.DBus.Peer";
constexpr mc::string_view object_manager_interface_name = "org.freedesktop.DBus.ObjectManager";
constexpr mc::string_view common_properties_interface_name = "bmc.kepler.Object.Properties";

namespace std_ifaces {

// 标准接口名
extern MC_API const mc::string properties;
extern MC_API const mc::string introspectable;
extern MC_API const mc::string peer;
extern MC_API const mc::string object_manager;

// 标准方法名
extern MC_API const mc::string get;
extern MC_API const mc::string get_all;
extern MC_API const mc::string set;
extern MC_API const mc::string ping;
extern MC_API const mc::string get_machine_id;
extern MC_API const mc::string introspect;
extern MC_API const mc::string get_managed_objects;

} // namespace std_ifaces

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

    mc::variant get(mc::string_view interface_name, mc::string_view property_name);
    mc::dict    get_all(mc::string_view interface_name);
    void        set(mc::string_view interface_name, mc::string_view property_name, const mc::variant& value);

    mc::signal<void(mc::string, mc::dict, std::vector<mc::string>)> properties_changed;

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

    mc::string introspect();

    // 中间节点 introspect：path 上未注册对象，但其下挂有已注册子对象时，
    // 按 DBus 规范返回仅含下一段节点名的最小 node XML；否则返回空串。
    mc::string introspect_intermediate_node(const service& svc, mc::string_view path);

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

    void       ping();
    mc::string get_machine_id();

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

    using interfaces_type = std::map<mc::string, mc::dict>;
    using objects_type    = std::map<mc::engine::path, interfaces_type>;

    objects_type get_managed_objects();

    mc::signal<void(mc::engine::path, interfaces_type)>         interfaces_added;
    mc::signal<void(mc::engine::path, std::vector<mc::string>)> interfaces_removed;

    static object_manager_interface& get_instance();
};

/*
<interface name="bmc.kepler.Object.Properties">
    <property name="ParentPath" type="s" access="read"/>
    <property name="ObjectName" type="s" access="read"/>
    <property name="ClassName" type="s" access="read"/>
    <property name="ObjectIdentifier" type="(ysss)" access="read"/>
    <method name="GetWithContext">
      <arg type="a{ss}" name="context" direction="in"/>
      <arg type="s" name="interface_name" direction="in"/>
      <arg type="s" name="property_name" direction="in"/>
      <arg type="v" name="value" direction="out"/>
    </method>
    <method name="SetWithContext">
      <arg type="a{ss}" name="context" direction="in"/>
      <arg type="s" name="interface_name" direction="in"/>
      <arg type="s" name="property_name" direction="in"/>
      <arg type="v" name="value" direction="in"/>
    </method>
    <method name="GetAllWithContext">
      <arg type="a{ss}" name="context" direction="in"/>
      <arg type="s" name="interface_name" direction="in"/>
      <arg type="a{sv}" name="properties" direction="out"/>
    </method>
    <method name="GetPropertyDetail">
      <arg type="a{ss}" name="context" direction="in"/>
      <arg type="s" name="interface_name" direction="in"/>
      <arg type="s" name="property_name" direction="in"/>
      <arg type="s" name="detail" direction="out"/>
    </method>
    <method name="GetPrivateProperties">
      <arg type="a{ss}" name="context" direction="in"/>
      <arg type="s" name="detail" direction="out"/>
    </method>
</interface>
*/
struct MC_API common_properties_interface : public mc::engine::interface<common_properties_interface> {
    MC_INTERFACE(common_properties_interface_name)

    ~common_properties_interface() override = default;

    mc::string_view     parent_path() const;
    mc::string_view     object_name() const;
    mc::string_view     class_name() const;
    object_identifier_t object_identifier() const;

    static mc::variant get(mc::string_view property_name);
    static mc::dict    get_all();

    mc::variant   get_with_context(std::map<mc::string, mc::string> context, mc::string_view interface_name,
                                   mc::string_view property_name);
    void          set_with_context(std::map<mc::string, mc::string> context, mc::string_view interface_name,
                                   mc::string_view property_name, const mc::variant& value);
    mc::dict      get_all_with_context(std::map<mc::string, mc::string> context, mc::string_view interface_name);
    mc::string    get_property_detail(std::map<mc::string, mc::string> context, mc::string_view interface_name,
                                      mc::string_view property_name);
    mc::string    get_private_properties(std::map<mc::string, mc::string> context);

    static common_properties_interface& get_instance();
};

class MC_API standard_interfaces {
public:
    struct invoke_hit {
        invoke_result   value;
        mc::string_view result_signature;
    };

    static std::optional<invoke_hit> try_invoke(const service& svc, abstract_object* object, mc::string_view path,
                                                mc::string_view method_name, const mc::variants& args,
                                                mc::string_view interface_name);

    static bool is_standard_interface(mc::string_view interface_name) noexcept;

    // common_properties 接口名
    static constexpr mc::string_view common_properties_name = "bmc.kepler.Object.Properties";
};

} // namespace mc::engine

#endif // MC_ENGINE_STD_INTERFACE_H
