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
#include <mc/engine/object_metadata.h>
#include <mc/exception.h>
#include <mc/log.h>

namespace mc::engine {

template <typename ObjectType>
class object : public mc::db::object<ObjectType>, public object_base {
public:
    using object_type    = ObjectType;
    using object_id_type = typename mc::db::object<ObjectType>::object_id_type;
    using metadata_type  = object_metadata<ObjectType>;
    using property_info  = typename metadata_type::property_info;

    object(object_id_type id = 0) : mc::db::object<ObjectType>(id) {
        /*
         * 初始化子类对象的属性（interface、property）
         *
         * 这个做法不符合 C++ 对象构造顺序，因为基类先于子类构造，这里强制转换成子类指针，
         * 并直接调用子类属性的方法，当基类构造函数返回后，子类又会重新构造子类属性，所以
         * 必须确保已经在这里设置的属性，不要在该属性的构造函数中覆盖掉。
         *
         * 基类构造函数 -> 设置子类属性的值 -> 子类构造函数 -> 子类属性构造
         *                    |                             |
         *                    v                             v
         *              设置子类属性值                 需要保证子类属性构造
         *                                          不要覆盖前面设置的值
         */
        ObjectType* obj = static_cast<ObjectType*>(this);
        mc::traits::tuple_for_each(get_static_interface_infos(), [obj](auto& member) {
            (obj->*member.member_ptr).set_object(obj);
        });
    }

    virtual ~object() = default;

    service* get_service() const override {
        return m_service;
    }

    void set_service(service& s) override {
        m_service = &s;
    }

    void set_parent(object_base* obj) override {
        m_parent = obj;
    }

    object_base* get_parent() const override {
        return m_parent;
    }

    void add_child(object_base* obj) override {
        obj->set_parent(this);
        ms_children[obj->get_object_path()] = obj;
    }

    void remove_child(object_base* obj) override {
        ms_children.erase(obj->get_object_path());
    }

    const childrens_type& get_childrens() const override {
        return ms_children;
    }

    static metadata_type& get_metadata() {
        return metadata_type::get_instance();
    }

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

    const std::string& get_object_name() const override {
        return m_object_name;
    }

    void set_object_name(std::string_view name) override {
        m_object_name = name;
    }

    const std::string& get_object_path() const override {
        if (m_object_path.empty()) {
            if (get_parent()) {
                m_object_path = get_parent()->get_object_path();
            }
            m_object_path += object_type::path_pattern;
        }
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
            return {};
        }

        return property_info_to_interface(*info)->get_property(property_name);
    }

    mc::dict get_all_properties(std::string_view interface_name) override {
        auto info = metadata_type::get_instance().get_interface_info(interface_name);
        if (info == nullptr) {
            return {};
        }

        return property_info_to_interface(*info)->get_all_properties();
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
        // 跟踪对象调用
        object_call_stack::context object_ctx{m_service, *this};

        auto result = standard_interfaces::invoke(this, method_name, args, interface_name);
        if (!result.is_null()) {
            return result;
        }

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

    void introspect(std::string& xml) override {
        get_metadata().introspect(static_cast<ObjectType&>(*this), xml);
    }

protected:
    mutable std::string m_object_name;
    mutable std::string m_object_path;

    service*       m_service{nullptr};
    object_base*   m_parent{nullptr};
    childrens_type ms_children;
};

} // namespace mc::engine

#endif // MC_ENGINE_OBJECT_H