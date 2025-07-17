/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 */

#include "mc/engine/std_interface.h"
#include <mc/engine/property.h>

namespace mc::engine {
mc::variant properties_interface::get(std::string_view interface_name,
                                      std::string_view property_name) const {
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        return {};
    }
    if (interface_name == common_properties_name) {
        return common_properties_interface::get(property_name);
    }
    return object->get_property(property_name, interface_name, mc::engine::property_options::from_mdb);
}

mc::dict properties_interface::get_all(std::string_view interface_name) const {
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        return {};
    }
    if (interface_name == common_properties_name) {
        return common_properties_interface::get_all();
    }
    return object->get_all_properties(interface_name, mc::engine::property_options::from_mdb);
}

void properties_interface::set(std::string_view interface_name, std::string_view property_name,
                               const mc::variant& value) {
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        return;
    }

    // 通用属性接口不支持修改属性
    if (interface_name == common_properties_name) {
        return;
    }
    object->set_property(property_name, value, interface_name);
}

struct inintrospect_vistor : visitor {
    void handle_interface_begin(const abstract_object&    obj,
                                const abstract_interface& iface) override {
        xml_data += "<interface name=\"";
        xml_data += iface.get_interface_name();
        xml_data += "\">";
    }

    void handle_interface_end(const abstract_object&    obj,
                              const abstract_interface& iface) override {
        xml_data += "</interface>";
    }

    /*
        <property name="Property" type="i" access="readwrite" />
    */
    void handle(const abstract_object& obj, const abstract_interface& iface,
                const property_meta& info) override {
        xml_data += "<property name=\"";
        xml_data += info.name;
        xml_data += "\" type=\"";
        xml_data += info.signature;
        xml_data += "\" access=\"readwrite\" />";
    }

    /*
        <method name="Method">
          <arg type="i" direction="in"/>
          <arg type="s" direction="in"/>
          <arg type="s" direction="out"/>
        </method>
    */
    void handle(const abstract_object& obj, const abstract_interface& iface,
                const method_meta& info) override {
        xml_data += "<method name=\"";
        xml_data += info.name;
        xml_data += "\">";

        auto it      = mc::dbus::signature_iterator(info.args_signature);
        auto args_it = it.get_content_iterator();
        for (size_t index = 0; !args_it.at_end(); args_it.next(), ++index) {
            xml_data += "<arg type=\"";
            xml_data += args_it.current_type();
            xml_data += "\" direction=\"in\" />";
        }

        if (!info.return_signature.empty()) {
            xml_data += "<arg type=\"";
            xml_data += info.return_signature;
            xml_data += "\" direction=\"out\" />";
        }

        xml_data += "</method>";
    }

    /*
        <signal name="ValueChanged">
            <arg type="i" />
            <arg type="i" />
        </signal>
    */
    void handle(const abstract_object& obj, const abstract_interface& iface,
                const signal_meta& info) override {
        xml_data += "<signal name=\"";
        xml_data += info.name;
        xml_data += "\">";

        auto it      = mc::dbus::signature_iterator(info.args_signature);
        auto args_it = it.get_content_iterator();
        for (size_t index = 0; !args_it.at_end(); args_it.next(), ++index) {
            xml_data += "<arg type=\"";
            xml_data += args_it.current_type();
            xml_data += "\" />";
        }

        xml_data += "</signal>";
    }

    void handle_children(abstract_object& obj) {
        auto& objs = obj.get_managed_objects();
        auto  path = obj.get_object_path();

        for (auto& obj : objs) {
            auto        obj_path = obj.second->get_object_path();
            std::string obj_name;
            if (mc::string::starts_with(obj_path, path)) {
                // 如果是子路径，跳过前缀只取后半部分
                obj_name = obj_path.substr(path.size() + 1);
            }

            xml_data += "<node name=\"";
            xml_data += obj_name;
            xml_data += "\"/>";
        }
    }

    std::string xml_data;
};

std::string introspectable_interface::introspect() const {
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        return {};
    }

    inintrospect_vistor v;
    v.xml_data = "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\" "
                 "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\"><node>";
    // 添加通用接口在最前面
    object->visit(v);

    // 添加标准接口
    this->visit(v);
    properties_interface::get_instance().visit(v);
    peer_interface::get_instance().visit(v);
    object_manager_interface::get_instance().visit(v);
    common_properties_interface::get_instance().visit(v);

    v.handle_children(*object);
    v.xml_data += "</node>";
    return v.xml_data;
}

void peer_interface::ping() const {
}

std::string peer_interface::get_machine_id() const {
    return {};
}

struct object_manager_vistor : visitor {
    void handle_interface_begin(const abstract_object&    obj,
                                const abstract_interface& iface) override {
        m_interfaces[std::string(iface.get_interface_name())] = {};
    }

    void handle(const abstract_object& obj, const abstract_interface& iface,
                const property_meta& info) override {
        auto  interface_name = std::string(iface.get_interface_name());
        auto& interface      = m_interfaces[interface_name];
        if (interface_name == common_properties_name) {
            mc::variant value                 = common_properties_interface::get(info.name);
            interface[std::string(info.name)] = value;
        } else {
            mc::variant value                 = iface.get_property(info.name, mc::engine::property_options::memory);
            interface[std::string(info.name)] = value;
        }
    }

    void handle(const mc::engine::abstract_object&      obj,
                const mc::engine::abstract_interface&   iface,
                const mc::engine::visitor::method_meta& info) override {
    }

    void handle(const mc::engine::abstract_object&      obj,
                const mc::engine::abstract_interface&   iface,
                const mc::engine::visitor::signal_meta& info) override {
    }

    void handle_interface_end(const abstract_object&    obj,
                              const abstract_interface& iface) override {
    }

    std::map<std::string, mc::mutable_dict> m_interfaces;
};

object_manager_interface::objects_type
object_manager_interface::get_managed_objects() const {
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        return {};
    }
    auto&        objs = object->get_managed_objects();
    objects_type objects;
    for (auto& [obj_path, obj] : objs) {
        object_manager_vistor v;
        obj->visit(v);
        object_call_stack::context object_ctx{obj->get_service(), *obj};
        common_properties_interface::get_instance().visit(v);
        interfaces_type interfaces;
        for (auto& [name, value] : v.m_interfaces) {
            interfaces[name] = value;
        }
        auto key     = path(std::string(obj_path));
        objects[key] = interfaces;
    }
    return objects;
}

mc::variant common_properties_interface::get(std::string_view property_name) {
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        return {};
    }
    if (property_name == "ParentPath") {
        return object->get_owner()->get_object_path();
    }
    if (property_name == "ObjectName") {
        return object->get_object_name();
    }
    if (property_name == "ClassName") {
        return object->get_class_name();
    }
    if (property_name == "ObjectIdentifier") {
        return object->get_object_identifier();
    }
    return {};
}

mc::variant common_properties_interface::get_with_context(std::map<std::string, std::string> context, std::string_view interface_name,
                                                          std::string_view property_name) {
    if (interface_name == common_properties_name) {
        return get(property_name);
    }
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        return {};
    }
    return object->get_property(property_name, interface_name);
}

mc::dict common_properties_interface::get_all() {
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        return {};
    }
    mc::mutable_dict dict;
    dict["ParentPath"]       = object->get_owner()->get_object_path();
    dict["ObjectName"]       = object->get_object_name();
    dict["ClassName"]        = object->get_class_name();
    dict["ObjectIdentifier"] = object->get_object_identifier();
    return dict;
}

void common_properties_interface::set_with_context(std::map<std::string, std::string> context, std::string_view interface_name,
                                                   std::string_view property_name, const mc::variant& value) {
    if (interface_name == common_properties_name) {
        return;
    }
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        elog("failed to get object from call stack");
        return;
    }
    object->set_property(property_name, value, interface_name);
}

invoke_result standard_interfaces::invoke(abstract_object* object, std::string_view method_name,
                                          const mc::variants& args,
                                          std::string_view    interface_name) {
    // 优化：所有的标准接口都有同样的前缀，前缀不匹配可以快速返回
    if (!mc::string::starts_with(interface_name, common_prefix)) {
        if (interface_name == common_properties_name) {
            return common_properties_interface::get_instance().invoke(method_name, args);
        }
        return {};
    }

    std::string_view name = interface_name.substr(common_prefix.size());
    if (name == properties_name) {
        return properties_interface::get_instance().invoke(method_name, args);
    } else if (name == introspectable_name) {
        return introspectable_interface::get_instance().invoke(method_name, args);
    } else if (name == peer_name) {
        return peer_interface::get_instance().invoke(method_name, args);
    } else if (name == object_manager_name) {
        return object_manager_interface::get_instance().invoke(method_name, args);
    }

    return {};
}
} // namespace mc::engine
