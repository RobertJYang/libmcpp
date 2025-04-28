/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 */

#include "mc/engine/std_interface.h"

namespace mc::engine {
mc::variant properties_interface::get(std::string_view interface_name,
                                      std::string_view property_name) const {
    return m_object->get_property(property_name, interface_name);
}

mc::dict properties_interface::get_all(std::string_view interface_name) const {
    return m_object->get_all_properties(interface_name);
}

void properties_interface::set(std::string_view interface_name, std::string_view property_name,
                               const mc::variant& value) {
    m_object->set_property(property_name, value, interface_name);
}

struct inintrospect_vistor : visitor {
    void handle_interface_begin(object_base& obj, interface_base& iface) override {
        xml_data += "<interface name=\"";
        xml_data += iface.get_interface_name();
        xml_data += "\">";
    }

    void handle_interface_end(object_base& obj, interface_base& iface) override {
        xml_data += "</interface>";
    }

    /*
        <property name="Property" type="i" access="readwrite" />
    */
    void handle(object_base& obj, interface_base& iface, property_meta& info) override {
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
    void handle(object_base& obj, interface_base& iface, method_meta& info) override {
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
    void handle(object_base& obj, interface_base& iface, signal_meta& info) override {
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

    void handle_children(object_base& obj) {
        auto& childs = obj.get_childrens();
        auto& path   = obj.get_object_path();

        for (auto& child : childs) {
            auto& child_path = child.second->get_object_path();
            if (mc::string::starts_with(child_path, path)) {
                auto child_name = child_path.substr(path.size());
                xml_data += "<node name=\"";
                xml_data += child_name;
                xml_data += "\"/>";
            }
        }
    }

    std::string xml_data;
};

std::string introspectable_interface::introspect() const {
    inintrospect_vistor v;
    v.xml_data = "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\" "
                 "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\"><node>";
    m_object->visit(v);
    v.handle_children(*m_object);
    v.xml_data += "</node>";
    return v.xml_data;
}

void peer_interface::ping() const {
}

std::string peer_interface::get_machine_id() const {
    return {};
}

object_manager_interface::objects_type object_manager_interface::get_managed_objects() const {
    return {};
}

} // namespace mc::engine
