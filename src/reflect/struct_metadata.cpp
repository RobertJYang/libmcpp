/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <mc/reflect/metadata.h>
#include <mc/sync/mutex_box.h>
#include <mc/sync/shared_mutex.h>

#define MC_REFLECT_MAX_PROPERTY_COUNT 1000

namespace mc::reflect::detail {

struct struct_metadata::impl {
    using ordered_property_t    = std::vector<const property_type_info*>;
    using property_map_t        = std::unordered_map<std::string_view, const property_type_info*>;
    using method_map_t          = std::unordered_map<std::string_view, const method_type_info*>;
    using base_class_map_t      = std::unordered_map<std::string_view, const base_class_type_info*>;
    using property_offset_map_t = std::unordered_map<size_t, const property_type_info*>;
    using method_offset_map_t   = std::unordered_map<size_t, const method_type_info*>;

    std::string_view name;

    ordered_property_t    ordered_properties; // visit_property 希望保持顺序
    property_map_t        name_to_properties;
    property_offset_map_t offset_to_properties;
    method_map_t          name_to_methods;
    method_offset_map_t   offset_to_methods;
    base_class_map_t      name_to_base_class;
};

struct_metadata::struct_metadata(std::string_view name, size_t property_count)
    : m_impl(std::make_unique<impl>()) {
    m_impl->name = name;
    if (property_count > 0 && property_count <= MC_REFLECT_MAX_PROPERTY_COUNT) {
        m_impl->ordered_properties.reserve(property_count);
    }
}

struct_metadata::~struct_metadata() {
}

std::string_view struct_metadata::get_name() const {
    return m_impl->name;
}

void struct_metadata::add_property_info(const property_type_info* property) {
    m_impl->name_to_properties[property->name]       = property;
    m_impl->offset_to_properties[property->offset()] = property;
    m_impl->ordered_properties.push_back(property);
}

void struct_metadata::add_method_info(const method_type_info* method) {
    m_impl->name_to_methods[method->name]       = method;
    m_impl->offset_to_methods[method->offset()] = method;
}

void struct_metadata::add_base_class_info(const base_class_type_info* base_class) {
    m_impl->name_to_base_class[base_class->name] = base_class;
}

const property_type_info* struct_metadata::get_property_info(std::string_view name) const {
    auto it = m_impl->name_to_properties.find(name);
    return it != m_impl->name_to_properties.end() ? it->second : nullptr;
}

const method_type_info* struct_metadata::get_method_info(std::string_view name) const {
    auto it = m_impl->name_to_methods.find(name);
    return it != m_impl->name_to_methods.end() ? it->second : nullptr;
}

const method_type_info* struct_metadata::get_method_info(size_t offset) const {
    auto it = m_impl->offset_to_methods.find(offset);
    return it != m_impl->offset_to_methods.end() ? it->second : nullptr;
}

const base_class_type_info* struct_metadata::get_base_class_info(std::string_view name) const {
    auto it = m_impl->name_to_base_class.find(name);
    return it != m_impl->name_to_base_class.end() ? it->second : nullptr;
}

const property_type_info* struct_metadata::get_property_info(size_t offset) const {
    auto it = m_impl->offset_to_properties.find(offset);
    return it != m_impl->offset_to_properties.end() ? it->second : nullptr;
}

void struct_metadata::visit_property(const property_visitor_t& visitor) const {
    for (auto& property : m_impl->ordered_properties) {
        visitor(property);
    }
}

void struct_metadata::visit_method(const method_visitor_t& visitor) const {
    for (auto& [_, method] : m_impl->offset_to_methods) {
        visitor(method);
    }
}

void struct_metadata::visit_base_class(const base_class_visitor_t& visitor) const {
    for (auto& [_, base_class] : m_impl->name_to_base_class) {
        visitor(base_class);
    }
}

} // namespace mc::reflect::detail