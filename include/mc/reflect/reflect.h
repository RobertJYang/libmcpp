/*
 * Copyright (c) 2023 Huawei Technologies Co., Ltd.
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
 * @file reflect.h
 * @brief 反射相关的宏定义
 */
#ifndef MC_REFLECT_REFLECT_H
#define MC_REFLECT_REFLECT_H

#include <array>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

#include <mc/common.h>
#include <mc/dict.h>
#include <mc/reflect/metadata.h>
#include <mc/reflect/metadata_info.h>
#include <mc/reflect/reflection.h>
#include <mc/reflect/reflection_enum.h>
#include <mc/reflect/signature.h>
#include <mc/traits.h>
#include <mc/variant.h>

// 检测是否为元组形式（双括号表达式）
#define MC_REFLECT_IS_TUPLE(x) BOOST_PP_IS_BEGIN_PARENS(x)

#define MC_REFLECT_ARG_COUNT(...) BOOST_PP_VARIADIC_SIZE(__VA_ARGS__)

// 处理带名称的成员（双括号形式）
#define MC_REFLECT_MEMBER_WITH_NAME_1(r, TYPE, MEMBER)                                   \
    mc::reflect::detail::create_member_info<TYPE>(&TYPE::BOOST_PP_TUPLE_ELEM(0, MEMBER), \
                                                  BOOST_PP_TUPLE_ELEM(1, MEMBER)),

#define MC_REFLECT_MEMBER_WITH_NAME_2(r, TYPE, MEMBER)                                    \
    mc::reflect::detail::create_member_info<TYPE>(                                        \
        &TYPE::BOOST_PP_TUPLE_ELEM(2, MEMBER),                                            \
        BOOST_PP_IIF(BOOST_PP_GREATER(BOOST_PP_TUPLE_SIZE(MEMBER), 3),                    \
                     &TYPE::BOOST_PP_TUPLE_ELEM(3, MEMBER), static_cast<void*>(nullptr)), \
        BOOST_PP_TUPLE_ELEM(1, MEMBER)),

#define MC_REFLECT_MEMBER_WITH_NAME(r, TYPE, MEMBER)                                              \
    BOOST_PP_IIF(BOOST_PP_GREATER(BOOST_PP_TUPLE_SIZE(MEMBER), 2), MC_REFLECT_MEMBER_WITH_NAME_2, \
                 MC_REFLECT_MEMBER_WITH_NAME_1)(r, TYPE, MEMBER)

// 处理不带名称的成员（单括号形式）
#define MC_REFLECT_MEMBER_WITHOUT_NAME(r, TYPE, MEMBER)                        \
    BOOST_PP_IIF(BOOST_PP_IS_EMPTY(MEMBER), std::tuple{},                      \
                 (mc::reflect::detail::create_member_info<TYPE>(&TYPE::MEMBER, \
                                                                BOOST_PP_STRINGIZE(MEMBER)))),

// 处理成员
#define MC_REFLECT_ELEMENT(r, TYPE, MEMBER)                                \
    BOOST_PP_IIF(MC_REFLECT_IS_TUPLE(MEMBER), MC_REFLECT_MEMBER_WITH_NAME, \
                 MC_REFLECT_MEMBER_WITHOUT_NAME)(r, TYPE, MEMBER)

// 简单基类，使用基类自身的反射名称
#define MC_REFLECT_BASE_CLASS_WITHOUT_NAME(r, TYPE, BASE) \
    BOOST_PP_IIF(                                         \
        BOOST_PP_IS_EMPTY(BASE), std::tuple{},            \
        (mc::reflect::detail::create_base_class_info<TYPE, BASE>())),

// 带别名的基类，允许在继承时重新指定该基类在这里的反射名称
#define MC_REFLECT_BASE_CLSS_WITH_NAME(r, TYPE, BASE)                                \
    mc::reflect::detail::create_base_class_info<TYPE, BOOST_PP_TUPLE_ELEM(0, BASE)>( \
        BOOST_PP_TUPLE_ELEM(1, BASE)),

// 处理基类
#define MC_REFLECT_BASE_ELEMENT(r, TYPE, BASE)                              \
    BOOST_PP_IIF(MC_REFLECT_IS_TUPLE(BASE), MC_REFLECT_BASE_CLSS_WITH_NAME, \
                 MC_REFLECT_BASE_CLASS_WITHOUT_NAME)(r, TYPE, BASE)

// 在命名空间中添加辅助函数
namespace mc::reflect::detail {
// 创建成员元数据（根据成员类型分发到属性、方法或用户自定义的成员信息）
template <typename T, typename M, typename BaseT>
constexpr auto create_member_info(M BaseT::* member_ptr, std::string_view name) {
    return member_info_creator<T, M, BaseT>::create(member_ptr, name);
}

template <typename T, typename Getter, typename Setter>
constexpr auto create_member_info(Getter getter, Setter setter, std::string_view name) {
    return computed_property_info_creator::create<T>(getter, setter, name);
}

template <typename T, typename R, typename... Args>
constexpr auto create_member_info(R (*static_func)(Args...), std::string_view name) {
    return member_info_creator<T, R (*)(Args...), void>::create(static_func, name);
}

template <typename T, typename BaseT>
constexpr auto create_base_class_info(std::string_view base_class_name = {}) {
    return base_class_info_creator<T, BaseT>::create(base_class_name);
}

template <typename EnumType>
constexpr auto create_enum_member_info(EnumType value, std::string_view name) {
    return enum_member_info(name, value);
}

template <size_t Index, typename... Tuples>
bool is_member_override(const member_info_base&      base_member,
                        const std::tuple<Tuples...>& all_members) {
    if constexpr (Index >= sizeof...(Tuples)) {
        return false;
    } else {
        if (std::get<Index>(all_members).name == base_member.name) {
            return true;
        } else {
            return is_member_override<Index + 1>(base_member, all_members);
        }
    }
}

// 默认的成员检查模板，总是返回true
template <typename T, typename Member, typename = void>
struct has_members_check {
    static constexpr bool check(const Member&) {
        return true;
    }
};

// 检测类型是否提供了check_member模板函数
template <typename T, typename Members, typename = void>
struct has_check_members : std::false_type {};

template <typename T, typename Members>
struct has_check_members<
    T, Members, std::void_t<decltype(T::template check_members<Members>(std::declval<Members>()))>>
    : std::true_type {};

// 辅助函数：验证某个成员
template <typename T, typename Members>
static constexpr bool validate_members(const Members& members) {
    if constexpr (has_check_members<T, Members>::value) {
        return T::check_members(members);
    }
    return true;
}

// 检测类型是否提供了initial_member模板函数
template <typename T, typename Members, typename = void>
struct has_initial_members : std::false_type {};

template <typename T, typename Members>
struct has_initial_members<
    T, Members,
    std::void_t<decltype(T::template initial_members<Members>(std::declval<Members>()))>>
    : std::true_type {};

// 辅助函数：初始化某个成员
template <typename T, typename Members>
static constexpr auto initial_members(const Members& members) {
    if constexpr (has_initial_members<T, Members>::value) {
        T::initial_members(members);
    }
    return members;
}
} // namespace mc::reflect::detail

namespace mc {
namespace reflect {

namespace detail {

template <typename T, typename = void>
struct has_custom_to_variant : std::false_type {};

template <typename T>
struct has_custom_to_variant<T, std::void_t<decltype(T::to_variant(
                                    std::declval<const T&>(), std::declval<mc::mutable_dict&>()))>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_custom_to_variant_v = has_custom_to_variant<T>::value;

template <typename T>
auto has_custom_from_variant(int)
    -> decltype(T::from_variant(std::declval<const mc::dict&>(), std::declval<T&>()),
                std::true_type{});

template <typename T>
std::false_type has_custom_from_variant(...);

template <typename T>
inline constexpr bool has_custom_from_variant_v = decltype(has_custom_from_variant<T>(0))::value;

template <typename T>
void custom_from_variant(const mc::dict& d, T& obj) {
    if constexpr (has_custom_from_variant_v<T>) {
        T::from_variant(d, obj);
    }
}

template <typename T>
void custom_to_variant(const T& obj, mc::mutable_dict& dict) {
    if constexpr (has_custom_to_variant_v<T>) {
        T::to_variant(obj, dict);
    }
}

} // namespace detail

/**
 * 将对象转换为变体
 * @tparam T 对象类型
 * @param obj 对象实例
 * @param var 转换后的变体
 */
template <typename T>
void to_variant(const T& obj, variant& var) {
    if constexpr (reflectable<T>::is_enum::value) {
        reflector<T>::to_variant(obj, var);
    } else {
        mutable_dict dict;
        reflector<T>::to_variant(obj, dict);
        var = dict;
    }
}

/**
 * 将对象直接转换为可变字典
 * @tparam T 对象类型
 * @param obj 对象实例
 * @param dict 转换后的可变字典
 */
template <typename T>
void to_variant(const T& obj, mutable_dict& dict) {
    if constexpr (reflectable<T>::is_enum::value) {
        variant var;
        reflector<T>::to_variant(obj, var);
        dict["value"] = var;
    } else {
        reflector<T>::to_variant(obj, dict);
    }
}

/**
 * 从变体转换为对象
 * @tparam T 对象类型
 * @param var 变体实例
 * @param obj 转换后的对象
 */
template <typename T>
void from_variant(const variant& var, T& obj) {
    if constexpr (reflectable<T>::is_enum::value) {
        reflector<T>::from_variant(var, obj);
    } else if (var.is_dict()) {
        // 支持从{key: value}字典转换为对象
        reflector<T>::from_variant(var.as_dict(), obj);
    } else if (var.is_array()) {
        // 支持从[value1, value2, ...]数组转换为对象,每个成员的顺序必须和反射时一致
        std::size_t index = 0;
        reflector<T>::visit([&](std::string_view, auto, auto setter) {
            if (index < var.size()) {
                setter(obj, var[index]);
            }
            ++index;
        });
    } else {
        throw_variant_cast(get_type_name<T>().data(), var.get_type_name());
    }
}

/**
 * 遍历类型的所有成员
 * @tparam T 要遍历的类型
 * @param visitor 访问者函数
 */
template <typename T, typename Visitor>
void visit_properties(const Visitor& visitor) {
    if constexpr (is_reflectable<T>()) {
        reflector<T>::visit(visitor);
    }
}

} // namespace reflect

template <typename T, std::enable_if_t<mc::reflect::is_reflectable<T>(), int> = 0>
void to_variant(const T& o, variant& v) {
    mc::reflect::to_variant(o, v);
}

template <typename T, std::enable_if_t<mc::reflect::is_reflectable<T>(), int> = 0>
void from_variant(const variant& v, T& o) {
    mc::reflect::from_variant(v, o);
}

template <typename T, std::enable_if_t<mc::reflect::is_normal_enum<T>(), int> = 0>
void to_variant(const T& o, variant& v) {
    v = mc::variant(static_cast<int32_t>(o));
}

template <typename T, std::enable_if_t<mc::reflect::is_normal_enum<T>(), int> = 0>
void from_variant(const variant& v, T& o) {
    o = static_cast<T>(v.as_int32());
}

} // namespace mc

namespace mc::reflect::detail {

// 对反射类型的特化
template <typename T>
struct signature_helper<T, std::enable_if_t<mc::reflect::is_reflectable<T>()>> {
    static void apply(std::string& sig) {
        if constexpr (mc::reflect::is_enum<T>()) {
            // 可反射的枚举类型用的是 string
            sig += type_to_char(type_code::string_type);
        } else {
            sig += type_to_char(type_code::struct_start);
            mc::reflect::reflector<T>::get_signature(sig);
            sig += type_to_char(type_code::struct_end);
        }
    }
};

// 普通的枚举类型用 int32_t 表示，可反射的枚举类型用的是 string
template <typename T>
struct signature_helper<T, std::enable_if_t<mc::reflect::is_normal_enum<T>()>> {
    static void apply(std::string& sig) {
        sig += type_to_char(type_code::int32_type);
    }
};

} // namespace mc::reflect::detail

namespace mc::reflect {

template <typename T>
struct MC_API reflector<
    T, std::enable_if_t<mc::reflect::is_reflectable<T>() && !mc::reflect::is_enum<T>(), void>> {
    using getter_t  = std::function<mc::variant(const T&)>;
    using setter_t  = std::function<void(T&, const mc::variant&)>;
    using visitor_t = std::function<void(std::string_view, getter_t, setter_t)>;
    using member_t  = std::pair<const void*, size_t>;

    static std::string_view get_name();
    static type_id_type     get_type_id();

    static type_id_type register_type();
    static void         unregister_type();

    static void visit(const visitor_t& visitor);

    // 辅助版本，包装其他 visitor 类型到 visitor_t，防止隐式构造 visitor_t 时拷贝 visitor 参数
    template <typename Visitor,
              std::enable_if_t<
                  std::is_invocable_v<Visitor, std::string_view, getter_t, setter_t> &&
                      !std::is_same_v<std::decay_t<Visitor>, visitor_t>,
                  int> = 0>
    static void visit(Visitor&& visitor) {
        visit(visitor_t([&](std::string_view name, getter_t getter, setter_t setter) {
            visitor(name, getter, setter);
        }));
    }

    static void to_variant(const T& obj, mc::mutable_dict& dict);
    static void from_variant(const mc::dict& d, T& obj);

    static const struct_metadata& get_metadata();
    static reflection<T>&         get_reflection();
    static void                   get_signature(std::string& sig);
};

template <typename T>
struct MC_API reflector<
    T, std::enable_if_t<mc::reflect::is_reflectable<T>() && mc::reflect::is_enum<T>(), void>> {
    static std::string_view get_name();
    static type_id_type     get_type_id();

    static type_id_type register_type();
    static void         unregister_type();

    static void to_variant(const T& obj, mc::variant& var);
    static void from_variant(const mc::variant& var, T& obj);

    static mc::variant get_value(const T& obj);

    static const enum_metadata& get_metadata();
    static reflection<T>&       get_reflection();
};

template <typename T, typename Members>
struct reflector_data {
};

} // namespace mc::reflect

#define MC_REFLECT_STATIC_METADATA(TYPE, BASES, MEMBERS)                                           \
    template <>                                                                                    \
    struct static_metadata<TYPE> {                                                                 \
        static type_id_type   type_id;                                                             \
        static constexpr auto members = mc::reflect::detail::initial_members<TYPE>(std::tuple_cat( \
            BOOST_PP_SEQ_FOR_EACH(MC_REFLECT_BASE_ELEMENT, TYPE, BASES)                            \
                BOOST_PP_SEQ_FOR_EACH(MC_REFLECT_ELEMENT, TYPE, MEMBERS) std::tuple<>()));         \
    };

#define MC_REFLECT_STATIC_MEMBERS(TYPE) \
    mc::reflect::static_metadata<TYPE>::members

/**
 * @brief 定义类的反射信息
 */
#define MC_REFLECT_IMPL(TYPE, BASES, MEMBERS)                                                          \
    namespace mc::reflect {                                                                            \
    template struct MC_API mc::reflect::reflector<TYPE>;                                               \
    static_assert(mc::reflect::is_reflectable<TYPE>(), BOOST_PP_STRINGIZE(TYPE)" is not reflectable"); \
    MC_REFLECT_STATIC_METADATA(TYPE, BASES, MEMBERS)                                                   \
    static_assert(mc::reflect::detail::validate_members<TYPE>(MC_REFLECT_STATIC_MEMBERS(TYPE)),        \
                  "members validate failed, please check members type");                               \
    type_id_type mc::reflect::static_metadata<TYPE>::type_id =                                         \
        get_reflect_factory<TYPE>().template register_type<TYPE>();                                    \
    template <>                                                                                        \
    std::string_view reflector<TYPE>::get_name() {                                                     \
        return get_type_name<TYPE>();                                                                  \
    }                                                                                                  \
    template <>                                                                                        \
    type_id_type reflector<TYPE>::get_type_id() {                                                      \
        return mc::reflect::static_metadata<TYPE>::type_id;                                            \
    }                                                                                                  \
    template <>                                                                                        \
    type_id_type reflector<TYPE>::register_type() {                                                    \
        return get_reflect_factory<TYPE>().template register_type<TYPE>(                               \
            mc::reflect::static_metadata<TYPE>::type_id);                                              \
    }                                                                                                  \
    template <>                                                                                        \
    void reflector<TYPE>::unregister_type() {                                                          \
        auto factory = try_get_reflect_factory<TYPE>();                                                \
        if (factory) {                                                                                 \
            factory->unregister_type_impl(get_name());                                                 \
        }                                                                                              \
        using reflection_ptr = reflection<TYPE>::reflection_ptr;                                       \
        if (mc::singleton<reflection_ptr>::try_get()) {                                                \
            mc::singleton<reflection_ptr>::reset_for_test();                                           \
        }                                                                                              \
    }                                                                                                  \
    template <>                                                                                        \
    const mc::reflect::struct_metadata& reflector<TYPE>::get_metadata() {                              \
        static mc::reflect::struct_metadata metadata(                                                  \
            get_name(), MC_REFLECT_STATIC_MEMBERS(TYPE), static_cast<TYPE*>(nullptr));                 \
        return metadata;                                                                               \
    }                                                                                                  \
    template <>                                                                                        \
    void reflector<TYPE>::visit(const visitor_t& visitor) {                                            \
        get_metadata().visit_property([&](const property_type_info* property) {                        \
            const auto* p = static_cast<const property_info_base<TYPE>*>(property);                    \
            visitor(property->name, p->getter(), p->setter());                                         \
            return visit_status::VS_CONTINUE;                                                          \
        });                                                                                            \
    }                                                                                                  \
    template <>                                                                                        \
    void reflector<TYPE>::to_variant(const TYPE& obj, mc::mutable_dict& dict) {                        \
        if constexpr (mc::reflect::detail::has_custom_to_variant_v<TYPE>) {                            \
            mc::reflect::detail::custom_to_variant(obj, dict);                                         \
        } else {                                                                                       \
            visit([&](std::string_view name, auto getter, auto) {                                      \
                if (!dict.contains(name)) {                                                            \
                    dict[name] = getter(obj);                                                          \
                }                                                                                      \
            });                                                                                        \
        }                                                                                              \
    }                                                                                                  \
    template <>                                                                                        \
    void reflector<TYPE>::from_variant(const mc::dict& d, TYPE& obj) {                                 \
        if constexpr (mc::reflect::detail::has_custom_from_variant_v<TYPE>) {                          \
            mc::reflect::detail::custom_from_variant(d, obj);                                          \
        } else {                                                                                       \
            visit([&](std::string_view name, auto, auto setter) {                                      \
                if (d.contains(name)) {                                                                \
                    setter(obj, d[name]);                                                              \
                }                                                                                      \
            });                                                                                        \
        }                                                                                              \
    }                                                                                                  \
    template <>                                                                                        \
    mc::reflect::reflection<TYPE>& reflector<TYPE>::get_reflection() {                                 \
        return mc::reflect::reflection<TYPE>::instance();                                              \
    }                                                                                                  \
    template <>                                                                                        \
    void reflector<TYPE>::get_signature(std::string& sig) {                                            \
        get_metadata().visit_property([&](const property_type_info* property) {                        \
            sig += property->get_signature();                                                          \
            return visit_status::VS_CONTINUE;                                                          \
        });                                                                                            \
    }                                                                                                  \
    }

#define MC_REFLECT_IMPL_WITH_BASES(TYPE, BASES, MEMBERS) \
    MC_REFLECT_IMPL(TYPE, BASES, MEMBERS)

// 一个参数版本 - 只有类成员
#define MC_REFLECT_1(TYPE, MEMBERS) MC_REFLECT_IMPL_WITH_BASES(TYPE, BOOST_PP_EMPTY(), MEMBERS)

// 二个参数版本 - 有基类和类成员
#define MC_REFLECT_2(TYPE, BASES, MEMBERS) MC_REFLECT_IMPL_WITH_BASES(TYPE, BASES, MEMBERS)

#define MC_REFLECT_DISPATCH(TYPE, ...) \
    BOOST_PP_CAT(MC_REFLECT_, MC_REFLECT_ARG_COUNT(__VA_ARGS__))(TYPE, __VA_ARGS__)

#define MC_REFLECT(...) MC_REFLECT_DISPATCH(__VA_ARGS__)

/**
 * @brief 定义枚举成员（不带别名）
 */
#define MC_REFLECT_ENUM_MEMBER_WITHOUT_NAME(r, ENUM_TYPE, VALUE)  \
    std::make_tuple(mc::reflect::detail::create_enum_member_info( \
                        ENUM_TYPE::VALUE, BOOST_PP_STRINGIZE(VALUE))),

/**
 * @brief 定义枚举成员（带别名）
 */
#define MC_REFLECT_ENUM_MEMBER_WITH_NAME(r, ENUM_TYPE, VALUE)     \
    std::make_tuple(mc::reflect::detail::create_enum_member_info( \
        ENUM_TYPE::BOOST_PP_TUPLE_ELEM(0, VALUE),                 \
        BOOST_PP_TUPLE_ELEM(1, VALUE))),

/**
 * @brief 处理枚举成员
 */
#define MC_REFLECT_ENUM_ELEMENT(r, ENUM_TYPE, VALUE)                           \
    BOOST_PP_IIF(MC_REFLECT_IS_TUPLE(VALUE), MC_REFLECT_ENUM_MEMBER_WITH_NAME, \
                 MC_REFLECT_ENUM_MEMBER_WITHOUT_NAME)(r, ENUM_TYPE, VALUE)

#define MC_REFLECT_ENUM_STATIC_METADATA(TYPE, VALUES)                                 \
    template <>                                                                       \
    struct static_metadata<TYPE> {                                                    \
        static type_id_type     type_id;                                              \
        static std::string_view name;                                                 \
        static constexpr auto   members = detail::enum_tuple_to_array(std::tuple_cat( \
            BOOST_PP_SEQ_FOR_EACH(MC_REFLECT_ENUM_ELEMENT, TYPE, VALUES)            \
                std::tuple<>{}));                                                   \
    }

/**
 * @brief 实现枚举的反射信息
 */
#define MC_REFLECT_ENUM_IMPL(TYPE, VALUES)                                                             \
    namespace mc::reflect {                                                                            \
    template struct MC_API mc::reflect::reflector<TYPE>;                                               \
    static_assert(mc::reflect::is_reflectable<TYPE>(), BOOST_PP_STRINGIZE(TYPE)" is not reflectable"); \
    MC_REFLECT_ENUM_STATIC_METADATA(TYPE, VALUES);                                                     \
    type_id_type mc::reflect::static_metadata<TYPE>::type_id =                                         \
        get_reflect_factory<TYPE>().template register_type<TYPE>();                                    \
    template <>                                                                                        \
    std::string_view reflector<TYPE>::get_name() {                                                     \
        return get_type_name<TYPE>();                                                                  \
    }                                                                                                  \
    template <>                                                                                        \
    type_id_type reflector<TYPE>::get_type_id() {                                                      \
        return mc::reflect::static_metadata<TYPE>::type_id;                                            \
    }                                                                                                  \
    template <>                                                                                        \
    type_id_type reflector<TYPE>::register_type() {                                                    \
        return get_reflect_factory<TYPE>().template register_type<TYPE>(                               \
            mc::reflect::static_metadata<TYPE>::type_id);                                              \
    }                                                                                                  \
    template <>                                                                                        \
    void reflector<TYPE>::unregister_type() {                                                          \
        auto factory = try_get_reflect_factory<TYPE>();                                                \
        if (factory) {                                                                                 \
            factory->unregister_type_impl(get_name());                                                 \
        }                                                                                              \
        using reflection_ptr = reflection<TYPE>::reflection_ptr;                                       \
        if (mc::singleton<reflection_ptr>::try_get()) {                                                \
            mc::singleton<reflection_ptr>::reset_for_test();                                           \
        }                                                                                              \
    }                                                                                                  \
    template <>                                                                                        \
    const mc::reflect::enum_metadata& reflector<TYPE>::get_metadata() {                                \
        static mc::reflect::enum_metadata metadata(                                                    \
            get_name(), mc::reflect::enum_values{MC_REFLECT_STATIC_MEMBERS(TYPE)});                    \
        return metadata;                                                                               \
    }                                                                                                  \
    template <>                                                                                        \
    void reflector<TYPE>::to_variant(const TYPE& e, mc::variant& var) {                                \
        auto value = get_metadata().try_get_name(static_cast<enum_value_type>(e));                     \
        if (!value) {                                                                                  \
            mc::reflect::throw_bad_enum_cast(static_cast<int64_t>(e), #TYPE);                          \
        }                                                                                              \
        var = *value;                                                                                  \
    }                                                                                                  \
    template <>                                                                                        \
    void reflector<TYPE>::from_variant(const mc::variant& var, TYPE& e) {                              \
        auto value = get_metadata().try_get_value_from_variant(var);                                   \
        if (!value) {                                                                                  \
            auto va = var.as_string();                                                                 \
            mc::reflect::throw_bad_enum_cast(va.c_str(), #TYPE);                                       \
        }                                                                                              \
        e = static_cast<TYPE>(*value);                                                                 \
    }                                                                                                  \
    template <>                                                                                        \
    mc::reflect::reflection<TYPE>& reflector<TYPE>::get_reflection() {                                 \
        return mc::reflect::reflection<TYPE>::instance();                                              \
    }                                                                                                  \
    }

#define MC_REFLECT_ENUM(TYPE, VALUES) \
    MC_REFLECT_ENUM_IMPL(TYPE, VALUES)

#define MC_DEFINE_NAMESPACE_1(TYPE, NAMESPACE) \
    namespace mc::reflect::detail {            \
    template <>                                \
    struct reflect_namespace<TYPE> {           \
        using type = NAMESPACE;                \
    };                                         \
    }

#define MC_DEFINE_NAMESPACE_2(TYPE, NAMESPACE) \
    MC_DEFINE_NAMESPACE_1(BOOST_PP_TUPLE_ELEM(0, TYPE), NAMESPACE)

#define MC_DEFINE_NAMESPACE(TYPE, NAMESPACE)                       \
    BOOST_PP_IIF(MC_REFLECT_IS_TUPLE(TYPE), MC_DEFINE_NAMESPACE_2, \
                 MC_DEFINE_NAMESPACE_1)(TYPE, NAMESPACE)

/**
 * @brief 定义带命名空间的类反射信息
 * @param TYPE 类型
 * @param NAMESPACE 命名空间类型
 * @param ... 其他参数（基类和成员）
 *
 * 用法示例：
 * MC_REFLECT_WITH_NAMESPACE(my_class, my_namespace_type, ((m_value, "value")))
 * MC_REFLECT_WITH_NAMESPACE(my_class, my_namespace_type, (base_class), ((m_value, "value")))
 */
#define MC_REFLECT_WITH_NAMESPACE(NAMESPACE, TYPE, ...) \
    MC_DEFINE_NAMESPACE(TYPE, NAMESPACE)                \
    MC_REFLECT(TYPE, __VA_ARGS__)

/**
 * @brief 定义带命名空间的枚举反射信息
 * @param TYPE 枚举类型（可以是元组形式 (Type, "Name")）
 * @param NAMESPACE 命名空间类型
 * @param VALUES 枚举值
 *
 * 用法示例：
 * MC_REFLECT_ENUM_WITH_NAMESPACE(my_enum, my_namespace_type, (VALUE1)(VALUE2))
 * MC_REFLECT_ENUM_WITH_NAMESPACE((my_enum, "MyEnum"), my_namespace_type, (VALUE1)(VALUE2))
 */
#define MC_REFLECT_ENUM_WITH_NAMESPACE(NAMESPACE, TYPE, VALUES) \
    MC_DEFINE_NAMESPACE(TYPE, NAMESPACE)                        \
    MC_REFLECT_ENUM(TYPE, VALUES)

#endif // MC_REFLECT_REFLECT_H