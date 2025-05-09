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
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        return {};
    }

    return object->get_property(property_name, interface_name);
}

mc::dict properties_interface::get_all(std::string_view interface_name) const {
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
        return {};
    }

    return object->get_all_properties(interface_name);
}

void properties_interface::set(std::string_view interface_name, std::string_view property_name,
                               const mc::variant& value) {
    auto* object = object_call_stack::top_value();
    if (object == nullptr) {
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
            auto obj_path = obj.second->get_object_path();
            if (mc::string::starts_with(obj_path, path)) {
                auto obj_name = obj_path.substr(path.size());
                xml_data += "<node name=\"";
                xml_data += obj_name;
                xml_data += "\"/>";
            }
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
    object->visit(v);

    // 添加标准接口
    this->visit(v);
    properties_interface::get_instance().visit(v);
    peer_interface::get_instance().visit(v);
    object_manager_interface::get_instance().visit(v);

    v.handle_children(*object);
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
