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

#include <mc/error.h>
#include <mc/future.h>
#include <mc/reflect/base.h>
#include <mc/reflect/signature_helper.h>
#include <mc/result.h>
#include <mc/traits.h>
#include <mc/variant.h>

namespace mc::reflect {

// 标签类型 - 用于区分不同类型的成员
struct property_tag {};
struct method_tag {};
struct base_class_tag {};

// 前向声明
class struct_metadata;

//------------------------------------------------------------------------------
// 类型特征检测
//------------------------------------------------------------------------------

// 检查元组元素是否具有特定的标签类型
template <typename Tag, typename T, typename = void>
struct has_tag : std::false_type {};

template <typename Tag, typename T>
struct has_tag<Tag, T, std::enable_if_t<std::is_same_v<typename T::tag_type, Tag>>>
    : std::true_type {};

template <typename Tag, typename T>
inline constexpr bool has_tag_v = has_tag<Tag, T>::value;

// 检测是否为方法指针
template <typename T>
struct is_method : std::false_type {};

template <typename R, typename C, typename... Args>
struct is_method<R (C::*)(Args...)> : std::true_type {};

template <typename R, typename C, typename... Args>
struct is_method<R (C::*)(Args...) const> : std::true_type {};

// 静态函数的特化
template <typename R, typename... Args>
struct is_method<R (*)(Args...)> : std::true_type {};

template <typename T>
inline constexpr bool is_method_v = is_method<T>::value;

// 检测是否为属性（数据成员）
template <typename T, typename U = void>
struct is_property : std::false_type {};

template <typename T, typename M>
struct is_property<M T::*,
                   std::enable_if_t<!is_method_v<M T::*> &&
                                    is_variant_constructible_v<mc::traits::remove_cvref_t<M>>>>
    : std::true_type {};

// 检测是否为属性
// reflect 要求属性必须是可构造为 mc::variant 的类型
template <typename T>
inline constexpr bool is_property_v = is_property<T>::value;

enum member_info_type {
    invalid,
    property,
    computed_property,
    method,
    base_class,

    custom_start = 100
};

//------------------------------------------------------------------------------
// 元数据结构基类
//------------------------------------------------------------------------------

// 成员信息基类
// @note 我们预留了 flags 用于存储自定义其他信息，比如 engine 模块的 MC_REFLECT_FLAG_PROPERTY_TPL 用于标记该属性是 property<T> 类型。
// 这里不做过多约定由用户自己决定如何使用，为了保证定义的 flags 位不冲突，要求必须用宏定义 FLAG，并以 MC_REFLECT_FLAG_* 作为开头命名，这样
// 通过简单的字符串查找就可以检查是否冲突
struct member_info_base {
    std::string_view name;
    uint32_t         flags;           // 扩展 flags，用于存储自定义其他信息
    uint32_t         base_offset = 0; // 如果该反射信息表示的是其他类的基类，则该值为基类相对于子类基址的偏移量，否则为 0
    uint64_t         data;            // 扩展数据，用于存储自定义其他信息

    constexpr member_info_base(std::string_view n)
        : name(n), flags(0), base_offset(0), data(0) {
    }

    virtual std::type_index   typeinfo() const  = 0;
    virtual std::string_view  type_name() const = 0;
    virtual int               type() const      = 0;
    virtual uint32_t          offset() const    = 0;
    virtual member_info_base* clone() const     = 0;

    template <typename T>
    T* adjust_object_pointer(void* obj) const noexcept {
        return reinterpret_cast<T*>(reinterpret_cast<char*>(obj) + base_offset);
    }

    template <typename T>
    const T* adjust_object_pointer(const void* obj) const noexcept {
        return reinterpret_cast<const T*>(reinterpret_cast<const char*>(obj) + base_offset);
    }

    bool is_property_type() const noexcept {
        auto t = this->type();
        return t == member_info_type::property || t == member_info_type::computed_property;
    }

    bool is_method_type() const noexcept {
        return this->type() == member_info_type::method;
    }

    bool is_base_class_type() const noexcept {
        return this->type() == member_info_type::base_class;
    }

    bool is_type(int type) const noexcept {
        return static_cast<int>(this->type()) == type;
    }

    constexpr bool has_flags(uint32_t flags) const noexcept {
        return (this->flags & flags) == flags;
    }

    constexpr void set_flags(uint32_t flags) noexcept {
        this->flags |= flags;
    }

    constexpr void set_data(uint32_t data) noexcept {
        this->data = data;
    }

    constexpr uint32_t get_data() const noexcept {
        return this->data;
    }
};

//------------------------------------------------------------------------------
// 属性元数据结构
//------------------------------------------------------------------------------

struct property_type_info : public member_info_base {
    constexpr property_type_info(std::string_view n) : member_info_base(n) {
    }

    virtual std::string_view get_signature() const = 0;

    // 使用反射信息基类直接调用对象属性，用于动态反射类型擦除后使用
    mc::variant get_value(const void* obj) const;
    void        set_value(void* obj, const mc::variant& value) const;
    template <typename C, std::enable_if_t<std::is_class_v<C>, int> = 0>
    mc::variant get_value(const C& obj) const {
        return get_value(&obj);
    }
    template <typename C, std::enable_if_t<std::is_class_v<C>, int> = 0>
    void set_value(C& obj, const mc::variant& value) const {
        set_value(&obj, value);
    }
};

// 属性信息基类
template <typename C>
struct property_info_base : public property_type_info {
    using class_type  = C;
    using getter_type = std::function<mc::variant(const C&)>;
    using setter_type = std::function<void(C&, const mc::variant&)>;

    constexpr property_info_base(std::string_view n) : property_type_info(n) {
    }

    virtual mc::variant get_value(const C& obj) const                     = 0;
    virtual void        set_value(C& obj, const mc::variant& value) const = 0;
    virtual getter_type getter() const                                    = 0;
    virtual setter_type setter() const                                    = 0;
};

// 类型擦除后通过反射获取属性值，用 std::monostate 类型作为对象类型，因为我们并不会真正使用这个类型，
// 只是为了计算指针偏移量到正确的对象地址
inline mc::variant property_type_info::get_value(const void* obj) const {
    return reinterpret_cast<const property_info_base<std::monostate>*>(this)->get_value(
        *static_cast<const std::monostate*>(obj));
}
inline void property_type_info::set_value(void* obj, const mc::variant& value) const {
    reinterpret_cast<const property_info_base<std::monostate>*>(this)->set_value(
        *static_cast<std::monostate*>(obj), value);
}

// 属性元数据具体实现
// TODO:: 目前为每个类的每个属性都实例化一个 property_info<C, M> 类型，后续可以给所有的
// 属性类型打个包，只实例化一个 propertis_info 类型，减少模板展开的代码大小
template <typename C, typename M, typename BaseT = C>
struct property_info : public property_info_base<C> {
    using class_type  = C;
    using member_type = M;
    using tag_type    = property_tag;
    using base_type   = BaseT;
    using typename property_info_base<C>::getter_type;
    using typename property_info_base<C>::setter_type;

    M BaseT::* member_ptr;

    constexpr property_info(std::string_view n, M BaseT::* ptr)
        : property_info_base<C>(n), member_ptr(ptr) {
    }

    // 获取属性值
    mc::variant get_value(const C& obj) const override {
        return mc::variant(get_object(obj).*member_ptr);
    }

    // 设置属性值
    void set_value(C& obj, const mc::variant& value) const override {
        value.as(get_object(obj).*member_ptr);
    }

    getter_type getter() const override {
        return [this](const C& obj) {
            return get_value(obj);
        };
    }

    setter_type setter() const override {
        return [this](C& obj, const mc::variant& value) {
            set_value(obj, value);
        };
    }

    uint32_t offset() const noexcept override {
        return MC_MEMBER_OFFSETOF(C, member_ptr);
    }

    std::type_index typeinfo() const override {
        return typeid(member_type);
    }

    std::string_view type_name() const noexcept override {
        return pretty_name<member_type>();
    }

    std::string_view get_signature() const override {
        return mc::reflect::get_signature<member_type>();
    }

    int type() const noexcept override {
        return static_cast<int>(member_info_type::property);
    }

    member_info_base* clone() const override {
        return new property_info<C, M, BaseT>(this->name, member_ptr);
    }

    const BaseT& get_object(const C& obj) const noexcept {
        return *this->template adjust_object_pointer<BaseT>(&obj);
    }

    BaseT& get_object(C& obj) const noexcept {
        return *this->template adjust_object_pointer<BaseT>(&obj);
    }
};

// 计算属性元数据具体实现
template <typename C, typename Getter, typename Setter = void*>
struct computed_property_info : public property_info_base<C> {
    using class_type  = C;
    using member_type = typename mc::traits::function_traits<Getter>::return_type;
    using tag_type    = property_tag;
    using base_type   = C;
    using typename property_info_base<C>::getter_type;
    using typename property_info_base<C>::setter_type;

    using set_function_type = Setter;
    using get_function_type = Getter;

    get_function_type m_getter;
    set_function_type m_setter;

    constexpr computed_property_info(std::string_view n, get_function_type getter,
                                     set_function_type setter = nullptr)
        : property_info_base<C>(n), m_getter(getter), m_setter(setter) {
    }

    // 获取属性值
    mc::variant get_value(const C& obj) const override {
        return (get_object(obj).*m_getter)();
    }

    // 设置属性值
    void set_value(C& obj, const mc::variant& value) const override {
        if constexpr (std::is_same_v<set_function_type, void*>) {
            MC_UNUSED(value);
        } else {
            (get_object(obj).*m_setter)(value.as<member_type>());
        }
    }

    getter_type getter() const override {
        return [this](const C& obj) {
            return get_value(obj);
        };
    }

    std::function<void(C&, const mc::variant&)> setter() const override {
        return [this](C& obj, const mc::variant& value) {
            set_value(obj, value);
        };
    }

    uint32_t offset() const noexcept override {
        return 0;
    }

    std::type_index typeinfo() const override {
        return typeid(member_type);
    }

    std::string_view type_name() const noexcept override {
        return pretty_name<member_type>();
    }

    std::string_view get_signature() const override {
        return mc::reflect::get_signature<member_type>();
    }

    int type() const noexcept override {
        return static_cast<int>(member_info_type::computed_property);
    }

    member_info_base* clone() const override {
        return new computed_property_info<C, Getter, Setter>(this->name, m_getter, m_setter);
    }

    const C& get_object(const C& obj) const noexcept {
        return *this->template adjust_object_pointer<C>(&obj);
    }

    C& get_object(C& obj) const noexcept {
        return *this->template adjust_object_pointer<C>(&obj);
    }
};

//------------------------------------------------------------------------------
// 方法元数据结构
//------------------------------------------------------------------------------

struct method_type_info : public member_info_base {
    constexpr method_type_info(std::string_view n) : member_info_base(n) {
    }

    virtual std::string_view get_args_signature() const   = 0;
    virtual std::string_view get_result_signature() const = 0;
    virtual size_t           arg_count() const            = 0;

    // 静态方法相关接口
    virtual bool         is_static() const                            = 0;
    virtual mc::variant  invoke(const mc::variants& args) const       = 0;
    virtual async_result async_invoke(const mc::variants& args) const = 0;

    // 使用反射信息基类直接调用对象方法，用于动态反射类型擦除后使用
    mc::variant  invoke(void* obj, const mc::variants& args) const;
    async_result async_invoke(void* obj, const mc::variants& args) const;
    template <typename C, std::enable_if_t<std::is_class_v<C>, int> = 0>
    mc::variant invoke(C& obj, const mc::variants& args) const noexcept {
        return invoke(&obj, args);
    }
    template <typename C, std::enable_if_t<std::is_class_v<C>, int> = 0>
    async_result async_invoke(C& obj, const mc::variants& args) const noexcept {
        return async_invoke(&obj, args);
    }
};

// 方法信息基类
template <typename C>
struct method_info_base : public method_type_info {
    using class_type = C;
    using method_type_info::async_invoke;
    using method_type_info::invoke;

    constexpr method_info_base(std::string_view n) : method_type_info(n) {
    }

    virtual mc::variant  invoke(C& obj, const mc::variants& args) const       = 0;
    virtual async_result async_invoke(C& obj, const mc::variants& args) const = 0;
};

// 方法元数据具体实现
template <typename Class, typename BaseT, bool IsConst, bool IsStatic, typename RetType,
          typename... Args>
class method_info : public method_info_base<Class> {
public:
    using tag_type = method_tag;

    using non_const_function_type = RetType (BaseT::*)(Args...);
    using const_function_type     = RetType (BaseT::*)(Args...) const;
    using static_function_type    = RetType (*)(Args...);
    using member_function_type =
        std::conditional_t<IsConst, const_function_type, non_const_function_type>;
    using function_type = std::conditional_t<IsStatic, static_function_type, member_function_type>;
    using result_type   = mc::traits::remove_cvref_t<RetType>;
    using args_type     = std::tuple<mc::traits::remove_cvref_t<Args>...>;
    using class_type    = Class;
    using base_type     = BaseT;

    // 静态断言确保返回类型可以转换为variant
    static_assert(std::is_void_v<RetType> || is_variant_constructible_v<RetType>,
                  "方法返回类型必须是void或者可以转换为mc::variant");

    // 静态断言确保所有参数类型都可以从variant转换
    static_assert(all_variant_constructible_v<mc::traits::remove_cvref_t<Args>...>,
                  "参数类型必须可转换为mc::variant");

    constexpr method_info(std::string_view name, function_type func)
        : method_info_base<Class>(name), m_function(func) {
    }

    bool is_static() const override {
        return IsStatic;
    }

    template <typename C>
    RetType call_impl(C& obj, Args&&... args) const {
        if constexpr (!IsStatic) {
            if constexpr (std::is_same_v<C, Class>) {
                return (get_object(obj).*m_function)(std::forward<Args>(args)...);
            } else {
                throw_method_not_exist(this->name);
            }
        } else {
            return m_function(std::forward<Args>(args)...);
        }
    }

    template <typename C, typename ResultType, size_t... I>
    ResultType call_with_exact_args(C& obj, const mc::variants& args, std::index_sequence<I...>) const {
        if constexpr (std::is_void_v<RetType>) {
            call_impl(
                obj, mc::detail::convert_arg<mc::traits::remove_cvref_t<Args>>(this->name, args[I])...);
            return {};
        } else {
            return ResultType(call_impl(
                obj, mc::detail::convert_arg<mc::traits::remove_cvref_t<Args>>(
                         this->name, args[I])...));
        }
    }

    // 调用方法，要求参数数量 >= 函数参数数量
    template <typename C, typename ResultType>
    ResultType invoke_impl(C& obj, const variants& args) const {
        constexpr size_t arg_count = sizeof...(Args);

        // 如果是单个参数，且参数类型是 mc::variants，优化一下直接调用
        if constexpr (std::is_same_v<args_type, std::tuple<mc::variants>>) {
            if constexpr (std::is_void_v<RetType>) {
                call_impl(obj, args);
                return {};
            } else {
                return call_impl(obj, args);
            }
        } else {
            if (args.size() < arg_count) {
                throw_method_arg_not_enough(this->name, arg_count, args.size());
            }

            return call_with_exact_args<C, ResultType>(
                obj, args, std::make_index_sequence<arg_count>());
        }
    }

    mc::variant invoke(Class& obj, const mc::variants& args) const override {
        return invoke_impl<Class, mc::variant>(obj, args);
    }

    mc::variant invoke(const mc::variants& args) const override {
        if (!IsStatic) {
            // 非静态方法不能直接调用，需要传入对象，这里抛出方法不存在异常
            throw_method_not_exist(this->name);
        }
        int dummy = 0;
        return invoke_impl<int, mc::variant>(dummy, args);
    }

    async_result async_invoke(Class& obj, const mc::variants& args) const override {
        try {
            return invoke_impl<Class, async_result>(obj, args);
        } catch (...) {
            return mc::reject<async_result>(std::current_exception());
        }
    }

    async_result async_invoke(const mc::variants& args) const override {
        try {
            if (!IsStatic) {
                // 非静态方法不能直接调用，需要传入对象，这里抛出方法不存在异常
                throw_method_not_exist(this->name);
            }
            int dummy = 0;
            return invoke_impl<int, async_result>(dummy, args);
        } catch (...) {
            return mc::reject<async_result>(std::current_exception());
        }
    }

    std::type_index typeinfo() const override {
        return typeid(RetType);
    }

    std::string_view type_name() const noexcept override {
        return pretty_name<RetType>();
    }

    size_t arg_count() const override {
        return sizeof...(Args);
    }

    std::string_view get_args_signature() const override {
        return mc::reflect::get_signature<args_type>();
    }

    std::string_view get_result_signature() const override {
        return mc::reflect::get_signature<result_type>();
    }

    uint32_t offset() const noexcept override {
        return get_function_offset(m_function);
    }

    int type() const noexcept override {
        return static_cast<int>(member_info_type::method);
    }

    member_info_base* clone() const override {
        return new method_info<Class, BaseT, IsConst, IsStatic, RetType, Args...>(this->name, m_function);
    }

    const BaseT& get_object(const Class& obj) const noexcept {
        return *this->template adjust_object_pointer<BaseT>(&obj);
    }

    BaseT& get_object(Class& obj) const noexcept {
        return *this->template adjust_object_pointer<BaseT>(&obj);
    }

    function_type m_function;
};

// 类型擦除后通过反射调用方法，用 std::monostate 类型作为对象类型，因为我们并不会真正使用这个类型，
// 只是为了计算指针偏移量到正确的对象地址
inline mc::variant method_type_info::invoke(void* obj, const mc::variants& args) const {
    return reinterpret_cast<const method_info_base<std::monostate>*>(this)->invoke(
        *static_cast<std::monostate*>(obj), args);
}

inline async_result method_type_info::async_invoke(void* obj, const mc::variants& args) const {
    return reinterpret_cast<const method_info_base<std::monostate>*>(this)->async_invoke(
        *static_cast<std::monostate*>(obj), args);
}

// 创建方法元数据的辅助函数
template <typename C, typename BaseT, typename R, typename... Args>
constexpr auto make_method_info(R (BaseT::*method)(Args...), std::string_view name) {
    return method_info<C, BaseT, false, false, R, Args...>{name, method};
}

template <typename C, typename BaseT, typename R, typename... Args>
constexpr auto make_method_info(R (BaseT::*method)(Args...) const, std::string_view name) {
    return method_info<C, BaseT, true, false, R, Args...>{name, method};
}

template <typename C, typename R, typename... Args>
constexpr auto make_static_method_info(R (*method)(Args...), std::string_view name) {
    return method_info<C, C, false, true, R, Args...>{name, method};
}

//------------------------------------------------------------------------------
// 基类元数据结构
//------------------------------------------------------------------------------
struct base_class_type_info : public member_info_base {
    constexpr base_class_type_info(std::string_view n) : member_info_base(n) {
    }

    virtual type_id_type           get_type_id() const   = 0;
    virtual std::string_view       get_signature() const = 0;
    virtual const struct_metadata& get_metadata() const  = 0;

    // 使用反射信息基类直接获取基类属性值和调用基类方法，用于动态反射类型擦除后使用
    mc::variant  get_value(void* obj, std::string_view name) const;
    void         set_value(void* obj, std::string_view name, const mc::variant& value) const;
    mc::variant  invoke(void* obj, std::string_view name, const mc::variants& args) const;
    async_result async_invoke(void* obj, std::string_view name, const mc::variants& args) const;
    template <typename C, std::enable_if_t<std::is_class_v<C>, int> = 0>
    mc::variant get_value(const C& obj, std::string_view name) const {
        return get_value(&obj, name);
    }
    template <typename C, std::enable_if_t<std::is_class_v<C>, int> = 0>
    void set_value(C& obj, std::string_view name, const mc::variant& value) const {
        set_value(&obj, name, value);
    }
    template <typename C, std::enable_if_t<std::is_class_v<C>, int> = 0>
    mc::variant invoke(C& obj, std::string_view name, const mc::variants& args) const {
        return invoke(&obj, name, args);
    }
    template <typename C, std::enable_if_t<std::is_class_v<C>, int> = 0>
    async_result async_invoke(C& obj, std::string_view name, const mc::variants& args) const {
        return async_invoke(&obj, name, args);
    }
};

template <typename C>
struct base_class_info_base : public base_class_type_info {
    constexpr base_class_info_base(std::string_view n = {})
        : base_class_type_info(n) {
    }

    virtual mc::variant  get_value(const C& obj, std::string_view name) const                        = 0;
    virtual void         set_value(C& obj, std::string_view name, const mc::variant& value) const    = 0;
    virtual mc::variant  invoke(C& obj, std::string_view name, const mc::variants& args) const       = 0;
    virtual async_result async_invoke(C& obj, std::string_view name, const mc::variants& args) const = 0;
};

// 属性元数据具体实现
template <typename C, typename BaseT>
struct base_class_info : public base_class_info_base<C> {
    using class_type  = C;
    using member_type = BaseT;
    using base_type   = BaseT;
    using tag_type    = base_class_tag;

    constexpr base_class_info(std::string_view base_class_name)
        : base_class_info_base<C>(base_class_name) {
    }

    std::type_index typeinfo() const override {
        return typeid(base_type);
    }

    std::string_view type_name() const noexcept override {
        return pretty_name<base_type>();
    }

    uint32_t offset() const noexcept override {
        return static_cast<uint32_t>(mc::get_base_offset<class_type, base_type>());
    }

    const struct_metadata& get_metadata() const override {
        return reflector<base_type>::get_metadata();
    }

    std::string_view get_signature() const override {
        return mc::reflect::get_signature<base_type>();
    }

    type_id_type get_type_id() const override {
        return reflector<base_type>::get_type_id();
    }

    mc::variant  get_value(const C& obj, std::string_view name) const override;
    void         set_value(C& obj, std::string_view name, const mc::variant& value) const override;
    mc::variant  invoke(C& obj, std::string_view name, const mc::variants& args) const override;
    async_result async_invoke(C& obj, std::string_view name, const mc::variants& args) const override;

    int type() const noexcept override {
        return static_cast<int>(member_info_type::base_class);
    }

    member_info_base* clone() const override {
        return new base_class_info<C, BaseT>(this->name);
    }

    const BaseT& get_object(const C& obj) const noexcept {
        return *this->template adjust_object_pointer<BaseT>(&obj);
    }

    BaseT& get_object(C& obj) const noexcept {
        return *this->template adjust_object_pointer<BaseT>(&obj);
    }
};

// 类型擦除后通过反射获取基类属性值和调用基类方法，用 std::monostate 类型作为对象类型，因为我们并不会真正使用这个类型，
// 只是为了计算指针偏移量到正确的对象地址
inline mc::variant base_class_type_info::get_value(void* obj, std::string_view name) const {
    return reinterpret_cast<const base_class_info_base<std::monostate>*>(this)->get_value(
        *static_cast<std::monostate*>(obj), name);
}
inline void base_class_type_info::set_value(void* obj, std::string_view name, const mc::variant& value) const {
    reinterpret_cast<const base_class_info_base<std::monostate>*>(this)->set_value(
        *static_cast<std::monostate*>(obj), name, value);
}
inline mc::variant base_class_type_info::invoke(void* obj, std::string_view name, const mc::variants& args) const {
    return reinterpret_cast<const base_class_info_base<std::monostate>*>(this)->invoke(
        *static_cast<std::monostate*>(obj), name, args);
}
inline async_result base_class_type_info::async_invoke(void* obj, std::string_view name, const mc::variants& args) const {
    return reinterpret_cast<const base_class_info_base<std::monostate>*>(this)->async_invoke(
        *static_cast<std::monostate*>(obj), name, args);
}

/**
 * @brief 枚举成员信息
 */
using enum_value_type = uint32_t;
struct enum_member_info {
    std::string_view name;  // 枚举成员名称
    enum_value_type  value; // 枚举值

    constexpr enum_member_info() : name(), value(0) {
    }

    template <typename EnumType>
    constexpr enum_member_info(std::string_view n, EnumType v)
        : name(n), value(static_cast<enum_value_type>(v)) {
        static_assert(std::is_enum_v<EnumType>, "EnumType must be an enum type");
    }

    constexpr enum_member_info(const enum_member_info& other)
        : name(other.name), value(other.value) {
    }
};

/**
 * @brief 用户可以通过特化此模板为自定义成员类型提供反射支持
 *
 * 示例：假设有信号成员类型 mc::signal<T>，用户可以这样特化：
 *
 * // 首先定义信号标签类型
 * namespace mc::reflect {
 * struct signal_tag {};
 * }
 *
 * // 然后定义信号信息类
 * template <typename C, typename Signature>
 * struct signal_info : public member_info_base {
 *     using tag_type = mc::reflect::signal_tag;
 *
 *     mc::signal<Signature> C::* signal_ptr;
 *
 *     constexpr signal_info(std::string_view n, mc::signal<Signature> C::* ptr)
 *         : member_info_base(n), signal_ptr(ptr) {}
 *
 *     std::type_index typeinfo() const override { return typeid(mc::signal<Signature>); }
 *     std::string_view type_name() const noexcept override { return "signal"; }
 * };
 *
 * // 最后特化 member_info_creator
 * template <typename T, typename Signature, typename BaseT>
 * struct member_info_creator<T, mc::signal<Signature>, BaseT, void> {
 *     static constexpr auto create(mc::signal<Signature> BaseT::* member_ptr, std::string_view
 * name) { return std::tuple<signal_info<T, Signature>>{signal_info<T, Signature>{name,
 * member_ptr}};
 *     }
 * };
 *
 * // 然后可以使用 get_members_by_tag 提取信号成员
 * auto signals = reflector<T>::get_members_by_tag<mc::reflect::signal_tag>();
 */
template <typename T, typename M, typename BaseT = T, typename = void>
struct member_info_creator {
    static constexpr auto create(M BaseT::* member_ptr, std::string_view name) {
        static_assert(is_property_v<M BaseT::*> || is_method_v<M BaseT::*>,
                      "不支持的成员类型，请为此类型创建特化版本的 member_info_creator");
        return std::tuple<>(); // 永远不会执行到这里
    }
};

// 属性成员特化
template <typename T, typename M, typename BaseT>
struct member_info_creator<T, M, BaseT, std::enable_if_t<is_property_v<M BaseT::*>>> {
    static constexpr auto create(M BaseT::* member_ptr, std::string_view name) {
        return std::tuple<property_info<T, M>>{property_info<T, M>{name, member_ptr}};
    }
};

// 方法成员特化
template <typename T, typename M, typename BaseT>
struct member_info_creator<T, M, BaseT, std::enable_if_t<is_method_v<M BaseT::*>>> {
    static constexpr auto create(M BaseT::* member_ptr, std::string_view name) {
        return std::make_tuple(make_method_info<T>(member_ptr, name));
    }
};

// 静态成员函数特化
template <typename T, typename R, typename... Args>
struct member_info_creator<T, R (*)(Args...), void, std::enable_if_t<is_method_v<R (*)(Args...)>>> {
    static constexpr auto create(R (*static_func)(Args...), std::string_view name) {
        return std::make_tuple(make_static_method_info<T>(static_func, name));
    }
};

struct computed_property_info_creator {
    template <typename T, typename Getter, typename Setter>
    static constexpr auto create(Getter getter, Setter setter, std::string_view name) {
        return std::make_tuple(computed_property_info<T, Getter, Setter>{name, getter, setter});
    }
};

template <typename T, typename Base>
struct base_class_info_creator {
    static_assert(std::is_base_of_v<Base, T>, "T must be derived from Base class");
    static_assert(is_reflectable<Base>(), "Base class must be reflectable");

    static constexpr auto create(std::string_view base_class_name) {
        if (base_class_name.empty()) {
            base_class_name = get_type_name<Base>();
        }
        return std::make_tuple(base_class_info<T, Base>{base_class_name});
    }
};

namespace detail {

// 递归过滤元组元素，仅保留具有特定标签的元素
template <typename T, typename Filter, size_t Index, typename Result,
          typename... Tuples>
constexpr auto filter_members_impl(const std::tuple<Tuples...>& all_members, Result result) {
    if constexpr (Index >= sizeof...(Tuples)) {
        return result;
    } else {
        using element_type =
            mc::traits::remove_cvref_t<std::tuple_element_t<Index, std::tuple<Tuples...>>>;
        if constexpr (Filter::template check<std::remove_pointer_t<element_type>>) {
            if constexpr (std::is_pointer_v<element_type>) {
                auto member_tuple = std::make_tuple(std::get<Index>(all_members));
                return filter_members_impl<T, Filter, Index + 1>(all_members, std::tuple_cat(result, member_tuple));
            } else {
                auto member_tuple = std::make_tuple(&std::get<Index>(all_members));
                return filter_members_impl<T, Filter, Index + 1>(all_members, std::tuple_cat(result, member_tuple));
            }
        } else {
            return filter_members_impl<T, Filter, Index + 1>(all_members, result);
        }
    }
}

template <typename T, typename Filter, typename Tuple>
constexpr auto filter_members(const Tuple& all_members) {
    return filter_members_impl<T, Filter, 0>(all_members, std::tuple<>{});
}

template <typename Tag>
struct filter_tag {
    template <typename ElementType>
    static constexpr bool check = has_tag_v<Tag, ElementType>;
};

template <typename T, typename Tag, typename Tuple>
auto filter_members_by_tag(const Tuple& all_members) {
    return filter_members<T, filter_tag<Tag>>(all_members);
}

// 编译时简单的冒泡排序算法
template <typename T, size_t N>
constexpr std::array<T, N> sort_enum_values(std::array<T, N> arr) {
    bool swapped = false;
    for (size_t i = 0; i < N - 1; ++i) {
        swapped = false;
        for (size_t j = 0; j < N - 1 - i; ++j) {
            if (arr[j].value > arr[j + 1].value) {
                auto tmp   = arr[j];
                arr[j]     = arr[j + 1];
                arr[j + 1] = tmp;
                swapped    = true;
            }
        }

        // 如果本轮未发生交换，说明数组已有序，提前结束
        if (!swapped) {
            break;
        }
    }

    return arr;
}

template <typename Members>
constexpr auto enum_tuple_to_array(const Members& members) {
    std::array<enum_member_info, std::tuple_size_v<Members>> values;

    std::size_t i = 0;
    mc::traits::tuple_for_each(members, [&](const auto& member) {
        values[i++] = member;
    });

    // 根据枚举值排个序，对于枚举值是连续的枚举类型可以提高查找效率
    return sort_enum_values(values);
}

} // namespace detail

} // namespace mc::reflect

#endif // MC_REFLECT_METADATA_INFO_H