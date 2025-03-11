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
 * @file variant.h
 * @brief 反射相关的变体定义
 */
#ifndef MC_REFLECT_VARIANT_H
#define MC_REFLECT_VARIANT_H

#include <mc/variant.h>
#include <mc/dict.h>
#include <string>
#include <functional>
#include <optional>

namespace mc {
namespace reflect {

// 前置声明
template<typename T>
struct reflector;

/**
 * @brief 将对象转换为变体
 * 
 * @tparam T 对象类型
 * @param obj 对象实例
 * @param var 转换后的变体
 */
template<typename T>
void to_variant(const T& obj, mc::mutable_dict& dict) {
    reflector<T>::to_variant(obj, dict);
}

/**
 * @brief 从变体转换为对象
 * 
 * @tparam T 对象类型
 * @param var 变体实例
 * @param obj 转换后的对象
 */
template<typename T>
void from_variant(const mc::dict& dict, T& obj) {
    reflector<T>::from_variant(dict, obj);
}

} // namespace reflect
} // namespace mc

#endif // MC_REFLECT_VARIANT_H 