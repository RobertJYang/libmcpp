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
 * @brief 包含所有反射相关的头文件
 */
#ifndef MC_REFLECT_H
#define MC_REFLECT_H

#include <mc/reflect/typename.h>
#include <mc/reflect/reflect.h>
#include <mc/reflect/variant.h>

/**
 * @namespace mc
 * @brief mc 命名空间
 */
namespace mc {

/**
 * @namespace reflect
 * @brief 反射相关的命名空间
 */
namespace reflect {

/**
 * @brief 检查类型是否可反射
 * 
 * @tparam T 要检查的类型
 * @return true 如果类型可反射
 * @return false 如果类型不可反射
 */
template<typename T>
constexpr bool is_reflectable() {
    return reflector<T>::is_defined::value;
}

/**
 * @brief 检查类型是否为枚举
 * 
 * @tparam T 要检查的类型
 * @return true 如果类型是枚举
 * @return false 如果类型不是枚举
 */
template<typename T>
constexpr bool is_enum() {
    return reflector<T>::is_enum::value;
}

/**
 * @brief 获取类型名称
 * 
 * @tparam T 要获取名称的类型
 * @return const char* 类型名称
 */
template<typename T>
const char* get_type_name() {
    return get_typename<T>::name();
}

/**
 * @brief 访问类型的所有成员
 * 
 * @tparam T 要访问的类型
 * @tparam Visitor 访问器类型
 * @param visitor 访问器实例
 */
template<typename T, typename Visitor>
void visit_members(const Visitor& visitor) {
    reflector<T>::visit(visitor);
}

/**
 * @brief 将对象转换为变体
 * 
 * @tparam T 对象类型
 * @param obj 对象实例
 * @param var 转换后的变体
 */
template<typename T>
void to_variant(const T& obj, variant& var) {
    if constexpr (reflector<T>::is_enum::value) {
        reflector<T>::to_variant(obj, var);
    } else {
        mc::mutable_dict dict;
        reflect::to_variant(obj, dict);
        var = variant(dict);
    }
}

/**
 * @brief 将对象转换为变体
 * 
 * @tparam T 对象类型
 * @param obj 对象实例
 * @return variant 转换后的变体
 */
template<typename T>
variant to_variant(const T& obj) {
    variant var;
    to_variant(obj, var);
    return var;
}

/**
 * @brief 从变体转换为对象
 * 
 * @tparam T 对象类型
 * @param var 变体实例
 * @param obj 转换后的对象
 */
template<typename T>
void from_variant(const variant& var, T& obj) {
    if constexpr (reflector<T>::is_enum::value) {
        reflector<T>::from_variant(var, obj);
    } else {
        const mc::dict& d = var.as<mc::dict>();
        reflect::from_variant(d, obj);
    }
}

/**
 * @brief 从变体转换为对象
 * 
 * @tparam T 对象类型
 * @param var 变体实例
 * @return T 转换后的对象
 */
template<typename T>
T from_variant(const variant& var) {
    T obj;
    from_variant(var, obj);
    return obj;
}

} // namespace reflect
} // namespace mc

#endif // MC_REFLECT_H 