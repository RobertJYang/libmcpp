/*
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

#include <mc/engine/property/ref_object.h>
#include <mc/engine/object.h>
#include <mc/engine/interface.h>

namespace mc::engine {

ref_object::ref_object(const std::string& object_name, object_finder_type finder)
    : m_object_name(object_name), m_object_finder(finder) {
}

mc::variant ref_object::get_property(const std::string_view property_name) const {
    auto* target_object = find_related_object();
    if (target_object == nullptr) {
        MC_THROW(mc::invalid_op_exception, "get_property failed, reference object not found: ${object_name}", ("object_name", m_object_name));
    }
    return target_object->get_property(property_name);
}

mc::variant ref_object::get_property(const std::string_view interface_name, const std::string_view property_name) const {
    auto* target_object = find_related_object();
    if (target_object == nullptr) {
        MC_THROW(mc::invalid_op_exception, "get_property failed, reference object not found: ${object_name}", ("object_name", m_object_name));
    }

    if (!interface_name.empty()) {
        auto interface_obj = target_object->get_interface(interface_name);
        if (interface_obj == nullptr) {
            MC_THROW(mc::invalid_op_exception, "Interface not found: ${interface} in object: ${object_name}",
                     ("interface", interface_name)("object_name", m_object_name));
        }
        return interface_obj->get_property(property_name);
    }

    return target_object->get_property(property_name);
}

void ref_object::set_property(const std::string_view property_name, const mc::variant& value) const {
    auto* target_object = find_related_object();
    if (target_object == nullptr) {
        MC_THROW(mc::invalid_op_exception, "set_property failed, reference object not found: ${object_name}", ("object_name", m_object_name));
    }
    target_object->set_property(property_name, value);
}

void ref_object::set_property(const std::string_view interface_name, const std::string_view property_name, const mc::variant& value) const {
    auto* target_object = find_related_object();
    if (target_object == nullptr) {
        MC_THROW(mc::invalid_op_exception, "set_property failed, reference object not found: ${object_name}", ("object_name", m_object_name));
    }

    if (!interface_name.empty()) {
        auto interface_obj = target_object->get_interface(interface_name);
        if (interface_obj == nullptr) {
            MC_THROW(mc::invalid_op_exception, "Interface not found: ${interface} in object: ${object_name}",
                     ("interface", interface_name)("object_name", m_object_name));
        }
        interface_obj->set_property(property_name, value);
    } else {
        target_object->set_property(property_name, value);
    }
}

invoke_result ref_object::invoke(std::string_view method_name, const mc::variants& args) {
    auto* target_object = find_related_object();
    if (target_object == nullptr) {
        MC_THROW(mc::invalid_op_exception, "reference object not found: ${object_name}", ("object_name", m_object_name));
    }

    return target_object->invoke(method_name, args);
}

invoke_result ref_object::invoke(const std::string& interface_name, std::string_view method_name, const mc::variants& args) {
    auto* target_object = find_related_object();
    if (target_object == nullptr) {
        MC_THROW(mc::invalid_op_exception, "reference object not found: ${object_name}", ("object_name", m_object_name));
    }

    if (!interface_name.empty()) {
        auto interface_obj = target_object->get_interface(interface_name);
        if (interface_obj == nullptr) {
            MC_THROW(mc::invalid_op_exception, "Interface not found: ${interface} in object: ${object_name}",
                     ("interface", interface_name)("object_name", m_object_name));
        }
        return interface_obj->invoke(method_name, args);
    }

    return target_object->invoke(method_name, args);
}

async_result ref_object::async_invoke(std::string_view method_name, const mc::variants& args) {
    auto* target_object = find_related_object();
    if (target_object == nullptr) {
        MC_THROW(mc::invalid_op_exception, "reference object not found: ${object_name}", ("object_name", m_object_name));
    }

    return target_object->async_invoke(method_name, args);
}

async_result ref_object::async_invoke(const std::string& interface_name, std::string_view method_name, const mc::variants& args) {
    auto* target_object = find_related_object();
    if (target_object == nullptr) {
        MC_THROW(mc::invalid_op_exception, "reference object not found: ${object_name}", ("object_name", m_object_name));
    }

    if (!interface_name.empty()) {
        auto interface_obj = target_object->get_interface(interface_name);
        if (interface_obj == nullptr) {
            MC_THROW(mc::invalid_op_exception, "Interface not found: ${interface} in object: ${object_name}",
                     ("interface", interface_name)("object_name", m_object_name));
        }
        return interface_obj->async_invoke(method_name, args);
    }

    return target_object->async_invoke(method_name, args);
}

const std::string& ref_object::get_object_name() const {
    return m_object_name;
}

bool ref_object::is_valid() const {
    return find_related_object() != nullptr;
}

abstract_object* ref_object::get_object() const {
    return find_related_object();
}

std::string ref_object::as_string() const {
    return m_object_name;
}

bool ref_object::equals(const variant_extension_base& other) const {
    if (auto* other_ref = dynamic_cast<const ref_object*>(&other)) {
        return m_object_name == other_ref->m_object_name;
    }
    return false;
}

mc::shared_ptr<variant_extension_base> ref_object::copy() const {
    return mc::make_shared<ref_object>(m_object_name, m_object_finder);
}

std::string_view ref_object::get_type_name() const {
    return "ref_object";
}

abstract_object* ref_object::find_related_object() const {
    if (m_object_finder) {
        return m_object_finder(m_object_name);
    }
    return nullptr;
}

} // namespace mc::engine