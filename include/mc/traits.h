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

#ifndef MC_TRAITS_H
#define MC_TRAITS_H

#include <functional>
#include <tuple>
#include <type_traits>

namespace mc {
// 函数特征提取工具
template <typename F>
struct function_traits;

// 普通函数
template <typename R, typename... Args>
struct function_traits<R(Args...)> {
    using return_type                  = R;
    using args_tuple                   = std::tuple<Args...>;
    static constexpr size_t args_count = sizeof...(Args);
};

// 函数指针
template <typename R, typename... Args>
struct function_traits<R (*)(Args...)> : function_traits<R(Args...)> {};

// 成员函数指针
template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...)> : function_traits<R(Args...)> {};

// const 成员函数指针
template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) const> : function_traits<R(Args...)> {};

// std::function
template <typename R, typename... Args>
struct function_traits<std::function<R(Args...)>> : function_traits<R(Args...)> {};

// lambda表达式、函数对象等
template <typename F>
struct function_traits {
private:
    using call_type = function_traits<decltype(&F::operator())>;

public:
    using return_type                  = typename call_type::return_type;
    using args_tuple                   = typename call_type::args_tuple;
    static constexpr size_t args_count = call_type::args_count;
};

// 引用类型的特化，移除引用
template <typename F>
struct function_traits<F&> : function_traits<F> {};

template <typename F>
struct function_traits<F&&> : function_traits<F> {};

// 这里可以添加其他通用的 trait 工具

} // namespace mc

#endif // MC_TRAITS_H