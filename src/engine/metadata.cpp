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

#include <cstring>
#include <new>

#include <mc/engine/base.h>
#include <mc/engine/metadata.h>

using namespace mc::reflect;

namespace mc::engine {

metadata_list::metadata_list(
    std::string_view name, const struct_metadata** obj_metadata, size_t count)
    : class_name(name) {
    array.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        array.push_back(obj_metadata[i]);
    }
}

metadata_list::~metadata_list() {
}

void metadata_list::visit_properties(const visit_properties_type& v) const {
    for (auto* metadata : array) {
        metadata->visit_properties([&](const property_type_info* property) {
            v(property);
            return visit_status::VS_CONTINUE;
        });
    }
}

void metadata_list::visit_methods(const visit_methods_type& v) const {
    for (auto* metadata : array) {
        metadata->visit_methods([&](const method_type_info* method) {
            v(method);
            return visit_status::VS_CONTINUE;
        });
    }
}

void metadata_list::visit_signals(const visit_signals_type& v) const {
    for (auto* metadata : array) {
        metadata->visit_customs([&](const member_info_base* member) {
            if (member->type() == MC_REFLECT_SIGNAL_TYPE) {
                v(static_cast<const signal_type_info*>(member));
            }
            return visit_status::VS_CONTINUE;
        });
    }
}

void metadata_list::visit(metadata_visitor& v) const {
    visit_properties([&](const property_type_info* property) {
        v.handle(property);
    });
    visit_methods([&](const method_type_info* method) {
        v.handle(method);
    });
    visit_signals([&](const signal_type_info* signal) {
        v.handle(signal);
    });
}

const signal_type_info* metadata_list::get_signal_info(std::string_view signal_name) const {
    for (auto* metadata : array) {
        const auto* signal_info = metadata->get_custom_info(signal_name, MC_REFLECT_SIGNAL_TYPE);
        if (signal_info != nullptr) {
            return static_cast<const signal_type_info*>(signal_info);
        }
    }

    return nullptr;
}

const method_type_info* metadata_list::get_method_info(std::string_view method_name) const {
    for (auto* metadata : array) {
        const auto* method_info = metadata->get_method_info(method_name);
        if (method_info != nullptr) {
            return method_info;
        }
    }

    return nullptr;
}

std::string_view metadata_list::get_class_name() const {
    return class_name;
}

const property_type_info* metadata_list::get_property_info(std::string_view property_name) const {
    for (auto* metadata : array) {
        const auto* property_info = metadata->get_property_info(property_name);
        if (property_info != nullptr) {
            return property_info;
        }
    }

    return nullptr;
}

const property_type_info* metadata_list::get_property_info(std::uintptr_t offset) const {
    for (auto* metadata : array) {
        const auto* property_info = metadata->get_property_info(offset);
        if (property_info != nullptr) {
            return property_info;
        }
    }

    return nullptr;
}

const std::vector<const mc::reflect::struct_metadata*>& metadata_list::get_struct_metadata() const {
    return array;
}

template <typename ReflectItem>
auto make_interface_item(const property_type_info* interface, const ReflectItem* item) {
    return interface_item<ReflectItem>{interface, item};
}

// 安全的 interface_item 类型转换辅助函数
template <typename To, typename From>
inline interface_item<To>& interface_item_cast(interface_item<From>& src) noexcept {
    static_assert(sizeof(interface_item<To>) == sizeof(interface_item<From>),
                  "interface_item types must have the same size");
    static_assert(alignof(interface_item<To>) == alignof(interface_item<From>),
                  "interface_item types must have the same alignment");
    return *std::launder(reinterpret_cast<interface_item<To>*>(&src));
}

template <typename To, typename From>
inline const interface_item<To>& interface_item_cast(const interface_item<From>& src) noexcept {
    static_assert(sizeof(interface_item<To>) == sizeof(interface_item<From>),
                  "interface_item types must have the same size");
    static_assert(alignof(interface_item<To>) == alignof(interface_item<From>),
                  "interface_item types must have the same alignment");
    return *std::launder(reinterpret_cast<const interface_item<To>*>(&src));
}

using interface_map_type = std::unordered_map<std::string_view, interface_metadata>;
using members_map_type   = std::unordered_map<std::string_view, interface_item<member_info_base>>;

struct object_metadata::impl {
    impl(std::string_view                     class_name,
         const mc::reflect::struct_metadata** obj_metadatas, size_t count)
        : m_object_metadata(class_name, obj_metadatas, count) {
    }

    ~impl() {
        m_ordered_properties.clear();
        m_ordered_methods.clear();
        m_ordered_signals.clear();
    }

    void load_object_metadata();
    void load_interface_metadata(const interface_metadata* metadata);

    template <typename Member>
    void add_member_info(const property_type_info* interface, const Member* member) {
        if (m_members.find(member->name) != m_members.end()) {
            return;
        }

        auto it = m_members.emplace(member->name, make_interface_item<member_info_base>(interface, member));
        if constexpr (std::is_same_v<Member, property_type_info>) {
            m_ordered_properties.push_front(interface_item_cast<property_type_info>(it.first->second));
        } else if constexpr (std::is_same_v<Member, method_type_info>) {
            m_ordered_methods.push_front(interface_item_cast<method_type_info>(it.first->second));
        } else if constexpr (std::is_same_v<Member, member_info_base>) {
            if (member->type() == MC_REFLECT_SIGNAL_TYPE) {
                m_ordered_signals.push_front(interface_item_cast<signal_type_info>(it.first->second));
            }
        }
    }

    metadata_list      m_object_metadata;
    interface_map_type m_interface; // 对象所有接口的元信息，key 为接口在对象中的属性名和接口类名
    members_map_type   m_members;   // 成员名到反射信息的映射

    // 保持顺序方便遍历
    property_list m_ordered_properties;
    method_list   m_ordered_methods;
    signal_list   m_ordered_signals;
};

void object_metadata::impl::load_object_metadata() {
    m_object_metadata.visit_properties([&](const property_type_info* property) {
        if (!property->has_flags(MC_REFLECT_FLAG_INTERFACE)) {
            add_member_info(nullptr, property);
        }
        return visit_status::VS_CONTINUE;
    });

    m_object_metadata.visit_methods([&](const method_type_info* method) {
        add_member_info(nullptr, method);
        return visit_status::VS_CONTINUE;
    });

    m_object_metadata.visit_signals([&](const member_info_base* member) {
        add_member_info(nullptr, member);
        return visit_status::VS_CONTINUE;
    });
}

void object_metadata::impl::load_interface_metadata(const interface_metadata* iface) {
    m_interface[iface->metadata->get_class_name()] = *iface;
    if (iface->metadata->get_class_name() != iface->interface->name) {
        m_interface[iface->interface->name] = *iface; // 内部反射名和接口名不一致也添加到映射中方便查询
    }

    iface->metadata->visit_properties([&](const property_type_info* property) {
        add_member_info(iface->interface, property);
    });
    iface->metadata->visit_methods([&](const method_type_info* method) {
        add_member_info(iface->interface, method);
    });
    iface->metadata->visit_signals([&](const member_info_base* member) {
        if (member->type() != MC_REFLECT_SIGNAL_TYPE) {
            return;
        }
        add_member_info(iface->interface, member);
    });
}

static const property_type_info* get_interface_property(const struct_metadata& metadata, int32_t index) {
    const mc::reflect::property_list& properties = metadata.get_properties();
    if (index < 0 || index >= properties.size()) {
        return nullptr;
    }

    for (const auto& info : properties) {
        if (info.member->has_flags(MC_REFLECT_FLAG_INTERFACE) && info.member->get_data() == index) {
            return info.member;
        }
    }

    return nullptr;
}

object_metadata::object_metadata(std::string_view class_name, const struct_metadata& metadata,
                                 const metadata_list** iface_metadatas, size_t count) {
    // 先加载对象自身的反射数据，对象会遮盖接口的属性
    const struct_metadata* obj_metadatas[2];
    obj_metadatas[0] = &metadata;
    obj_metadatas[1] = &mc::reflect::reflector<abstract_object>::get_metadata();
    m_impl           = std::make_unique<impl>(class_name, obj_metadatas, 2);
    m_impl->load_object_metadata();
    if (iface_metadatas == nullptr || count == 0) {
        return; // 该对象没有 interface 成员，直接返回
    }

    const auto& properties = metadata.get_properties();
    for (size_t i = 0; i < count; ++i) {
        const auto& struct_metadata = iface_metadatas[i]->get_struct_metadata();
        if (struct_metadata.empty()) {
            continue;
        }

        auto interface_property = get_interface_property(metadata, i);
        if (interface_property == nullptr) {
            continue;
        }

        interface_metadata im{interface_property, iface_metadatas[i]};
        m_impl->load_interface_metadata(&im);
    }
}

object_metadata::~object_metadata() {
}

const metadata_list& object_metadata::get_object_metadata() const {
    return m_impl->m_object_metadata;
}

const interface_type_info* object_metadata::get_interface_info(std::string_view name) const {
    auto it = m_impl->m_interface.find(name);
    if (it == m_impl->m_interface.end()) {
        return nullptr;
    }

    return it->second.interface;
}

const interface_item<property_type_info> object_metadata::get_property_info(
    std::string_view name, std::string_view interface_name) const {
    if (interface_name.empty()) {
        auto it = m_impl->m_members.find(name);
        if (it == m_impl->m_members.end() || !it->second.item->is_property_type()) {
            return {};
        }

        return interface_item_cast<property_type_info>(it->second);
    }

    auto it = m_impl->m_interface.find(interface_name);
    if (it == m_impl->m_interface.end()) {
        return {};
    }

    auto* info = it->second.metadata->get_property_info(name);
    if (info == nullptr) {
        return {};
    }

    return {it->second.interface, info};
}

const interface_item<method_type_info> object_metadata::get_method_info(
    std::string_view name, std::string_view interface_name) const {
    if (interface_name.empty()) {
        auto it = m_impl->m_members.find(name);
        if (it == m_impl->m_members.end() || it->second.item->type() != member_info_type::method) {
            return {};
        }

        return interface_item_cast<method_type_info>(it->second);
    }

    auto it = m_impl->m_interface.find(interface_name);
    if (it == m_impl->m_interface.end()) {
        return {};
    }

    auto* info = it->second.metadata->get_method_info(name);
    if (info == nullptr) {
        return {};
    }

    return {it->second.interface, info};
}

const interface_item<signal_type_info> object_metadata::get_signal_info(
    std::string_view name, std::string_view interface_name) const {
    if (interface_name.empty()) {
        auto it = m_impl->m_members.find(name);
        if (it == m_impl->m_members.end() || it->second.item->type() != MC_REFLECT_SIGNAL_TYPE) {
            return {};
        }

        return interface_item_cast<signal_type_info>(it->second);
    }

    auto it = m_impl->m_interface.find(interface_name);
    if (it == m_impl->m_interface.end()) {
        return {};
    }

    auto* info = it->second.metadata->get_signal_info(name);
    if (info == nullptr) {
        return {};
    }

    return {it->second.interface, info};
}

void object_metadata::visit_properties(const visit_properties_type& v) const {
    for (const auto& property : m_impl->m_ordered_properties) {
        v(property);
    }
}

void object_metadata::visit_methods(const visit_methods_type& v) const {
    for (const auto& method : m_impl->m_ordered_methods) {
        v(method);
    }
}

void object_metadata::visit_signals(const visit_signals_type& v) const {
    for (const auto& signal : m_impl->m_ordered_signals) {
        v(signal);
    }
}

void object_metadata::visit_interfaces(const visit_interfaces_type& v) const {
    for (const auto& [key, interface] : m_impl->m_interface) {
        if (key == interface.metadata->get_class_name()) {
            v(interface); // 我们只遍历 interface 类名作为 key 的场景，内部反射名字和接口名不一致
        }
    }
}

void object_metadata::visit(metadata_visitor& v) const {
    visit_interfaces([&](const interface_metadata& interface) {
        v.handle_interface_begin(interface);
        interface.metadata->visit(v);
        v.handle_interface_end(interface);
    });
}

std::vector<const interface_type_info*> object_metadata::get_interfaces() const {
    std::vector<const interface_type_info*> interfaces;
    visit_interfaces([&](const interface_metadata& interface) {
        interfaces.push_back(interface.interface);
    });
    return interfaces;
}

const property_list& object_metadata::get_propertis() const {
    return m_impl->m_ordered_properties;
}

const method_list& object_metadata::get_methods() const {
    return m_impl->m_ordered_methods;
}

const signal_list& object_metadata::get_signals() const {
    return m_impl->m_ordered_signals;
}

} // namespace mc::engine
