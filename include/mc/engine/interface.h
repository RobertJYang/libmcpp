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
            (obj.*signal_ptr)(mc::detail::convert_arg<mc::traits::remove_cvref_t<Args>>(
                this->name, args[I])...);
            return mc::variant();
        } else {
            auto ret = (obj.*signal_ptr)(
                mc::detail::convert_arg<mc::traits::remove_cvref_t<Args>>(this->name,
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
        sigs[member.name] = reinterpret_cast<const signal_info_base<T>*>(&member);
    });
    return sigs;
}

template <typename T>
signal_map<T>& get_signals() {
    static signal_map<T> signals = detail::init_signal_map<T>();
    return signals;
}
} // namespace detail

class interface_impl : public abstract_interface {
public:
    ~interface_impl() override = default;

    abstract_object* get_owner() const override {
        return m_owner;
    }

    void set_owner(abstract_object* owner) {
        m_owner = owner;
    }

    property_changed_signal& property_changed() override {
        if (m_property_changed_signal == nullptr) {
            m_property_changed_signal = std::make_unique<property_changed_signal>();
        }

        return *m_property_changed_signal;
    }

    void notify_property_changed(const mc::variant& value, const property_base& prop) override {
        if (m_property_changed_signal) {
            (*m_property_changed_signal)(value, prop);
        }
    }

protected:
    abstract_object*                         m_owner;
    std::unique_ptr<property_changed_signal> m_property_changed_signal;
};

template <typename T, typename BaseT = interface_impl>
class interface : public BaseT {
public:
    static_assert(std::is_same_v<interface_impl, BaseT> || std::is_base_of_v<interface_impl, BaseT>,
                  "BaseT must be interface_impl or derived from interface_impl");

    using is_interface   = std::true_type;
    using interface_type = T;
    using base_type      = std::conditional_t<std::is_same_v<interface_impl, BaseT>, void, BaseT>;
    using abstract_interface::connect;
    static constexpr bool has_base_v = !std::is_void_v<base_type>;

    template <typename Members>
    static constexpr void initial_members(const Members& members) {
        foreach_property(members, [&](auto& member) {
            member.flags |= MC_ENGINE_PROPERTY_TYPE;
        });
    }

    interface() {
        interface_type* self  = static_cast<interface_type*>(this);
        auto&           props = mc::reflect::reflector<interface_type>::get_properties();
        foreach_property(props, [&](auto& member) {
            using member_info_type = mc::traits::remove_pointers_t<std::decay_t<decltype(member)>>;
            using member_type      = typename member_info_type::member_type;
            if constexpr (std::is_same_v<detail::interface_observer,
                                         typename member_type::observer_type>) {
                (self->*member.member_ptr).get_observer().set_interface(self);
            }
        });
    }

    static bool has_signal(std::string_view signal_name) {
        auto& sigs = detail::get_signals<interface_type>();
        if (sigs.find(signal_name) != sigs.end()) {
            return true;
        }

        if constexpr (has_base_v) {
            return base_type::has_signal(signal_name);
        } else {
            return false;
        }
    }

    static const mc::engine::signal_info_base<interface_type>*
    get_signal(std::string_view signal_name) {
        auto& sigs = detail::get_signals<interface_type>();
        auto  it   = sigs.find(signal_name);
        if (it != sigs.end()) {
            return it->second;
        }

        if constexpr (has_base_v) {
            auto* info = base_type::get_signal(signal_name);
            return reinterpret_cast<const signal_info_base<interface_type>*>(info);
        } else {
            return nullptr;
        }
    }

    template <typename F>
    static void visit_static_signals(F&& visitor) {
        auto& sigs = detail::get_static_signals<interface_type>();
        mc::traits::tuple_for_each(sigs, [&](auto& signal) {
            if (signal.is_override == 0) {
                visitor(signal);
            }
        });

        if constexpr (has_base_v) {
            base_type::visit_static_signals(std::forward<F>(visitor));
        }
    }

    template <typename F>
    static void visit_static_properties(F&& visitor) {
        auto& props = mc::reflect::reflector<interface_type>::get_properties();
        mc::traits::tuple_for_each(props, [&](auto& property) {
            if (property.is_override == 0) {
                visitor(property);
            }
        });

        if constexpr (has_base_v) {
            base_type::visit_static_properties(std::forward<F>(visitor));
        }
    }

    template <typename F>
    static void visit_static_methods(F&& visitor) {
        auto& methods = mc::reflect::reflector<interface_type>::get_methods();
        mc::traits::tuple_for_each(methods, [&](auto& method) {
            if (method.is_override == 0) {
                visitor(method);
            }
        });

        if constexpr (has_base_v) {
            base_type::visit_static_methods(std::forward<F>(visitor));
        }
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
        // 先从当前接口中查找方法
        auto method = mc::reflect::get_method_info<interface_type>(method_name);
        if (method) {
            auto* ctx = context_stack::top_value();
            if (ctx) {
                ctx->set_method(method);
            }

            return method->invoke(static_cast<interface_type&>(*this), args);
        }

        // 如果当前接口中没有找到方法，则从基类中查找
        if constexpr (has_base_v) {
            return base_type::invoke(method_name, args);
        } else {
            return {};
        }
    }

    async_result async_invoke(std::string_view method_name, const mc::variants& args = {}) override {
        // 先从当前接口中查找方法
        auto method = mc::reflect::get_method_info<interface_type>(method_name);
        if (method) {
            auto* ctx = context_stack::top_value();
            if (ctx) {
                ctx->set_method(method);
            }

            return method->async_invoke(static_cast<interface_type&>(*this), args);
        }

        // 如果当前接口中没有找到方法，则从基类中查找
        if constexpr (has_base_v) {
            return base_type::async_invoke(method_name, args);
        } else {
            return {};
        }
    }

    bool has_method(std::string_view method_name) const override {
        if (mc::reflect::get_method_info<interface_type>(method_name) != nullptr) {
            return true;
        }

        if constexpr (has_base_v) {
            return base_type::has_method(method_name);
        } else {
            return false;
        }
    }

    mc::variant get_property(std::string_view property_name, int options = 0) const override {
        auto* info = mc::reflect::get_property_info<interface_type>(property_name);
        if (info != nullptr) {
            if (info->flags & MC_ENGINE_PROPERTY_TYPE) {
                return prop_info_to_base(info)->get_value(options);
            } else {
                return info->get_value(static_cast<const interface_type&>(*this));
            }
        }

        if constexpr (has_base_v) {
            return base_type::get_property(property_name);
        } else {
            return {};
        }
    }

    property_base*
    prop_info_to_base(const mc::reflect::property_info_base<interface_type>* info) const {
        return reinterpret_cast<property_base*>(reinterpret_cast<std::uintptr_t>(this) +
                                                info->offset());
    }

    property_base* get_property_base(std::string_view property_name) override {
        auto* info = mc::reflect::get_property_info<interface_type>(property_name);
        if (info != nullptr) {
            if (info->flags & MC_ENGINE_PROPERTY_TYPE) {
                return prop_info_to_base(info);
            }

            // 子类覆盖了基类的属性，虽然覆盖的类型不是 mc::engine::property<T>，
            // 但是也不再看见基类的属性
            return nullptr;
        }

        if constexpr (has_base_v) {
            return base_type::get_property_base(property_name);
        } else {
            return nullptr;
        }
    }

    bool has_property(std::string_view property_name) override {
        auto* info = mc::reflect::get_property_info<interface_type>(property_name);
        if (info != nullptr) {
            return true;
        }

        if constexpr (has_base_v) {
            return base_type::has_property(property_name);
        } else {
            return false;
        }
    }

    std::string_view get_property_name(const property_base* prop) override {
        std::uintptr_t offset =
            reinterpret_cast<std::uintptr_t>(prop) - reinterpret_cast<std::uintptr_t>(this);
        auto name = mc::reflect::get_property_name<interface_type>(offset);
        if (!name.empty()) {
            return name;
        }

        if constexpr (has_base_v) {
            return base_type::get_property_name(prop);
        } else {
            return {};
        }
    }

    mc::dict get_all_properties(int options = 0) override {
        mc::mutable_dict dict;
        visit_static_properties([&](auto& property) {
            if (dict.contains(property.name)) {
                return;
            }

            auto* info = mc::reflect::get_property_info<interface_type>(property.name);
            if (info != nullptr) {
                if (info->flags & MC_ENGINE_PROPERTY_TYPE) {
                    const auto* prop_base = reinterpret_cast<const property_base*>(
                        reinterpret_cast<std::uintptr_t>(this) + info->offset());
                    dict[property.name] = prop_base->get_value(options);
                } else {
                    dict[property.name] = info->get_value(static_cast<const interface_type&>(*this));
                }
            } else {
                if constexpr (has_base_v) {
                    // 递归调用基类的 get_all_properties 方法来处理基类属性
                    auto base_dict = base_type::get_all_properties(options);
                    if (base_dict.contains(property.name)) {
                        dict[property.name] = base_dict[property.name];
                    } else {
                        dict[property.name] = {};
                    }
                } else {
                    dict[property.name] = {};
                }
            }
        });
        return dict;
    }

    bool set_property(std::string_view property_name, const mc::variant& value) override {
        try {
            // 先从当前接口中查找属性
            auto* info = mc::reflect::get_property_info<interface_type>(property_name);
            if (info != nullptr) {
                if (info->flags & MC_ENGINE_PROPERTY_TYPE) {
                    prop_info_to_base(info)->set_value(value);
                } else {
                    info->set_value(static_cast<interface_type&>(*this), value);
                }
                return true;
            }

            // 如果当前接口中没有找到属性，则从基类中查找
            if constexpr (has_base_v) {
                return base_type::set_property(property_name, value);
            } else {
                return false;
            }
        } catch (const std::exception& e) {
            dlog("set property ${class}.${name} failed: ${error}",
                 ("class", mc::pretty_name<interface_type>())("name", property_name)("error",
                                                                                     e.what()));
            return false;
        }
    }

    void visit(visitor& v) const override {
        v.handle_interface_begin(*this->get_owner(), *this);
        visit_properties(v);
        visit_methods(v);
        visit_signals(v);
        v.handle_interface_end(*this->get_owner(), *this);
    }

    static void from_variant(const mc::dict& d, interface_type& obj) {
        visit_static_properties([&](auto& property) {
            if (d.contains(property.name)) {
                property.set_value(obj, d[property.name]);
            }
        });
    }

    static void to_variant(const interface_type& obj, mc::mutable_dict& dict, int options = 0) {
        visit_static_properties([&obj, &dict, options](auto& property) {
            if (dict.contains(property.name)) {
                return;
            }

            auto* info = mc::reflect::get_property_info<interface_type>(property.name);
            if (info != nullptr) {
                if (info->flags & MC_ENGINE_PROPERTY_TYPE) {
                    const auto* prop_base = reinterpret_cast<const property_base*>(
                        reinterpret_cast<std::uintptr_t>(&obj) + info->offset());
                    dict[property.name] = prop_base->get_value(options);
                } else {
                    dict[property.name] = info->get_value(obj);
                }
            } else {
                if constexpr (has_base_v) {
                    // 递归调用基类的 to_variant 方法来处理基类属性
                    mc::mutable_dict base_dict;
                    base_type::to_variant(static_cast<const base_type&>(obj), base_dict, options);
                    if (base_dict.contains(property.name)) {
                        dict[property.name] = base_dict[property.name];
                    } else {
                        dict[property.name] = {};
                    }
                } else {
                    dict[property.name] = {};
                }
            }
        });
    }

protected:
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
        visit_static_properties([&](auto& property) {
            using property_type =
                typename mc::traits::remove_cvref_t<decltype(property)>::member_type;

            visitor::property_meta info;
            info.name            = property.name;
            info.signature       = mc::reflect::get_signature<property_type>();
            info.read_privilege  = 0;
            info.write_privilege = 0;
            info.flags           = 0;
            v.handle(*this->get_owner(), *this, info);
        });
    }

    void visit_methods(visitor& v) const {
        visit_static_methods([&](auto& method) {
            using method_info_type = mc::traits::remove_cvref_t<decltype(method)>;
            using result_type      = typename method_info_type::result_type;
            using args_type        = typename method_info_type::args_type;

            visitor::method_meta info;
            info.name = method.name;
            if constexpr (!std::is_void_v<result_type>) {
                info.return_signature = mc::reflect::get_signature<result_type>();
            }
            info.args_signature = mc::reflect::get_signature<args_type>();
            info.privilege      = 0;
            info.flags          = 0;
            v.handle(*this->get_owner(), *this, info);
        });
    }

    void visit_signals(visitor& v) const {
        visit_static_signals([&](auto& signal) {
            using signal_info_type = mc::traits::remove_cvref_t<decltype(signal)>;
            using result_type      = typename signal_info_type::result_type;
            using args_type        = typename signal_info_type::args_type;

            visitor::signal_meta info;
            info.name             = signal.name;
            info.return_signature = mc::reflect::get_signature<result_type>();
            info.args_signature   = mc::reflect::get_signature<args_type>();
            info.flags            = 0;
            v.handle(*this->get_owner(), *this, info);
        });
    }
};

} // namespace mc::engine

#endif // MC_ENGINE_INTERFACE_H