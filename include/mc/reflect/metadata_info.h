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

/**
 * @file metadata_info.h
 * @brief 反射元数据结构定义
 */
#ifndef MC_REFLECT_METADATA_INFO_H
#define MC_REFLECT_METADATA_INFO_H

#include <functional>
#include <string_view>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <vector>

#include <mc/reflect/signature.h>
#include <mc/variant.h>

namespace mc::reflect {
// 异常抛出辅助函数
[[noreturn]] void throw_method_arg_not_match(std::string_view method_name,
                                             std::string_view expect_type,
                                             std::string_view actual_type);
[[noreturn]] void throw_method_arg_not_enough(std::string_view method_name, size_t expect_count,
                                              size_t actual_count);
[[noreturn]] void throw_method_not_exist(std::string_view method_name);

/**
 * @brief 反射器模板类
 *
 * @tparam T 要反射的类型
 */
template <typename T>
struct reflector {
    using is_defined = std::false_type;
    using is_enum    = std::false_type;
};

/**
 * 检查类型是否可反射
 * @tparam T 要检查的类型
 * @return 如果类型可反射则返回true，否则返回false
 */
template <typename T>
constexpr bool is_reflectable() {
    return reflector<std::decay_t<T>>::is_defined::value;
}

/**
 * 检查类型是否为枚举
 * @tparam T 要检查的类型
 * @return 如果类型是枚举则返回true，否则返回false
 */
template <typename T>
constexpr bool is_enum() {
    return reflector<T>::is_enum::value;
}

/**
 * 检查类型是否为非反射的普通枚举
 * @tparam T 要检查的类型
 * @return 如果类型是普通的枚举则返回true，否则返回false
 */
template <typename T>
constexpr bool is_normal_enum() {
    return std::is_enum_v<T> && !is_reflectable<T>();
}

// 标签类型 - 用于区分不同类型的成员
struct property_tag {};
struct method_tag {};
struct base_class_tag {};

//------------------------------------------------------------------------------
// 类型特征检测
//------------------------------------------------------------------------------

// 检查元组元素是否具有特定的标签类型
template <typename Tag, typename T, typename = void>
struct has_tag : std::false_type {};

template <typename Tag, typename T>
struct has_tag<Tag, T, std::enable_if_t<std::is_same_v<typename T::tag_type, Tag>>>
    : std::true_type {};

template <typename Tag, typename T>
inline constexpr bool has_tag_v = has_tag<Tag, T>::value;

// 检测是否为方法指针
template <typename T>
struct is_method : std::false_type {};

template <typename R, typename C, typename... Args>
struct is_method<R (C::*)(Args...)> : std::true_type {};

template <typename R, typename C, typename... Args>
struct is_method<R (C::*)(Args...) const> : std::true_type {};

// 静态函数的特化
template <typename R, typename... Args>
struct is_method<R (*)(Args...)> : std::true_type {};

template <typename T>
inline constexpr bool is_method_v = is_method<T>::value;

// 检测是否为属性（数据成员）
template <typename T, typename U = void>
struct is_property : std::false_type {};

template <typename T, typename M>
struct is_property<M T::*,
                   std::enable_if_t<!is_method_v<M T::*> &&
                                    is_variant_constructible_v<mc::traits::remove_cvref_t<M>>>>
    : std::true_type {};

// 检测是否为属性
// reflect 要求属性必须是可构造为 mc::variant 的类型
template <typename T>
inline constexpr bool is_property_v = is_property<T>::value;

//------------------------------------------------------------------------------
// 元数据结构基类
//------------------------------------------------------------------------------

// 成员信息基类
struct member_info_base {
    std::string_view name;
    mutable uint32_t flags{0}; // 扩展 flags，用于存储自定义其他信息

    member_info_base(std::string_view n) : name(n) {
    }

    virtual std::type_index  typeinfo() const  = 0;
    virtual std::string_view type_name() const = 0;
};

//------------------------------------------------------------------------------
// 属性元数据结构
//------------------------------------------------------------------------------

struct property_type_info : public member_info_base {
    property_type_info(std::string_view n) : member_info_base(n) {
    }

    virtual std::string_view get_signature() const = 0;
};

// 属性信息基类
template <typename C>
struct property_info_base : public property_type_info {
    using class_type = C;

    property_info_base(std::string_view n) : property_type_info(n) {
    }

    virtual mc::variant get_value(const C& obj) const                     = 0;
    virtual void        set_value(C& obj, const mc::variant& value) const = 0;
    virtual std::function<mc::variant(const C&)>        getter() const    = 0;
    virtual std::function<void(C&, const mc::variant&)> setter() const    = 0;
    virtual size_t                                      offset() const    = 0;
};

// 属性元数据具体实现
template <typename C, typename M, typename BaseT = C>
struct property_info : public property_info_base<C> {
    using class_type  = C;
    using member_type = M;
    using tag_type    = property_tag;
    using base_type   = BaseT;

    M BaseT::* member_ptr;

    constexpr property_info(std::string_view n, M BaseT::* ptr)
        : property_info_base<C>(n), member_ptr(ptr) {
    }

    // 获取属性值
    mc::variant get_value(const C& obj) const override {
        return mc::variant(obj.*member_ptr);
    }

    // 设置属性值
    void set_value(C& obj, const mc::variant& value) const override {
        value.as(obj.*member_ptr);
    }

    std::function<mc::variant(const C&)> getter() const override {
        return [this](const C& obj) {
            return get_value(obj);
        };
    }

    std::function<void(C&, const mc::variant&)> setter() const override {
        return [this](C& obj, const mc::variant& value) {
            set_value(obj, value);
        };
    }

    size_t offset() const override {
        return MC_MEMBER_OFFSETOF(C, member_ptr);
    }

    std::type_index typeinfo() const override {
        return typeid(member_type);
    }

    std::string_view type_name() const override {
        return pretty_name<member_type>();
    }

    std::string_view get_signature() const override {
        return mc::reflect::get_signature<member_type>();
    }

    template <typename Derived>
    constexpr auto to_derived() const {
        static_assert(std::is_base_of_v<class_type, Derived>,
                      "Derived must be derived from class_type");
        return property_info<Derived, member_type, class_type>{this->name, member_ptr};
    }
};

//------------------------------------------------------------------------------
// 方法元数据结构
//------------------------------------------------------------------------------
struct method_type_info : public member_info_base {
    method_type_info(std::string_view n) : member_info_base(n) {
    }

    virtual std::string_view get_args_signature() const   = 0;
    virtual std::string_view get_result_signature() const = 0;
};

// 方法信息基类
template <typename C>
struct method_info_base : public method_type_info {
    using class_type = C;

    method_info_base(std::string_view n) : method_type_info(n) {
    }

    virtual bool        is_static() const                              = 0;
    virtual mc::variant invoke(C& obj, const mc::variants& args) const = 0;
    virtual mc::variant invoke(const mc::variants& args) const         = 0;
    virtual size_t      arg_count() const = 0; // 返回方法所需的参数数量
};

namespace detail {
template <typename Arg>
static auto convert_arg(std::string_view name, const mc::variant& var)
    -> std::enable_if_t<!mc::is_variant_v<std::decay_t<Arg>>, Arg> {
    if (auto arg = var.try_as<Arg>(); arg) {
        return *arg;
    }

    throw_method_arg_not_match(name, pretty_name<Arg>(), var.get_type_name());
}

template <typename Arg>
static auto convert_arg(std::string_view name, const mc::variant& var)
    -> std::enable_if_t<mc::is_variant_v<std::decay_t<Arg>>, const mc::variant&> {
    return var;
}
} // namespace detail

// 方法元数据具体实现 - 根据方法是否为const使用不同实现
template <typename Class, typename BaseT, bool IsConst, bool IsStatic, typename RetType,
          typename... Args>
class method_info : public method_info_base<Class> {
public:
    using tag_type = method_tag;

    using non_const_function_type = RetType (BaseT::*)(Args...);
    using const_function_type     = RetType (BaseT::*)(Args...) const;
    using static_function_type    = RetType (*)(Args...);
    using member_function_type =
        std::conditional_t<IsConst, const_function_type, non_const_function_type>;
    using function_type = std::conditional_t<IsStatic, static_function_type, member_function_type>;
    using result_type   = mc::traits::remove_cvref_t<RetType>;
    using args_type     = std::tuple<mc::traits::remove_cvref_t<Args>...>;
    using class_type    = Class;
    using base_type     = BaseT;

    // 静态断言确保返回类型可以转换为variant
    static_assert(std::is_void_v<RetType> || is_variant_constructible_v<RetType>,
                  "方法返回类型必须是void或者可以转换为mc::variant");

    // 静态断言确保所有参数类型都可以从variant转换
    static_assert(all_variant_constructible_v<mc::traits::remove_cvref_t<Args>...>,
                  "参数类型必须可转换为mc::variant");

    method_info(std::string_view name, function_type func)
        : method_info_base<Class>(name), m_function(func) {
    }

    bool is_static() const override {
        return IsStatic;
    }

    template <typename C>
    RetType call_impl(C& obj, Args&&... args) const {
        if constexpr (!IsStatic) {
            if constexpr (std::is_same_v<C, Class>) {
                return (obj.*m_function)(std::forward<Args>(args)...);
            } else {
                throw_method_not_exist(this->name);
            }
        } else {
            return m_function(std::forward<Args>(args)...);
        }
    }

    template <typename C, size_t... I>
    variant call_with_exact_args(C& obj, const variants& args, std::index_sequence<I...>) const {
        if constexpr (std::is_void_v<RetType>) {
            call_impl(
                obj, detail::convert_arg<mc::traits::remove_cvref_t<Args>>(this->name, args[I])...);
            return variant();
        } else {
            return variant(call_impl(obj, detail::convert_arg<mc::traits::remove_cvref_t<Args>>(
                                              this->name, args[I])...));
        }
    }

    // 调用方法，要求参数数量 >= 函数参数数量
    template <typename C>
    variant invoke_impl(C& obj, const variants& args) const {
        constexpr size_t arg_count = sizeof...(Args);

        // 如果是单个参数，且参数类型是 mc::variants，优化一下直接调用
        if constexpr (std::is_same_v<args_type, std::tuple<mc::variants>>) {
            if constexpr (std::is_void_v<RetType>) {
                call_impl(obj, args);
                return {};
            } else {
                return call_impl(obj, args);
            }
        } else {
            if (args.size() < arg_count) {
                throw_method_arg_not_enough(this->name, arg_count, args.size());
            }

            return call_with_exact_args(obj, args, std::make_index_sequence<arg_count>());
        }
    }

    variant invoke(Class& obj, const variants& args) const override {
        return invoke_impl(obj, args);
    }

    mc::variant invoke(const mc::variants& args) const override {
        if (!IsStatic) {
            // 非静态方法不能直接调用，需要传入对象，这里抛出方法不存在异常
            throw_method_not_exist(this->name);
        }
        int dummy = 0;
        return invoke_impl<int>(dummy, args);
    }

    std::type_index typeinfo() const override {
        return typeid(RetType);
    }

    std::string_view type_name() const override {
        return pretty_name<RetType>();
    }

    size_t arg_count() const override {
        return sizeof...(Args);
    }

    std::string_view get_args_signature() const override {
        return mc::reflect::get_signature<args_type>();
    }

    std::string_view get_result_signature() const override {
        return mc::reflect::get_signature<result_type>();
    }

    template <typename Derived>
    constexpr auto to_derived() const {
        static_assert(std::is_base_of_v<class_type, Derived>,
                      "Derived must be derived from class_type");
        return method_info<Derived, base_type, IsConst, IsStatic, RetType, Args...>{this->name,
                                                                                    m_function};
    }

    function_type m_function;
};

// 创建方法元数据的辅助函数
template <typename C, typename R, typename... Args>
constexpr auto make_method_info(R (C::*method)(Args...), std::string_view name) {
    return method_info<C, C, false, false, R, Args...>{name, method};
}

template <typename C, typename R, typename... Args>
constexpr auto make_method_info(R (C::*method)(Args...) const, std::string_view name) {
    return method_info<C, C, true, false, R, Args...>{name, method};
}

template <typename C, typename R, typename... Args>
constexpr auto make_static_method_info(R (*method)(Args...), std::string_view name) {
    return method_info<C, C, false, true, R, Args...>{name, method};
}

//------------------------------------------------------------------------------
// 基类元数据结构
//------------------------------------------------------------------------------

template <typename C>
struct base_class_info_base : public member_info_base {
    base_class_info_base(std::string_view n = {}) : member_info_base(n) {
    }

    virtual std::string_view get_signature() const                                            = 0;
    virtual mc::variant      get_value(const C& obj, std::string_view name) const             = 0;
    virtual void set_value(C& obj, std::string_view name, const mc::variant& value) const     = 0;
    virtual mc::variant invoke(C& obj, std::string_view name, const mc::variants& args) const = 0;
};

// 属性元数据具体实现
template <typename C, typename BaseT>
struct base_class_info : public base_class_info_base<C> {
    using class_type  = C;
    using member_type = BaseT;
    using base_type   = BaseT;
    using tag_type    = base_class_tag;

    constexpr base_class_info(std::string_view n) : base_class_info_base<C>(n) {
        this->name = remove_common_namespace(n, pretty_name<class_type>());
    }

    base_class_info(std::string n) : base_class_info_base<C>(), m_class_name(n) {
        base_class_info_base<C>::name = m_class_name;
    }

    base_class_info(base_class_info&& other)
        : base_class_info_base<C>(other.name), m_class_name(std::move(other.m_class_name)) {
        set_class_name(m_class_name);
    }

    base_class_info& operator=(base_class_info&& other) {
        if (this != &other) {
            this->name = other.name;
            set_class_name(std::move(other.m_class_name));
        }

        return *this;
    }

    base_class_info(const base_class_info& other)
        : base_class_info_base<C>(other.name), m_class_name(other.m_class_name) {
        set_class_name(other.m_class_name);
    }

    base_class_info& operator=(const base_class_info& other) {
        if (this != &other) {
            this->name = other.name;
            set_class_name(other.m_class_name);
        }

        return *this;
    }

    void set_class_name(std::string n) {
        m_class_name = std::move(n);
        if (!m_class_name.empty()) {
            base_class_info_base<C>::name = m_class_name;
        }
    }

    std::type_index typeinfo() const override {
        return typeid(base_type);
    }

    std::string_view type_name() const override {
        return pretty_name<base_type>();
    }

    std::string_view get_signature() const override {
        return mc::reflect::get_signature<base_type>();
    }

    mc::variant get_value(const C& obj, std::string_view name) const override;
    void        set_value(C& obj, std::string_view name, const mc::variant& value) const override;
    mc::variant invoke(C& obj, std::string_view name, const mc::variants& args) const override;

    template <typename Derived>
    auto to_derived(std::string_view name) const {
        static_assert(std::is_base_of_v<class_type, Derived>,
                      "Derived must be derived from class_type");
        if (name.empty()) {
            return base_class_info<Derived, member_type>{this->name};
        }

        std::string new_name(name);
        new_name += "::";
        new_name += remove_common_namespace(this->name, name);
        return base_class_info<Derived, member_type>{new_name};
    }

    static std::string_view remove_common_namespace(std::string_view s1, std::string_view s2) {
        auto prefix = mc::string::longest_common_prefix(s1, s2);
        if (prefix.empty()) {
            return s1;
        }

        auto pos = prefix.find_last_of(':');
        if (pos == std::string_view::npos) {
            return s1;
        }

        return s1.substr(pos + 1);
    }

    std::string m_class_name;
};

/**
 * @brief 用户可以通过特化此模板为自定义成员类型提供反射支持
 *
 * 示例：假设有信号成员类型 mc::signal<T>，用户可以这样特化：
 *
 * // 首先定义信号标签类型
 * namespace mc::reflect {
 * struct signal_tag {};
 * }
 *
 * // 然后定义信号信息类
 * template <typename C, typename Signature>
 * struct signal_info : public member_info_base {
 *     using tag_type = mc::reflect::signal_tag;
 *
 *     mc::signal<Signature> C::* signal_ptr;
 *
 *     constexpr signal_info(std::string_view n, mc::signal<Signature> C::* ptr)
 *         : member_info_base(n), signal_ptr(ptr) {}
 *
 *     std::type_index typeinfo() const override { return typeid(mc::signal<Signature>); }
 *     std::string_view type_name() const override { return "signal"; }
 * };
 *
 * // 最后特化 member_info_creator
 * template <typename T, typename Signature, typename BaseT>
 * struct member_info_creator<T, mc::signal<Signature>, BaseT, void> {
 *     static constexpr auto create(mc::signal<Signature> BaseT::* member_ptr, std::string_view
 * name) { return std::tuple<signal_info<T, Signature>>{signal_info<T, Signature>{name,
 * member_ptr}};
 *     }
 * };
 *
 * // 然后可以使用 get_members_by_tag 提取信号成员
 * auto signals = reflector<T>::get_members_by_tag<mc::reflect::signal_tag>();
 */
template <typename T, typename M, typename BaseT = T, typename = void>
struct member_info_creator {
    static constexpr auto create(M BaseT::* member_ptr, std::string_view name) {
        static_assert(is_property_v<M BaseT::*> || is_method_v<M BaseT::*>,
                      "不支持的成员类型，请为此类型创建特化版本的 member_info_creator");
        return std::tuple<>(); // 永远不会执行到这里
    }
};

// 属性成员特化
template <typename T, typename M, typename BaseT>
struct member_info_creator<T, M, BaseT, std::enable_if_t<is_property_v<M BaseT::*>>> {
    static constexpr auto create(M BaseT::* member_ptr, std::string_view name) {
        return std::tuple<property_info<T, M>>{property_info<T, M>{name, member_ptr}};
    }
};

// 方法成员特化
template <typename T, typename M, typename BaseT>
struct member_info_creator<T, M, BaseT, std::enable_if_t<is_method_v<M BaseT::*>>> {
    static constexpr auto create(M BaseT::* member_ptr, std::string_view name) {
        return std::make_tuple(make_method_info(member_ptr, name));
    }
};

// 静态成员函数特化
template <typename T, typename R, typename... Args>
struct member_info_creator<T, R (*)(Args...), void, std::enable_if_t<is_method_v<R (*)(Args...)>>> {
    static constexpr auto create(R (*static_func)(Args...), std::string_view name) {
        return std::make_tuple(make_static_method_info<T>(static_func, name));
    }
};
template <typename T, typename Base>
struct base_class_info_creator {
    static_assert(std::is_base_of_v<Base, T>, "T must be derived from Base class");
    static_assert(is_reflectable<Base>(), "Base class must be reflectable");

    static constexpr auto create(std::string_view name) {
        auto info = base_class_info<T, Base>{name};
        return std::tuple_cat(
            std::make_tuple(info),
            append_base_classes(mc::reflect::reflector<Base>::get_base_classes(), info.name));
    }

    template <typename Tuple>
    static constexpr auto append_base_classes(Tuple&& base_classes, std::string_view name) {
        return mc::traits::tuple_map(base_classes, [&](auto& base_class) {
            using member_type = typename std::decay_t<decltype(base_class)>::member_type;
            auto info         = base_class.template to_derived<T>(name);
            return std::tuple_cat(
                std::make_tuple(info),
                append_base_classes(mc::reflect::reflector<member_type>::get_base_classes(),
                                    info.name));
        });
    }
};

namespace detail {
template <typename T, typename Derived, typename = void>
struct has_to_derived : std::false_type {};

template <typename T, typename Derived>
struct has_to_derived<T, Derived,
                      std::void_t<decltype(std::declval<T>().template to_derived<Derived>())>>
    : std::true_type {};

template <typename Derived, typename Member>
constexpr auto apply_to_derived(const Member& member) {
    if constexpr (has_to_derived<Member, Derived>::value) {
        return std::make_tuple(member.template to_derived<Derived>());
    } else {
        return std::make_tuple(member);
    }
}
} // namespace detail

} // namespace mc::reflect

#endif // MC_REFLECT_METADATA_INFO_H