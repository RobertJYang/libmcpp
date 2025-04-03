/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
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
 * @file property.h
 * @brief 实现基于反射的属性访问元数据，提高反射性能
 */
#ifndef MC_REFLECT_PROPERTY_H
#define MC_REFLECT_PROPERTY_H

#include <mc/reflect/reflect_metadata.h>

namespace mc::reflect {

/**
 * 获取对象属性值
 * @tparam T 类型
 * @param obj 对象
 * @param key 属性名
 * @return 返回 mc::variant
 */
template <typename T, typename = std::enable_if_t<is_reflectable<T>()>>
mc::variant get_property(const T& obj, std::string_view key) {
    return get_metadata<T>().get_property(obj, key);
}

/**
 * 设置对象属性值
 * @tparam T 类型
 * @param obj 对象
 * @param key 属性名
 * @param value 属性值
 * @return 设置是否成功
 */
template <typename T, typename = std::enable_if_t<is_reflectable<T>()>>
bool set_property(T& obj, std::string_view key, const mc::variant& value) {
    try {
        return get_metadata<T>().set_property(obj, key, value);
    } catch (const std::exception& e) {
        return false;
    }
}

} // namespace mc::reflect

#endif // MC_REFLECT_PROPERTY_H