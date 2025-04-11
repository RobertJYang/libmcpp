/*
 * Copyright (c) 2023 Huawei Technologies Co., Ltd.
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
 * @file reflect.h
 * @brief 反射相关的宏定义
 */
#ifndef MC_REFLECT_REFLECT_H
#define MC_REFLECT_REFLECT_H

#include <functional>
#include <string>
#include <type_traits>
#include <vector>

// Boost.Preprocessor
#include <boost/preprocessor/control/if.hpp>
#include <boost/preprocessor/control/iif.hpp>
#include <boost/preprocessor/facilities/is_empty.hpp>
#include <boost/preprocessor/logical/compl.hpp>
#include <boost/preprocessor/punctuation/is_begin_parens.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/tuple/elem.hpp>

#include <mc/common.h>
#include <mc/dict.h>
#include <mc/reflect/typename.h>
#include <mc/signal_slot.h>
#include <mc/variant.h>

namespace mc::reflect {

// 成员类型标签
struct property_tag {};
struct method_tag {};
struct signal_tag {};

//------------------------------------------------------------------------------
// 类型特征检测
//------------------------------------------------------------------------------

namespace detail {

// 检测是否为 mc::signal 类型
template <typename T>
struct is_signal : std::false_type {};

template <typename Ret, typename... Args>
struct is_signal<mc::signal<Ret(Args...)>> : std::true_type {};

template <typename T>
inline constexpr bool is_signal_v = is_signal<remove_cvref_t<T>>::value;

// 检测是否为方法指针
template <typename T>
struct is_method : std::false_type {};

template <typename R, typename C, typename... Args>
struct is_method<R (C::*)(Args...)> : std::true_type {};

template <typename R, typename C, typename... Args>
struct is_method<R (C::*)(Args...) const> : std::true_type {};

template <typename T>
inline constexpr bool is_method_v = is_method<T>::value;

// 检测是否为属性（数据成员）
template <typename T, typename U = void>
struct is_property : std::false_type {};

template <typename T>
struct is_property<
    T, std::enable_if_t<std::is_member_object_pointer_v<T> && !is_method_v<T> && !is_signal_v<T>>>
    : std::true_type {};

template <typename T>
inline constexpr bool is_property_v = is_property<T>::value;

// 方法特征提取
template <typename T>
struct method_traits;

template <typename R, typename C, typename... Args>
struct method_traits<R (C::*)(Args...)> {
    using class_type                   = C;
    using return_type                  = R;
    using args_tuple                   = std::tuple<Args...>;
    static constexpr bool   is_const   = false;
    static constexpr size_t args_count = sizeof...(Args);
};

template <typename R, typename C, typename... Args>
struct method_traits<R (C::*)(Args...) const> {
    using class_type                   = C;
    using return_type                  = R;
    using args_tuple                   = std::tuple<Args...>;
    static constexpr bool   is_const   = true;
    static constexpr size_t args_count = sizeof...(Args);
};

// 类型特征检测辅助类
template <typename T, typename = void>
struct member_traits {
    static constexpr bool is_property = false;
    static constexpr bool is_method   = false;
    static constexpr bool is_signal   = false;
};

// 属性特化
template <typename C, typename M>
struct member_traits<M C::*, std::enable_if_t<!is_method_v<M C::*> && !is_signal_v<M>>> {
    static constexpr bool is_property = true;
    static constexpr bool is_method   = false;
    static constexpr bool is_signal   = false;

    using class_type  = C;
    using member_type = M;
};

// 方法特化 - 非const版本
template <typename C, typename R, typename... Args>
struct member_traits<R (C::*)(Args...)> {
    static constexpr bool is_property = false;
    static constexpr bool is_method   = true;
    static constexpr bool is_signal   = false;

    using class_type               = C;
    using return_type              = R;
    using args_tuple               = std::tuple<Args...>;
    static constexpr bool is_const = false;
};

// 方法特化 - const版本
template <typename C, typename R, typename... Args>
struct member_traits<R (C::*)(Args...) const> {
    static constexpr bool is_property = false;
    static constexpr bool is_method   = true;
    static constexpr bool is_signal   = false;

    using class_type               = C;
    using return_type              = R;
    using args_tuple               = std::tuple<Args...>;
    static constexpr bool is_const = true;
};

// 信号特化
template <typename C, typename S>
struct member_traits<S C::*, std::enable_if_t<!is_method_v<S C::*> && is_signal_v<S>>> {
    static constexpr bool is_property = false;
    static constexpr bool is_method   = false;
    static constexpr bool is_signal   = true;

    using class_type  = C;
    using signal_type = S;
};
} // namespace detail

//------------------------------------------------------------------------------
// 元数据结构
//------------------------------------------------------------------------------
template <typename C>
struct property_info_base {
    std::string_view name;

    property_info_base(std::string_view n) : name(n) {
    }

    virtual mc::variant get_value(const C& obj) const                     = 0;
    virtual void        set_value(C& obj, const mc::variant& value) const = 0;
    virtual std::function<mc::variant(const C&)>        getter() const    = 0;
    virtual std::function<void(C&, const mc::variant&)> setter() const    = 0;
    virtual size_t                                      offset() const    = 0;
};

// 属性元数据
template <typename C, typename M>
struct property_info : public property_info_base<C> {
    using class_type  = C;
    using member_type = M;
    using tag_type    = property_tag;

    M C::* member_ptr;

    constexpr property_info(std::string_view n, M C::* ptr)
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
};

// 方法元数据
template <typename C, typename R, typename... Args>
struct method_info {
    using class_type  = C;
    using return_type = R;
    using tag_type    = method_tag;

    std::string_view name;
    union {
        R (C::*method_ptr)(Args...);
        R (C::*const_method_ptr)(Args...) const;
    };
    bool is_const;

    constexpr method_info(std::string_view n, R (C::*ptr)(Args...))
        : name(n), method_ptr(ptr), is_const(false) {
    }

    constexpr method_info(std::string_view n, R (C::*ptr)(Args...) const)
        : name(n), const_method_ptr(ptr), is_const(true) {
    }

    // 调用方法
    mc::variant invoke(C* obj, const std::vector<mc::variant>& args) const {
        if (sizeof...(Args) != args.size()) {
            return mc::variant(); // 参数数量不匹配
        }

        if (is_const) {
            return invoke_const(obj, args, std::index_sequence_for<Args...>{});
        } else {
            return invoke_non_const(obj, args, std::index_sequence_for<Args...>{});
        }
    }

private:
    // 调用非const方法
    template <size_t... I>
    mc::variant invoke_non_const(C* obj, const std::vector<mc::variant>& args,
                                 std::index_sequence<I...>) const {
        if constexpr (std::is_same_v<R, void>) {
            (obj->*method_ptr)(args[I].as<Args>()...);
            return mc::variant();
        } else {
            return mc::variant((obj->*method_ptr)(args[I].as<Args>()...));
        }
    }

    // 调用const方法
    template <size_t... I>
    mc::variant invoke_const(const C* obj, const std::vector<mc::variant>& args,
                             std::index_sequence<I...>) const {
        if constexpr (std::is_same_v<R, void>) {
            (obj->*const_method_ptr)(args[I].as<Args>()...);
            return mc::variant();
        } else {
            return mc::variant((obj->*const_method_ptr)(args[I].as<Args>()...));
        }
    }
};

// 信号元数据
template <typename C, typename S>
struct signal_info {
    using class_type  = C;
    using signal_type = S;
    using tag_type    = signal_tag;

    std::string_view name;
    S C::* signal_ptr;

    constexpr signal_info(std::string_view n, S C::* ptr) : name(n), signal_ptr(ptr) {
    }

    // 获取信号对象引用
    S& get_signal(C& obj) const {
        return obj.*signal_ptr;
    }

    // 获取信号对象的const引用
    const S& get_signal(const C& obj) const {
        return obj.*signal_ptr;
    }

    auto getter() {
        return [this](C& obj) {
            return get_signal(obj);
        };
    }

    auto setter() {
        return [this](C& obj, const S& value) {
            get_signal(obj) = value;
        };
    }
};
} // namespace mc::reflect

// 创建方法元数据的辅助函数
namespace mc::reflect {
template <typename C, typename R, typename... Args>
constexpr auto make_method_info(R (C::*method)(Args...), std::string_view name) {
    return method_info<C, R, Args...>{name, method};
}

template <typename C, typename R, typename... Args>
constexpr auto make_method_info(R (C::*method)(Args...) const, std::string_view name) {
    return method_info<C, R, Args...>{name, method};
}
} // namespace mc::reflect

// 检测是否为属性
#define MC_REFLECT_IS_PROPERTY(type, member)                                                       \
    mc::reflect::detail::is_property_v<decltype(&type::member)>

// 处理自定义名称的属性
#define MC_REFLECT_PROCESS_PROPERTY_CUSTOM(r, type, elem)                                          \
    std::tuple<mc::reflect::property_info<type, decltype(std::declval<type>().BOOST_PP_TUPLE_ELEM( \
                                                    0, elem))>> {                                  \
        BOOST_PP_TUPLE_ELEM(1, elem), &type::BOOST_PP_TUPLE_ELEM(0, elem)                          \
    }

// 处理简单名称的属性
#define MC_REFLECT_PROCESS_PROPERTY_SIMPLE(r, type, elem)                                          \
    std::tuple<mc::reflect::property_info<type, decltype(std::declval<type>().elem)>> {            \
        BOOST_PP_STRINGIZE(elem), &type::elem                                                      \
    }

// 处理属性
#define MC_REFLECT_PROPERTY(r, type, elem)                                                         \
    BOOST_PP_IIF(MC_REFLECT_IS_TUPLE(elem), MC_REFLECT_PROCESS_PROPERTY_SIMPLE,                    \
                 MC_REFLECT_PROCESS_PROPERTY_CUSTOM)

// 空处理
#define MC_REFLECT_EMPTY(r, type, elem)

// 根据成员类型过滤属性
#define MC_REFLECT_FILTER_PROPERTY(r, type, elem)                                                  \
    BOOST_PP_IIF(MC_REFLECT_IS_PROPERTY(type, BOOST_PP_TUPLE_ELEM(0, elem)), MC_REFLECT_PROPERTY,  \
                 MC_REFLECT_EMPTY)(r, type, elem)

// 检测是否为元组形式（双括号表达式）
#define MC_REFLECT_IS_TUPLE(x) BOOST_PP_IS_BEGIN_PARENS(x)

// 处理成员
#define MC_REFLECT_ELEMENT(r, TYPE, MEMBER)                                                        \
    BOOST_PP_IIF(MC_REFLECT_IS_TUPLE(MEMBER), MC_REFLECT_MEMBER_WITH_NAME,                         \
                 MC_REFLECT_MEMBER_WITHOUT_NAME)(r, TYPE, MEMBER)

// 处理带名称的成员（双括号形式）
#define MC_REFLECT_MEMBER_WITH_NAME(r, TYPE, MEMBER)                                               \
    mc::reflect::detail::create_member_info<TYPE>(&TYPE::BOOST_PP_TUPLE_ELEM(0, MEMBER),           \
                                                  BOOST_PP_TUPLE_ELEM(1, MEMBER)),

// 处理不带名称的成员（单括号形式）
#define MC_REFLECT_MEMBER_WITHOUT_NAME(r, TYPE, MEMBER)                                            \
    mc::reflect::detail::create_member_info<TYPE>(&TYPE::MEMBER, BOOST_PP_STRINGIZE(MEMBER)),

// 在命名空间中添加辅助函数
namespace mc::reflect::detail {

// 创建成员元数据（根据成员类型分发到属性、方法或信号）
template <typename T, typename M, typename BaseT>
constexpr auto create_member_info(M BaseT::* member_ptr, std::string_view name) {
    if constexpr (is_signal_v<M>) {
        // 信号
        return std::tuple<signal_info<T, M>>{signal_info<T, M>{name, member_ptr}};
    } else if constexpr (is_property_v<M BaseT::*>) {
        // 属性
        return std::tuple<property_info<T, M>>{property_info<T, M>{name, member_ptr}};
    } else if constexpr (is_method_v<M BaseT::*>) {
        // 方法
        return std::tuple<decltype(make_method_info(member_ptr, name))>{
            make_method_info(member_ptr, name)};
    } else {
        // 未识别的成员类型
        return std::tuple<>{};
    }
}

// 检查元组元素是否具有特定的标签类型
template <typename Tag, typename T, typename = void>
struct has_tag : std::false_type {};

template <typename Tag, typename T>
struct has_tag<Tag, T, std::enable_if_t<std::is_same_v<typename T::tag_type, Tag>>>
    : std::true_type {};

template <typename Tag, typename T>
inline constexpr bool has_tag_v = has_tag<Tag, T>::value;

// 辅助函数：获取具有特定索引的元组元素（如果元素具有特定标签，则返回它，否则返回空元组）
template <typename Tag, typename Tuple, size_t Index>
constexpr auto get_if_has_tag(const Tuple& tuple) {
    using element_type = mc::remove_cvref_t<std::tuple_element_t<Index, Tuple>>;
    if constexpr (has_tag_v<Tag, element_type>) {
        return std::get<Index>(tuple);
    } else {
        return std::tuple<>();
    }
}

// 辅助模板：连接多个元组元素
template <typename Tag, typename Tuple, size_t... Indices>
constexpr auto filter_members_indices(const Tuple& tuple, std::index_sequence<Indices...>) {
    return std::tuple_cat(get_if_has_tag<Tag, Tuple, Indices>(tuple)...);
}

// 递归过滤元组元素，仅保留具有特定标签的元素
template <typename Tag, size_t Index, typename Result, typename... Tuples>
constexpr auto filter_members_impl(const std::tuple<Tuples...>& all_members, Result result) {
    if constexpr (Index >= sizeof...(Tuples)) {
        return result;
    } else {
        using element_type = mc::remove_cvref_t<std::tuple_element_t<Index, std::tuple<Tuples...>>>;
        if constexpr (has_tag_v<Tag, element_type>) {
            return filter_members_impl<Tag, Index + 1>(
                all_members,
                std::tuple_cat(result, std::tuple<element_type>{std::get<Index>(all_members)}));
        } else {
            return filter_members_impl<Tag, Index + 1>(all_members, result);
        }
    }
}

// 公共API：按标签类型过滤成员元组
template <typename Tag, typename Tuple>
constexpr auto filter_members(const Tuple& all_members) {
    return filter_members_impl<Tag, 0>(all_members, std::tuple<>{});
}

} // namespace mc::reflect::detail

/**
 * @brief 开始定义枚举的反射信息
 */
#define MC_REFLECT_ENUM_BEGIN(TYPE)                                                                \
    namespace mc {                                                                                 \
    template <>                                                                                    \
    struct reflect::reflector<TYPE> {                                                              \
        using is_defined = std::true_type;                                                         \
        using is_enum    = std::true_type;                                                         \
        static const char* name() {                                                                \
            return #TYPE;                                                                          \
        }                                                                                          \
        static void to_variant(const TYPE& e, variant& var) {                                      \
            switch (e) {
/**
 * @brief 定义枚举值的反射信息
 */
#define MC_REFLECT_ENUM_VALUE(r, ENUM, VALUE)                                                      \
    case ENUM::VALUE:                                                                              \
        var = BOOST_PP_STRINGIZE(VALUE);                                                           \
        break;

/**
 * @brief 结束定义枚举的反射信息
 */
#define MC_REFLECT_ENUM_END(TYPE)                                                                  \
    default:                                                                                       \
        mc::reflect::throw_bad_enum_cast(static_cast<int64_t>(e), #TYPE);                          \
        }                                                                                          \
        }                                                                                          \
        static void from_variant(const variant& var, TYPE& e) {
/**
 * @brief 定义枚举值的字符串转换
 */
#define MC_REFLECT_ENUM_FROM_STRING(r, ENUM, VALUE)                                                \
    if (var == BOOST_PP_STRINGIZE(VALUE)) {                                                        \
        e = ENUM::VALUE;                                                                           \
        return;                                                                                    \
    }

/**
 * @brief 结束定义枚举值的字符串转换
 */
#define MC_REFLECT_ENUM_FROM_STRING_END(TYPE)                                                      \
    mc::reflect::throw_bad_enum_cast(var.as_string().c_str(), #TYPE);                              \
    }                                                                                              \
    }                                                                                              \
    ;                                                                                              \
    } // namespace mc

namespace mc {
namespace reflect {

void throw_bad_enum_cast(int64_t i, const char* e);
void throw_bad_enum_cast(const char* k, const char* e);

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
    return reflector<T>::is_defined::value;
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
 * 获取类型名称
 * @tparam T 要获取名称的类型
 * @return std::string_view 类型名称
 */
template <typename T>
std::string_view get_type_name() {
    return reflector<T>::name();
}

/**
 * 将对象转换为变体
 * @tparam T 对象类型
 * @param obj 对象实例
 * @param var 转换后的变体
 */
template <typename T>
void to_variant(const T& obj, variant& var) {
    if constexpr (reflector<T>::is_enum::value) {
        reflector<T>::to_variant(obj, var);
    } else {
        mutable_dict dict;
        reflector<T>::to_variant(obj, dict);
        var = dict;
    }
}

/**
 * 将对象直接转换为可变字典
 * @tparam T 对象类型
 * @param obj 对象实例
 * @param dict 转换后的可变字典
 */
template <typename T>
void to_variant(const T& obj, mutable_dict& dict) {
    if constexpr (reflector<T>::is_enum::value) {
        variant var;
        reflector<T>::to_variant(obj, var);
        dict["value"] = var;
    } else {
        reflector<T>::to_variant(obj, dict);
    }
}

/**
 * 从变体转换为对象
 * @tparam T 对象类型
 * @param var 变体实例
 * @param obj 转换后的对象
 */
template <typename T>
void from_variant(const variant& var, T& obj) {
    if constexpr (reflector<T>::is_enum::value) {
        reflector<T>::from_variant(var, obj);
    } else {
        reflector<T>::from_variant(var.as<mc::dict>(), obj);
    }
}

/**
 * 遍历类型的所有成员
 * @tparam T 要遍历的类型
 * @param visitor 访问者函数
 */
template <typename T, typename Visitor>
void visit_properties(const Visitor& visitor) {
    if constexpr (is_reflectable<T>()) {
        reflector<T>::visit(visitor);
    }
}

} // namespace reflect

template <typename T, std::enable_if_t<mc::reflect::is_reflectable<T>(), int> = 0>
void to_variant(const T& o, variant& v) {
    mc::reflect::to_variant(o, v);
}

template <typename T, std::enable_if_t<mc::reflect::is_reflectable<T>(), int> = 0>
void from_variant(const variant& v, T& o) {
    mc::reflect::from_variant(v, o);
}

} // namespace mc

/**
 * @brief 定义类的反射信息
 */
#define MC_REFLECT(TYPE, MEMBERS)                                                                  \
    namespace mc::reflect {                                                                        \
    template <>                                                                                    \
    struct reflector<TYPE> {                                                                       \
        using is_defined = std::true_type;                                                         \
        using is_enum    = std::false_type;                                                        \
        static constexpr std::string_view name() {                                                 \
            return #TYPE;                                                                          \
        }                                                                                          \
                                                                                                   \
        /* 反射成员信息 */                                                                         \
        static const auto& get_members() {                                                         \
            static auto members = std::tuple_cat(                                                  \
                BOOST_PP_SEQ_FOR_EACH(MC_REFLECT_ELEMENT, TYPE, MEMBERS) std::tuple<>());          \
            return members;                                                                        \
        }                                                                                          \
                                                                                                   \
        /* 属性处理 */                                                                             \
        static const auto& get_properties() {                                                      \
            static auto properties =                                                               \
                mc::reflect::detail::filter_members<mc::reflect::property_tag>(get_members());     \
            return properties;                                                                     \
        }                                                                                          \
                                                                                                   \
        /* 方法处理 */                                                                             \
        static const auto& get_methods() {                                                         \
            static auto methods =                                                                  \
                mc::reflect::detail::filter_members<mc::reflect::method_tag>(get_members());       \
            return methods;                                                                        \
        }                                                                                          \
                                                                                                   \
        /* 信号处理 */                                                                             \
        static const auto& get_signals() {                                                         \
            static auto signals =                                                                  \
                mc::reflect::detail::filter_members<mc::reflect::signal_tag>(get_members());       \
            return signals;                                                                        \
        }                                                                                          \
        template <typename Visitor>                                                                \
        static void visit(const Visitor& visitor) {                                                \
            std::apply(                                                                            \
                [&](auto&... member) {                                                             \
                    (visitor(member.name, member.getter(), member.setter()), ...);                 \
                },                                                                                 \
                get_properties());                                                                 \
        }                                                                                          \
        static void to_variant(const TYPE& obj, mc::mutable_dict& dict) {                          \
            visit([&](std::string_view name, auto getter, auto) {                                  \
                dict[name] = getter(obj);                                                          \
            });                                                                                    \
        }                                                                                          \
        static void from_variant(const mc::dict& d, TYPE& obj) {                                   \
            visit([&](std::string_view name, auto, auto setter) {                                  \
                if (d.contains(name)) {                                                            \
                    setter(obj, d[name]);                                                          \
                }                                                                                  \
            });                                                                                    \
        }                                                                                          \
    };                                                                                             \
    }

/**
 * @brief 定义枚举的反射信息
 */
#define MC_REFLECT_ENUM(TYPE, VALUES)                                                              \
    MC_REFLECT_ENUM_BEGIN(TYPE)                                                                    \
    BOOST_PP_SEQ_FOR_EACH(MC_REFLECT_ENUM_VALUE, TYPE, VALUES)                                     \
    MC_REFLECT_ENUM_END(TYPE)                                                                      \
    BOOST_PP_SEQ_FOR_EACH(MC_REFLECT_ENUM_FROM_STRING, TYPE, VALUES)                               \
    MC_REFLECT_ENUM_FROM_STRING_END(TYPE)

#endif // MC_REFLECT_REFLECT_H