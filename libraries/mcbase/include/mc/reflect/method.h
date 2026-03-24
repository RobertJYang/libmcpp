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
 * @file method.h
 * @brief 反射方法调用辅助函数
 */
#ifndef MC_REFLECT_METHOD_H
#define MC_REFLECT_METHOD_H

#include <mc/reflect/reflection.h>
#include <mc/variant.h>

namespace mc::reflect {
/**
 * @brief 获取指定名称的方法信息
 *
 * @tparam T 类型
 * @param method_name 方法名称
 * @return const method_info_base<T>* 方法信息指针，如果不存在则返回nullptr
 */
template <typename T>
const method_info_base<T>* get_method_info(mc::string_view method_name)
{
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_reflection<clean_type>().get_method_info(method_name);
}

/**
 * @brief 获取指定名称的方法信息
 *
 * @tparam T 类型
 * @param method_name 方法名称
 * @return const method_info_base<T>* 方法信息指针，如果不存在则返回nullptr
 */
template <typename T, typename M>
const method_info_base<T>* get_method_info(M T::*member_ptr)
{
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_reflection<clean_type>().get_method_info(member_ptr);
}

/**
 * @brief 调用对象的方法
 *
 * @tparam T 对象类型
 * @param obj 对象实例
 * @param method_name 方法名
 * @param args 方法参数
 * @return mc::variant 方法返回值
 *
 * 示例:
 * ```cpp
 * MyClass obj;
 * // 调用无参数方法
 * auto result1 =  mc::reflect::invoke(obj, "no_arg_method");
 * // 调用有参数方法
 * auto result2 = mc::reflect::invoke(obj, "method_with_args", {1, "arg2", 3.14});
 * ```
 */
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

/**
 * @brief 异步调用对象的方法
 *
 * @tparam T 对象类型
 * @param obj 对象实例
 * @param method_name 方法名
 * @param args 方法参数
 * @return mc::variant 方法返回值
 */
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

/**
 * @brief 检查对象是否有指定名称的方法
 *
 * @tparam T 对象类型
 * @param method_name 方法名
 * @return bool 是否存在方法
 */
template <typename T>
bool has_method(mc::string_view method_name)
{
    using clean_type = std::remove_cv_t<std::remove_reference_t<T>>;
    return get_reflection<clean_type>().get_method_info(method_name) != nullptr;
}

/**
 * @brief 获取对象方法的参数数量
 *
 * @tparam T 对象类型
 * @param method_name 方法名
 * @return size_t 参数数量，如果方法不存在返回size_t(-1)
 */
template <typename T>
size_t method_arg_count(mc::string_view method_name)
{
    using clean_type   = std::remove_cv_t<std::remove_reference_t<T>>;
    const auto* method = get_reflection<clean_type>().get_method_info(method_name);
    return method ? method->arg_count() : static_cast<size_t>(-1);
}

/**
 * @brief 获取对象方法的返回类型名称
 *
 * @tparam T 对象类型
 * @param method_name 方法名
 * @return mc::string_view 返回类型名称，如果方法不存在返回空字符串
 */
template <typename T>
mc::string_view method_return_type(mc::string_view method_name)
{
    using clean_type   = std::remove_cv_t<std::remove_reference_t<T>>;
    const auto* method = get_reflection<clean_type>().get_method_info(method_name);
    return method ? method->get_result_signature() : mc::string_view{};
}

} // namespace mc::reflect

#endif // MC_REFLECT_METHOD_H