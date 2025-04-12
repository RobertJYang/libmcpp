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
 * @file metadata_info.h
 * @brief 反射元数据结构定义
 */
#ifndef MC_REFLECT_METADATA_INFO_H
#define MC_REFLECT_METADATA_INFO_H

#include <functional>
#include <string_view>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <vector>

#include <mc/reflect/typename.h>
#include <mc/variant.h>

namespace mc::reflect {

// 标签类型 - 用于区分不同类型的成员
struct property_tag {};
struct method_tag {};

// 检测是否存在 to_variant 函数
template <typename T>
auto has_to_variant_function(int)
    -> decltype(to_variant(std::declval<T>(), std::declval<mc::variant&>()), std::true_type{});

template <typename T>
std::false_type has_to_variant_function(...);

template <typename T>
inline constexpr bool has_to_variant_function_v = decltype(has_to_variant_function<T>(0))::value;

// 检测类型是否可构造为 variant，分两步检测避免编译错误
template <typename T>
struct is_variant_constructible {
    // 第一步：检查是否可以构造
    static constexpr bool is_constructible = std::is_constructible_v<mc::variant, T>;

    // 第二步：仅当可构造时，才检查是否有 to_variant 函数
    template <typename U, bool IsConstructible>
    struct check_to_variant {
        static constexpr bool value = false;
    };

    template <typename U>
    struct check_to_variant<U, true> {
        static constexpr bool value = has_to_variant_function_v<U>;
    };

    static constexpr bool value = check_to_variant<T, is_constructible>::value;
};

template <typename T>
inline constexpr bool is_variant_constructible_v = is_variant_constructible<T>::value;

// 检查所有类型是否都可以转换为variant
template <typename... Args>
struct all_variant_constructible;

template <>
struct all_variant_constructible<> : std::true_type {};

template <typename T, typename... Rest>
struct all_variant_constructible<T, Rest...> {
    static constexpr bool value =
        is_variant_constructible_v<T> && all_variant_constructible<Rest...>::value;
};

template <typename... Args>
inline constexpr bool all_variant_constructible_v = all_variant_constructible<Args...>::value;

//------------------------------------------------------------------------------
// 元数据结构基类
//------------------------------------------------------------------------------

// 成员信息基类
template <typename C>
struct member_info_base {
    std::string_view name;

    member_info_base(std::string_view n) : name(n) {
    }

    virtual std::type_index  typeinfo() const  = 0;
    virtual std::string_view type_name() const = 0;
};

//------------------------------------------------------------------------------
// 属性元数据结构
//------------------------------------------------------------------------------

// 属性信息基类
template <typename C>
struct property_info_base : public member_info_base<C> {
    property_info_base(std::string_view n) : member_info_base<C>(n) {
    }

    virtual mc::variant get_value(const C& obj) const                     = 0;
    virtual void        set_value(C& obj, const mc::variant& value) const = 0;
    virtual std::function<mc::variant(const C&)>        getter() const    = 0;
    virtual std::function<void(C&, const mc::variant&)> setter() const    = 0;
    virtual size_t                                      offset() const    = 0;
};

// 属性元数据具体实现
template <typename C, typename M>
struct property_info : public property_info_base<C> {
    using class_type  = C;
    using member_type = M;
    using tag_type    = property_tag;

    static_assert(is_variant_constructible_v<M>, "属性类型必须可以转换为mc::variant");

    M C::* member_ptr;

    constexpr property_info(std::string_view n, M C::* ptr)
        : property_info_base<C>(n), member_ptr(ptr) {
    }

    // 获取属性值
    mc::variant get_value(const C& obj) const override {
        return mc::variant(obj.*member_ptr);
    }

    // 设置属性值
    void set_value(C& obj, const mc::variant& value) const override {
        value.as(obj.*member_ptr);
    }

    std::function<mc::variant(const C&)> getter() const override {
        return [this](const C& obj) {
            return get_value(obj);
        };
    }

    std::function<void(C&, const mc::variant&)> setter() const override {
        return [this](C& obj, const mc::variant& value) {
            set_value(obj, value);
        };
    }

    size_t offset() const override {
        return MC_MEMBER_OFFSETOF(C, member_ptr);
    }

    std::type_index typeinfo() const override {
        return typeid(member_type);
    }

    std::string_view type_name() const override {
        return pretty_name<member_type>();
    }
};

//------------------------------------------------------------------------------
// 方法元数据结构
//------------------------------------------------------------------------------

// 方法信息基类
template <typename C>
struct method_info_base : public member_info_base<C> {
    method_info_base(std::string_view n) : member_info_base<C>(n) {
    }

    virtual mc::variant invoke(C& obj, const mc::variants& args) const = 0;
    virtual size_t      arg_count() const = 0; // 返回方法所需的参数数量
};

// 异常抛出辅助函数
void throw_method_arg_not_match(const char* method_name, const char* expect_type,
                                const char* actual_type);
void throw_method_arg_not_enough(const char* method_name, size_t expect_count, size_t actual_count);

// 方法元数据具体实现 - 根据方法是否为const使用不同实现
template <typename Class, bool IsConst, typename RetType, typename... Args>
class method_info : public method_info_base<Class> {
public:
    using tag_type = method_tag;

    using non_const_function_type = RetType (Class::*)(Args...);
    using const_function_type     = RetType (Class::*)(Args...) const;
    using function_type = std::conditional_t<IsConst, const_function_type, non_const_function_type>;

    // 静态断言确保返回类型可以转换为variant
    static_assert(std::is_void_v<RetType> || is_variant_constructible_v<RetType>,
                  "方法返回类型必须是void或者可以转换为mc::variant");

    // 静态断言确保所有参数类型都可以从variant转换
    static_assert(all_variant_constructible_v<mc::remove_cvref_t<Args>...>,
                  "参数类型必须可转换为mc::variant");

    method_info(std::string_view name, function_type func)
        : method_info_base<Class>(name), m_function(func) {
    }

    // 参数转换
    template <typename Arg>
    static Arg convert_arg(const char* name, const variant& var) {
        if (var.is_null()) {
            throw_method_arg_not_match(name, mc::pretty_name<Arg>(), "null");
        }
        return var.as<Arg>();
    }

    // 使用精确参数调用方法
    template <size_t... I>
    variant call_with_exact_args(Class& obj, const variants& args,
                                 std::index_sequence<I...>) const {
        if constexpr (std::is_void_v<RetType>) {
            (obj.*m_function)(convert_arg<mc::remove_cvref_t<Args>>(this->name.data(), args[I])...);
            return variant();
        } else {
            return variant((obj.*m_function)(
                convert_arg<mc::remove_cvref_t<Args>>(this->name.data(), args[I])...));
        }
    }

    // 调用方法，要求参数数量 >= 函数参数数量
    variant invoke(Class& obj, const variants& args) const override {
        constexpr size_t arg_count = sizeof...(Args);

        if (args.size() < arg_count) {
            throw_method_arg_not_enough(this->name.data(), arg_count, args.size());
        }

        return call_with_exact_args(obj, args, std::make_index_sequence<arg_count>());
    }

    std::type_index typeinfo() const override {
        return typeid(RetType);
    }

    std::string_view type_name() const override {
        return pretty_name<RetType>();
    }

    size_t arg_count() const override {
        return sizeof...(Args);
    }

private:
    function_type m_function;
};

// 创建方法元数据的辅助函数
template <typename C, typename R, typename... Args>
constexpr auto make_method_info(R (C::*method)(Args...), std::string_view name) {
    return method_info<C, false, R, Args...>{name, method};
}

template <typename C, typename R, typename... Args>
constexpr auto make_method_info(R (C::*method)(Args...) const, std::string_view name) {
    return method_info<C, true, R, Args...>{name, method};
}

} // namespace mc::reflect

#endif // MC_REFLECT_METADATA_INFO_H