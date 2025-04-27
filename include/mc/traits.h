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

namespace traits {

template <typename T>
struct remove_cvref {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
};

template <typename T>
using remove_cvref_t = typename remove_cvref<T>::type;

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

// tuple_for_each实现：遍历元组中的每个元素并应用函数
template <typename Tuple, typename Func, std::size_t... Is>
constexpr void tuple_for_each_impl(Tuple& tuple, Func&& func, std::index_sequence<Is...>) {
    (func(std::get<Is>(tuple)), ...);
}

template <typename Tuple, typename Func>
constexpr void tuple_for_each(Tuple& tuple, Func&& func) {
    tuple_for_each_impl(tuple, std::forward<Func>(func),
                        std::make_index_sequence<std::tuple_size_v<std::decay_t<Tuple>>>{});
}

template <typename Tuple, typename Func, size_t... I>
void tuple_element_for_each_impl(Func&& func, std::index_sequence<I...>) {
    (func(static_cast<mc::traits::remove_cvref_t<std::tuple_element_t<I, Tuple>>*>(nullptr), I),
     ...);
}
template <typename Tuple, typename Func>
void tuple_element_for_each(Func&& func) {
    tuple_element_for_each_impl<Tuple>(
        std::forward<Func>(func),
        std::make_index_sequence<std::tuple_size_v<std::decay_t<Tuple>>>{});
}

} // namespace traits
} // namespace mc

#endif // MC_TRAITS_H