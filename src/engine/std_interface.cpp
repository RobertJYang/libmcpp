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

std::string introspectable_interface::introspect() const {
    std::string xml_data =
        "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\" "
        "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\"><node>";
    m_object->introspect(xml_data);
    xml_data += "</node>";
    return xml_data;
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
