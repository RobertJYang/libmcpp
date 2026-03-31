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

#include <boost/preprocessor/comparison/greater.hpp>
#include <boost/preprocessor/control/iif.hpp>
#include <boost/preprocessor/variadic/size.hpp>

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

// 声明类型或者枚举是可反射的
// @param name 反射名称，用于在反射系统中标识类型或者枚举，支持多级命名空间，以 . 或 ::
// 分隔，例如：mc.devices.TemperatureSensor
// @param ... 枚举类型
// @note 为了使用方便，我们支持在类内部添加 MC_REFLECTABLE 或在全局命名空间 MC_REFLECTABLE 来标记该类支持反射。
// 例如：
// namespace mc::devices {
// struct TemperatureSensor {
//     MC_REFLECTABLE("mc.devices.TemperatureSensor"); // 在类声明内部声明反射
// };
// } // namespace mc::devices
// 或者
// namespace {
//     MC_REFLECTABLE("mc.devices.TemperatureSensor", mc::devices::TemperatureSensor); // 在全局命名空间声明反射
// }
// 优先建议使用内部声明的方式声明反射，但如过想对一些第三方类声明反射，则必须使用全局声明的方式。
// 另外对于枚举类型，也只能使用全局的方式声明反射。
// 我们通过识别 MC_REFLECTABLE 的参数个数来决定是全局反射还是类内反射，这样可以避免再增加一个宏名称，
// 如果 ... 可变参数大于一个参数，则表示是全局反射，否则表示是类内反射。
// 建议在反射系统中的名称保持与C++中的命名空间一致，避免混淆，为此反射系统中也支持 :: 分隔的命名空间。
#define MC_REFLECTABLE(...)                                                                                            \
    BOOST_PP_IIF(BOOST_PP_GREATER(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), 1), MC_GLOBAL_REFLECTABLE,                      \
                 MC_CLASS_REFLECTABLE)                                                                                 \
    (__VA_ARGS__)

#endif // MC_REFLECT_REFLECTABLE_MACRO_H
