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

/** @file property.h */
#ifndef MC_REFLECT_PROPERTY_H
#define MC_REFLECT_PROPERTY_H

#include <mc/reflect/reflection.h>

namespace mc::reflect {

template <typename T>
const property_info_base<T>* get_property_info(mc::string_view name)
{
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_reflection<clean_type>().get_property_info(name);
}

// quark 重载
template <typename T>
const property_info_base<T>* get_property_info(mc::quark name)
{
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_reflection<clean_type>().get_property_info(name);
}

template <typename T>
mc::string_view get_property_name(size_t offset)
{
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_reflection<clean_type>().get_property_name(offset);
}

template <typename T, typename M, typename BaseT>
mc::string_view get_property_name(M BaseT::*member)
{
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_reflection<clean_type>().get_property_name(member);
}

template <typename T, typename = std::enable_if_t<is_reflectable<std::remove_cv_t<std::remove_reference_t<T>>>()>>
mc::variant get_property(const T& obj, mc::string_view key)
{
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_reflection<clean_type>().get_property(obj, key);
}

template <typename T, typename = std::enable_if_t<is_reflectable<std::remove_cv_t<std::remove_reference_t<T>>>()>>
mc::variant get_property(const T& obj, mc::quark key)
{
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_reflection<clean_type>().get_property(obj, key);
}

template <typename T, typename = std::enable_if_t<is_reflectable<std::remove_cv_t<std::remove_reference_t<T>>>()>>
mc::variant get_property(const T& obj, mc::string_view key, mc::string_view base_class_name)
{
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_reflection<clean_type>().get_property(obj, key, base_class_name);
}

template <typename T, typename = std::enable_if_t<is_reflectable<std::remove_cv_t<std::remove_reference_t<T>>>()>>
mc::dict get_all_properties(const T& obj)
{
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_reflection<clean_type>().get_all_properties(obj);
}

template <typename T, typename = std::enable_if_t<is_reflectable<std::remove_cv_t<std::remove_reference_t<T>>>()>>
bool set_property(T& obj, mc::string_view key, const mc::variant& value)
{
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    try {
        return get_reflection<clean_type>().set_property(obj, key, value);
    } catch (const std::exception& e) {
        return false;
    }
}

template <typename T, typename = std::enable_if_t<is_reflectable<std::remove_cv_t<std::remove_reference_t<T>>>()>>
bool set_property(T& obj, mc::quark key, const mc::variant& value)
{
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    try {
        return get_reflection<clean_type>().set_property(obj, key, value);
    } catch (const std::exception& e) {
        return false;
    }
}

template <typename T, typename = std::enable_if_t<is_reflectable<std::remove_cv_t<std::remove_reference_t<T>>>()>>
bool set_property(T& obj, mc::string_view key, mc::string_view base_class_name, const mc::variant& value)
{
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    try {
        return get_reflection<clean_type>().set_property(obj, key, base_class_name, value);
    } catch (const std::exception& e) {
        return false;
    }
}

} // namespace mc::reflect

#endif // MC_REFLECT_PROPERTY_H