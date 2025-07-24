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
#ifndef MC_ENGINE_SIGNAL_INFO_H
#define MC_ENGINE_SIGNAL_INFO_H

#include <mc/reflect/metadata.h>
#include <mc/signal_slot.h>

namespace mc::engine {
using slot_type = std::function<mc::variant(const mc::variants&)>;

// 信号标签类型
struct signal_tag {};

// 定义信号反射元信息类型为 custom_start + 1，一定要注意不能冲突
#define MC_REFLECT_SIGNAL_TYPE static_cast<int>(mc::reflect::member_info_type::custom_start) + 1

struct signal_type_info : public mc::reflect::member_info_base {
    constexpr signal_type_info(std::string_view n) : mc::reflect::member_info_base(n) {
    }

    virtual std::string_view get_args_signature() const   = 0;
    virtual std::string_view get_result_signature() const = 0;
    virtual size_t           arg_count() const            = 0;

    mc::variant         emit(void* obj, const mc::variants& args) const;
    mc::connection_type connect(void* obj, slot_type slot) const;

    template <typename C, std::enable_if_t<std::is_class_v<C>, int> = 0>
    mc::variant emit(C& obj, const mc::variants& args) const {
        return emit(&obj, args);
    }
    template <typename C, std::enable_if_t<std::is_class_v<C>, int> = 0>
    mc::connection_type connect(C& obj, slot_type slot) const {
        return connect(&obj, slot);
    }
};

template <typename Class>
struct signal_info_base : public signal_type_info {
    using signal_type_info::connect;
    using signal_type_info::emit;

    constexpr signal_info_base(std::string_view n) : signal_type_info(n) {
    }

    // 使用反射信息基类直接调用对象信号，用于动态反射类型擦除后使用
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

    std::type_index typeinfo() const noexcept override {
        return typeid(signature_type);
    }
    std::string_view type_name() const noexcept override {
        return mc::pretty_name<signature_type>();
    }
    int type() const noexcept override {
        return MC_REFLECT_SIGNAL_TYPE;
    }
    std::uintptr_t offset() const noexcept override {
        return MC_MEMBER_OFFSETOF(Class, signal_ptr);
    }
    mc::reflect::member_info_base* clone() const override {
        return new signal_info<Class, RetType, Args...>(this->name, this->signal_ptr);
    }
    size_t arg_count() const override {
        return sizeof...(Args);
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

inline mc::variant signal_type_info::emit(void* obj, const mc::variants& args) const {
    return reinterpret_cast<const signal_info_base<std::monostate>*>(this)->emit(
        *static_cast<std::monostate*>(obj), args);
}
inline mc::connection_type signal_type_info::connect(void* obj, slot_type slot) const {
    return reinterpret_cast<const signal_info_base<std::monostate>*>(this)->connect(
        *static_cast<std::monostate*>(obj), slot);
}

template <typename Class, typename RetType, typename... Args>
constexpr auto make_signal_info(mc::signal<RetType(Args...)> Class::* signal_ptr,
                                std::string_view                      name) {
    return std::make_tuple(signal_info<Class, RetType, Args...>(name, signal_ptr));
}
} // namespace mc::engine

namespace mc::reflect {
template <typename T, typename Signature>
struct member_info_creator<T, mc::signal<Signature>> {
    static constexpr auto create(mc::signal<Signature> T::* signal_ptr, std::string_view name) {
        return mc::engine::make_signal_info(signal_ptr, name);
    }
};

template <typename T>
auto& get_static_signals() {
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return mc::reflect::get_static_members_by_tag<clean_type, mc::engine::signal_tag>();
}
} // namespace mc::reflect

#endif // MC_ENGINE_SIGNAL_INFO_H