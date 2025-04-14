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

// Boost.Preprocessor
#include <boost/preprocessor/control/if.hpp>
#include <boost/preprocessor/control/iif.hpp>
#include <boost/preprocessor/facilities/is_empty.hpp>
#include <boost/preprocessor/logical/compl.hpp>
#include <boost/preprocessor/punctuation/is_begin_parens.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>

#include <mc/common.h>
#include <mc/dict.h>
#include <mc/reflect/metadata_info.h>
#include <mc/variant.h>

namespace mc::reflect {
[[noreturn]] void throw_bad_enum_cast(int64_t i, const char* e);
[[noreturn]] void throw_bad_enum_cast(const char* k, const char* e);
} // namespace mc::reflect

// 检测是否为元组形式（双括号表达式）
#define MC_REFLECT_IS_TUPLE(x) BOOST_PP_IS_BEGIN_PARENS(x)

// 处理带名称的成员（双括号形式）
#define MC_REFLECT_MEMBER_WITH_NAME(r, TYPE, MEMBER)                                               \
    mc::reflect::detail::create_member_info<TYPE>(&TYPE::BOOST_PP_TUPLE_ELEM(0, MEMBER),           \
                                                  BOOST_PP_TUPLE_ELEM(1, MEMBER)),

// 处理不带名称的成员（单括号形式）
#define MC_REFLECT_MEMBER_WITHOUT_NAME(r, TYPE, MEMBER)                                            \
    mc::reflect::detail::create_member_info<TYPE>(&TYPE::MEMBER, BOOST_PP_STRINGIZE(MEMBER)),

// 处理成员
#define MC_REFLECT_ELEMENT(r, TYPE, MEMBER)                                                        \
    BOOST_PP_IIF(MC_REFLECT_IS_TUPLE(MEMBER), MC_REFLECT_MEMBER_WITH_NAME,                         \
                 MC_REFLECT_MEMBER_WITHOUT_NAME)(r, TYPE, MEMBER)

// 在命名空间中添加辅助函数
namespace mc::reflect::detail {
// 创建成员元数据（根据成员类型分发到属性、方法或用户自定义的成员信息）
template <typename T, typename M, typename BaseT>
constexpr auto create_member_info(M BaseT::* member_ptr, std::string_view name) {
    return member_info_creator<T, M, BaseT>::create(member_ptr, name);
}

// 检查元组元素是否具有特定的标签类型
template <typename Tag, typename T, typename = void>
struct has_tag : std::false_type {};

template <typename Tag, typename T>
struct has_tag<Tag, T, std::enable_if_t<std::is_same_v<typename T::tag_type, Tag>>>
    : std::true_type {};

template <typename Tag, typename T>
inline constexpr bool has_tag_v = has_tag<Tag, T>::value;

// 辅助函数：获取具有特定索引的元组元素
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
        static const auto& get_members() {                                                         \
            static auto members = std::tuple_cat(                                                  \
                BOOST_PP_SEQ_FOR_EACH(MC_REFLECT_ELEMENT, TYPE, MEMBERS) std::tuple<>());          \
            return members;                                                                        \
        }                                                                                          \
        template <typename Tag>                                                                    \
        static const auto& get_members_by_tag() {                                                  \
            static auto filtered_members =                                                         \
                mc::reflect::detail::filter_members<Tag>(get_members());                           \
            return filtered_members;                                                               \
        }                                                                                          \
        static const auto& get_properties() {                                                      \
            return get_members_by_tag<mc::reflect::property_tag>();                                \
        }                                                                                          \
        static const auto& get_methods() {                                                         \
            return get_members_by_tag<mc::reflect::method_tag>();                                  \
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