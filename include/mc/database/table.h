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

#ifndef MC_DATABASE_TABLE_H
#define MC_DATABASE_TABLE_H

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <mc/database/common.h>
#include <mc/database/index.h>
#include <mc/database/index_tag.h>
#include <mc/database/key.h>
#include <mc/exception.h>
#include <mc/im/radix_tree.h>

namespace mc::database {

template <typename ObjectType, typename ObjectIdType = uint32_t>
class object_base {
public:
    using id_type = ObjectIdType;

    object_base() = default;

    object_base(id_type id) : m_id(id) {
    }

    auto get_id() const {
        return m_id;
    }

    void set_id(id_type id) {
        m_id = id;
    }

protected:
    id_type m_id = {0};
};

/**
 * 辅助函数，从索引定义中提取信息
 */
namespace detail {

/**
 * 验证所有索引使用的对象类型相同
 */
template <typename First, typename... Rest>
struct verify_object_type {
    using object_type = typename First::object_type;

    template <typename T>
    struct is_same_object {
        static constexpr bool value = std::is_same_v<typename T::object_type, object_type>;
    };

    static constexpr bool value = (is_same_object<Rest>::value && ...) && sizeof...(Rest) > 0;
};

template <typename T>
struct verify_object_type<T> {
    static constexpr bool value = true;
};

/**
 * 递归检查索引类型是否匹配
 */
template <typename ObjectType, typename Tuple, std::size_t... Is>
constexpr bool verify_indices_object_type_impl(std::index_sequence<Is...>) {
    if constexpr (sizeof...(Is) <= 1) {
        return true;
    } else {
        using FirstType = typename std::tuple_element_t<0, Tuple>::object_type;
        return (std::is_same_v<typename std::tuple_element_t<Is, Tuple>::object_type, FirstType> &&
                ...);
    }
}

/**
 * 检查一个索引元组中所有索引是否使用相同的对象类型
 */
template <typename ObjectType, typename Tuple>
constexpr bool verify_indices_object_type() {
    return verify_indices_object_type_impl<ObjectType, Tuple>(
        std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

/**
 * 生成索引元组帮助类
 */
template <typename ObjectType, typename Tuple, std::size_t... Is>
auto make_indices_impl(std::index_sequence<Is...>) {
    return std::make_tuple(
        index<ObjectType, typename std::tuple_element_t<Is, Tuple>::key_extractor_type,
              std::tuple_element_t<Is, Tuple>::is_unique>(
            typename std::tuple_element_t<Is, Tuple>::key_extractor_type{})...);
}

template <typename ObjectType, typename Tuple>
auto make_indices() {
    return make_indices_impl<ObjectType, Tuple>(
        std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

/**
 * 遍历索引元组并执行操作的辅助函数
 */
template <typename Tuple, typename Func, std::size_t... Is>
bool for_each_index_impl(Tuple& tuple, Func&& func, std::index_sequence<Is...>) {
    return (func(std::get<Is>(tuple)) && ...);
}

template <typename Tuple, typename Func>
bool for_each_index(Tuple& tuple, Func&& func) {
    return for_each_index_impl(tuple, std::forward<Func>(func),
                               std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

} // namespace detail

/**
 * 表的实现，它是多个索引的组合
 * @tparam ObjectType 对象类型
 * @tparam IndexDef 索引定义类型
 */
template <typename ObjectType, typename IndexDef>
class table {
public:
    using object_type      = ObjectType;
    using indices_def      = typename IndexDef::indices;
    using index_map_config = mc::im::tree_config<object_type*, std::allocator<char>>;
    using index_txn_type   = mc::im::transaction<index_map_config>;
    using id_type          = typename ObjectType::id_type;

    // 确保所有索引使用相同的对象类型
    static_assert(detail::verify_indices_object_type<object_type, indices_def>(),
                  "All indices must use the same object type");

    /**
     * 构造函数
     * @param name 表名
     */
    explicit table() : m_indices(detail::make_indices<object_type, indices_def>()) {
    }

    /**
     * 向表中添加一条记录
     * @param obj 记录对象
     * @return 成功返回true，失败返回false
     */
    bool add(const object_type& obj) {
        // 添加到所有索引，任一失败即返回失败
        return detail::for_each_index(m_indices, [&](auto& idx) {
            return idx.add(obj);
        });
    }

    /**
     * 更新表中的记录
     * @param old_obj 旧记录
     * @param new_obj 新记录
     * @return 成功返回true，失败返回false
     */
    bool update(const object_type& old_obj, const object_type& new_obj) {
        // 更新所有索引，任一失败即返回失败
        return detail::for_each_index(m_indices, [&](auto& idx) {
            return idx.update(old_obj, new_obj);
        });
    }

    /**
     * 删除表中的记录
     * @param obj 要删除的记录
     * @return 删除的记录数量
     */
    auto remove(const object_type& obj) {
        // 删除操作仍使用第一个索引，因为需要返回一个值
        return std::get<0>(m_indices).remove(obj);
    }

    /**
     * 根据键查找记录
     * @param keys 键值
     * @return 迭代器
     */
    template <typename... KeyTypes>
    auto find(const KeyTypes&... keys) {
        return std::get<0>(m_indices).find(keys...);
    }

    /**
     * 统一的获取索引方法，既支持数字索引又支持标签类型
     * @tparam I 索引序号
     * @return 索引引用
     */
    template <size_t I>
    auto& get() {
        return std::get<I>(m_indices);
    }

    /**
     * 统一的获取索引方法，既支持数字索引又支持标签类型
     * @tparam Tag 标签类型
     * @return 索引引用
     */
    template <typename Tag, typename = std::enable_if_t<mc::database::is_tag<Tag>::value>>
    auto& get() {
        constexpr size_t index = mc::database::detail::tag_index_of<Tag, indices_def>::value;
        return std::get<index>(m_indices);
    }

private:
    decltype(detail::make_indices<object_type, indices_def>()) m_indices;
};

} // namespace mc::database

#endif // MC_DATABASE_TABLE_H