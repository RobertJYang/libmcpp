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

#include <mc/database/key_extractor.h>
#include <tuple>
#include <type_traits>

namespace mc::database {

/**
 * 标签基类，用于标识索引
 */
struct tag_base {};

/**
 * 默认的 object_id 索引标签
 */
struct by_object_id_tag : public tag_base {};

/**
 * 确定标签类型是否有效
 */
template <typename T>
struct is_tag : std::is_base_of<tag_base, T> {};

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

namespace detail {

// 提取器选择器的前向声明
template <typename Class, typename KeyType, auto Ptr, typename = void>
struct extractor_selector;

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

// 成员变量指针特化
template <typename Class, typename KeyType, auto Ptr>
struct extractor_selector<Class, KeyType, Ptr,
                          std::enable_if_t<std::is_member_object_pointer_v<decltype(Ptr)>>> {
    using type = member_key<Class, KeyType, Ptr>;
};

// 成员函数指针特化
template <typename Class, typename KeyType, auto Ptr>
struct extractor_selector<Class, KeyType, Ptr,
                          std::enable_if_t<std::is_member_function_pointer_v<decltype(Ptr)>>> {
    using type = member_function_key<Class, KeyType, Ptr>;
};

} // namespace detail

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

/**
 * 统一的成员提取器，自动判断是成员变量还是成员函数
 * @tparam Class 对象类型
 * @tparam KeyType 键类型
 * @tparam Ptr 成员变量或成员函数指针
 */
template <typename Class, typename KeyType, auto Ptr>
using member = typename detail::extractor_selector<Class, KeyType, Ptr>::type;

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

} // namespace mc::database

#endif // MC_DATABASE_INDEX_TAG_H