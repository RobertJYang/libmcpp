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

#ifndef MC_DATABASE_INDEX_TAG_H
#define MC_DATABASE_INDEX_TAG_H

#include <mc/db/key_extractor.h>
#include <mc/db/query/proto_query.h>

#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace mc::db {

/**
 * 标签基类，用于标识索引
 */
template <typename Tag>
struct tag_base {
    using base_type = tag_base<Tag>;
    using tag_type  = Tag;

    tag_type*                  dummy;
    static constexpr tag_type* base_type::*tag = &base_type::dummy;
};

/**
 * 默认的 object_id 索引标签
 */
struct by_object_id_tag : public tag_base<by_object_id_tag> {};

/**
 * 带字段名称的标签
 */
template <const char* FieldName>
struct field_tag : public tag_base<field_tag<FieldName>> {
    static constexpr const char* field_name = FieldName;

    /**
     * 获取字段名称列表
     * @return 字段名称列表
     */
    static std::vector<std::string> get_field_names()
    {
        return {field_name};
    }

    static mc::db::query::dsl::filed_expr field;
};

template <const char* FieldName>
inline mc::db::query::dsl::filed_expr field_tag<FieldName>::field = mc::db::query::dsl::field(field_name);

/**
 * 确定标签类型是否有效
 */

template <typename T>
inline constexpr bool is_tag_v = std::is_base_of_v<tag_base<T>, T>;

/**
 * 判断类型是否为 field_tag 类型
 * @tparam T 要检查的类型
 */
template <typename T, typename = void>
struct is_field_tag : std::false_type {};

// field_tag 类型的特化，检测是否继承自 tag_base 且有 field_names 成员
template <typename T>
struct is_field_tag<T, std::void_t<std::enable_if_t<std::is_base_of_v<tag_base<T>, T>>, decltype(T::field_name),
                                   decltype(T::get_field_names())>> : std::true_type {};

template <typename T>
inline constexpr bool is_field_tag_v = is_field_tag<T>::value;

/**
 * 查找类型在元组中的索引位置
 * @tparam T 要查找的类型
 * @tparam Tuple 元组类型
 */
template <typename T, typename Tuple>
struct index_of;

template <typename T, typename... Types>
struct index_of<T, std::tuple<T, Types...>> {
    static constexpr size_t value = 0;
};

template <typename T, typename U, typename... Types>
struct index_of<T, std::tuple<U, Types...>> {
    static constexpr size_t value = 1 + index_of<T, std::tuple<Types...>>::value;
};

/**
 * 索引定义组合
 * @tparam Indices 索引定义类型列表
 */
template <typename... Indices>
struct indexed_by {
    using indices = std::tuple<Indices...>;
};

/**
 * 空索引定义
 */
struct no_indices {
    using indices = std::tuple<>;
};

namespace detail {

template <typename T>
struct member_pointer_traits {};

// 成员变量指针的类型特征
template <typename Class, typename T>
struct member_pointer_traits<T Class::*> {
    using class_type = Class;
    using value_type = T;
};

// 成员函数指针的类型特征
template <typename Class, typename Ret, typename... Args>
struct member_pointer_traits<Ret (Class::*)(Args...)> {
    using class_type = Class;
    using value_type = Ret;
};

// const 成员函数指针的类型特征
template <typename Class, typename Ret, typename... Args>
struct member_pointer_traits<Ret (Class::*)(Args...) const> {
    using class_type = Class;
    using value_type = Ret;
};

// 提取器选择器的前向声明
template <auto Ptr, typename = void>
struct extractor_selector;

// 成员变量指针特化
template <auto Ptr>
struct extractor_selector<Ptr, std::enable_if_t<std::is_member_object_pointer_v<decltype(Ptr)>>> {
    using class_type = typename member_pointer_traits<decltype(Ptr)>::class_type;
    using value_type = typename member_pointer_traits<decltype(Ptr)>::value_type;
    using type       = member_key<class_type, value_type, Ptr>;
};

// 成员函数指针特化
template <auto Ptr>
struct extractor_selector<Ptr, std::enable_if_t<std::is_member_function_pointer_v<decltype(Ptr)>>> {
    using class_type = typename member_pointer_traits<decltype(Ptr)>::class_type;
    using value_type = typename member_pointer_traits<decltype(Ptr)>::value_type;
    using type       = member_function_key<class_type, value_type, Ptr>;
};

template <auto Ptr>
using extractor_selector_t = typename extractor_selector<Ptr>::type;

/**
 * 查找标签在索引定义元组中的位置
 * @tparam Tag 标签类型
 * @tparam Tuple 索引定义元组类型
 */
template <typename Tag, typename Tuple>
struct tag_index_of;

template <typename Tag, typename... Indices>
struct tag_index_of<Tag, std::tuple<Indices...>> {
    static constexpr size_t value = index_of<Tag, std::tuple<typename Indices::tag_type...>>::value;
};

/**
 * 从参数包中提取标签类型
 */
template <auto... Ptrs>
struct extract_tag;

template <auto Ptr, auto... Ptrs>
struct extract_tag<Ptr, Ptrs...> {
    using value_type = typename member_pointer_traits<decltype(Ptr)>::value_type;
    using tag_type   = mc::traits::remove_pointers_t<value_type>;
    using type       = std::conditional_t<is_tag_v<tag_type>, tag_type, typename extract_tag<Ptrs...>::type>;
};

template <>
struct extract_tag<> {
    using type = void;
};

template <auto... Ptrs>
using extract_tag_t = typename extract_tag<Ptrs...>::type;

template <typename Tuple, auto... Ptrs>
struct extract_extractor;

template <typename Tuple, auto Ptr, auto... Ptrs>
struct extract_extractor<Tuple, Ptr, Ptrs...> {
    using value_type = typename member_pointer_traits<decltype(Ptr)>::value_type;
    static auto get_type()
    {
        if constexpr (is_tag_v<mc::traits::remove_pointers_t<value_type>>) {
            return Tuple{};
        } else {
            return mc::traits::type_append_t<Tuple, extractor_selector_t<Ptr>>{};
        }
    }
    using type = typename extract_extractor<decltype(get_type()), Ptrs...>::type;
};

template <typename Tuple>
struct extract_extractor<Tuple> {
    using type = Tuple;
};

template <typename... Ts>
struct make_extractor;
template <typename T, typename... Ts>
struct make_extractor<std::tuple<T, Ts...>> {
    using type = std::conditional_t<sizeof...(Ts) == 0, T, composite_key<T, Ts...>>;
};

template <auto... Ptrs>
using extract_extractor_t = typename make_extractor<typename extract_extractor<std::tuple<>, Ptrs...>::type>::type;

/**
 * 有序唯一索引定义
 * @tparam KeyExtractor 键提取器类型
 * @tparam Tag 可选的标签类型，默认为 void
 */
template <typename KeyExtractor, typename Tag = void>
struct ordered_unique {
    using key_extractor_type        = KeyExtractor;
    using object_type               = typename KeyExtractor::object_type;
    using tag_type                  = Tag;
    static constexpr bool is_unique = true;
};

/**
 * 有序非唯一索引定义
 * @tparam KeyExtractor 键提取器类型
 * @tparam Tag 可选的标签类型，默认为 void
 */
template <typename KeyExtractor, typename Tag = void>
struct ordered_non_unique {
    using key_extractor_type        = KeyExtractor;
    using object_type               = typename KeyExtractor::object_type;
    using tag_type                  = Tag;
    static constexpr bool is_unique = false;
};
} // namespace detail

/**
 * 有序唯一索引定义
 */
template <auto... Ptrs>
using ordered_unique = detail::ordered_unique<detail::extract_extractor_t<Ptrs...>, detail::extract_tag_t<Ptrs...>>;

/**
 * 有序非唯一索引定义
 */
template <auto... Ptrs>
using ordered_non_unique =
    detail::ordered_non_unique<detail::extract_extractor_t<Ptrs...>, detail::extract_tag_t<Ptrs...>>;

} // namespace mc::db

// 定义单个字段标签
#define MC_FIELD_INDEX_TAG(tag_name, field_name)                                                                       \
    inline constexpr char field_##tag_name[] = field_name;                                                             \
    using tag_name                           = mc::db::field_tag<field_##tag_name>;

#endif // MC_DATABASE_INDEX_TAG_H