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

/** @file method.h */
#ifndef MC_REFLECT_METHOD_H
#define MC_REFLECT_METHOD_H

#include <mc/reflect/reflection.h>
#include <mc/variant.h>

namespace mc::reflect {
template <typename T>
const method_info_base<T>* get_method_info(mc::string_view method_name)
{
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_reflection<clean_type>().get_method_info(method_name);
}

template <typename T, typename M>
const method_info_base<T>* get_method_info(M T::*member_ptr)
{
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_reflection<clean_type>().get_method_info(member_ptr);
}

template <typename T>
mc::variant invoke(T& obj, mc::string_view method_name, const mc::variants& args = {})
{
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_reflection<clean_type>().invoke_method(obj, method_name, args);
}

template <typename T>
mc::variant invoke(mc::string_view method_name, const mc::variants& args = {})
{
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_reflection<clean_type>().invoke_method(method_name, args);
}

template <typename T>
async_result async_invoke(T& obj, mc::string_view method_name, const mc::variants& args = {})
{
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_reflection<clean_type>().async_invoke_method(obj, method_name, args);
}

template <typename T>
async_result async_invoke(mc::string_view method_name, const mc::variants& args = {})
{
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_reflection<clean_type>().async_invoke_method(method_name, args);
}

template <typename T>
bool has_method(mc::string_view method_name)
{
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_reflection<clean_type>().get_method_info(method_name) != nullptr;
}

template <typename T>
size_t method_arg_count(mc::string_view method_name)
{
    using clean_type   = std::remove_cv_t<std::remove_reference_t<T>>;
    const auto* method = get_reflection<clean_type>().get_method_info(method_name);
    return method ? method->arg_count() : static_cast<size_t>(-1);
}

template <typename T>
mc::string_view method_return_type(mc::string_view method_name)
{
    using clean_type   = std::remove_cv_t<std::remove_reference_t<T>>;
    const auto* method = get_reflection<clean_type>().get_method_info(method_name);
    return method ? method->get_result_signature() : mc::string_view{};
}

} // namespace mc::reflect

#endif // MC_REFLECT_METHOD_H