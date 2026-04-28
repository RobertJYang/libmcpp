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

#include <cstring>
#include <functional>
#include <memory>
#include <string_view>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <vector>

#include <mc/error.h>
#include <mc/future.h>
#include <mc/quark.h>
#include <mc/reflect/base.h>
#include <mc/reflect/signature_helper.h>
#include <mc/result.h>
#include <mc/traits.h>
#include <mc/variant.h>

namespace mc::reflect {

#ifndef MC_REFLECT_FLAG_READONLY
#define MC_REFLECT_FLAG_READONLY 0x08 // 属性只读
#endif

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
struct has_tag<Tag, T, std::enable_if_t<std::is_same_v<typename T::tag_type, Tag>>> : std::true_type {};

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
struct is_property<
    M T::*, std::enable_if_t<!is_method_v<M T::*> && (is_variant_v<mc::traits::remove_cvref_t<M>> ||
                                                      is_variant_constructible_v<mc::traits::remove_cvref_t<M>>)>>
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
struct member_info_base {
    mc::string_view name;
    // 与 name 对应的 quark
    mc::quark name_quark{};
    uint32_t  flags;           // 扩展 flags，用于存储自定义其他信息
    uint32_t  base_offset = 0; // 如果该反射信息表示的是其他类的基类，则该值为基类相对于子类基址的偏移量，否则为 0
    uint64_t  data;            // 扩展数据，用于存储自定义其他信息

    constexpr member_info_base(mc::string_view n) : name(n), name_quark(), flags(0), base_offset(0), data(0)
    {}

    virtual std::type_index   typeinfo() const  = 0;
    virtual mc::string_view   type_name() const = 0;
    virtual int               type() const      = 0;
    virtual uint32_t          offset() const    = 0;
    virtual member_info_base* clone() const     = 0;

    template <typename T>
    T* adjust_object_pointer(void* obj) const noexcept
    {
        return reinterpret_cast<T*>(reinterpret_cast<char*>(obj) + base_offset);
    }

    template <typename T>
    const T* adjust_object_pointer(const void* obj) const noexcept
    {
        return reinterpret_cast<const T*>(reinterpret_cast<const char*>(obj) + base_offset);
    }

    bool is_property_type() const noexcept
    {
        auto t = this->type();
        return t == member_info_type::property || t == member_info_type::computed_property;
    }

    bool is_method_type() const noexcept
    {
        return this->type() == member_info_type::method;
    }

    bool is_base_class_type() const noexcept
    {
        return this->type() == member_info_type::base_class;
    }

    bool is_type(int type) const noexcept
    {
        return static_cast<int>(this->type()) == type;
    }

    constexpr bool has_flags(uint32_t flags) const noexcept
    {
        return (this->flags & flags) == flags;
    }

    constexpr void set_flags(uint32_t flags) noexcept
    {
        this->flags |= flags;
    }

    constexpr void set_data(uint32_t data) noexcept
    {
        this->data = data;
    }

    constexpr uint32_t get_data() const noexcept
    {
        return this->data;
    }
};

//------------------------------------------------------------------------------
// 属性元数据结构
//------------------------------------------------------------------------------

struct property_type_info : public member_info_base {
    constexpr property_type_info(mc::string_view n) : member_info_base(n)
    {}

    virtual mc::string_view get_signature() const                                       = 0;
    virtual mc::variant     get_value_erased(const void* obj) const                     = 0;
    virtual void            set_value_erased(void* obj, const mc::variant& value) const = 0;

    bool is_writable() const noexcept
    {
        return !has_flags(MC_REFLECT_FLAG_READONLY);
    }

    // 使用反射信息基类直接调用对象属性，用于动态反射类型擦除后使用
    mc::variant get_value(const void* obj) const;
    void        set_value(void* obj, const mc::variant& value) const;
    template <typename C, std::enable_if_t<std::is_class_v<C>, int> = 0>
    mc::variant get_value(const C& obj) const
    {
        return get_value(&obj);
    }
    template <typename C, std::enable_if_t<std::is_class_v<C>, int> = 0>
    void set_value(C& obj, const mc::variant& value) const
    {
        set_value(&obj, value);
    }

    template <typename C, std::enable_if_t<std::is_class_v<C>, int> = 0>
    std::function<mc::variant(const C&)> getter() const
    {
        return [this](const C& obj) {
            return get_value(obj);
        };
    }

    template <typename C, std::enable_if_t<std::is_class_v<C>, int> = 0>
    std::function<void(C&, const mc::variant&)> setter() const
    {
        return [this](C& obj, const mc::variant& value) {
            set_value(obj, value);
        };
    }
};

template <typename C>
using property_info_base = property_type_info;

namespace detail {

template <typename MemberPtr>
struct property_member_pointer_traits;

template <typename Member, typename BaseT>
struct property_member_pointer_traits<Member BaseT::*> {
    using member_type = Member;
    using base_type   = BaseT;

    static uint32_t offset(Member BaseT::* member_ptr) noexcept
    {
        return static_cast<uint32_t>(MC_MEMBER_OFFSETOF(BaseT, member_ptr));
    }
};

template <typename MemberType>
struct property_member_ops {
    static mc::variant get_value(const void* member_ptr)
    {
        const auto& member = *static_cast<const MemberType*>(member_ptr);
        if constexpr (std::is_constructible_v<mc::variant, const MemberType&>) {
            return mc::variant(member);
        } else {
            mc::variant value;
            mc::to_variant(member, value);
            return value;
        }
    }

    static void set_value(void* member_ptr, const mc::variant& value)
    {
        value.as(*static_cast<MemberType*>(member_ptr));
    }

    static std::type_index typeinfo()
    {
        return typeid(MemberType);
    }

    static mc::string_view type_name() noexcept
    {
        return pretty_name<MemberType>();
    }

    static mc::string_view get_signature()
    {
        return mc::reflect::get_signature<MemberType>();
    }
};

} // namespace detail

struct property_info : public property_type_info {
    using get_value_func_t = mc::variant (*)(const void*);
    using set_value_func_t = void (*)(void*, const mc::variant&);
    using string_func_t    = mc::string_view (*)();

    property_info(mc::string_view n, uint32_t relative_offset, get_value_func_t getter, set_value_func_t setter,
                  std::type_index typeinfo, mc::string_view type_name, mc::string_view signature);

    mc::variant       get_value_erased(const void* obj) const override;
    void              set_value_erased(void* obj, const mc::variant& value) const override;
    std::type_index   typeinfo() const override;
    mc::string_view   type_name() const noexcept override;
    mc::string_view   get_signature() const override;
    int               type() const noexcept override;
    uint32_t          offset() const noexcept override;
    member_info_base* clone() const override;

private:
    const void* get_member_ptr(const void* obj) const noexcept;
    void*       get_member_ptr(void* obj) const noexcept;

    uint32_t         m_relative_offset;
    get_value_func_t m_get_value;
    set_value_func_t m_set_value;
    std::type_index  m_typeinfo;
    mc::string_view  m_type_name;
    mc::string_view  m_signature;
};

template <typename MemberPtr>
struct static_property_info {
    static_assert(std::is_member_object_pointer_v<MemberPtr>, "static_property_info 只支持成员对象指针");

    using member_pointer_type   = MemberPtr;
    using member_pointer_traits = detail::property_member_pointer_traits<MemberPtr>;
    using member_type           = typename member_pointer_traits::member_type;
    using tag_type              = property_tag;
    using base_type             = typename member_pointer_traits::base_type;

    mc::string_view name;
    uint32_t        flags       = 0;
    uint32_t        base_offset = 0;
    uint64_t        data        = 0;
    MemberPtr       member_ptr;

    constexpr static_property_info(mc::string_view n, MemberPtr ptr) : name(n), member_ptr(ptr)
    {}

    constexpr int type() const noexcept
    {
        return static_cast<int>(member_info_type::property);
    }

    constexpr bool has_flags(uint32_t value) const noexcept
    {
        return (flags & value) == value;
    }

    constexpr void set_flags(uint32_t value) noexcept
    {
        flags |= value;
    }

    constexpr void set_data(uint32_t value) noexcept
    {
        data = value;
    }

    constexpr uint32_t get_data() const noexcept
    {
        return static_cast<uint32_t>(data);
    }

    property_info* create_runtime_property() const
    {
        using ops             = detail::property_member_ops<member_type>;
        auto* property        = new property_info(name, member_pointer_traits::offset(member_ptr), &ops::get_value,
                                                  &ops::set_value, typeid(member_type), pretty_name<member_type>(),
                                                  mc::reflect::get_signature<member_type>());
        property->flags       = flags;
        property->base_offset = base_offset;
        property->data        = data;
        return property;
    }
};

// 计算属性元数据具体实现
template <typename C, typename Getter, typename Setter = void*>
struct computed_property_info : public property_type_info {
    using class_type  = C;
    using member_type = typename mc::traits::function_traits<Getter>::return_type;
    using tag_type    = property_tag;
    using base_type   = C;

    using set_function_type = Setter;
    using get_function_type = Getter;

    get_function_type m_getter;
    set_function_type m_setter;

    constexpr computed_property_info(mc::string_view n, get_function_type getter, set_function_type setter = nullptr)
        : property_type_info(n), m_getter(getter), m_setter(setter)
    {
        if constexpr (std::is_same_v<set_function_type, void*>) {
            this->set_flags(MC_REFLECT_FLAG_READONLY);
        }
    }

    // 获取属性值
    mc::variant get_value(const C& obj) const
    {
        auto result = (get_object(obj).*m_getter)();
        if constexpr (std::is_constructible_v<mc::variant, decltype(result)>) {
            return mc::variant(result);
        } else {
            mc::variant value;
            mc::to_variant(result, value);
            return value;
        }
    }

    // 设置属性值
    void set_value(C& obj, const mc::variant& value) const
    {
        if constexpr (std::is_same_v<set_function_type, void*>) {
            MC_UNUSED(value);
        } else {
            (get_object(obj).*m_setter)(value.as<member_type>());
        }
    }

    mc::variant get_value_erased(const void* obj) const override
    {
        return get_value(*static_cast<const C*>(obj));
    }

    void set_value_erased(void* obj, const mc::variant& value) const override
    {
        set_value(*static_cast<C*>(obj), value);
    }

    uint32_t offset() const noexcept override
    {
        return 0;
    }

    std::type_index typeinfo() const override
    {
        return typeid(member_type);
    }

    mc::string_view type_name() const noexcept override
    {
        return pretty_name<member_type>();
    }

    mc::string_view get_signature() const override
    {
        return mc::reflect::get_signature<member_type>();
    }

    int type() const noexcept override
    {
        return static_cast<int>(member_info_type::computed_property);
    }

    member_info_base* clone() const override
    {
        auto* p = new computed_property_info<C, Getter, Setter>(this->name, m_getter, m_setter);
        p->name_quark =
            this->name_quark.valid() ? this->name_quark : mc::quark{mc::detail::intern_trusted_literal(this->name)};
        p->flags       = this->flags;
        p->base_offset = this->base_offset;
        p->data        = this->data;
        return p;
    }

    const C& get_object(const C& obj) const noexcept
    {
        return *this->template adjust_object_pointer<C>(&obj);
    }

    C& get_object(C& obj) const noexcept
    {
        return *this->template adjust_object_pointer<C>(&obj);
    }
};

//------------------------------------------------------------------------------
// 方法元数据结构
//------------------------------------------------------------------------------

struct method_type_info : public member_info_base {
    constexpr method_type_info(mc::string_view n) : member_info_base(n)
    {}

    virtual mc::string_view get_args_signature() const   = 0;
    virtual mc::string_view get_result_signature() const = 0;
    virtual size_t          arg_count() const            = 0;

    virtual bool         is_static() const                                              = 0;
    virtual mc::variant  invoke_erased(void* obj, const mc::variants& args) const       = 0;
    virtual async_result async_invoke_erased(void* obj, const mc::variants& args) const = 0;
    virtual mc::variant  invoke_static_erased(const mc::variants& args) const           = 0;
    virtual async_result async_invoke_static_erased(const mc::variants& args) const     = 0;

    // 使用反射信息基类直接调用对象方法，用于动态反射类型擦除后使用
    mc::variant  invoke(void* obj, const mc::variants& args) const;
    async_result async_invoke(void* obj, const mc::variants& args) const;
    template <typename C, std::enable_if_t<std::is_class_v<C>, int> = 0>
    mc::variant invoke(C& obj, const mc::variants& args) const
    {
        return invoke(&obj, args);
    }
    template <typename C, std::enable_if_t<std::is_class_v<C>, int> = 0>
    async_result async_invoke(C& obj, const mc::variants& args) const
    {
        return async_invoke(&obj, args);
    }

    mc::variant invoke(const mc::variants& args) const
    {
        return invoke_static_erased(args);
    }

    async_result async_invoke(const mc::variants& args) const
    {
        return async_invoke_static_erased(args);
    }
};

template <typename C>
using method_info_base = method_type_info;

struct method_info : public method_type_info {
    using invoke_func_t                  = mc::variant (*)(const method_info&, void*, const mc::variants&);
    using async_invoke_func_t            = async_result (*)(const method_info&, void*, const mc::variants&);
    using invoke_static_func_t           = mc::variant (*)(const method_info&, const mc::variants&);
    using async_invoke_static_func_t     = async_result (*)(const method_info&, const mc::variants&);
    using invoke_raw_func_t              = mc::variant (*)(const method_info&, void*, const void*);
    using async_invoke_raw_func_t        = async_result (*)(const method_info&, void*, const void*);
    using invoke_static_raw_func_t       = mc::variant (*)(const method_info&, const void*);
    using async_invoke_static_raw_func_t = async_result (*)(const method_info&, const void*);

    method_info(mc::string_view n, uint32_t relative_offset, uint32_t function_size, uint32_t method_arg_count,
                bool is_static_method, invoke_func_t invoke_func, async_invoke_func_t async_invoke_func,
                invoke_static_func_t invoke_static_func, async_invoke_static_func_t async_invoke_static_func,
                invoke_raw_func_t invoke_raw_func, async_invoke_raw_func_t async_invoke_raw_func,
                invoke_static_raw_func_t       invoke_static_raw_func,
                async_invoke_static_raw_func_t async_invoke_static_raw_func, std::type_index typeinfo,
                mc::string_view type_name, mc::string_view args_signature, mc::string_view result_signature);

    static method_info* create(mc::string_view n, uint32_t relative_offset, uint32_t function_size,
                               uint32_t method_arg_count, bool is_static_method, invoke_func_t invoke_func,
                               async_invoke_func_t async_invoke_func, invoke_static_func_t invoke_static_func,
                               async_invoke_static_func_t async_invoke_static_func, invoke_raw_func_t invoke_raw_func,
                               async_invoke_raw_func_t        async_invoke_raw_func,
                               invoke_static_raw_func_t       invoke_static_raw_func,
                               async_invoke_static_raw_func_t async_invoke_static_raw_func, std::type_index typeinfo,
                               mc::string_view type_name, mc::string_view args_signature,
                               mc::string_view result_signature, const void* function_data);

    bool              is_static() const override;
    mc::variant       invoke_erased(void* obj, const mc::variants& args) const override;
    async_result      async_invoke_erased(void* obj, const mc::variants& args) const override;
    mc::variant       invoke_static_erased(const mc::variants& args) const override;
    async_result      async_invoke_static_erased(const mc::variants& args) const override;
    std::type_index   typeinfo() const override;
    mc::string_view   type_name() const noexcept override;
    size_t            arg_count() const override;
    mc::string_view   get_args_signature() const override;
    mc::string_view   get_result_signature() const override;
    uint32_t          offset() const noexcept override;
    int               type() const noexcept override;
    member_info_base* clone() const override;
    const void*       function_storage() const noexcept;
    void*             function_storage() noexcept;
    mc::variant       invoke_raw(void* obj, const void* converted_args) const;
    mc::variant       invoke_static_raw(const void* converted_args) const;
    async_result      async_invoke_raw(void* obj, const void* converted_args) const;
    async_result      async_invoke_static_raw(const void* converted_args) const;

private:
    uint32_t                       m_relative_offset;
    uint32_t                       m_function_size;
    uint32_t                       m_arg_count;
    bool                           m_is_static;
    invoke_func_t                  m_invoke;
    async_invoke_func_t            m_async_invoke;
    invoke_static_func_t           m_invoke_static;
    async_invoke_static_func_t     m_async_invoke_static;
    invoke_raw_func_t              m_invoke_raw;
    async_invoke_raw_func_t        m_async_invoke_raw;
    invoke_static_raw_func_t       m_invoke_static_raw;
    async_invoke_static_raw_func_t m_async_invoke_static_raw;
    std::type_index                m_typeinfo;
    mc::string_view                m_type_name;
    mc::string_view                m_args_signature;
    mc::string_view                m_result_signature;
};

struct runtime_method_deleter {
    void operator()(method_info* method) const noexcept
    {
        if (!method) {
            return;
        }
        ::operator delete(method);
    }
};

using method_info_ptr = std::unique_ptr<method_info, runtime_method_deleter>;

namespace detail {

template <typename FunctionPtr>
struct method_function_traits;

template <typename T>
struct is_result_type : std::false_type {};

template <typename T>
struct is_result_type<mc::result<T>> : std::true_type {};

template <typename T>
constexpr bool is_result_type_v = is_result_type<mc::traits::remove_cvref_t<T>>::value;

template <typename T>
constexpr bool is_async_return_type_v =
    is_result_type_v<T> || mc::futures::detail::is_future_v<mc::traits::remove_cvref_t<T>>;

template <typename R, typename BaseT, typename... Args>
struct method_function_traits<R (BaseT::*)(Args...)> {
    using function_type               = R (BaseT::*)(Args...);
    using raw_result_type             = R;
    using result_type                 = mc::traits::remove_cvref_t<R>;
    using args_type                   = std::tuple<mc::traits::remove_cvref_t<Args>...>;
    using base_type                   = BaseT;
    static constexpr bool   is_static = false;
    static constexpr size_t arg_count = sizeof...(Args);
};

template <typename R, typename BaseT, typename... Args>
struct method_function_traits<R (BaseT::*)(Args...) const> {
    using function_type               = R (BaseT::*)(Args...) const;
    using raw_result_type             = R;
    using result_type                 = mc::traits::remove_cvref_t<R>;
    using args_type                   = std::tuple<mc::traits::remove_cvref_t<Args>...>;
    using base_type                   = BaseT;
    static constexpr bool   is_static = false;
    static constexpr size_t arg_count = sizeof...(Args);
};

template <typename R, typename... Args>
struct method_function_traits<R (*)(Args...)> {
    using function_type               = R (*)(Args...);
    using raw_result_type             = R;
    using result_type                 = mc::traits::remove_cvref_t<R>;
    using args_type                   = std::tuple<mc::traits::remove_cvref_t<Args>...>;
    using base_type                   = void;
    static constexpr bool   is_static = true;
    static constexpr size_t arg_count = sizeof...(Args);
};

template <typename FunctionType>
FunctionType load_method_function(const method_info& info)
{
    FunctionType function;
    std::memcpy(&function, info.function_storage(), sizeof(function));
    return function;
}

template <typename R, typename... Args>
struct method_signature_ops {
    using raw_result_type = R;
    using result_type     = mc::traits::remove_cvref_t<R>;
    using args_type       = std::tuple<mc::traits::remove_cvref_t<Args>...>;

    static_assert(std::is_void_v<R> || is_variant_constructible_v<R> || is_variant_v<R>,
                  "方法返回类型必须是void或者可以转换为mc::variant");
    static_assert(all_variant_convertible_v<mc::traits::remove_cvref_t<Args>...>, "参数类型必须可从mc::variant转换");

    static std::type_index typeinfo()
    {
        return typeid(R);
    }

    static mc::string_view type_name() noexcept
    {
        return pretty_name<R>();
    }

    static mc::string_view get_args_signature()
    {
        return mc::reflect::get_signature<args_type>();
    }

    static mc::string_view get_result_signature()
    {
        return mc::reflect::get_signature<result_type>();
    }

    template <size_t... I>
    static args_type convert_args_impl(mc::string_view name, const mc::variants& args, std::index_sequence<I...>)
    {
        return args_type{mc::detail::convert_arg<mc::traits::remove_cvref_t<Args>>(name.data(), args[I])...};
    }

    static args_type convert_args(mc::string_view name, const mc::variants& args)
    {
        constexpr size_t arg_count = sizeof...(Args);
        if constexpr (std::is_same_v<args_type, std::tuple<mc::variants>>) {
            return args_type{args};
        } else {
            if (args.size() < arg_count) {
                throw_method_arg_not_enough(name, arg_count, args.size());
            }
            return convert_args_impl(name, args, std::make_index_sequence<arg_count>());
        }
    }

    static mc::variant invoke_erased(const method_info& info, void* obj, const mc::variants& args)
    {
        auto converted = convert_args(info.name, args);
        return info.invoke_raw(obj, &converted);
    }

    static mc::variant invoke_static(const method_info& info, const mc::variants& args)
    {
        auto converted = convert_args(info.name, args);
        return info.invoke_static_raw(&converted);
    }

    static async_result async_invoke_erased(const method_info& info, void* obj, const mc::variants& args)
    {
        auto converted = convert_args(info.name, args);
        return info.async_invoke_raw(obj, &converted);
    }

    static async_result async_invoke_static(const method_info& info, const mc::variants& args)
    {
        auto converted = convert_args(info.name, args);
        return info.async_invoke_static_raw(&converted);
    }
};

template <typename FunctionPtr>
struct method_signature_for;

template <typename R, typename BaseT, typename... Args>
struct method_signature_for<R (BaseT::*)(Args...)> : method_signature_ops<R, Args...> {};

template <typename R, typename BaseT, typename... Args>
struct method_signature_for<R (BaseT::*)(Args...) const> : method_signature_ops<R, Args...> {};

template <typename R, typename... Args>
struct method_signature_for<R (*)(Args...)> : method_signature_ops<R, Args...> {};

inline mc::variant invoke_non_static_method_as_static(const method_info& info, const mc::variants&)
{
    throw_method_not_exist(info.name);
}

template <typename FunctionPtr>
struct method_function_ops;

template <typename R, typename BaseT, typename... Args>
struct method_function_ops<R (BaseT::*)(Args...)> {
    using function_type = R (BaseT::*)(Args...);
    using args_type     = typename method_signature_ops<R, Args...>::args_type;

    static mc::variant invoke_raw(const method_info& info, void* obj, const void* converted_args)
    {
        auto        function = load_method_function<function_type>(info);
        auto&       target   = *reinterpret_cast<BaseT*>(reinterpret_cast<char*>(obj) + info.base_offset);
        const auto& values   = *static_cast<const args_type*>(converted_args);
        if constexpr (std::is_void_v<R>) {
            std::apply([&](const auto&... converted) {
                (target.*function)(converted...);
            }, values);
            return {};
        } else {
            return std::apply([&](const auto&... converted) {
                return mc::variant((target.*function)(converted...));
            }, values);
        }
    }

    static async_result async_invoke_raw(const method_info& info, void* obj, const void* converted_args)
    {
        auto        function = load_method_function<function_type>(info);
        auto&       target   = *reinterpret_cast<BaseT*>(reinterpret_cast<char*>(obj) + info.base_offset);
        const auto& values   = *static_cast<const args_type*>(converted_args);
        return std::apply([&](const auto&... converted) -> async_result {
            return async_result((target.*function)(converted...));
        }, values);
    }
};

template <typename R, typename BaseT, typename... Args>
struct method_function_ops<R (BaseT::*)(Args...) const> {
    using function_type = R (BaseT::*)(Args...) const;
    using args_type     = typename method_signature_ops<R, Args...>::args_type;

    static mc::variant invoke_raw(const method_info& info, void* obj, const void* converted_args)
    {
        auto        function = load_method_function<function_type>(info);
        const auto& target   = *reinterpret_cast<const BaseT*>(reinterpret_cast<const char*>(obj) + info.base_offset);
        const auto& values   = *static_cast<const args_type*>(converted_args);
        if constexpr (std::is_void_v<R>) {
            std::apply([&](const auto&... converted) {
                (target.*function)(converted...);
            }, values);
            return {};
        } else {
            return std::apply([&](const auto&... converted) {
                return mc::variant((target.*function)(converted...));
            }, values);
        }
    }

    static async_result async_invoke_raw(const method_info& info, void* obj, const void* converted_args)
    {
        auto        function = load_method_function<function_type>(info);
        const auto& target   = *reinterpret_cast<const BaseT*>(reinterpret_cast<const char*>(obj) + info.base_offset);
        const auto& values   = *static_cast<const args_type*>(converted_args);
        return std::apply([&](const auto&... converted) -> async_result {
            return async_result((target.*function)(converted...));
        }, values);
    }
};

template <typename R, typename... Args>
struct method_function_ops<R (*)(Args...)> {
    using function_type = R (*)(Args...);
    using args_type     = typename method_signature_ops<R, Args...>::args_type;

    static mc::variant invoke_static_raw(const method_info& info, const void* converted_args)
    {
        auto        function = load_method_function<function_type>(info);
        const auto& values   = *static_cast<const args_type*>(converted_args);
        if constexpr (std::is_void_v<R>) {
            std::apply([&](const auto&... converted) {
                function(converted...);
            }, values);
            return {};
        } else {
            return std::apply([&](const auto&... converted) {
                return mc::variant(function(converted...));
            }, values);
        }
    }

    static async_result async_invoke_static_raw(const method_info& info, const void* converted_args)
    {
        auto        function = load_method_function<function_type>(info);
        const auto& values   = *static_cast<const args_type*>(converted_args);
        return std::apply([&](const auto&... converted) -> async_result {
            return async_result(function(converted...));
        }, values);
    }
};

template <typename FunctionPtr, bool IsStatic, bool IsAsync>
struct method_runtime_bindings_impl;

template <typename FunctionPtr>
struct method_runtime_bindings_impl<FunctionPtr, false, false> {
    using signature_ops = method_signature_for<FunctionPtr>;
    using function_ops  = method_function_ops<FunctionPtr>;

    inline static constexpr method_info::invoke_func_t        invoke_func        = &signature_ops::invoke_erased;
    inline static constexpr method_info::async_invoke_func_t  async_invoke_func  = nullptr;
    inline static constexpr method_info::invoke_static_func_t invoke_static_func = &invoke_non_static_method_as_static;
    inline static constexpr method_info::async_invoke_static_func_t     async_invoke_static_func = nullptr;
    inline static constexpr method_info::invoke_raw_func_t              invoke_raw_func = &function_ops::invoke_raw;
    inline static constexpr method_info::async_invoke_raw_func_t        async_invoke_raw_func        = nullptr;
    inline static constexpr method_info::invoke_static_raw_func_t       invoke_static_raw_func       = nullptr;
    inline static constexpr method_info::async_invoke_static_raw_func_t async_invoke_static_raw_func = nullptr;
};

template <typename FunctionPtr>
struct method_runtime_bindings_impl<FunctionPtr, false, true> {
    using signature_ops = method_signature_for<FunctionPtr>;
    using function_ops  = method_function_ops<FunctionPtr>;

    inline static constexpr method_info::invoke_func_t        invoke_func        = &signature_ops::invoke_erased;
    inline static constexpr method_info::async_invoke_func_t  async_invoke_func  = &signature_ops::async_invoke_erased;
    inline static constexpr method_info::invoke_static_func_t invoke_static_func = &invoke_non_static_method_as_static;
    inline static constexpr method_info::async_invoke_static_func_t async_invoke_static_func = nullptr;
    inline static constexpr method_info::invoke_raw_func_t          invoke_raw_func = &function_ops::invoke_raw;
    inline static constexpr method_info::async_invoke_raw_func_t    async_invoke_raw_func =
        &function_ops::async_invoke_raw;
    inline static constexpr method_info::invoke_static_raw_func_t       invoke_static_raw_func       = nullptr;
    inline static constexpr method_info::async_invoke_static_raw_func_t async_invoke_static_raw_func = nullptr;
};

template <typename FunctionPtr>
struct method_runtime_bindings_impl<FunctionPtr, true, false> {
    using signature_ops = method_signature_for<FunctionPtr>;
    using function_ops  = method_function_ops<FunctionPtr>;

    inline static constexpr method_info::invoke_func_t              invoke_func        = nullptr;
    inline static constexpr method_info::async_invoke_func_t        async_invoke_func  = nullptr;
    inline static constexpr method_info::invoke_static_func_t       invoke_static_func = &signature_ops::invoke_static;
    inline static constexpr method_info::async_invoke_static_func_t async_invoke_static_func = nullptr;
    inline static constexpr method_info::invoke_raw_func_t          invoke_raw_func          = nullptr;
    inline static constexpr method_info::async_invoke_raw_func_t    async_invoke_raw_func    = nullptr;
    inline static constexpr method_info::invoke_static_raw_func_t   invoke_static_raw_func =
        &function_ops::invoke_static_raw;
    inline static constexpr method_info::async_invoke_static_raw_func_t async_invoke_static_raw_func = nullptr;
};

template <typename FunctionPtr>
struct method_runtime_bindings_impl<FunctionPtr, true, true> {
    using signature_ops = method_signature_for<FunctionPtr>;
    using function_ops  = method_function_ops<FunctionPtr>;

    inline static constexpr method_info::invoke_func_t              invoke_func        = nullptr;
    inline static constexpr method_info::async_invoke_func_t        async_invoke_func  = nullptr;
    inline static constexpr method_info::invoke_static_func_t       invoke_static_func = &signature_ops::invoke_static;
    inline static constexpr method_info::async_invoke_static_func_t async_invoke_static_func =
        &signature_ops::async_invoke_static;
    inline static constexpr method_info::invoke_raw_func_t        invoke_raw_func       = nullptr;
    inline static constexpr method_info::async_invoke_raw_func_t  async_invoke_raw_func = nullptr;
    inline static constexpr method_info::invoke_static_raw_func_t invoke_static_raw_func =
        &function_ops::invoke_static_raw;
    inline static constexpr method_info::async_invoke_static_raw_func_t async_invoke_static_raw_func =
        &function_ops::async_invoke_static_raw;
};

template <typename FunctionPtr>
using method_runtime_bindings =
    method_runtime_bindings_impl<FunctionPtr, method_function_traits<FunctionPtr>::is_static,
                                 is_async_return_type_v<typename method_function_traits<FunctionPtr>::raw_result_type>>;

template <auto Function>
struct method_function_constant {
    using function_type                         = decltype(Function);
    inline static constexpr function_type value = Function;
};

template <typename FunctionPtr>
struct erased_method_registration_factory {
    using function_traits = method_function_traits<FunctionPtr>;
    using bindings        = method_runtime_bindings<FunctionPtr>;

    static method_info* create(mc::string_view name, const void* function_data)
    {
        const auto& function = *static_cast<const FunctionPtr*>(function_data);
        return method_info::create(
            name, static_cast<uint32_t>(get_function_offset(function)), static_cast<uint32_t>(sizeof(FunctionPtr)),
            static_cast<uint32_t>(function_traits::arg_count), function_traits::is_static, bindings::invoke_func,
            bindings::async_invoke_func, bindings::invoke_static_func, bindings::async_invoke_static_func,
            bindings::invoke_raw_func, bindings::async_invoke_raw_func, bindings::invoke_static_raw_func,
            bindings::async_invoke_static_raw_func, typeid(typename function_traits::raw_result_type),
            pretty_name<typename function_traits::raw_result_type>(),
            mc::reflect::get_signature<typename function_traits::args_type>(),
            mc::reflect::get_signature<typename function_traits::result_type>(), &function);
    }
};

} // namespace detail

struct method_registration_info {
    using create_runtime_method_func_t = method_info* (*)(mc::string_view name, const void* function_data);
    using tag_type                     = method_tag;

    mc::string_view              name;
    uint32_t                     flags           = 0;
    uint32_t                     base_offset     = 0;
    uint64_t                     data            = 0;
    create_runtime_method_func_t m_create_method = nullptr;
    const void*                  m_function_data = nullptr;

    constexpr method_registration_info(mc::string_view n, create_runtime_method_func_t create_method,
                                       const void* function_data)
        : name(n), m_create_method(create_method), m_function_data(function_data)
    {}

    constexpr int type() const noexcept
    {
        return static_cast<int>(member_info_type::method);
    }

    constexpr bool has_flags(uint32_t value) const noexcept
    {
        return (flags & value) == value;
    }

    constexpr void set_flags(uint32_t value) noexcept
    {
        flags |= value;
    }

    constexpr void set_data(uint32_t value) noexcept
    {
        data = value;
    }

    constexpr uint32_t get_data() const noexcept
    {
        return static_cast<uint32_t>(data);
    }

    method_info* create_runtime_method() const
    {
        auto* method        = m_create_method(name, m_function_data);
        method->flags       = flags;
        method->base_offset = base_offset;
        method->data        = data;
        return method;
    }

    method_info_ptr create_runtime_method_ptr() const
    {
        return method_info_ptr(create_runtime_method());
    }
};

template <typename FunctionPtr>
struct static_method_info : public method_registration_info {
    static_assert(std::is_member_function_pointer_v<FunctionPtr> || mc::detail::is_function_pointer<FunctionPtr>::value,
                  "static_method_info 只支持成员函数或静态函数");

    using function_type   = FunctionPtr;
    using function_traits = detail::method_function_traits<FunctionPtr>;
    using result_type     = typename function_traits::result_type;
    using args_type       = typename function_traits::args_type;
    using class_type      = std::conditional_t<function_traits::is_static, void, typename function_traits::base_type>;
    using base_type       = std::conditional_t<function_traits::is_static, void, typename function_traits::base_type>;

    FunctionPtr m_function;

    constexpr static_method_info(mc::string_view n, FunctionPtr function)
        : method_registration_info(n, &detail::erased_method_registration_factory<FunctionPtr>::create, &m_function),
          m_function(function)
    {}
};

template <auto Function>
constexpr auto make_erased_static_method_info(mc::string_view name)
{
    using function_type = decltype(Function);
    return std::tuple<method_registration_info>{
        method_registration_info{name, &detail::erased_method_registration_factory<function_type>::create,
                                 &detail::method_function_constant<Function>::value}};
}

//------------------------------------------------------------------------------
// 基类元数据结构
//------------------------------------------------------------------------------
struct base_class_type_info : public member_info_base {
    constexpr base_class_type_info(mc::string_view n) : member_info_base(n)
    {}

    virtual type_id_type           get_type_id() const   = 0;
    virtual mc::string_view        get_signature() const = 0;
    virtual const struct_metadata& get_metadata() const  = 0;

    mc::variant  get_value(void* obj, mc::string_view name) const;
    void         set_value(void* obj, mc::string_view name, const mc::variant& value) const;
    mc::variant  invoke(void* obj, mc::string_view name, const mc::variants& args) const;
    async_result async_invoke(void* obj, mc::string_view name, const mc::variants& args) const;
    template <typename C, std::enable_if_t<std::is_class_v<C>, int> = 0>
    mc::variant get_value(const C& obj, mc::string_view name) const
    {
        return get_value(&obj, name);
    }
    template <typename C, std::enable_if_t<std::is_class_v<C>, int> = 0>
    void set_value(C& obj, mc::string_view name, const mc::variant& value) const
    {
        set_value(&obj, name, value);
    }
    template <typename C, std::enable_if_t<std::is_class_v<C>, int> = 0>
    mc::variant invoke(C& obj, mc::string_view name, const mc::variants& args) const
    {
        return invoke(&obj, name, args);
    }
    template <typename C, std::enable_if_t<std::is_class_v<C>, int> = 0>
    async_result async_invoke(C& obj, mc::string_view name, const mc::variants& args) const
    {
        return async_invoke(&obj, name, args);
    }
};

template <typename C>
struct base_class_info_base : public base_class_type_info {
    constexpr base_class_info_base(mc::string_view n = {}) : base_class_type_info(n)
    {}

    virtual mc::variant  get_value(const C& obj, mc::string_view name) const                        = 0;
    virtual void         set_value(C& obj, mc::string_view name, const mc::variant& value) const    = 0;
    virtual mc::variant  invoke(C& obj, mc::string_view name, const mc::variants& args) const       = 0;
    virtual async_result async_invoke(C& obj, mc::string_view name, const mc::variants& args) const = 0;
};

// 属性元数据具体实现
template <typename C, typename BaseT>
struct base_class_info : public base_class_info_base<C> {
    using class_type  = C;
    using member_type = BaseT;
    using base_type   = BaseT;
    using tag_type    = base_class_tag;

    constexpr base_class_info(mc::string_view base_class_name) : base_class_info_base<C>(base_class_name)
    {}

    std::type_index typeinfo() const override
    {
        return typeid(base_type);
    }

    mc::string_view type_name() const noexcept override
    {
        return pretty_name<base_type>();
    }

    uint32_t offset() const noexcept override
    {
        return static_cast<uint32_t>(mc::get_base_offset<class_type, base_type>());
    }

    const struct_metadata& get_metadata() const override
    {
        return reflector<base_type>::get_metadata();
    }

    mc::string_view get_signature() const override
    {
        return mc::reflect::get_signature<base_type>();
    }

    type_id_type get_type_id() const override
    {
        return reflector<base_type>::get_type_id();
    }

    mc::variant  get_value(const C& obj, mc::string_view name) const override;
    void         set_value(C& obj, mc::string_view name, const mc::variant& value) const override;
    mc::variant  invoke(C& obj, mc::string_view name, const mc::variants& args) const override;
    async_result async_invoke(C& obj, mc::string_view name, const mc::variants& args) const override;

    int type() const noexcept override
    {
        return static_cast<int>(member_info_type::base_class);
    }

    member_info_base* clone() const override
    {
        auto* p = new base_class_info<C, BaseT>(this->name);
        p->name_quark =
            this->name_quark.valid() ? this->name_quark : mc::quark{mc::detail::intern_trusted_literal(this->name)};
        p->flags       = this->flags;
        p->base_offset = this->base_offset;
        p->data        = this->data;
        return p;
    }

    const BaseT& get_object(const C& obj) const noexcept
    {
        return *this->template adjust_object_pointer<BaseT>(&obj);
    }

    BaseT& get_object(C& obj) const noexcept
    {
        return *this->template adjust_object_pointer<BaseT>(&obj);
    }
};

/**
 * @brief 枚举成员信息
 */
using enum_value_type = uint32_t;
struct enum_member_info {
    mc::string_view name;  // 枚举成员名称
    enum_value_type value; // 枚举值

    constexpr enum_member_info() : name(), value(0)
    {}

    template <typename EnumType>
    constexpr enum_member_info(mc::string_view n, EnumType v) : name(n), value(static_cast<enum_value_type>(v))
    {
        static_assert(std::is_enum_v<EnumType>, "EnumType must be an enum type");
    }

    // 使用默认的拷贝/移动操作
    constexpr enum_member_info(const enum_member_info&)            = default;
    constexpr enum_member_info(enum_member_info&&)                 = default;
    constexpr enum_member_info& operator=(const enum_member_info&) = default;
    constexpr enum_member_info& operator=(enum_member_info&&)      = default;
};

/** @brief 自定义成员类型的扩展点 */
template <typename T, typename M, typename BaseT = T, typename = void>
struct member_info_creator {
    static constexpr auto create(M BaseT::* member_ptr, mc::string_view name)
    {
        MC_UNUSED(member_ptr);
        MC_UNUSED(name);
        static_assert(is_property_v<M BaseT::*> || is_method_v<M BaseT::*>,
                      "不支持的成员类型，请为此类型创建特化版本的 member_info_creator");
        return std::tuple<>();
    }
};

// 属性成员特化
template <typename T, typename M, typename BaseT>
struct member_info_creator<T, M, BaseT, std::enable_if_t<is_property_v<M BaseT::*>>> {
    static constexpr auto create(M BaseT::* member_ptr, mc::string_view name)
    {
        return std::tuple<static_property_info<M BaseT::*>>{static_property_info<M BaseT::*>{name, member_ptr}};
    }
};

template <typename T, typename M, typename BaseT>
struct member_info_creator<T, M, BaseT,
                           std::enable_if_t<!is_property_v<M BaseT::*> && std::is_member_object_pointer_v<M BaseT::*> &&
                                            (is_variant_constructible_v<M> || is_variant_convertible_v<M>)>> {
    static constexpr auto create(M BaseT::* member_ptr, mc::string_view name)
    {
        return std::tuple<static_property_info<M BaseT::*>>{static_property_info<M BaseT::*>{name, member_ptr}};
    }
};

// 方法成员特化
template <typename T, typename M, typename BaseT>
struct member_info_creator<T, M, BaseT, std::enable_if_t<is_method_v<M BaseT::*>>> {
    static constexpr auto create(M BaseT::* member_ptr, mc::string_view name)
    {
        return std::tuple<static_method_info<M BaseT::*>>{static_method_info<M BaseT::*>{name, member_ptr}};
    }
};

// 静态成员函数特化
template <typename T, typename R, typename... Args>
struct member_info_creator<T, R (*)(Args...), void, std::enable_if_t<is_method_v<R (*)(Args...)>>> {
    static constexpr auto create(R (*static_func)(Args...), mc::string_view name)
    {
        return std::tuple<static_method_info<R (*)(Args...)>>{static_method_info<R (*)(Args...)>{name, static_func}};
    }
};

struct computed_property_info_creator {
    template <typename T, typename Getter, typename Setter>
    static constexpr auto create(Getter getter, Setter setter, mc::string_view name)
    {
        return std::make_tuple(computed_property_info<T, Getter, Setter>{name, getter, setter});
    }
};

template <typename T, typename Base>
struct base_class_info_creator {
    static_assert(std::is_base_of_v<Base, T>, "T must be derived from Base class");
    static_assert(is_reflectable<Base>(), "Base class must be reflectable");

    static constexpr auto create(mc::string_view base_class_name)
    {
        if (base_class_name.empty()) {
            base_class_name = get_type_name<Base>();
        }
        return std::make_tuple(base_class_info<T, Base>{base_class_name});
    }
};

namespace detail {

// 递归过滤元组元素，仅保留具有特定标签的元素
template <typename T, typename Filter, size_t Index, typename Result, typename... Tuples>
constexpr auto filter_members_impl(const std::tuple<Tuples...>& all_members, Result result)
{
    if constexpr (Index >= sizeof...(Tuples)) {
        return result;
    } else {
        using element_type = mc::traits::remove_cvref_t<std::tuple_element_t<Index, std::tuple<Tuples...>>>;
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
constexpr auto filter_members(const Tuple& all_members)
{
    return filter_members_impl<T, Filter, 0>(all_members, std::tuple<>{});
}

template <typename Tag>
struct filter_tag {
    template <typename ElementType>
    static constexpr bool check = has_tag_v<Tag, ElementType>;
};

template <typename T, typename Tag, typename Tuple>
auto filter_members_by_tag(const Tuple& all_members)
{
    return filter_members<T, filter_tag<Tag>>(all_members);
}

// 编译时简单的冒泡排序算法
template <typename T, size_t N>
constexpr std::array<T, N> sort_enum_values(std::array<T, N> arr)
{
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
constexpr auto enum_tuple_to_array(const Members& members)
{
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