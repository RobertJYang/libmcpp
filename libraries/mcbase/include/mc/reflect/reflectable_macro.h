/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef MC_REFLECT_REFLECTABLE_MACRO_H
#define MC_REFLECT_REFLECTABLE_MACRO_H

#include <mc/reflect/base.h>

#include <mc/pp.h>

/**
 * @brief 定义类的反射信息
 */
#define MC_GLOBAL_REFLECTABLE(name, TYPE)                                                                              \
    template <>                                                                                                        \
    struct mc::reflect::reflectable<TYPE> {                                                                            \
        using is_defined = std::true_type;                                                                             \
        using is_enum    = std::conditional_t<std::is_enum_v<TYPE>, std::true_type, std::false_type>;                  \
        constexpr static mc::string_view reflect_name = mc::reflect::detail::reflect_name_from_literal(name);          \
    };

#define MC_CLASS_REFLECTABLE(name)                                                                                     \
    using is_reflectable = std::true_type;                                                                             \
    template <typename, typename>                                                                                      \
    friend struct mc::reflect::reflector;                                                                              \
    template <typename>                                                                                                \
    friend struct mc::reflect::static_metadata;                                                                        \
    constexpr static mc::string_view reflect_name = mc::reflect::detail::reflect_name_from_literal(name);

// 声明类型或枚举支持反射。
// 单参数用于类内声明；多参数用于全局声明。
#define MC_REFLECTABLE(...)                                                                                            \
    MC_PP_IIF(MC_PP_GREATER(MC_PP_VARIADIC_SIZE(__VA_ARGS__), 1), MC_GLOBAL_REFLECTABLE,                      \
                 MC_CLASS_REFLECTABLE)                                                                                 \
    (__VA_ARGS__)

#endif // MC_REFLECT_REFLECTABLE_MACRO_H
