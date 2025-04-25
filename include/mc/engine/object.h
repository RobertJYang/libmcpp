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

#ifndef MC_ENGINE_OBJECT_H
#define MC_ENGINE_OBJECT_H

#include <mc/db/object.h>
#include <mc/engine/interface.h>
#include <mc/exception.h>
#include <mc/log.h>
#include <mc/reflect.h>
#include <mc/traits.h>

#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace mc::engine {

namespace detail {

template <typename ObjectType>
struct object_metadata {
public:
    using property_info          = const mc::reflect::property_info_base<ObjectType>;
    using name_to_interface_info = std::unordered_map<std::string_view, property_info*>;

    static object_metadata& get_instance() {
        static object_metadata instance;
        return instance;
    }

    static const auto& get_static_interface_infos() {
        static auto interfaces = mc::reflect::detail::filter_members<detail::filter_interface>(
            mc::reflect::reflector<ObjectType>::get_properties());
        return interfaces;
    }

    const name_to_interface_info& get_interface_infos() const {
        return m_name_to_interface;
    }

    property_info* get_interface_info(std::string_view name) const {
        auto it = m_iname_to_interface.find(name);
        if (it != m_iname_to_interface.end()) {
            return it->second;
        }

        auto it2 = m_name_to_interface.find(name);
        if (it2 != m_name_to_interface.end()) {
            return it2->second;
        }

        return nullptr;
    }

    property_info* get_property_interface(std::string_view name,
                                          std::string_view interface_name) const {
        if (!interface_name.empty()) {
            return get_interface_info(interface_name);
        }

        auto it = m_name_to_property.find(name);
        if (it == m_name_to_property.end()) {
            return nullptr;
        }

        return it->second;
    }

    property_info* get_method_interface(std::string_view name,
                                        std::string_view interface_name) const {
        if (!interface_name.empty()) {
            return get_interface_info(interface_name);
        }

        auto it = m_name_to_method.find(name);
        if (it == m_name_to_method.end()) {
            return nullptr;
        }
        return it->second;
    }

    property_info* get_signal_interface(std::string_view name,
                                        std::string_view interface_name) const {
        if (!interface_name.empty()) {
            return get_interface_info(interface_name);
        }

        auto it = m_name_to_signal.find(name);
        if (it == m_name_to_signal.end()) {
            return nullptr;
        }
        return it->second;
    }

private:
    object_metadata() {
        mc::traits::tuple_for_each(get_static_interface_infos(), [&](auto& member) {
            add_interface(member);
            init_properties(member);
            init_methods(member);
            init_signals(member);
        });
    }

    template <typename PropertyInfo>
    void add_interface(const PropertyInfo& member) {
        using interface_type             = typename PropertyInfo::member_type;
        m_name_to_interface[member.name] = &member;

        auto it = m_iname_to_interface.find(interface_type::interface_name);
        if (it == m_iname_to_interface.end()) {
            m_iname_to_interface[interface_type::interface_name] = &member;
            return;
        }

        auto& old_member = *it->second;
        elog("对象${obj}::${prop}实现的接口${interface}已经在${obj}::${old_prop}实现",
             ("obj", mc::pretty_name<ObjectType>())("prop", member.name)(
                 "interface", interface_type::interface_name)("old_prop", old_member.name));
    }

    template <typename PropertyInfo>
    void init_properties(PropertyInfo& member) {
        using interface_type = typename PropertyInfo::member_type;
        mc::traits::tuple_for_each(interface_type::get_static_properties(), [&](auto& property) {
            if (m_name_to_property.find(property.name) == m_name_to_property.end()) {
                m_name_to_property[property.name] = &member;
            }
        });
    }

    template <typename PropertyInfo>
    void init_methods(PropertyInfo& member) {
        using interface_type = typename PropertyInfo::member_type;
        mc::traits::tuple_for_each(interface_type::get_static_methods(), [&](auto& method) {
            if (m_name_to_method.find(method.name) == m_name_to_method.end()) {
                m_name_to_method[method.name] = &member;
            }
        });
    }

    template <typename PropertyInfo>
    void init_signals(PropertyInfo& member) {
        using interface_type = typename PropertyInfo::member_type;
        mc::traits::tuple_for_each(interface_type::get_static_signals(), [&](auto& signal) {
            if (m_name_to_signal.find(signal.name) == m_name_to_signal.end()) {
                m_name_to_signal[signal.name] = &member;
            }
        });
    }

    name_to_interface_info m_name_to_interface;  // interface 在对象中的属性名到反射信息的映射
    name_to_interface_info m_iname_to_interface; // interface_name 到反射信息的映射
    name_to_interface_info m_name_to_property;   // 属性名到反射信息的映射
    name_to_interface_info m_name_to_method;     // 方法名到反射信息的映射
    name_to_interface_info m_name_to_signal;     // 信号名到反射信息的映射
};

} // namespace detail

template <typename ObjectType>
class object : public mc::db::object<ObjectType>, public object_base {
public:
    using object_type    = ObjectType;
    using object_id_type = typename mc::db::object<ObjectType>::object_id_type;
    using metadata_type  = detail::object_metadata<ObjectType>;
    using property_info  = typename metadata_type::property_info;

    object(object_id_type id = 0) : mc::db::object<ObjectType>(id) {
    }

    virtual ~object() = default;

    static const auto& get_static_interface_infos() {
        return metadata_type::get_static_interface_infos();
    }

    static const auto& get_interface_infos() {
        return metadata_type::get_instance().get_interface_infos();
    }

    static const auto* get_interface_info(std::string_view name) {
        return metadata_type::get_instance().get_interface_info(name);
    }

    void ref() override {
        mc::im::ref_ptr<ObjectType> ref_ptr(static_cast<ObjectType*>(this));
        ref_ptr.detach();
    }

    void unref() override {
        mc::im::ref_ptr<ObjectType> ref_ptr(static_cast<ObjectType*>(this), false);
        ref_ptr.reset();
    }

    const std::string& get_service_name() const override {
        return m_service_name;
    }

    void set_service_name(std::string_view name) override {
        m_service_name = name;
    }

    const std::string& get_object_name() const override {
        return m_object_name;
    }

    void set_object_name(std::string_view name) override {
        m_object_name = name;
    }

    const std::string& get_object_path() const override {
        return m_object_path;
    }

    void set_object_path(std::string_view path) override {
        m_object_path = path;
    }

    bool has_interface(std::string_view interface_name) const override {
        return get_interface_info(interface_name) != nullptr;
    }

    inline interface_base* property_info_to_interface(property_info& info) {
        intptr_t p_obj = reinterpret_cast<intptr_t>(static_cast<ObjectType*>(this));
        return reinterpret_cast<interface_base*>(p_obj + info.offset());
    }

    interface_base* get_interface(std::string_view interface_name) override {
        auto info = metadata_type::get_instance().get_interface_info(interface_name);
        if (info == nullptr) {
            return nullptr;
        }

        return property_info_to_interface(*info);
    }

    mc::variant get_property(std::string_view property_name,
                             std::string_view interface_name = {}) override {
        auto info =
            metadata_type::get_instance().get_property_interface(property_name, interface_name);
        if (info == nullptr) {
            return mc::variant();
        }

        return property_info_to_interface(*info)->get_property(property_name);
    }

    bool set_property(std::string_view property_name, const mc::variant& value,
                      std::string_view interface_name = {}) override {
        auto info =
            metadata_type::get_instance().get_property_interface(property_name, interface_name);
        if (info == nullptr) {
            return false;
        }

        return property_info_to_interface(*info)->set_property(property_name, value);
    }

    mc::variant invoke(std::string_view method_name, const mc::variants& args,
                       std::string_view interface_name = {}) override {
        auto info = metadata_type::get_instance().get_method_interface(method_name, interface_name);
        if (info == nullptr) {
            return mc::variant();
        }

        return property_info_to_interface(*info)->invoke(method_name, args);
    }

    mc::connection_type connect(std::string_view signal_name, slot_type slot,
                                std::string_view interface_name = {}) override {
        auto info = metadata_type::get_instance().get_signal_interface(signal_name, interface_name);
        if (info == nullptr) {
            return mc::connection_type();
        }

        return property_info_to_interface(*info)->connect(signal_name, slot);
    }

    mc::variant emit(std::string_view signal_name, const mc::variants& args,
                     std::string_view interface_name = {}) override {
        auto info = metadata_type::get_instance().get_signal_interface(signal_name, interface_name);
        if (info == nullptr) {
            return mc::variant();
        }

        return property_info_to_interface(*info)->emit(signal_name, args);
    }

protected:
    std::string m_object_name;
    std::string m_object_path;
    std::string m_service_name;
};

} // namespace mc::engine

#endif // MC_ENGINE_OBJECT_H