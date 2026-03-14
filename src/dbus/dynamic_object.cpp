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

#include <mc/dbus/dynamic_object.h>
#include <mc/dbus/shm/shm_tree.h>
#include <mc/exception.h>

namespace mc::dbus {

dynamic_interface::dynamic_interface(std::string_view name) : m_interface_name(name)
{}

void dynamic_interface::add_method(std::string_view name, dynamic_method m)
{
    m_methods.emplace(name, std::move(m));
}

void dynamic_interface::add_property(std::string_view name, dynamic_property prop)
{
    std::lock_guard<mc::sync::spin_mutex> lock(m_property_access_mutex);
    m_properties.emplace(name, std::move(prop));
}

void dynamic_interface::add_signal(std::string_view name, dynamic_signal s)
{
    m_signals.emplace(name, std::move(s));
}

bool dynamic_interface::set_property(std::string property_name, const mc::variant& value)
{
    std::lock_guard<mc::sync::spin_mutex> lock(m_property_access_mutex);
    auto                                  it = m_properties.find(property_name);
    if (it == m_properties.end()) {
        return false;
    }
    dynamic_property& prop = m_properties.at(property_name);
    if (prop.readonly) {
        MC_THROW(mc::exception, "property is read-only");
    }
    prop.value     = value;
    auto inner_ptr = prop.shm_prop.lock();
    if (inner_ptr) {
        shm_tree::set_property_inner(inner_ptr, value);
    }
    return true;
}

mc::variant dynamic_interface::get_property(std::string property_name) const
{
    std::lock_guard<mc::sync::spin_mutex> lock(m_property_access_mutex);
    auto                                  it = m_properties.find(property_name);
    if (it == m_properties.end()) {
        return {};
    }
    const dynamic_property& prop = it->second;
    return prop.value;
}

std::string_view dynamic_interface::get_name() const
{
    return m_interface_name;
}

bool dynamic_interface::has_property(std::string property_name) const
{
    std::lock_guard<mc::sync::spin_mutex> lock(m_property_access_mutex);
    return m_properties.find(property_name) != m_properties.end();
}

mc::dict dynamic_interface::get_all_properties() const
{
    std::lock_guard<mc::sync::spin_mutex> lock(m_property_access_mutex);
    mc::dict                              dict;
    for (const auto& prop : m_properties) {
        dict[prop.first] = prop.second.value;
    }
    return dict;
}

void dynamic_interface::update_shm_prop(std::string_view property_name, const mc::variant& value)
{
    std::lock_guard<mc::sync::spin_mutex> lock(m_property_access_mutex);
    auto                                  it = m_properties.find(std::string(property_name));
    if (it == m_properties.end()) {
        return;
    }
    auto inner_ptr = it->second.shm_prop.lock();
    if (!inner_ptr) {
        return;
    }
    shm_tree::set_property_inner(inner_ptr, value);
}

std::map<std::string, dynamic_property>& dynamic_interface::get_properties()
{
    return m_properties;
}

dynamic_object::dynamic_object(mc::core::object* parent) : mc::engine::object_impl(parent), m_metadata(nullptr)
{}

bool dynamic_object::set_property(std::string_view property_name, const mc::variant& value,
                                  std::string_view interface_name)
{
    auto it = m_interfaces.find(std::string(interface_name));
    if (it == m_interfaces.end()) {
        return false;
    }
    return it->second->set_property(std::string(property_name), value);
}

mc::variant dynamic_object::get_property(std::string_view property_name, std::string_view interface_name,
                                         int options) const
{
    MC_UNUSED(options);
    auto it = m_interfaces.find(std::string(interface_name));
    if (it == m_interfaces.end()) {
        return {};
    }
    return it->second->get_property(std::string(property_name));
}

mc::dict dynamic_object::get_all_properties(std::string_view interface_name, int options) const
{
    MC_UNUSED(options);
    auto it = m_interfaces.find(std::string(interface_name));
    if (it == m_interfaces.end()) {
        return {};
    }
    return it->second->get_all_properties();
}

bool dynamic_object::has_property(std::string_view property_name, std::string_view interface_name) const
{
    auto it = m_interfaces.find(std::string(interface_name));
    if (it == m_interfaces.end()) {
        return false;
    }
    return it->second->has_property(std::string(property_name));
}

std::tuple<int, mc::variant> dynamic_object::try_get_property(std::string_view property_name,
                                                              std::string_view interface_name) const
{
    auto it = m_interfaces.find(std::string(interface_name));
    if (it == m_interfaces.end()) {
        return {static_cast<int>(access_property_rsp_code::interface_not_exist_err), {}};
    }
    try {
        mc::variant value = it->second->get_property(std::string(property_name));
        if (value.is_null()) {
            return {static_cast<int>(access_property_rsp_code::property_not_exist_err), {}};
        }
        return {static_cast<int>(access_property_rsp_code::success), value};
    } catch (...) {
        return {static_cast<int>(access_property_rsp_code::property_invalid_err), {}};
    }
}

int dynamic_object::try_set_property(std::string_view property_name, const mc::variant& value,
                                     std::string_view interface_name)
{
    auto it = m_interfaces.find(std::string(interface_name));
    if (it == m_interfaces.end()) {
        return static_cast<int>(access_property_rsp_code::interface_not_exist_err);
    }
    try {
        bool ret = it->second->set_property(std::string(property_name), value);
        if (!ret) {
            return static_cast<int>(access_property_rsp_code::property_not_exist_err);
        }
        return static_cast<int>(access_property_rsp_code::success);
    } catch (...) {
        return static_cast<int>(access_property_rsp_code::set_property_unknown_err);
    }
}

void dynamic_object::add_interface(mc::shared_ptr<dynamic_interface> interface)
{
    m_interfaces.emplace(interface->get_name(), interface);
}

mc::shared_ptr<dynamic_interface> dynamic_object::get_interface(std::string intf_name) const
{
    auto it = m_interfaces.find(intf_name);
    if (it == m_interfaces.end()) {
        return nullptr;
    }
    return it->second;
}

std::string_view dynamic_object::get_class_name() const
{
    return "";
}

std::string_view dynamic_object::get_path_pattern() const
{
    return "*";
}

const mc::engine::object_metadata& dynamic_object::get_metadata() const
{
    if (!m_metadata) {
        if (!m_reflect_metadata) {
            m_reflect_metadata = std::make_unique<mc::reflect::struct_metadata>("", -1);
        }
        m_metadata = std::make_unique<mc::engine::object_metadata>("", *m_reflect_metadata);
    }
    return *m_metadata;
}

void dynamic_object::update_shm_prop(std::string_view property_name, const mc::variant& value,
                                     std::string_view interface_name)
{
    auto it = m_interfaces.find(std::string(interface_name));
    if (it == m_interfaces.end()) {
        return;
    }
    it->second->update_shm_prop(property_name, value);
}

std::map<std::string, mc::shared_ptr<dynamic_interface>>& dynamic_object::get_interfaces()
{
    return m_interfaces;
}

} // namespace mc::dbus