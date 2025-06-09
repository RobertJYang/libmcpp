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
#include <variant>

namespace mc {

namespace traits {

// 追加类型到一个 Types 的后面
template <typename Types, typename T>
struct type_append;

template <typename... Types, typename T>
struct type_append<std::tuple<Types...>, T> {
    using type = std::tuple<Types..., T>;
};

template <typename... Types>
using type_append_t = typename type_append<Types...>::type;

// 追加类型到一个 Types 的前面
template <typename Types, typename T>
struct type_prepend;

template <typename... Types, typename T>
struct type_prepend<std::tuple<Types...>, T> {
    using type = std::tuple<T, Types...>;
};

template <typename... Types>
using type_prepend_t = typename type_prepend<Types...>::type;

// 类型去重
template <typename... Ts>
struct type_set;

template <>
struct type_set<> {
    using type = std::tuple<>;
};

template <typename T>
struct type_set<T> {
    using type = std::tuple<T>;
};

template <typename T, typename... Rest>
struct type_set<T, Rest...> {
private:
    template <typename U, typename Tuple>
    struct contains_impl;

    template <typename U>
    struct contains_impl<U, std::tuple<>> {
        static constexpr bool value = false;
    };

    template <typename U, typename First, typename... Types>
    struct contains_impl<U, std::tuple<First, Types...>> {
        static constexpr bool value = std::is_same_v<U, First> || contains_impl<U, std::tuple<Types...>>::value;
    };

    using rest_unique                  = typename type_set<Rest...>::type;
    static constexpr bool is_contained = contains_impl<T, rest_unique>::value;

public:
    using type = std::conditional_t<
        is_contained,
        rest_unique,
        decltype(std::tuple_cat(std::declval<std::tuple<T>>(), std::declval<rest_unique>()))>;
};

template <typename... Ts>
using type_set_t = typename type_set<Ts...>::type;

// 定义一个模板，将一个模板应用到 Types 上
template <template <typename...> class To, typename Types>
struct apply_type;

// 特化处理std::tuple
template <template <typename...> class To, typename... Types>
struct apply_type<To, std::tuple<Types...>> {
    using type = To<Types...>;
};

// 辅助别名模板
template <template <typename...> class To, typename Types>
using apply_type_t = typename apply_type<To, Types>::type;

template <typename T>
struct remove_cvref {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
};

template <typename T>
using remove_cvref_t = typename remove_cvref<T>::type;

// 检查类型是否为模板特化
template <typename T, template <typename...> class Template>
struct is_specialization_of : std::false_type {};

template <template <typename...> class Template, typename... Args>
struct is_specialization_of<Template<Args...>, Template> : std::true_type {};

template <typename T, template <typename...> class Template>
inline constexpr bool is_specialization_of_v = is_specialization_of<T, Template>::value;

// 函数特征提取工具
template <typename F>
struct function_traits;

// 普通函数
template <typename R, typename... Args>
struct function_traits<R(Args...)> {
    using class_type                   = void;
    using return_type                  = R;
    using args_type                    = std::tuple<Args...>;
    static constexpr size_t args_count = sizeof...(Args);
    using function_type                = std::function<R(Args...)>;
};

// 成员函数指针
template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...)> {
    using class_type                   = C;
    using return_type                  = R;
    using args_type                    = std::tuple<Args...>;
    static constexpr size_t args_count = sizeof...(Args);
    using function_type                = std::function<R(Args...)>;
};

// 函数指针
template <typename R, typename... Args>
struct function_traits<R (*)(Args...)> : function_traits<R(Args...)> {};

// const 限定的函数指针特化
template <typename R, typename... Args>
struct function_traits<R (*const)(Args...)> : function_traits<R(Args...)> {};

// const 成员函数指针
template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) const> : function_traits<R (C::*)(Args...)> {};

// 处理 volatile 成员函数
template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) volatile> : function_traits<R (C::*)(Args...)> {};

// 处理 const volatile 成员函数
template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) const volatile> : function_traits<R (C::*)(Args...)> {};

// std::function
template <typename R, typename... Args>
struct function_traits<std::function<R(Args...)>> : function_traits<R(Args...)> {};

// lambda表达式、函数对象等
template <typename F>
struct function_traits {
private:
    using call_type = function_traits<decltype(&F::operator())>;

public:
    using class_type                   = typename call_type::class_type;
    using return_type                  = typename call_type::return_type;
    using args_type                    = typename call_type::args_type;
    static constexpr size_t args_count = call_type::args_count;
    using function_type                = typename call_type::function_type;
};

// 引用类型的特化，移除引用
template <typename F>
struct function_traits<F&> : function_traits<F> {};

template <typename F>
struct function_traits<F&&> : function_traits<F> {};

namespace detail {
template <typename T, typename F, typename = void>
struct is_getter_impl : std::false_type {};

// 普通函数和函数对象的特化
template <typename T, typename F>
struct is_getter_impl<
    T, F,
    std::enable_if_t<!std::is_void_v<std::decay_t<T>> &&
                     std::is_invocable_r_v<std::decay_t<T>, F> && std::is_invocable_v<F>>>
    : std::true_type {};

// 成员函数指针的特化
template <typename T, typename ClassType, typename ReturnType>
struct is_getter_impl<T, ReturnType (ClassType::*)(),
                      std::enable_if_t<!std::is_void_v<std::decay_t<T>> &&
                                       std::is_same_v<std::decay_t<T>, std::decay_t<ReturnType>>>>
    : std::true_type {};

// const 成员函数指针的特化
template <typename T, typename ClassType, typename ReturnType>
struct is_getter_impl<T, ReturnType (ClassType::*)() const,
                      std::enable_if_t<!std::is_void_v<std::decay_t<T>> &&
                                       std::is_same_v<std::decay_t<T>, std::decay_t<ReturnType>>>>
    : std::true_type {};

template <typename T, typename F, typename = void>
struct is_setter_impl : std::false_type {};

// 普通函数和函数对象的特化
template <typename T, typename F>
struct is_setter_impl<T, F,
                      std::enable_if_t<!std::is_void_v<std::decay_t<T>> &&
                                       std::is_invocable_r_v<void, F, std::decay_t<T>>>>
    : std::true_type {};

// 成员函数指针的特化
template <typename T, typename ClassType>
struct is_setter_impl<T, void (ClassType::*)(std::decay_t<T>),
                      std::enable_if_t<!std::is_void_v<std::decay_t<T>>>> : std::true_type {};

// const 成员函数指针的特化
template <typename T, typename ClassType>
struct is_setter_impl<T, void (ClassType::*)(std::decay_t<T>) const,
                      std::enable_if_t<!std::is_void_v<std::decay_t<T>>>> : std::true_type {};
} // namespace detail

template <typename T, typename F>
inline constexpr bool is_getter_v = detail::is_getter_impl<T, F>::value;

template <typename T, typename F>
inline constexpr bool is_setter_v = detail::is_setter_impl<T, F>::value;

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

// 遍历元组中的每个元素并应用函数，返回 map 后的新元组
template <typename Tuple, typename Func>
constexpr auto tuple_map(Tuple& tuple, Func&& func) {
    return std::apply(
        [&](auto&... element) {
        return (std::tuple_cat(func(element)...));
    },
        tuple);
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

template <typename F, size_t I = 0, typename... Ts>
void apply_tuple_element_impl(F&& func, const std::tuple<Ts...>& tup, size_t index) {
    if constexpr (I < sizeof...(Ts)) {
        if (I == index) {
            func(std::get<I>(tup));
        }
        return apply_tuple_element_impl<F, I + 1>(std::forward<F>(func), tup, index);
    }
}

/**
 * @brief 应用元组的第 index 个元素
 *
 * @tparam F 函数类型
 * @tparam Ts 元组元素类型
 * @param func 函数
 * @param tup 元组
 * @param index 索引
 */
template <typename F, typename... Ts>
void apply_tuple_element(F&& func, const std::tuple<Ts...>& tup, size_t index) {
    apply_tuple_element_impl<F>(std::forward<F>(func), tup, index);
}

template <typename T>
constexpr bool is_copyable_v = std::is_copy_assignable_v<T> && std::is_copy_constructible_v<T>;

// 检测类型T和U是否支持相等比较运算符 (==)
template <typename T, typename U = T, typename = void>
struct has_operator_equal : std::false_type {};

template <typename T, typename U>
struct has_operator_equal<T, U, std::void_t<decltype(std::declval<T>() == std::declval<U>())>>
    : std::true_type {};

template <typename T, typename U = T>
inline constexpr bool has_operator_equal_v = has_operator_equal<T, U>::value;

// 移除多级指针类型
template <typename T>
struct remove_pointers {
    using type = T;
};
template <typename T>
struct remove_pointers<T*> {
    using type = typename remove_pointers<remove_cvref_t<T>>::type;
};

// 别名模板简化使用
template <typename T>
using remove_pointers_t = typename remove_pointers<remove_cvref_t<T>>::type;

template <typename T>
class property_traits {
    struct disable_rvalue_typeerence {};

public:
    static constexpr bool is_basic_type = std::is_arithmetic_v<T> || std::is_enum_v<T>;

    using value_type  = T;
    using param_type  = std::conditional_t<is_basic_type, T, const T&>;
    using rvalue_type = std::conditional_t<is_basic_type, disable_rvalue_typeerence, T&&>;
};

template <>
class property_traits<void> {
    struct disable_rvalue_typeerence {};

public:
    using value_type  = std::monostate;
    using param_type  = std::monostate;
    using rvalue_type = disable_rvalue_typeerence;
};
} // namespace traits
} // namespace mc

#endif // MC_TRAITS_H