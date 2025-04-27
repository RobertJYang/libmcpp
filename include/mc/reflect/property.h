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
 * @brief 获取指定名称的成员信息
 *
 * @tparam T 类型
 * @param name 成员名称
 * @return const property_info_base<T>* 成员信息指针，如果不存在则返回nullptr
 */
template <typename T>
const property_info_base<T>* get_property_info(std::string_view name) {
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_metadata<clean_type>().get_property_info(name);
}

/**
 * @brief 根据偏移量获取成员名称
 *
 * @tparam T 类型
 * @param offset 偏移量
 * @return std::string_view 成员名称
 */
template <typename T>
std::string_view get_property_name(size_t offset) {
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_metadata<clean_type>().get_property_name(offset);
}

/**
 * @brief 根据成员指针获取成员名称
 *
 * @tparam T 类型
 * @tparam M 成员类型
 * @param member 成员指针
 * @return std::string_view 成员名称
 */
template <typename T, typename M, typename BaseT>
std::string_view get_property_name(M BaseT::* member) {
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_metadata<clean_type>().get_property_name(member);
}

/**
 * 获取对象属性值
 * @tparam T 类型
 * @param obj 对象
 * @param key 属性名
 * @return 返回 mc::variant
 */
template <typename T, typename = std::enable_if_t<
                          is_reflectable<std::remove_cv_t<std::remove_reference_t<T>>>()>>
mc::variant get_property(const T& obj, std::string_view key) {
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_metadata<clean_type>().get_property(obj, key);
}

/**
 * 获取对象所有属性值
 * @tparam T 类型
 * @param obj 对象
 * @return 返回 mc::dict
 */
template <typename T, typename = std::enable_if_t<
                          is_reflectable<std::remove_cv_t<std::remove_reference_t<T>>>()>>
mc::dict get_all_properties(const T& obj) {
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_metadata<clean_type>().get_all_properties(obj);
}

/**
 * 设置对象属性值
 * @tparam T 类型
 * @param obj 对象
 * @param key 属性名
 * @param value 属性值
 * @return 设置是否成功
 */
template <typename T, typename = std::enable_if_t<
                          is_reflectable<std::remove_cv_t<std::remove_reference_t<T>>>()>>
bool set_property(T& obj, std::string_view key, const mc::variant& value) {
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    try {
        return get_metadata<clean_type>().set_property(obj, key, value);
    } catch (const std::exception& e) {
        return false;
    }
}

} // namespace mc::reflect

#endif // MC_REFLECT_PROPERTY_H