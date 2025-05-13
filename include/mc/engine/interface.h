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

#ifndef MC_ENGINE_INTERFACE_H
#define MC_ENGINE_INTERFACE_H

#include <mc/core/object.h>
#include <mc/engine/base.h>
#include <mc/engine/property.h>

namespace mc::engine {

#define MC_ENGINE_PROPERTY_TYPE 0x01

// 信号标签类型
struct signal_tag {};

template <typename Class>
struct signal_info_base : public mc::reflect::method_type_info {
    signal_info_base(std::string_view n) : mc::reflect::method_type_info(n) {
    }

    virtual mc::variant         emit(Class& obj, const mc::variants& args) const = 0;
    virtual mc::connection_type connect(Class& obj, slot_type slot) const        = 0;
};

// 信号反射信息类
template <typename Class, typename RetType, typename... Args>
struct signal_info : public signal_info_base<Class> {
    using tag_type       = mc::engine::signal_tag;
    using args_type      = std::tuple<mc::traits::remove_cvref_t<Args>...>;
    using result_type    = mc::traits::remove_cvref_t<RetType>;
    using signature_type = mc::signal<RetType(Args...)>;

    signature_type Class::* signal_ptr;

    constexpr signal_info(std::string_view n, signature_type Class::* ptr)
        : signal_info_base<Class>(n), signal_ptr(ptr) {
    }

    std::type_index typeinfo() const override {
        return typeid(signature_type);
    }
    std::string_view type_name() const override {
        return mc::pretty_name<signature_type>();
    }

    template <size_t... I>
    mc::variant call_with_exact_args(Class& obj, const mc::variants& args,
                                     std::index_sequence<I...>) const {
        if constexpr (std::is_void_v<RetType>) {
            (obj.*signal_ptr)(mc::reflect::detail::convert_arg<mc::traits::remove_cvref_t<Args>>(
                this->name, args[I])...);
            return mc::variant();
        } else {
            auto ret = (obj.*signal_ptr)(
                mc::reflect::detail::convert_arg<mc::traits::remove_cvref_t<Args>>(this->name,
                                                                                   args[I])...);
            return ret ? mc::variant(*ret) : mc::variant();
        }
    }

    mc::variant emit(Class& obj, const mc::variants& args) const override {
        constexpr size_t arg_count = sizeof...(Args);

        if (args.size() < arg_count) {
            mc::reflect::throw_method_arg_not_enough(this->name, arg_count, args.size());
        }

        return call_with_exact_args(obj, args, std::make_index_sequence<arg_count>());
    }

    mc::connection_type connect(Class& obj, slot_type slot) const override {
        return (obj.*signal_ptr).connect([slot = std::move(slot)](Args... args) mutable {
            if constexpr (std::is_void_v<RetType>) {
                slot(mc::variants{args...});
            } else {
                return slot(mc::variants{args...}).as<RetType>();
            }
        });
    }

    std::string_view get_args_signature() const override {
        return mc::reflect::get_signature<args_type>();
    }

    std::string_view get_result_signature() const override {
        return mc::reflect::get_signature<result_type>();
    }
};

template <typename Class, typename RetType, typename... Args>
constexpr auto make_signal_info(mc::signal<RetType(Args...)> Class::* signal_ptr,
                                std::string_view                      name) {
    return std::make_tuple(signal_info<Class, RetType, Args...>(name, signal_ptr));
}
} // namespace mc::engine

namespace mc::reflect {
template <typename T, typename Signature>
struct member_info_creator<T, mc::signal<Signature>> {
    static auto create(mc::signal<Signature> T::* signal_ptr, std::string_view name) {
        return mc::engine::make_signal_info(signal_ptr, name);
    }
};

} // namespace mc::reflect

namespace mc::engine {

template <typename T>
using signal_map = std::unordered_map<std::string_view, const mc::engine::signal_info_base<T>*>;

namespace detail {
template <typename T>
auto& get_static_signals() {
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return mc::reflect::reflector<clean_type>::template get_members_by_tag<
        mc::engine::signal_tag>();
}

template <typename T>
signal_map<T> init_signal_map() {
    signal_map<T> sigs;
    mc::traits::tuple_for_each(get_static_signals<T>(), [&](auto& member) {
        sigs[member.name] = &member;
    });
    return sigs;
}
} // namespace detail

template <typename T>
struct interface : public abstract_interface {
    using is_interface   = std::true_type;
    using interface_type = T;
    using abstract_interface::connect;

    template <typename Members>
    static constexpr void initial_members(const Members& members) {
        foreach_property(members, [&](auto& member) {
            member.flags |= MC_ENGINE_PROPERTY_TYPE;
        });
    }

    interface() {
        interface_type* self = static_cast<interface_type*>(this);
        foreach_property(get_static_properties(), [&](auto& member) {
            using member_info_type = mc::traits::remove_pointers_t<std::decay_t<decltype(member)>>;
            using member_type      = typename member_info_type::member_type;
            if constexpr (std::is_same_v<detail::interface_observer,
                                         typename member_type::observer_type>) {
                (self->*member.member_ptr).get_observer().set_interface(self);
            }
        });
    }

    abstract_object* get_owner() const override {
        return m_owner;
    }

    void set_owner(abstract_object* owner) {
        m_owner = owner;
    }

    static signal_map<interface_type>& get_signals() {
        static signal_map<interface_type> signals = detail::init_signal_map<interface_type>();
        return signals;
    }

    static bool has_signal(std::string_view signal_name) {
        return get_signals().find(signal_name) != get_signals().end();
    }

    static const mc::engine::signal_info_base<interface_type>*
    get_signal(std::string_view signal_name) {
        auto& sigs = get_signals();
        auto  it   = sigs.find(signal_name);
        if (it == sigs.end()) {
            return nullptr;
        }

        return it->second;
    }

    static const auto& get_static_signals() {
        return detail::get_static_signals<interface_type>();
    }

    static const auto& get_static_properties() {
        return mc::reflect::reflector<interface_type>::get_properties();
    }

    static const auto& get_static_methods() {
        return mc::reflect::reflector<interface_type>::get_methods();
    }

    std::string_view get_interface_name() const override {
        return interface_type::interface_name;
    }

    mc::connection_type connect(std::string_view signal_name, slot_type slot) override {
        auto signal = get_signal(signal_name);
        if (!signal) {
            return {};
        }

        return signal->connect(static_cast<interface_type&>(*this), std::move(slot));
    }

    mc::variant emit(std::string_view signal_name, const mc::variants& args) override {
        auto signal = get_signal(signal_name);
        if (!signal) {
            return {};
        }

        return signal->emit(static_cast<interface_type&>(*this), args);
    }

    invoke_result invoke(std::string_view method_name, const mc::variants& args) override {
        auto method = mc::reflect::get_method_info<interface_type>(method_name);
        if (!method) {
            return {nullptr, mc::variant()};
        }
        return {method, method->invoke(static_cast<interface_type&>(*this), args)};
    }

    mc::variant get_property(std::string_view property_name) override {
        return mc::reflect::get_property(static_cast<interface_type&>(*this), property_name);
    }

    property_base* get_property_base(std::string_view property_name) override {
        auto* info = mc::reflect::get_property_info<interface_type>(property_name);
        if (info == nullptr || (info->flags & MC_ENGINE_PROPERTY_TYPE) == 0) {
            return nullptr;
        }

        return reinterpret_cast<property_base*>(reinterpret_cast<std::uintptr_t>(this) +
                                                info->offset());
    }

    void notify_property_changed(const mc::variant& value, const property_base& prop) override {
        if (m_property_changed_signal) {
            (*m_property_changed_signal)(value, prop);
        }
    }

    std::string_view get_property_name(const property_base* prop) override {
        std::uintptr_t offset =
            reinterpret_cast<std::uintptr_t>(prop) - reinterpret_cast<std::uintptr_t>(this);
        return mc::reflect::get_property_name<interface_type>(offset);
    }

    mc::dict get_all_properties() override {
        return mc::reflect::get_all_properties(static_cast<interface_type&>(*this));
    }

    bool set_property(std::string_view property_name, const mc::variant& value) override {
        auto* prop = get_property_base(property_name);
        if (prop == nullptr) {
            return mc::reflect::set_property(static_cast<interface_type&>(*this), property_name,
                                             value);
        }

        prop->set_value(value);
        return true;
    }

    property_changed_signal& property_changed() override {
        if (m_property_changed_signal == nullptr) {
            m_property_changed_signal = std::make_unique<property_changed_signal>();
        }

        return *m_property_changed_signal;
    }

    void visit(visitor& v) const override {
        v.handle_interface_begin(*get_owner(), *this);
        visit_properties(v);
        visit_methods(v);
        visit_signals(v);
        v.handle_interface_end(*get_owner(), *this);
    }

protected:
    mc::core::service_base* get_service() const override {
        if (!m_owner) {
            return nullptr;
        }

        return m_owner->get_service();
    }

    template <typename Members, typename F>
    static void foreach_property(Members& members, F&& f) {
        mc::traits::tuple_for_each(members, [f = std::forward<F>(f)](auto& member) {
            using member_info_type = mc::traits::remove_pointers_t<std::decay_t<decltype(member)>>;
            if constexpr (mc::reflect::has_tag_v<mc::reflect::property_tag, member_info_type>) {
                using member_type = typename member_info_type::member_type;
                if constexpr (mc::traits::is_specialization_of_v<member_type, property>) {
                    f(member);
                }
            }
        });
    }

    void visit_properties(visitor& v) const {
        mc::traits::tuple_for_each(interface_type::get_static_properties(), [&](auto& property) {
            using property_type =
                typename mc::traits::remove_cvref_t<decltype(property)>::member_type;

            visitor::property_meta info;
            info.name      = property.name;
            info.signature = mc::reflect::get_signature<property_type>();
            info.access    = 0;
            v.handle(*get_owner(), *this, info);
        });
    }

    void visit_methods(visitor& v) const {
        mc::traits::tuple_for_each(interface_type::get_static_methods(), [&](auto& method) {
            using method_info_type = mc::traits::remove_cvref_t<decltype(method)>;
            using result_type      = typename method_info_type::result_type;
            using args_type        = typename method_info_type::args_type;

            visitor::method_meta info;
            info.name = method.name;
            if constexpr (!std::is_void_v<result_type>) {
                info.return_signature = mc::reflect::get_signature<result_type>();
            }
            info.args_signature = mc::reflect::get_signature<args_type>();
            v.handle(*get_owner(), *this, info);
        });
    }

    void visit_signals(visitor& v) const {
        mc::traits::tuple_for_each(interface_type::get_static_signals(), [&](auto& signal) {
            using signal_info_type = mc::traits::remove_cvref_t<decltype(signal)>;
            using result_type      = typename signal_info_type::result_type;
            using args_type        = typename signal_info_type::args_type;

            visitor::signal_meta info;
            info.name             = signal.name;
            info.return_signature = mc::reflect::get_signature<result_type>();
            info.args_signature   = mc::reflect::get_signature<args_type>();
            v.handle(*get_owner(), *this, info);
        });
    }

protected:
    abstract_object*                         m_owner;
    std::unique_ptr<property_changed_signal> m_property_changed_signal;
};

} // namespace mc::engine

#endif // MC_ENGINE_INTERFACE_H