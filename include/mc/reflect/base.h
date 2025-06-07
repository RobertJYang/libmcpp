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
#ifndef MC_REFLECT_BASE_H
#define MC_REFLECT_BASE_H

#include <mc/variant.h>
#include <string_view>

namespace mc::reflect {
// 异常抛出辅助函数
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

} // namespace mc::reflect

#endif // MC_REFLECT_BASE_H