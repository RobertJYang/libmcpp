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
#include <mc/exception.h>
#include <mc/expr.h>
#include <mc/log.h>

namespace mc::engine {
using mc::string::starts_with;

class object_impl : public abstract_object {
public:
    object_impl(core_object* parent);
    ~object_impl() override;

    const managed_objects& get_managed_objects() const override;

    void notify_property_changed(const mc::variant& value, const property_base& prop) override;
    property_changed_signal& property_changed() override;

    abstract_object* get_owner() const override;
    void             set_owner(abstract_object* owner) override;

    std::string_view get_object_name() const override;
    void             set_object_name(std::string_view name) override;

    void set_object_path(std::string_view path) override;

    std::string_view get_position() const override;
    void             set_position(std::string_view position) override;

protected:
    void add_managed_object(abstract_object* obj) override;
    void remove_managed_object(abstract_object* obj) override;

protected:
    mutable std::string                      m_object_path;
    mutable std::string                      m_position;
    abstract_object*                         m_owner{nullptr};
    managed_objects                          m_managed_objects;
    std::unique_ptr<property_changed_signal> m_property_changed_signal;
};

template <typename ObjectType>
class object : public object_impl {
public:
    using object_type   = ObjectType;
    using metadata_type = object_metadata<ObjectType>;
    using property_info = typename metadata_type::property_info;

    object(core_object* parent = nullptr) : object_impl(parent) {
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

    std::string_view get_object_path() const override {
        if (!m_object_path.empty()) {
            return m_object_path;
        }

        const_cast<object<ObjectType>*>(this)->set_object_path(
            mc::engine::service::resolve_object_path(object_type::path_pattern, *this));
        return m_object_path;
    }

    std::string_view get_class_name() const override {
        return object_type::class_name;
    }

    bool has_interface(std::string_view interface_name) const override {
        return get_interface_info(interface_name) != nullptr;
    }

    inline abstract_interface* property_info_to_interface(property_info& info) const {
        intptr_t p_obj = reinterpret_cast<intptr_t>(static_cast<const ObjectType*>(this));
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
        // 先检查对象是否存在该属性
        auto object_prop_info = get_object_property_info(property_name, interface_name);
        if (object_prop_info != nullptr) {
            return object_prop_info->get_value(static_cast<object_type&>(*this));
        }

        // 再检查对象实现的接口是否存在该属性
        auto info =
            metadata_type::get_instance().get_property_interface(property_name, interface_name);
        if (info == nullptr) {
            return mc::reflect::get_property<abstract_object>(*this, property_name);
        }

        return property_info_to_interface(*info)->get_property(property_name);
    }

    property_base* get_property_base(std::string_view property_name,
                                     std::string_view interface_name) override {
        // 先检查对象是否存在该属性
        auto object_prop_info = get_object_property_info(property_name, interface_name);
        if (object_prop_info != nullptr &&
            (object_prop_info->flags & MC_ENGINE_PROPERTY_TYPE) != 0) {
            return reinterpret_cast<property_base*>(reinterpret_cast<std::uintptr_t>(this) +
                                                    object_prop_info->offset());
        }

        // 再检查对象实现的接口是否存在该属性
        auto info =
            metadata_type::get_instance().get_property_interface(property_name, interface_name);
        if (info == nullptr) {
            return nullptr;
        }

        return property_info_to_interface(*info)->get_property_base(property_name);
    }

    bool has_property(std::string_view property_name, std::string_view interface_name) override {
        // 先检查对象是否存在该属性
        auto object_prop_info = get_object_property_info(property_name, interface_name);
        if (object_prop_info != nullptr) {
            return true;
        }

        // 再检查对象实现的接口是否存在该属性
        auto info =
            metadata_type::get_instance().get_property_interface(property_name, interface_name);
        if (info == nullptr) {
            return mc::reflect::get_property_info<abstract_object>(property_name) != nullptr;
        }

        return interface_name.empty()
                   ? true
                   : property_info_to_interface(*info)->has_property(property_name);
    }

    mc::dict get_all_properties(std::string_view interface_name = {}) override {
        if (interface_name.empty()) {
            mc::mutable_dict dict;
            to_variant(static_cast<const object_type&>(*this), dict);
            return dict;
        }

        auto info = metadata_type::get_instance().get_interface_info(interface_name);
        if (info == nullptr) {
            return {};
        }

        return property_info_to_interface(*info)->get_all_properties();
    }

    bool set_property(std::string_view property_name, const mc::variant& value,
                      std::string_view interface_name = {}) override {
        // 先检查对象是否存在该属性
        auto object_prop_info = get_object_property_info(property_name, interface_name);
        if (object_prop_info != nullptr) {
            object_prop_info->set_value(static_cast<object_type&>(*this), value);
            return true;
        }

        // 再检查对象实现的接口是否存在该属性
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

        auto method_info = get_object_method_info(method_name, interface_name);
        if (method_info != nullptr) {
            return {method_info, method_info->invoke(static_cast<object_type&>(*this), args)};
        }

        auto info = metadata_type::get_instance().get_method_interface(method_name, interface_name);
        if (info == nullptr) {
            return {nullptr, mc::variant()};
        }

        return property_info_to_interface(*info)->invoke(method_name, args);
    }

    bool has_method(std::string_view method_name,
                    std::string_view interface_name = {}) const override {
        if (get_object_method_info(method_name, interface_name) != nullptr) {
            return true;
        }

        auto info = metadata_type::get_instance().get_method_interface(method_name, interface_name);
        if (info == nullptr) {
            return false;
        }

        return interface_name.empty() ? true
                                      : property_info_to_interface(*info)->has_method(method_name);
    }

    mc::connection_type connect(std::string_view signal_name, slot_type slot,
                                std::string_view interface_name = {}) override {
        auto obj_info = get_object_sig_info(signal_name, interface_name);
        if (obj_info) {
            return obj_info->connect(static_cast<object_type&>(*this), slot);
        }

        auto info = metadata_type::get_instance().get_signal_interface(signal_name, interface_name);
        if (info == nullptr) {
            return mc::connection_type();
        }

        return property_info_to_interface(*info)->connect(signal_name, slot);
    }

    mc::variant emit(std::string_view signal_name, const mc::variants& args,
                     std::string_view interface_name = {}) override {
        auto obj_info = get_object_sig_info(signal_name, interface_name);
        if (obj_info) {
            return obj_info->emit(static_cast<object_type&>(*this), args);
        }

        auto info = metadata_type::get_instance().get_signal_interface(signal_name, interface_name);
        if (info == nullptr) {
            return mc::variant();
        }

        return property_info_to_interface(*info)->emit(signal_name, args);
    }

    void visit(visitor& v) const override {
        get_metadata().visit(static_cast<const ObjectType&>(*this), v);
    }

    static mc::core::ref_ptr<object_type> create() {
        return mc::core::make_ref<object_type>();
    }

    static void from_variant(const mc::dict& d, object_type& obj) {
        mc::traits::tuple_for_each(get_static_interface_infos(), [&](auto& member) {
            using prop_type      = std::decay_t<decltype(member)>;
            using interface_type = typename prop_type::member_type;
            if (d.contains(interface_type::interface_name)) {
                const auto& sub_dict = d[interface_type::interface_name];
                mc::reflect::from_variant(sub_dict, obj.*member.member_ptr);
            }
        });

        auto& prop_infos = mc::reflect::reflector<object_type>::get_properties();
        mc::traits::tuple_for_each(prop_infos, [&](auto& prop) {
            if (d.contains(prop.name)) {
                prop.set_value(obj, d[prop.name]);
            }
        });
    }

    static void to_variant(const object_type& obj, mc::mutable_dict& dict) {
        mc::traits::tuple_for_each(get_static_interface_infos(), [&](auto& member) {
            using prop_type      = std::decay_t<decltype(member)>;
            using interface_type = typename prop_type::member_type;
            mc::variant sub_dict;
            mc::reflect::to_variant(obj.*member.member_ptr, sub_dict);
            dict[interface_type::interface_name] = std::move(sub_dict);
        });

        auto& prop_infos = mc::reflect::reflector<object_type>::get_properties();
        mc::traits::tuple_for_each(prop_infos, [&](auto& prop) {
            using prop_type   = std::decay_t<decltype(prop)>;
            using member_type = typename prop_type::member_type;
            if constexpr (detail::is_interface_v<member_type>) {
                // 是 interface 类型，但并不是在对象中声明过实现的 interface，当作普通属性看
                if (dict.contains(member_type::interface_name)) {
                    return;
                }
            }

            dict[prop.name] = prop.get_value(obj);
        });
    }

    const mc::reflect::property_info_base<object_type>*
    get_object_property_info(std::string_view property_name, std::string_view interface_name) {
        // 如果精确的指定 interface_name 则不读取对象的属性
        if (!interface_name.empty()) {
            return nullptr;
        }

        return mc::reflect::get_property_info<object_type>(property_name);
    }

    const mc::reflect::method_info_base<object_type>*
    get_object_method_info(std::string_view method_name, std::string_view interface_name) const {
        // 如果精确的指定 interface_name 则不判断对象的方法
        if (!interface_name.empty()) {
            return nullptr;
        }

        return mc::reflect::get_method_info<object_type>(method_name);
    }

    const signal_info_base<object_type>* get_object_sig_info(std::string_view method_name,
                                                             std::string_view interface_name) {
        // 如果精确的指定 interface_name 则不判断对象的信号
        if (!interface_name.empty()) {
            return nullptr;
        }

        auto& sigs = detail::get_signals<object_type>();
        auto  it   = sigs.find(method_name);
        if (it != sigs.end()) {
            return it->second;
        }

        return nullptr;
    }
};

} // namespace mc::engine

#endif // MC_ENGINE_OBJECT_H