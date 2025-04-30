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

#include <mc/engine/base.h>

namespace mc::engine {

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
    using is_interface = std::true_type;
    using object_type  = T;

    interface() {
    }

    void set_object(abstract_object* obj) {
        m_object = obj;
    }

    abstract_object* get_object() const override {
        return m_object;
    }

    static signal_map<object_type>& get_signals() {
        static signal_map<object_type> signals = detail::init_signal_map<object_type>();
        return signals;
    }

    static bool has_signal(std::string_view signal_name) {
        return get_signals().find(signal_name) != get_signals().end();
    }

    static const mc::engine::signal_info_base<object_type>*
    get_signal(std::string_view signal_name) {
        auto& sigs = get_signals();
        auto  it   = sigs.find(signal_name);
        if (it == sigs.end()) {
            return nullptr;
        }

        return it->second;
    }

    static const auto& get_static_signals() {
        return detail::get_static_signals<object_type>();
    }

    static const auto& get_static_properties() {
        return mc::reflect::reflector<object_type>::get_properties();
    }

    static const auto& get_static_methods() {
        return mc::reflect::reflector<object_type>::get_methods();
    }

    std::string_view get_interface_name() const override {
        return object_type::interface_name;
    }

    mc::connection_type connect(std::string_view signal_name, slot_type slot) override {
        auto signal = get_signal(signal_name);
        if (!signal) {
            return {};
        }

        return signal->connect(static_cast<object_type&>(*this), std::move(slot));
    }

    mc::variant emit(std::string_view signal_name, const mc::variants& args) override {
        auto signal = get_signal(signal_name);
        if (!signal) {
            return {};
        }

        return signal->emit(static_cast<object_type&>(*this), args);
    }

    invoke_result invoke(std::string_view method_name, const mc::variants& args) override {
        auto method = mc::reflect::get_method_info<object_type>(method_name);
        if (!method) {
            return {nullptr, mc::variant()};
        }
        return {method, method->invoke(static_cast<object_type&>(*this), args)};
    }

    mc::variant get_property(std::string_view property_name) override {
        return mc::reflect::get_property(static_cast<object_type&>(*this), property_name);
    }

    mc::dict get_all_properties() override {
        return mc::reflect::get_all_properties(static_cast<object_type&>(*this));
    }

    bool set_property(std::string_view property_name, const mc::variant& value) override {
        return mc::reflect::set_property(static_cast<object_type&>(*this), property_name, value);
    }

protected:
    abstract_object* m_object;
};

} // namespace mc::engine

#endif // MC_ENGINE_INTERFACE_H