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

#include <mc/core/object.h>
#include <mc/engine/interface.h>
#include <mc/engine/object_metadata.h>
#include <mc/engine/service.h>
#include <mc/exception.h>
#include <mc/log.h>

namespace mc::engine {

template <typename ObjectType>
class object : public abstract_object {
public:
    using object_type   = ObjectType;
    using metadata_type = object_metadata<ObjectType>;
    using property_info = typename metadata_type::property_info;

    object(core_object* parent = nullptr) : abstract_object(parent) {
        /* 初始化子类对象的属性（interface、property）
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
            (obj->*member.member_ptr).set_owner(obj);
        });
    }

    virtual ~object() = default;

    service* get_service() const override {
        return static_cast<service*>(mc::core::object::get_service());
    }

    void set_service(service& s) override {
        mc::core::object::set_service(&s);
    }

    const managed_objects& get_managed_objects() const override {
        return m_managed_objects;
    }

    void add_managed_object(abstract_object* obj) override {
        m_managed_objects[obj->get_object_path()] = obj;
    }

    void remove_managed_object(abstract_object* obj) override {
        m_managed_objects.erase(obj->get_object_path());
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

    std::string_view get_object_name() const override {
        return this->get_name();
    }

    void set_object_name(std::string_view name) override {
        this->set_name(name);
    }

    std::string_view get_object_path() const override {
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

    inline abstract_interface* property_info_to_interface(property_info& info) {
        intptr_t p_obj = reinterpret_cast<intptr_t>(static_cast<ObjectType*>(this));
        return reinterpret_cast<abstract_interface*>(p_obj + info.offset());
    }

    abstract_interface* get_interface(std::string_view interface_name) override {
        if (interface_name.empty()) {
            return nullptr;
        }

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

    property_base* get_property_base(std::string_view property_name,
                                     std::string_view interface_name) override {
        auto info =
            metadata_type::get_instance().get_property_interface(property_name, interface_name);
        if (info == nullptr) {
            return nullptr;
        }

        return property_info_to_interface(*info)->get_property_base(property_name);
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

    invoke_result invoke(std::string_view method_name, const mc::variants& args,
                         std::string_view interface_name = {}) override {
        // 跟踪对象调用
        object_call_stack::context object_ctx{get_service(), *this};

        auto result = standard_interfaces::invoke(this, method_name, args, interface_name);
        if (result.is_valid()) {
            return result;
        }

        auto info = metadata_type::get_instance().get_method_interface(method_name, interface_name);
        if (info == nullptr) {
            return {nullptr, mc::variant()};
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

    void visit(visitor& v) const override {
        get_metadata().visit(static_cast<const ObjectType&>(*this), v);
    }

    void notify_property_changed(const mc::variant& value, const property_base& prop) override {
        if (m_property_changed_signal) {
            (*m_property_changed_signal)(value, prop);
        }
    }

    property_changed_signal& property_changed() override {
        if (m_property_changed_signal == nullptr) {
            m_property_changed_signal = std::make_unique<property_changed_signal>();
        }

        return *m_property_changed_signal;
    }

    static mc::core::ref_ptr<object_type> create() {
        return mc::core::make_ref<object_type>();
    }

protected:
    mutable std::string                      m_object_path;
    managed_objects                          m_managed_objects;
    std::unique_ptr<property_changed_signal> m_property_changed_signal;
};

} // namespace mc::engine

#endif // MC_ENGINE_OBJECT_H