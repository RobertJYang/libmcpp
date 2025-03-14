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

#include <boost/preprocessor/seq/enum.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/size.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <mc/common.h>
#include <mc/dict.h>
#include <mc/exception.h>
#include <mc/reflect/typename.h>
#include <mc/variant.h>

/**
 * @brief 定义成员反射信息
 */
#define MC_REFLECT_MEMBER(r, TYPE, MEMBER)                                                         \
    {BOOST_PP_STRINGIZE(MEMBER),                                                                   \
                        [](const TYPE& obj) -> variant {                                           \
                            return obj.MEMBER;                                                     \
                        },                                                                         \
                        [](TYPE& obj, const variant& var) {                                        \
                            var.as(obj.MEMBER);                                                    \
                        }},

/**
 * @brief 开始定义类的反射信息
 */
#define MC_REFLECT_BEGIN(TYPE)                                                                     \
    namespace mc {                                                                                 \
    template <>                                                                                    \
    struct reflector<TYPE> {                                                                       \
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
    struct reflector<TYPE> {                                                                       \
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
        throw bad_enum_cast("无效的枚举值");                                                       \
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
    throw bad_enum_cast("无效的枚举字符串");                                                       \
    }                                                                                              \
    }                                                                                              \
    ;                                                                                              \
    } // namespace mc

namespace mc {
namespace reflect {

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
 * @brief 枚举转换异常类
 */
class bad_enum_cast : public mc::exception {
public:
    enum code_enum {
        code_value = mc::bad_cast_exception_code,
    };

    explicit bad_enum_cast(const std::string& msg)
        : mc::exception(mc::bad_cast_exception_code, "bad_enum_cast", msg) {
    }
};

} // namespace reflect
} // namespace mc

#endif // MC_REFLECT_REFLECT_H