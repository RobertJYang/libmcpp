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

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/comparison/equal.hpp>
#include <boost/preprocessor/comparison/greater.hpp>
#include <boost/preprocessor/control/if.hpp>
#include <boost/preprocessor/control/iif.hpp>
#include <boost/preprocessor/facilities/is_empty.hpp>
#include <boost/preprocessor/punctuation/is_begin_parens.hpp>
#include <boost/preprocessor/punctuation/remove_parens.hpp>
#include <boost/preprocessor/seq/enum.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/pop_front.hpp>
#include <boost/preprocessor/seq/transform.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/variadic/size.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>

#include <mc/common.h>
#include <mc/dict.h>
#include <mc/reflect/metadata.h>
#include <mc/reflect/metadata_info.h>
#include <mc/reflect/reflectable_macro.h>
#include <mc/reflect/reflection.h>
#include <mc/reflect/reflection_enum.h>
#include <mc/reflect/reflection_factory.h>
#include <mc/reflect/signature.h>
#include <mc/traits.h>
#include <mc/variant.h>

// ------------------------ 一些辅助宏用于解决宏参数中存在逗号被当作参数分隔符的问题 ------------------------

#define MC_REFLECT_AUXILIARY_0(...) ((__VA_ARGS__)) MC_REFLECT_AUXILIARY_1
#define MC_REFLECT_AUXILIARY_1(...) ((__VA_ARGS__)) MC_REFLECT_AUXILIARY_0
#define MC_REFLECT_AUXILIARY_0_END
#define MC_REFLECT_AUXILIARY_1_END

// 检测是否为元组形式的参数，比如 (a)(b)("name", c) 这种形式
#define MC_REFLECT_IS_TUPLE(x) BOOST_PP_IS_BEGIN_PARENS(x)

// 将 (a)(b)("name", c) 形式转换为 ((a))((b))(("name", c)) 形式，确保 BOOST_PP_* 宏能正确处理中间带逗号的参数
#define MC_REFLECT_CONVERT_TO_SEQ(seq) BOOST_PP_SEQ_POP_FRONT(BOOST_PP_CAT(MC_REFLECT_AUXILIARY_0(0) seq, _END))

// 单括号参数，比如 (a)(b)("name", c) 这种形式，遍历每一个参数加上一对括号
#define MC_REFLECT_SINGLE_PARENS_PARAM(param) MC_REFLECT_CONVERT_TO_SEQ(param)

// 双括号参数，什么都不用做
#define MC_REFLECT_DOUBLE_PARENS_PARAM(param) param

// 普通参数，比如 a, b, "name", c 这种形式，直接加上双括号
#define MC_REFLECT_NO_PARENS_PARAM(param) ((param))

#define MC_REMOVE_DOUBLE_PARENS(MEMBER) BOOST_PP_REMOVE_PARENS(BOOST_PP_REMOVE_PARENS(MEMBER))

#define MC_REMOVE_PARENS(MEMBER)                                                                                       \
    BOOST_PP_IIF(MC_REFLECT_IS_DOUBLE_PARENS(MEMBER), MC_REMOVE_DOUBLE_PARENS, BOOST_PP_REMOVE_PARENS)                 \
    (MEMBER)

#define MC_REFLECT_IS_DOUBLE_PARENS(param) MC_REFLECT_IS_TUPLE(BOOST_PP_REMOVE_PARENS(param))

// 将参数打包成 ((a))((b))((c)) 形式，确保 BOOST_PP_* 宏能正确处理中间带逗号的参数
#define MC_REFLECT_PARAM_TO_SEQ(param)                                                                                 \
    BOOST_PP_IF(MC_REFLECT_IS_TUPLE(param),                                                                            \
                BOOST_PP_IIF(MC_REFLECT_IS_DOUBLE_PARENS(param), MC_REFLECT_DOUBLE_PARENS_PARAM,                       \
                             MC_REFLECT_SINGLE_PARENS_PARAM),                                                          \
                MC_REFLECT_NO_PARENS_PARAM)                                                                            \
    (param)

// ------------------------------------------------------------------------------------------------

namespace mc::reflect::detail {
MC_API void struct_to_variant_default(const struct_metadata& metadata, const void* obj, mc::dict& dict);
MC_API void struct_from_variant_default(const struct_metadata& metadata, const mc::dict& dict, void* obj);

template <typename T, auto MemberPtr>
constexpr auto create_member_info(mc::string_view name = {})
{
    using member_ptr_type = decltype(MemberPtr);
    if constexpr (std::is_member_function_pointer_v<member_ptr_type> ||
                  mc::detail::is_function_pointer<member_ptr_type>::value) {
        MC_UNUSED(sizeof(T));
        return mc::reflect::make_erased_static_method_info<MemberPtr>(name);
    } else {
        using member_traits = property_member_pointer_traits<member_ptr_type>;
        return member_info_creator<T, typename member_traits::member_type, typename member_traits::base_type>::create(
            MemberPtr, name);
    }
}

template <typename T, auto MemberPtr, std::size_t N>
constexpr auto create_member_info(const char (&name)[N])
{
    return create_member_info<T, MemberPtr>(reflect_name_from_literal(name));
}

// 创建成员元数据（根据成员类型分发到属性、方法或用户自定义的成员信息）
template <typename T, typename M, typename BaseT>
constexpr auto create_member_info(M BaseT::* member_ptr, mc::string_view name = {})
{
    return member_info_creator<T, M, BaseT>::create(member_ptr, name);
}

template <typename T, typename M, typename BaseT, std::size_t N>
constexpr auto create_member_info(M BaseT::* member_ptr, const char (&name)[N])
{
    return create_member_info<T, M, BaseT>(member_ptr, reflect_name_from_literal(name));
}

template <typename T, typename Getter, typename Setter>
constexpr auto create_member_info(Getter getter, Setter setter, mc::string_view name)
{
    return computed_property_info_creator::create<T>(getter, setter, name);
}

template <typename T, typename Getter, typename Setter, std::size_t N>
constexpr auto create_member_info(Getter getter, Setter setter, const char (&name)[N])
{
    return create_member_info<T>(getter, setter, reflect_name_from_literal(name));
}

template <typename T, typename R, typename... Args>
constexpr auto create_member_info(R (*static_func)(Args...), mc::string_view name = {})
{
    return member_info_creator<T, R (*)(Args...), void>::create(static_func, name);
}

template <typename T, typename R, typename... Args, std::size_t N>
constexpr auto create_member_info(R (*static_func)(Args...), const char (&name)[N])
{
    return create_member_info<T>(static_func, reflect_name_from_literal(name));
}

template <typename T, typename BaseT>
constexpr auto create_base_class_info(mc::string_view base_class_name = {})
{
    return base_class_info_creator<T, BaseT>::create(base_class_name);
}

template <typename T, typename BaseT, std::size_t N>
constexpr auto create_base_class_info(const char (&base_class_name)[N])
{
    return create_base_class_info<T, BaseT>(reflect_name_from_literal(base_class_name));
}

template <typename T, typename... Args>
constexpr auto create_type_info(Args...)
{
    return 1;
}

template <typename EnumType>
constexpr auto create_enum_member_info(EnumType value, mc::string_view name)
{
    return enum_member_info(name, value);
}

template <typename EnumType, std::size_t N>
constexpr auto create_enum_member_info(EnumType value, const char (&name)[N])
{
    return create_enum_member_info(value, reflect_name_from_literal(name));
}

template <size_t Index, typename... Tuples>
bool is_member_override(const member_info_base& base_member, const std::tuple<Tuples...>& all_members)
{
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
    static constexpr bool check(const Member&)
    {
        return true;
    }
};

// 检测类型是否提供了check_member模板函数
template <typename T, typename Members, typename = void>
struct has_check_members : std::false_type {};

template <typename T, typename Members>
struct has_check_members<T, Members, std::void_t<decltype(T::template check_members<Members>(std::declval<Members>()))>>
    : std::true_type {};

// 辅助函数：验证某个成员
template <typename T, typename Members>
static constexpr bool validate_members(const Members& members)
{
    if constexpr (has_check_members<T, Members>::value) {
        return T::check_members(members);
    }
    return true;
}

// 检测类型是否提供了initial_member模板函数
template <typename T, typename Members, typename = void>
struct has_initial_members : std::false_type {};

template <typename T, typename Members>
struct has_initial_members<T, Members,
                           std::void_t<decltype(T::template initial_members<Members>(std::declval<Members&>()))>>
    : std::true_type {};

// 辅助函数：初始化某个成员
template <typename T, typename Members>
static constexpr auto initial_members(Members members)
{
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
struct has_custom_to_variant<T,
                             std::void_t<decltype(T::to_variant(std::declval<const T&>(), std::declval<mc::dict&>()))>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_custom_to_variant_v = has_custom_to_variant<T>::value;

template <typename T>
auto has_custom_from_variant(int)
    -> decltype(T::from_variant(std::declval<const mc::dict&>(), std::declval<T&>()), std::true_type{});

template <typename T>
std::false_type has_custom_from_variant(...);

template <typename T>
inline constexpr bool has_custom_from_variant_v = decltype(has_custom_from_variant<T>(0))::value;

template <typename T>
void custom_from_variant(const mc::dict& d, T& obj)
{
    if constexpr (has_custom_from_variant_v<T>) {
        T::from_variant(d, obj);
    }
}

template <typename T>
void custom_to_variant(const T& obj, mc::dict& dict)
{
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
void to_variant(const T& obj, variant& var)
{
    if constexpr (reflectable<T>::is_enum::value) {
        reflector<T>::to_variant(obj, var);
    } else {
        mc::dict dict;
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
void to_variant(const T& obj, mc::dict& dict)
{
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
void from_variant(const variant& var, T& obj)
{
    if constexpr (reflectable<T>::is_enum::value) {
        reflector<T>::from_variant(var, obj);
    } else if (var.is_object()) {
        // 支持从{key: value}字典转换为对象
        reflector<T>::from_variant(var.get_object(), obj);
    } else if (var.is_array()) {
        // 支持从[value1, value2, ...]数组转换为对象,每个成员的顺序必须和反射时一致
        std::size_t index = 0;
        reflector<T>::visit_properties([&](const property_type_info* property) {
            if (index < var.size()) {
                property->set_value(obj, var[index]);
            }
            ++index;
            return visit_status::VS_CONTINUE;
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
void visit_properties(const Visitor& visitor)
{
    if constexpr (is_reflectable<T>()) {
        reflector<T>::visit(visitor);
    }
}

} // namespace reflect

template <typename T, std::enable_if_t<mc::reflect::is_reflectable<T>(), int> = 0>
void to_variant(const T& o, variant& v)
{
    mc::reflect::to_variant(o, v);
}

template <typename T, std::enable_if_t<mc::reflect::is_reflectable<T>(), int> = 0>
void from_variant(const variant& v, T& o)
{
    mc::reflect::from_variant(v, o);
}

template <typename T, std::enable_if_t<mc::reflect::is_normal_enum<T>(), int> = 0>
void to_variant(const T& o, variant& v)
{
    v = mc::variant(static_cast<int32_t>(o));
}

template <typename T, std::enable_if_t<mc::reflect::is_normal_enum<T>(), int> = 0>
void from_variant(const variant& v, T& o)
{
    o = static_cast<T>(v.as_int32());
}

} // namespace mc

namespace mc::reflect::detail {

// 对反射类型的特化
template <typename T>
struct signature_helper<T, std::enable_if_t<mc::reflect::is_reflectable<T>()>> {
    static void apply(mc::string& sig)
    {
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
    static void apply(mc::string& sig)
    {
        sig += type_to_char(type_code::int32_type);
    }
};

} // namespace mc::reflect::detail

namespace mc::reflect {

struct metadata_extension {
    void* data{nullptr};
    void (*destroy)(void*){nullptr};
};

template <typename T>
struct MC_API reflector<T, std::enable_if_t<mc::reflect::is_reflectable<T>() && !mc::reflect::is_enum<T>(), void>> {
    using getter_t           = std::function<mc::variant(const T&)>;
    using setter_t           = std::function<void(T&, const mc::variant&)>;
    using visitor_t          = std::function<void(mc::string_view, getter_t, setter_t)>;
    using property_visitor_t = std::function<visit_status(const property_type_info*)>;
    using member_t           = std::pair<const void*, size_t>;

    static mc::string_view     get_name();
    static type_id_type        get_type_id();
    static metadata_extension& get_extension();

    static type_id_type register_type();
    static void         unregister_type();

    static void visit_properties(const property_visitor_t& visitor);

    template <typename Visitor,
              std::enable_if_t<std::is_invocable_r_v<visit_status, Visitor, const property_type_info*> &&
                                   !std::is_same_v<std::decay_t<Visitor>, property_visitor_t>,
                               int> = 0>
    static void visit_properties(Visitor&& visitor)
    {
        visit_properties(property_visitor_t([&](const property_type_info* property) {
            return visitor(property);
        }));
    }

    // 兼容旧版 visit(name, getter, setter) 接口，但改成按需实例化，
    // 避免每个 MC_REFLECT 类型都无条件生成一套 std::function 桥接层。
    template <typename Visitor, std::enable_if_t<std::is_invocable_v<Visitor, mc::string_view, getter_t, setter_t> &&
                                                     !std::is_same_v<std::decay_t<Visitor>, property_visitor_t>,
                                                 int> = 0>
    static void visit(Visitor&& visitor)
    {
        visit_properties([&](const property_type_info* property) {
            const auto* p = static_cast<const property_info_base<T>*>(property);
            visitor(property->name, p->template getter<T>(), p->template setter<T>());
            return visit_status::VS_CONTINUE;
        });
    }

    static void to_variant(const T& obj, mc::dict& dict);
    static void from_variant(const mc::dict& d, T& obj);

    static const struct_metadata& get_metadata();
    static reflection<T>&         get_reflection();
    static void                   get_signature(mc::string& sig);
};

template <typename T>
struct MC_API reflector<T, std::enable_if_t<mc::reflect::is_reflectable<T>() && mc::reflect::is_enum<T>(), void>> {
    static mc::string_view get_name();
    static type_id_type    get_type_id();

    static type_id_type register_type();
    static void         unregister_type();

    static void to_variant(const T& obj, mc::variant& var);
    static void from_variant(const mc::variant& var, T& obj);

    static mc::variant get_value(const T& obj);

    static const enum_metadata& get_metadata();
    static reflection<T>&       get_reflection();
};

} // namespace mc::reflect

// ------------------------------------------ 定义类型反射宏 MC_REFLECT ----------------------------------------

// MC_REFLECT_EXPAND_PARAM_II：将反射参数最终展开成需要的代码
// 对于类成员我们默认用 mc::reflect::detail::create_member_info 创建成员反射信息，
// 对于扩展参数调用用户提供的扩展宏，比如 MC_BASE_CLASS, MC_COMPUTED_PROPERTY 等。
//
// TODO:: 目前类成员的展开是默认实现，后续如果想在反射时填充一些元信息，比如 member_info_base 基类的 flags 或者
// data 字段，可以修改这里的默认实现或者自定义扩展宏，可以参考 MC_BASE_CLASS 和 MC_COMPUTED_PROPERTY 的实现
#define MC_REFLECT_CREATE_MEMBER_INFO(TYPE, MEMBER, ...)                                                               \
    (mc::reflect::detail::create_member_info<TYPE, &TYPE::MEMBER>(__VA_ARGS__))

#define MC_REFLECT_EXPAND_PARAM_II(TYPE, MEMBER, ...)                                                                  \
    BOOST_PP_IF(BOOST_PP_GREATER(BOOST_PP_VARIADIC_SIZE(dummy, ##__VA_ARGS__), 2), MEMBER(TYPE, __VA_ARGS__),          \
                BOOST_PP_IF(BOOST_PP_GREATER(BOOST_PP_VARIADIC_SIZE(dummy, ##__VA_ARGS__), 1),                         \
                            MC_REFLECT_CREATE_MEMBER_INFO(TYPE, MEMBER, __VA_ARGS__),                                  \
                            MC_REFLECT_CREATE_MEMBER_INFO(TYPE, MEMBER,                                                \
                                                          mc::reflect::detail::reflect_name_from_literal(#MEMBER))))

// MC_REFLECT_EXPAND_PARAM_I：仅为了接收 MC_REFLECT_EXPAND_PARAM 构造的 (TYPE, 展开的 MEMBER 参数包) 再调用下一个宏
// 我们中转一次而不是直接在 MC_REFLECT_EXPAND_PARAM 调用下一个宏的目的是避免编译器将 (TYPE, 展开的 MEMBER 参数包)
// 认为只有 两个参数而不是完全展开
#define MC_REFLECT_EXPAND_PARAM_I(TYPE, ...)                                                                           \
    BOOST_PP_IIF(BOOST_PP_IS_EMPTY(__VA_ARGS__), std::tuple<>{}, MC_REFLECT_EXPAND_PARAM_II(TYPE, __VA_ARGS__))

// 展开反射参数
// @param r: BOOST_PP_SEQ_TRANSFORM() 调用传递的占位符，忽略
// @param TYPE: 从 MC_REFLECT 传递下来的反射类型
// @param MEMBER: 单括号打包的 (params...) 反射参数
// 这里我们将 MEMBER 展开，补充 TYPE 作为第一个参数调用 MC_REFLECT_EXPAND_PARAM_I(TYPE, param1, param2 ...)
#define MC_REFLECT_EXPAND_PARAM(r, TYPE, MEMBER) MC_REFLECT_EXPAND_PARAM_I(TYPE, MC_REMOVE_PARENS(MEMBER))

// 循环展开所有参数
#define MC_REFLECT_EXPAND_PARAMS(r, TYPE, param)                                                                       \
    BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_TRANSFORM(MC_REFLECT_EXPAND_PARAM, TYPE, MC_REFLECT_PARAM_TO_SEQ(param))),

#define MC_REFLECT_METADATA_MEMBERS(TYPE, ...)                                                                         \
    BOOST_PP_IIF(BOOST_PP_IS_EMPTY(__VA_ARGS__), mc::reflect::detail::initial_members<TYPE>(std::tuple<>{}),           \
                 mc::reflect::detail::initial_members<TYPE>(std::tuple_cat(BOOST_PP_SEQ_FOR_EACH(                      \
                     MC_REFLECT_EXPAND_PARAMS, TYPE, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) std::tuple<>())))

#define MC_REFLECT_STATIC_METADATA(TYPE, ...)                                                                          \
    template <>                                                                                                        \
    struct static_metadata<TYPE> {                                                                                     \
        static type_id_type       type_id;                                                                             \
        static metadata_extension extension;                                                                           \
        static constexpr auto     members = MC_REFLECT_METADATA_MEMBERS(TYPE, __VA_ARGS__);                            \
    };

#define MC_REFLECT_STATIC_MEMBERS(TYPE) mc::reflect::static_metadata<TYPE>::members

/**
 * @brief 定义类的反射信息
 */
#define MC_REFLECT_IMPL(TYPE, ...)                                                                                     \
    namespace mc::reflect {                                                                                            \
    template struct MC_API mc::reflect::reflector<TYPE>;                                                               \
    static_assert(mc::reflect::is_reflectable<TYPE>(), BOOST_PP_STRINGIZE(TYPE)" is not reflectable");                 \
    MC_REFLECT_STATIC_METADATA(TYPE, __VA_ARGS__)                                                                      \
    static_assert(mc::reflect::detail::validate_members<TYPE>(MC_REFLECT_STATIC_MEMBERS(TYPE)),                        \
                  "members validate failed, please check members type");                                               \
    type_id_type mc::reflect::static_metadata<TYPE>::type_id =                                                         \
        get_reflect_factory<TYPE>().template register_type<TYPE>();                                                    \
    metadata_extension mc::reflect::static_metadata<TYPE>::extension = {nullptr, nullptr};                             \
    template <>                                                                                                        \
    [[maybe_unused]] mc::string_view reflector<TYPE>::get_name()                                                       \
    {                                                                                                                  \
        return get_type_name<TYPE>();                                                                                  \
    }                                                                                                                  \
    template <>                                                                                                        \
    [[maybe_unused]] type_id_type reflector<TYPE>::get_type_id()                                                       \
    {                                                                                                                  \
        return mc::reflect::static_metadata<TYPE>::type_id;                                                            \
    }                                                                                                                  \
    template <>                                                                                                        \
    [[maybe_unused]] metadata_extension& reflector<TYPE>::get_extension()                                              \
    {                                                                                                                  \
        return mc::reflect::static_metadata<TYPE>::extension;                                                          \
    }                                                                                                                  \
    template <>                                                                                                        \
    [[maybe_unused]] type_id_type reflector<TYPE>::register_type()                                                     \
    {                                                                                                                  \
        return get_reflect_factory<TYPE>().template register_type<TYPE>(mc::reflect::static_metadata<TYPE>::type_id);  \
    }                                                                                                                  \
    template <>                                                                                                        \
    [[maybe_unused]] void reflector<TYPE>::unregister_type()                                                           \
    {                                                                                                                  \
        mc::reflect::detail::unregister_reflection_type<TYPE>();                                                       \
        if (reflection<TYPE>::has_instance()) {                                                                        \
            reflection<TYPE>::reset_instance_for_test();                                                               \
        }                                                                                                              \
    }                                                                                                                  \
    template <>                                                                                                        \
    [[maybe_unused]] const mc::reflect::struct_metadata& reflector<TYPE>::get_metadata()                               \
    {                                                                                                                  \
        static const auto* metadata =                                                                                  \
            new mc::reflect::struct_metadata(get_name(), get_type_id(), MC_REFLECT_STATIC_MEMBERS(TYPE));              \
        return *metadata;                                                                                              \
    }                                                                                                                  \
    template <>                                                                                                        \
    [[maybe_unused]] void reflector<TYPE>::visit_properties(const property_visitor_t& visitor)                         \
    {                                                                                                                  \
        get_metadata().visit_properties(visitor);                                                                      \
    }                                                                                                                  \
    template <>                                                                                                        \
    [[maybe_unused]] void reflector<TYPE>::to_variant(const TYPE& obj, mc::dict& dict)                                 \
    {                                                                                                                  \
        if constexpr (mc::reflect::detail::has_custom_to_variant_v<TYPE>) {                                            \
            mc::reflect::detail::custom_to_variant(obj, dict);                                                         \
        } else {                                                                                                       \
            mc::reflect::detail::struct_to_variant_default(get_metadata(), &obj, dict);                                \
        }                                                                                                              \
    }                                                                                                                  \
    template <>                                                                                                        \
    [[maybe_unused]] void reflector<TYPE>::from_variant(const mc::dict& d, TYPE& obj)                                  \
    {                                                                                                                  \
        if constexpr (mc::reflect::detail::has_custom_from_variant_v<TYPE>) {                                          \
            mc::reflect::detail::custom_from_variant(d, obj);                                                          \
        } else {                                                                                                       \
            mc::reflect::detail::struct_from_variant_default(get_metadata(), d, &obj);                                 \
        }                                                                                                              \
    }                                                                                                                  \
    template <>                                                                                                        \
    [[maybe_unused]] mc::reflect::reflection<TYPE>& reflector<TYPE>::get_reflection()                                  \
    {                                                                                                                  \
        return mc::reflect::reflection<TYPE>::instance();                                                              \
    }                                                                                                                  \
    template <>                                                                                                        \
    [[maybe_unused]] void reflector<TYPE>::get_signature(mc::string& sig)                                              \
    {                                                                                                                  \
        get_metadata().append_signature(sig);                                                                          \
    }                                                                                                                  \
    }

#define MC_REFLECT(...) MC_REFLECT_IMPL(__VA_ARGS__)

// ------------------------------------------ 类型反射宏参数扩展机制 ----------------------------------------
// 类型反射宏参数扩展机制：
//
// 我们通过构造 (MACRO, param1, param2 ...) 形式的参数实现反射宏参数的扩展，MC_REFLECT
// 会自动识别扩展参数并调用给定的宏， 调用形式为 MACRO(TYPE, param1, param2 ...)，其中第一个参数 TYPE 是从 MC_REFLECT
// 的第一个参数传递过来的。
//
// 实现反射宏参数扩展一般需要定义两个配套的扩展宏：一个用于生成扩展参数，一个用于展开扩展参数。
// 例如：
// MC_BASE_CLASS(test_struct) 用于生成扩展参数 (MC_REFLECT_BASE_CLASS_1, test_struct, ~)
// MC_REFLECT_BASE_CLASS_1(TYPE, BASE) 用于展开扩展参数 mc::reflect::detail::create_base_class_info<TYPE, BASE>()
//
// 注意：为了与普通的 (member)(member, "name") 类成员参数做区分，扩展参数个数必须为 3
// 个以上，其中第一个是扩展参数展开宏， 后面是扩展参数，参数不足 3 个的用 ~ 占位符填充，展开的时候自行忽略。

// 1、定义基类扩展参数的生成宏 MC_BASE_CLASS
//
// 使用方式：
// MC_REFLECT(test_struct, MC_BASE_CLASS(base_struct))
// MC_REFLECT(test_struct, MC_BASE_CLASS(base_struct, "base_name"))
//
// 单参数的基类展开宏：其中 r 是占位符，展开时候忽略
#define MC_REFLECT_BASE_CLASS_1(TYPE, r, BASE) (mc::reflect::detail::create_base_class_info<TYPE, BASE>())

// 双参数的基类展开宏：参数为基类 + 基类名
#define MC_REFLECT_BASE_CLASS_2(TYPE, BASE, NAME) (mc::reflect::detail::create_base_class_info<TYPE, BASE>(NAME))

// 生成基类参数扩展宏：根据参数个数选择基类展开宏
#define MC_BASE_CLASS(...)                                                                                             \
    BOOST_PP_IIF(BOOST_PP_GREATER(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), 1), (MC_REFLECT_BASE_CLASS_2, __VA_ARGS__),     \
                 (MC_REFLECT_BASE_CLASS_1, ~, __VA_ARGS__))

// 2、定义计算属性扩展参数的生成宏 MC_COMPUTED_PROPERTY
//
// 使用方式：
// MC_REFLECT(test_struct, MC_COMPUTED_PROPERTY("id", get_id))
// MC_REFLECT(test_struct, MC_COMPUTED_PROPERTY("id", get_id, set_id))
//
// 单参数的计算属性展开宏：参数为计算属性名 + 计算属性 getter 函数
#define MC_REFLECT_COMPUTE_PROPERTY_1(TYPE, NAME, GETTER)                                                              \
    mc::reflect::detail::create_member_info<TYPE>(&TYPE::GETTER, static_cast<void*>(nullptr), NAME)

// 双参数的计算属性展开宏：参数为计算属性名 + 计算属性 getter 函数 + 计算属性 setter 函数
#define MC_REFLECT_COMPUTE_PROPERTY_2(TYPE, NAME, GETTER, SETTER)                                                      \
    mc::reflect::detail::create_member_info<TYPE>(&TYPE::GETTER, &TYPE::SETTER, NAME)

// 生成计算属性参数的扩展宏：根据参数个数选择计算属性展开宏
// 由于计算属性最少需要 名称 + getter 函数，再加上第一个参数是展开宏，整体参数为 3 个，不需要填充占位参数
#define MC_COMPUTED_PROPERTY(...)                                                                                      \
    BOOST_PP_IIF(BOOST_PP_GREATER(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), 2),                                             \
                 (MC_REFLECT_COMPUTE_PROPERTY_2, __VA_ARGS__), (MC_REFLECT_COMPUTE_PROPERTY_1, __VA_ARGS__))

// ------------------------------------------ 定义枚举反射宏 MC_REFLECT_ENUM ----------------------------------------

/**
 * @brief 定义枚举成员（不带别名）
 */
#define MC_REFLECT_ENUM_MEMBER_WITHOUT_NAME(r, ENUM_TYPE, VALUE)                                                       \
    std::make_tuple(mc::reflect::detail::create_enum_member_info(                                                      \
                        ENUM_TYPE::VALUE, mc::reflect::detail::reflect_name_from_literal(BOOST_PP_STRINGIZE(VALUE)))),

/**
 * @brief 定义枚举成员（带别名）
 */
#define MC_REFLECT_ENUM_MEMBER_WITH_NAME(r, ENUM_TYPE, VALUE)                                                          \
    std::make_tuple(mc::reflect::detail::create_enum_member_info(ENUM_TYPE::BOOST_PP_TUPLE_ELEM(0, VALUE),             \
                                                                 BOOST_PP_TUPLE_ELEM(1, VALUE))),

/**
 * @brief 处理枚举成员
 */
#define MC_REFLECT_ENUM_ELEMENT(r, ENUM_TYPE, VALUE)                                                                   \
    BOOST_PP_IIF(MC_REFLECT_IS_TUPLE(VALUE), MC_REFLECT_ENUM_MEMBER_WITH_NAME, MC_REFLECT_ENUM_MEMBER_WITHOUT_NAME)    \
    (r, ENUM_TYPE, VALUE)

#define MC_REFLECT_ENUM_STATIC_METADATA(TYPE, VALUES)                                                                  \
    template <>                                                                                                        \
    struct static_metadata<TYPE> {                                                                                     \
        static type_id_type    type_id;                                                                                \
        static mc::string_view name;                                                                                   \
        static constexpr auto  members = detail::enum_tuple_to_array(                                                  \
            std::tuple_cat(BOOST_PP_SEQ_FOR_EACH(MC_REFLECT_ENUM_ELEMENT, TYPE, VALUES) std::tuple<>{}));             \
    }

/**
 * @brief 实现枚举的反射信息
 */
#define MC_REFLECT_ENUM_IMPL(TYPE, VALUES)                                                                             \
    namespace mc::reflect {                                                                                            \
    template struct MC_API mc::reflect::reflector<TYPE>;                                                               \
    static_assert(mc::reflect::is_reflectable<TYPE>(), BOOST_PP_STRINGIZE(TYPE)" is not reflectable");                 \
    MC_REFLECT_ENUM_STATIC_METADATA(TYPE, VALUES);                                                                     \
    type_id_type mc::reflect::static_metadata<TYPE>::type_id =                                                         \
        get_reflect_factory<TYPE>().template register_type<TYPE>();                                                    \
    template <>                                                                                                        \
    [[maybe_unused]] mc::string_view reflector<TYPE>::get_name()                                                       \
    {                                                                                                                  \
        return get_type_name<TYPE>();                                                                                  \
    }                                                                                                                  \
    template <>                                                                                                        \
    [[maybe_unused]] type_id_type reflector<TYPE>::get_type_id()                                                       \
    {                                                                                                                  \
        return mc::reflect::static_metadata<TYPE>::type_id;                                                            \
    }                                                                                                                  \
    template <>                                                                                                        \
    [[maybe_unused]] type_id_type reflector<TYPE>::register_type()                                                     \
    {                                                                                                                  \
        return get_reflect_factory<TYPE>().template register_type<TYPE>(mc::reflect::static_metadata<TYPE>::type_id);  \
    }                                                                                                                  \
    template <>                                                                                                        \
    [[maybe_unused]] void reflector<TYPE>::unregister_type()                                                           \
    {                                                                                                                  \
        mc::reflect::detail::unregister_reflection_type<TYPE>();                                                       \
        if (reflection<TYPE>::has_instance()) {                                                                        \
            reflection<TYPE>::reset_instance_for_test();                                                               \
        }                                                                                                              \
    }                                                                                                                  \
    template <>                                                                                                        \
    [[maybe_unused]] const mc::reflect::enum_metadata& reflector<TYPE>::get_metadata()                                 \
    {                                                                                                                  \
        static const auto* metadata =                                                                                  \
            new mc::reflect::enum_metadata(get_name(), mc::reflect::enum_values{MC_REFLECT_STATIC_MEMBERS(TYPE)});     \
        return *metadata;                                                                                              \
    }                                                                                                                  \
    template <>                                                                                                        \
    [[maybe_unused]] void reflector<TYPE>::to_variant(const TYPE& e, mc::variant& var)                                 \
    {                                                                                                                  \
        auto value = get_metadata().try_get_name(static_cast<enum_value_type>(e));                                     \
        if (!value) {                                                                                                  \
            mc::reflect::throw_bad_enum_cast(static_cast<int64_t>(e), #TYPE);                                          \
        }                                                                                                              \
        var = *value;                                                                                                  \
    }                                                                                                                  \
    template <>                                                                                                        \
    [[maybe_unused]] void reflector<TYPE>::from_variant(const mc::variant& var, TYPE& e)                               \
    {                                                                                                                  \
        auto value = get_metadata().try_get_value_from_variant(var);                                                   \
        if (!value) {                                                                                                  \
            auto va = var.as_string();                                                                                 \
            mc::reflect::throw_bad_enum_cast(va.c_str(), #TYPE);                                                       \
        }                                                                                                              \
        e = static_cast<TYPE>(*value);                                                                                 \
    }                                                                                                                  \
    template <>                                                                                                        \
    [[maybe_unused]] mc::reflect::reflection<TYPE>& reflector<TYPE>::get_reflection()                                  \
    {                                                                                                                  \
        return mc::reflect::reflection<TYPE>::instance();                                                              \
    }                                                                                                                  \
    }

#define MC_REFLECT_ENUM(TYPE, VALUES) MC_REFLECT_ENUM_IMPL(TYPE, VALUES)

#define MC_DEFINE_NAMESPACE(TYPE, NAMESPACE)                                                                           \
    namespace mc::reflect::detail {                                                                                    \
    template <>                                                                                                        \
    struct reflect_namespace<TYPE> {                                                                                   \
        using type = NAMESPACE;                                                                                        \
    };                                                                                                                 \
    }

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
#define MC_REFLECT_WITH_NAMESPACE(NAMESPACE, TYPE, ...)                                                                \
    MC_DEFINE_NAMESPACE(TYPE, NAMESPACE)                                                                               \
    MC_REFLECT(TYPE, __VA_ARGS__)

/**
 * @brief 定义带命名空间的枚举反射信息
 * @param TYPE 枚举类型（可以是元组形式 (Type, "Name")）
 * @param NAMESPACE 命名空间类型
 * @param VALUES 枚举值
 *
 * 用法示例：
 * MC_REFLECT_ENUM_WITH_NAMESPACE(my_enum, my_namespace_type, (VALUE1)(VALUE2))
 */
#define MC_REFLECT_ENUM_WITH_NAMESPACE(NAMESPACE, TYPE, VALUES)                                                        \
    MC_DEFINE_NAMESPACE(TYPE, NAMESPACE)                                                                               \
    MC_REFLECT_ENUM(TYPE, VALUES)

#endif // MC_REFLECT_REFLECT_H