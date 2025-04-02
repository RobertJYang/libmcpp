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
#include <mc/variant.h>

/**
 * @brief 辅助宏，用于检测是否为元组形式的成员
 * 检测方法：BOOST_PP_IS_BEGIN_PARENS 检测是否以左括号开头
 */
#define MC_REFLECT_IS_TUPLE(x) BOOST_PP_IS_BEGIN_PARENS(x)

/**
 * @brief 处理简单成员（使用成员名称作为反射名称）
 */
#define MC_REFLECT_MEMBER_SIMPLE(r, TYPE, MEMBER)                                                  \
    {BOOST_PP_STRINGIZE(MEMBER),                                                                   \
                        [](const TYPE& obj) -> variant {                                           \
                            return obj.MEMBER;                                                     \
                        },                                                                         \
                        [](TYPE& obj, const variant& var) {                                        \
                            var.as(obj.MEMBER);                                                    \
                        }},

/**
 * @brief 处理自定义名称的成员
 */
#define MC_REFLECT_MEMBER_CUSTOM(r, TYPE, MEMBER)                                                  \
    {BOOST_PP_TUPLE_ELEM(1, MEMBER),                                                               \
     [](const TYPE& obj) -> variant {                                                              \
         return obj.BOOST_PP_TUPLE_ELEM(0, MEMBER);                                                \
     },                                                                                            \
     [](TYPE& obj, const variant& var) {                                                           \
         var.as(obj.BOOST_PP_TUPLE_ELEM(0, MEMBER));                                               \
     }},

/**
 * @brief 处理成员反射定义
 *
 * 根据成员的格式自动选择使用简单方式还是自定义名称方式
 * 这里我们使用 BOOST_PP_IIF 来条件选择不同的宏
 */
#define MC_REFLECT_MEMBER(r, TYPE, MEMBER)                                                         \
    BOOST_PP_IIF(MC_REFLECT_IS_TUPLE(MEMBER), MC_REFLECT_MEMBER_CUSTOM,                            \
                 MC_REFLECT_MEMBER_SIMPLE)(r, TYPE, MEMBER)

/**
 * @brief 开始定义类的反射信息
 */
#define MC_REFLECT_BEGIN(TYPE)                                                                     \
    namespace mc {                                                                                 \
    template <>                                                                                    \
    struct reflect::reflector<TYPE> {                                                              \
        using is_defined = std::true_type;                                                         \
        using is_enum    = std::false_type;                                                        \
        static const char* name() {                                                                \
            return #TYPE;                                                                          \
        }                                                                                          \
        template <typename Visitor>                                                                \
        static void visit(const Visitor& visitor) {                                                \
            static const std::vector<member_info<TYPE>> members = {
/**
 * @brief 结束定义类的反射信息
 */
#define MC_REFLECT_END(TYPE)                                                                       \
    }                                                                                              \
    ;                                                                                              \
    for (const auto& member : members) {                                                           \
        visitor(member.name, member.getter, member.setter);                                        \
    }                                                                                              \
    }                                                                                              \
    static void to_variant(const TYPE& obj, mc::mutable_dict& dict) {                              \
        visit([&](const char* name, auto getter, auto) {                                           \
            dict[name] = getter(obj);                                                              \
        });                                                                                        \
    }                                                                                              \
    static void from_variant(const mc::dict& d, TYPE& obj) {                                       \
        visit([&](const char* name, auto, auto setter) {                                           \
            if (d.contains(name)) {                                                                \
                setter(obj, d[name]);                                                              \
            }                                                                                      \
        });                                                                                        \
    }                                                                                              \
    }                                                                                              \
    ;                                                                                              \
    } // namespace mc

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
 * @brief 成员信息结构体
 *
 * @tparam T 类型
 */
template <typename T>
struct member_info {
    const char*                             name;
    std::function<variant(const T&)>        getter;
    std::function<void(T&, const variant&)> setter;
};

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
 * @return const char* 类型名称
 */
template <typename T>
const char* get_type_name() {
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
void visit_members(const Visitor& visitor) {
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
    MC_REFLECT_BEGIN(TYPE)                                                                         \
    BOOST_PP_SEQ_FOR_EACH(MC_REFLECT_MEMBER, TYPE, MEMBERS)                                        \
    MC_REFLECT_END(TYPE)

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