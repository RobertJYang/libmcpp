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

#include <array>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

#include <mc/common.h>
#include <mc/dict.h>
#include <mc/reflect/metadata_info.h>
#include <mc/reflect/signature.h>
#include <mc/traits.h>
#include <mc/variant.h>

namespace mc::reflect {
[[noreturn]] void throw_bad_enum_cast(int64_t i, const char* e);
[[noreturn]] void throw_bad_enum_cast(const char* k, const char* e);
[[noreturn]] void throw_variant_cast(const char* k, const char* e);
} // namespace mc::reflect

// 检测是否为元组形式（双括号表达式）
#define MC_REFLECT_IS_TUPLE(x) BOOST_PP_IS_BEGIN_PARENS(x)

#define MC_REFLECT_ARG_COUNT(...) BOOST_PP_VARIADIC_SIZE(__VA_ARGS__)

// 处理带名称的成员（双括号形式）
#define MC_REFLECT_MEMBER_WITH_NAME_1(r, TYPE, MEMBER)                                   \
    mc::reflect::detail::create_member_info<TYPE>(&TYPE::BOOST_PP_TUPLE_ELEM(0, MEMBER), \
                                                  BOOST_PP_TUPLE_ELEM(1, MEMBER)),

#define MC_REFLECT_MEMBER_WITH_NAME_2(r, TYPE, MEMBER)                                    \
    mc::reflect::detail::create_member_info<TYPE>(                                        \
        &TYPE::BOOST_PP_TUPLE_ELEM(2, MEMBER),                                            \
        BOOST_PP_IIF(BOOST_PP_GREATER(BOOST_PP_TUPLE_SIZE(MEMBER), 3),                    \
                     &TYPE::BOOST_PP_TUPLE_ELEM(3, MEMBER), static_cast<void*>(nullptr)), \
        BOOST_PP_TUPLE_ELEM(1, MEMBER)),

#define MC_REFLECT_MEMBER_WITH_NAME(r, TYPE, MEMBER)                                              \
    BOOST_PP_IIF(BOOST_PP_GREATER(BOOST_PP_TUPLE_SIZE(MEMBER), 2), MC_REFLECT_MEMBER_WITH_NAME_2, \
                 MC_REFLECT_MEMBER_WITH_NAME_1)(r, TYPE, MEMBER)

// 处理不带名称的成员（单括号形式）
#define MC_REFLECT_MEMBER_WITHOUT_NAME(r, TYPE, MEMBER)                        \
    BOOST_PP_IIF(BOOST_PP_IS_EMPTY(MEMBER), std::tuple{},                      \
                 (mc::reflect::detail::create_member_info<TYPE>(&TYPE::MEMBER, \
                                                                BOOST_PP_STRINGIZE(MEMBER)))),

// 处理成员
#define MC_REFLECT_ELEMENT(r, TYPE, MEMBER)                                \
    BOOST_PP_IIF(MC_REFLECT_IS_TUPLE(MEMBER), MC_REFLECT_MEMBER_WITH_NAME, \
                 MC_REFLECT_MEMBER_WITHOUT_NAME)(r, TYPE, MEMBER)

// 简单基类，没有别名
#define MC_REFLECT_BASE_CLASS_WITHOUT_NAME(r, TYPE, BASE) \
    BOOST_PP_IIF(                                         \
        BOOST_PP_IS_EMPTY(BASE), std::tuple{},            \
        (mc::reflect::detail::create_base_class_info<TYPE, BASE>())),

// 带别名的基类
#define MC_REFLECT_BASE_CLSS_WITH_NAME(r, TYPE, BASE)                                \
    mc::reflect::detail::create_base_class_info<TYPE, BOOST_PP_TUPLE_ELEM(0, BASE)>( \
        BOOST_PP_TUPLE_ELEM(1, BASE)),

// 处理基类
#define MC_REFLECT_BASE_ELEMENT(r, TYPE, BASE)                              \
    BOOST_PP_IIF(MC_REFLECT_IS_TUPLE(BASE), MC_REFLECT_BASE_CLSS_WITH_NAME, \
                 MC_REFLECT_BASE_CLASS_WITHOUT_NAME)(r, TYPE, BASE)

/**
 * @brief 定义枚举值的反射信息
 */
#define MC_REFLECT_ENUM_VALUE(r, ENUM, VALUE) \
    case ENUM::VALUE:                         \
        var = BOOST_PP_STRINGIZE(VALUE);      \
        break;

/**
 * @brief 定义枚举值的字符串转换
 */
#define MC_REFLECT_ENUM_FROM_STRING(r, ENUM, VALUE) \
    if (var == BOOST_PP_STRINGIZE(VALUE)) {         \
        e = ENUM::VALUE;                            \
        return;                                     \
    }

// 在命名空间中添加辅助函数
namespace mc::reflect::detail {
// 创建成员元数据（根据成员类型分发到属性、方法或用户自定义的成员信息）
template <typename T, typename M, typename BaseT>
constexpr auto create_member_info(M BaseT::* member_ptr, std::string_view name) {
    return member_info_creator<T, M, BaseT>::create(member_ptr, name);
}

template <typename T, typename Getter, typename Setter>
constexpr auto create_member_info(Getter getter, Setter setter, std::string_view name) {
    return computed_property_info_creator::create<T>(getter, setter, name);
}

template <typename T, typename R, typename... Args>
constexpr auto create_member_info(R (*static_func)(Args...), std::string_view name) {
    return member_info_creator<T, R (*)(Args...), void>::create(static_func, name);
}

template <typename T, typename BaseT>
constexpr auto create_base_class_info(std::string_view name = {}) {
    return base_class_info_creator<T, BaseT>::create(name);
}

template <size_t Index, typename... Tuples>
bool is_member_override(const member_info_base&      base_member,
                        const std::tuple<Tuples...>& all_members) {
    if constexpr (Index >= sizeof...(Tuples)) {
        return false;
    } else {
        if (std::get<Index>(all_members).name == base_member.name) {
            return true;
        } else {
            return is_member_override<Index + 1>(base_member, all_members);
        }
    }
}

template <size_t BaseIndex, size_t Index, typename... BaseTuples, typename... Tuples>
void filter_base_members(const std::tuple<BaseTuples...>& base_members,
                         const std::tuple<Tuples...>&     all_members) {
    if constexpr (BaseIndex < sizeof...(BaseTuples)) {
        const member_info_base& base_member = std::get<BaseIndex>(base_members);
        if (is_member_override<Index + 1>(base_member, all_members)) {
            base_member.is_override = 1;
        }
        filter_base_members<BaseIndex + 1, Index>(base_members, all_members);
    }
}

// 递归过滤元组元素，仅保留具有特定标签的元素
template <typename T, typename Derived, typename Filter, size_t Index, typename Result,
          typename... Tuples>
auto filter_members_impl(const std::tuple<Tuples...>& all_members, Result result) {
    if constexpr (Index >= sizeof...(Tuples)) {
        return result;
    } else {
        using element_type =
            mc::traits::remove_cvref_t<std::tuple_element_t<Index, std::tuple<Tuples...>>>;
        if constexpr (Filter::template check<element_type>) {
            auto member = std::get<Index>(all_members);
            if constexpr (!std::is_same_v<T, Derived> && std::is_base_of_v<T, Derived>) {
                // 修复基类信息到继承类
                auto member_tuple = std::make_tuple(detail::apply_to_derived<Derived>(member));
                return filter_members_impl<T, Derived, Filter, Index + 1>(
                    all_members, std::tuple_cat(result, member_tuple));
            } else {
                auto member_tuple = std::make_tuple(member);
                return filter_members_impl<T, Derived, Filter, Index + 1>(
                    all_members, std::tuple_cat(result, member_tuple));
            }
        } else if constexpr (has_tag_v<base_class_tag, element_type>) {
            // 整合基类的成员
            using base_type    = typename element_type::base_type;
            auto& base_members = mc::reflect::reflector<base_type>::get_members();
            auto  base_result =
                filter_members_impl<base_type, Derived, Filter, 0>(base_members, std::tuple<>{});
            filter_base_members<0, Index + 1>(base_result, all_members);
            return filter_members_impl<T, Derived, Filter, Index + 1>(
                all_members, std::tuple_cat(result, base_result));
        } else {
            return filter_members_impl<T, Derived, Filter, Index + 1>(all_members, result);
        }
    }
}

template <typename T, typename Filter, typename Tuple>
auto filter_members(const Tuple& all_members) {
    return filter_members_impl<T, T, Filter, 0>(all_members, std::tuple<>{});
}

template <typename Tag>
struct filter_tag {
    template <typename ElementType>
    static constexpr bool check = has_tag_v<Tag, ElementType>;
};

template <typename T, typename Tag, typename Tuple>
auto filter_members_by_tag(const Tuple& all_members) {
    return filter_members<T, filter_tag<Tag>>(all_members);
}

// 默认的成员检查模板，总是返回true
template <typename T, typename Member, typename = void>
struct has_members_check {
    static constexpr bool check(const Member&) {
        return true;
    }
};

// 检测类型是否提供了check_member模板函数
template <typename T, typename Members, typename = void>
struct has_check_members : std::false_type {};

template <typename T, typename Members>
struct has_check_members<
    T, Members, std::void_t<decltype(T::template check_members<Members>(std::declval<Members>()))>>
    : std::true_type {};

// 辅助函数：验证某个成员
template <typename T, typename Members>
static constexpr bool validate_members(const Members& members) {
    if constexpr (has_check_members<T, Members>::value) {
        return T::check_members(members);
    }
    return true;
}

// 检测类型是否提供了initial_member模板函数
template <typename T, typename Members, typename = void>
struct has_initial_members : std::false_type {};

template <typename T, typename Members>
struct has_initial_members<
    T, Members,
    std::void_t<decltype(T::template initial_members<Members>(std::declval<Members>()))>>
    : std::true_type {};

// 辅助函数：初始化某个成员
template <typename T, typename Members>
static auto initial_members(const Members& members) {
    if constexpr (has_initial_members<T, Members>::value) {
        T::initial_members(members);
    }
    return members;
}
} // namespace mc::reflect::detail

namespace mc {
namespace reflect {

namespace detail {

template <typename T, typename = void>
struct has_custom_to_variant : std::false_type {};

template <typename T>
struct has_custom_to_variant<T, std::void_t<decltype(T::to_variant(
                                    std::declval<const T&>(), std::declval<mc::mutable_dict&>()))>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_custom_to_variant_v = has_custom_to_variant<T>::value;

template <typename T>
auto has_custom_from_variant(int)
    -> decltype(T::from_variant(std::declval<const mc::dict&>(), std::declval<T&>()),
                std::true_type{});

template <typename T>
std::false_type has_custom_from_variant(...);

template <typename T>
inline constexpr bool has_custom_from_variant_v = decltype(has_custom_from_variant<T>(0))::value;

template <typename T>
void custom_from_variant(const mc::dict& d, T& obj) {
    if constexpr (has_custom_from_variant_v<T>) {
        T::from_variant(d, obj);
    }
}

template <typename T>
void custom_to_variant(const T& obj, mc::mutable_dict& dict) {
    if constexpr (has_custom_to_variant_v<T>) {
        T::to_variant(obj, dict);
    }
}

} // namespace detail

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
    } else if (var.is_dict()) {
        // 支持从{key: value}字典转换为对象
        reflector<T>::from_variant(var.get_object(), obj);
    } else if (var.is_array()) {
        // 支持从[value1, value2, ...]数组转换为对象
        std::size_t index = 0;
        mc::traits::tuple_for_each(reflector<T>::get_properties(), [&](auto& v) {
            if (index < var.size()) {
                v.set_value(obj, var[index]);
            }
            ++index;
        });
    } else {
        throw_variant_cast(get_type_name<T>().data(), var.get_type_name());
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

template <typename T, std::enable_if_t<mc::reflect::is_normal_enum<T>(), int> = 0>
void to_variant(const T& o, variant& v) {
    v = mc::variant(static_cast<int32_t>(o));
}

template <typename T, std::enable_if_t<mc::reflect::is_normal_enum<T>(), int> = 0>
void from_variant(const variant& v, T& o) {
    o = static_cast<T>(v.as_int32());
}

} // namespace mc

namespace mc::reflect::detail {

// 对反射类型的特化
template <typename T>
struct signature_helper<T, std::enable_if_t<mc::reflect::is_reflectable<T>()>> {
    static void apply(std::string& sig) {
        if constexpr (mc::reflect::is_enum<T>()) {
            // 可反射的枚举类型用的是 string
            sig += type_to_char(type_code::string_type);
        } else {
            sig += type_to_char(type_code::struct_start);
            mc::traits::tuple_for_each(mc::reflect::reflector<T>::get_properties(), [&](auto& p) {
                using member_type = typename std::decay_t<decltype(p)>::member_type;
                signature_helper<member_type>::apply(sig);
            });
            sig += type_to_char(type_code::struct_end);
        }
    }
};

// 普通的枚举类型用 int32_t 表示，可反射的枚举类型用的是 string
template <typename T>
struct signature_helper<T, std::enable_if_t<mc::reflect::is_normal_enum<T>()>> {
    static void apply(std::string& sig) {
        sig += type_to_char(type_code::int32_type);
    }
};

} // namespace mc::reflect::detail

/**
 * @brief 定义类的反射信息
 */
#define MC_REFLECT_IMPL(TYPE, NAME, BASES, MEMBERS)                                             \
    namespace mc::reflect {                                                                     \
    template <>                                                                                 \
    struct reflector<TYPE> {                                                                    \
        using is_defined = std::true_type;                                                      \
        using is_enum    = std::false_type;                                                     \
        static constexpr std::string_view name() {                                              \
            return NAME;                                                                        \
        }                                                                                       \
        static inline const int type_id = reflection_factory::instance().register_type<TYPE>(); \
        static int              get_type_id() {                                                 \
            return type_id;                                                        \
        }                                                                                       \
        static const auto& get_members() {                                                      \
            static auto members = mc::reflect::detail::initial_members<TYPE>(std::tuple_cat(    \
                BOOST_PP_SEQ_FOR_EACH(MC_REFLECT_BASE_ELEMENT, TYPE, BASES)                     \
                    BOOST_PP_SEQ_FOR_EACH(MC_REFLECT_ELEMENT, TYPE, MEMBERS) std::tuple<>()));  \
                                                                                                \
            static_assert(mc::reflect::detail::validate_members<TYPE>(members),                 \
                          "members validate failed, please check members type");                \
            return members;                                                                     \
        }                                                                                       \
                                                                                                \
        template <typename Tag>                                                                 \
        static const auto& get_members_by_tag() {                                               \
            static auto filtered_members =                                                      \
                mc::reflect::detail::filter_members_by_tag<TYPE, Tag>(get_members());           \
            return filtered_members;                                                            \
        }                                                                                       \
                                                                                                \
        static const auto& get_properties() {                                                   \
            return get_members_by_tag<mc::reflect::property_tag>();                             \
        }                                                                                       \
                                                                                                \
        static const auto& get_methods() {                                                      \
            return get_members_by_tag<mc::reflect::method_tag>();                               \
        }                                                                                       \
                                                                                                \
        static const auto& get_base_classes() {                                                 \
            return get_members_by_tag<mc::reflect::base_class_tag>();                           \
        }                                                                                       \
                                                                                                \
        template <typename Visitor>                                                             \
        static void visit(const Visitor& visitor) {                                             \
            mc::traits::tuple_for_each(get_properties(), [&](auto& member) {                    \
                if (member.is_override == 0) {                                                  \
                    visitor(member.name, member.getter(), member.setter());                     \
                }                                                                               \
            });                                                                                 \
        }                                                                                       \
                                                                                                \
        static void to_variant(const TYPE& obj, mc::mutable_dict& dict) {                       \
            if constexpr (mc::reflect::detail::has_custom_to_variant_v<TYPE>) {                 \
                mc::reflect::detail::custom_to_variant(obj, dict);                              \
            } else {                                                                            \
                visit([&](std::string_view name, auto getter, auto) {                           \
                    if (!dict.contains(name)) {                                                 \
                        dict[name] = getter(obj);                                               \
                    }                                                                           \
                });                                                                             \
            }                                                                                   \
        }                                                                                       \
                                                                                                \
        static void from_variant(const mc::dict& d, TYPE& obj) {                                \
            if constexpr (mc::reflect::detail::has_custom_from_variant_v<TYPE>) {               \
                mc::reflect::detail::custom_from_variant(d, obj);                               \
            } else {                                                                            \
                visit([&](std::string_view name, auto, auto setter) {                           \
                    if (d.contains(name)) {                                                     \
                        setter(obj, d[name]);                                                   \
                    }                                                                           \
                });                                                                             \
            }                                                                                   \
        }                                                                                       \
    };                                                                                          \
    }

#define MC_REFLECT_IMPL_WITH_NAME(r, TYPE, BASES, MEMBERS) \
    MC_REFLECT_IMPL(BOOST_PP_TUPLE_ELEM(0, TYPE), BOOST_PP_TUPLE_ELEM(1, TYPE), BASES, MEMBERS)

#define MC_REFLECT_IMPL_WITHOUT_NAME(r, TYPE, BASES, MEMBERS) \
    MC_REFLECT_IMPL(TYPE, BOOST_PP_STRINGIZE(TYPE), BASES, MEMBERS)

#define MC_REFLECT_IMPL_DISPATCH(TYPE, ...)                            \
    BOOST_PP_IIF(MC_REFLECT_IS_TUPLE(TYPE), MC_REFLECT_IMPL_WITH_NAME, \
                 MC_REFLECT_IMPL_WITHOUT_NAME)(r, TYPE, __VA_ARGS__)

// 一个参数版本 - 只有类成员
#define MC_REFLECT_1(TYPE, MEMBERS) MC_REFLECT_IMPL_DISPATCH(TYPE, BOOST_PP_EMPTY(), MEMBERS)

// 二个参数版本 - 有基类和类成员
#define MC_REFLECT_2(TYPE, BASES, MEMBERS) MC_REFLECT_IMPL_DISPATCH(TYPE, BASES, MEMBERS)

#define MC_REFLECT_DISPATCH(TYPE, ...) \
    BOOST_PP_CAT(MC_REFLECT_, MC_REFLECT_ARG_COUNT(__VA_ARGS__))(TYPE, __VA_ARGS__)

#define MC_REFLECT(...) MC_REFLECT_DISPATCH(__VA_ARGS__)

/**
 * @brief 定义枚举的反射信息
 */
#define MC_REFLECT_ENUM_IMPL(TYPE, NAME, VALUES)                                  \
    namespace mc {                                                                \
    template <>                                                                   \
    struct reflect::reflector<TYPE> {                                             \
        using is_defined = std::true_type;                                        \
        using is_enum    = std::true_type;                                        \
        static constexpr std::string_view name() {                                \
            return NAME;                                                          \
        }                                                                         \
        static void to_variant(const TYPE& e, variant& var) {                     \
            switch (e) {                                                          \
                BOOST_PP_SEQ_FOR_EACH(MC_REFLECT_ENUM_VALUE, TYPE, VALUES)        \
            default:                                                              \
                mc::reflect::throw_bad_enum_cast(static_cast<int64_t>(e), #TYPE); \
            }                                                                     \
        }                                                                         \
        static void from_variant(const variant& var, TYPE& e) {                   \
            BOOST_PP_SEQ_FOR_EACH(MC_REFLECT_ENUM_FROM_STRING, TYPE, VALUES)      \
            mc::reflect::throw_bad_enum_cast(var.as_string().c_str(), #TYPE);     \
        }                                                                         \
    };                                                                            \
    }

#define MC_REFLECT_ENUM_WITH_NAME(r, TYPE, VALUES) \
    MC_REFLECT_ENUM_IMPL(BOOST_PP_TUPLE_ELEM(0, TYPE), BOOST_PP_TUPLE_ELEM(1, TYPE), VALUES)

#define MC_REFLECT_ENUM_WITHOUT_NAME(r, TYPE, VALUES) \
    MC_REFLECT_ENUM_IMPL(TYPE, BOOST_PP_STRINGIZE(TYPE), VALUES)

#define MC_REFLECT_ENUM_DISPATCH(TYPE, ...)                            \
    BOOST_PP_IIF(MC_REFLECT_IS_TUPLE(TYPE), MC_REFLECT_ENUM_WITH_NAME, \
                 MC_REFLECT_ENUM_WITHOUT_NAME)(r, TYPE, __VA_ARGS__)

#define MC_REFLECT_ENUM(TYPE, VALUES) \
    MC_REFLECT_ENUM_DISPATCH(TYPE, VALUES)

#endif // MC_REFLECT_REFLECT_H