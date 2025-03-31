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

#include <atomic>
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
#include <mc/database/key_extractor.h>
#include <mc/database/object.h>
#include <mc/exception.h>
#include <mc/im/radix_tree.h>
#include <mc/im/ref_ptr.h>

namespace mc::database {

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
 * 创建对象ID索引
 */
template <typename ObjectType, typename Alloc>
auto make_object_id_index(const Alloc& alloc = Alloc()) {
    return index<ObjectType, object_id_key<ObjectType>, true>(object_id_key<ObjectType>{}, alloc);
}

/**
 * 生成索引元组帮助类，包含用户定义的索引
 */
template <typename ObjectType, typename Tuple, std::size_t... Is, typename Alloc>
auto make_indices_with_user_indices_impl(std::index_sequence<Is...>, const Alloc& alloc) {
    return std::make_tuple(
        make_object_id_index<ObjectType>(alloc),
        index<ObjectType, typename std::tuple_element_t<Is, Tuple>::key_extractor_type,
              std::tuple_element_t<Is, Tuple>::is_unique>(
            typename std::tuple_element_t<Is, Tuple>::key_extractor_type{}, alloc)...);
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

/**
 * 遍历索引元组并执行操作，直到某个操作返回false
 */
template <typename Tuple, typename Func, std::size_t... Is>
void for_each_index_until_impl(Tuple& tuple, Func&& func, std::index_sequence<Is...>) {
    (func(std::get<Is>(tuple)) && ...);
}

template <typename Tuple, typename Func>
void for_each_index_until(Tuple& tuple, Func&& func) {
    for_each_index_until_impl(tuple, std::forward<Func>(func),
                              std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

} // namespace detail

/**
 * 表的实现，它是多个索引的组合
 * @tparam ObjectType 对象类型
 * @tparam IndexDef 索引定义类型，默认为 no_indices
 */
template <typename ObjectType, typename IndexDef = no_indices>
class table {
public:
    using object_type      = ObjectType;
    using indices_def      = typename IndexDef::indices;
    using object_ptr_type  = mc::im::ref_ptr<object_type>;
    using index_map_config = mc::im::tree_config<object_ptr_type, typename object_type::alloc_type>;
    using index_txn_type   = mc::im::transaction<index_map_config>;
    using object_id_type   = typename ObjectType::object_id_type;
    using alloc_type       = typename index_txn_type::allocator_type;

    // 确保所有索引使用相同的对象类型
    static_assert(detail::verify_indices_object_type<object_type, indices_def>(),
                  "All indices must use the same object type");

    // 索引元组类型
    using indices_tuple_type = std::conditional_t<
        std::is_same_v<IndexDef, no_indices>,
        std::tuple<decltype(detail::make_object_id_index<ObjectType>(std::declval<alloc_type>()))>,
        decltype(detail::make_indices_with_user_indices_impl<ObjectType, indices_def>(
            std::make_index_sequence<std::tuple_size_v<indices_def>>{},
            std::declval<alloc_type>()))>;

    /**
     * 构造函数
     * @param alloc 内存分配器
     */
    explicit table(const alloc_type& alloc = alloc_type())
        : m_indices(make_indices(alloc)), m_next_id(1) {
    }

    ~table() {
    }

    /**
     * 向表中添加一条记录
     * @param obj 记录对象
     * @return 成功返回true，失败返回false
     */
    bool add(const object_type& obj) {
        auto obj_ptr = object_type::create(obj);
        return add(obj_ptr);
    }

    /**
     * 向表中添加一条记录（接受引用计数指针）
     * @param obj_ptr 记录对象的引用计数指针
     * @return 成功返回true，失败返回false
     */
    bool add(object_ptr_type obj_ptr) {
        // 如果对象没有有效ID，则分配新ID
        if (!obj_ptr->has_valid_id()) {
            obj_ptr->set_object_id(generate_id());
        }

        bool success      = true;
        auto add_to_index = [&](auto& idx) -> bool {
            bool result = idx.add(obj_ptr);
            if (!result) {
                success = false;
            }
            return result;
        };

        // 添加到所有索引，任一失败即中断
        detail::for_each_index_until(m_indices, add_to_index);
        if (success) {
            commit();
        } else {
            rollback();
        }

        return success;
    }

    /**
     * 更新表中的记录
     * @param old_obj 旧记录
     * @param new_obj 新记录
     * @return 成功返回true，失败返回false
     */
    bool update(const object_type& old_obj, const object_type& new_obj) {
        bool success      = true;
        auto update_index = [&](auto& idx) -> bool {
            bool result = idx.update(old_obj, new_obj);
            if (!result) {
                success = false;
            }
            return result;
        };

        // 更新所有索引，任一失败即中断
        detail::for_each_index_until(m_indices, update_index);
        if (success) {
            commit();
        } else {
            rollback();
        }
        return success;
    }

    /**
     * 删除表中的记录
     * @param obj 要删除的记录
     * @return 删除的记录数量
     */
    auto remove(const object_type& obj) {
        // 从第一个索引删除，并保存结果
        auto result  = std::get<0>(m_indices).remove(obj);
        bool success = result.has_value();
        if (!success) {
            std::get<0>(m_indices).rollback();
            return decltype(result)();
        }

        // 从剩余索引删除
        bool remove_rest_success = true;
        auto remove_from_rest    = [&](auto& idx) -> bool {
            if (&idx == &std::get<0>(m_indices)) {
                return true;
            }

            auto removed = idx.remove(obj);
            if (!removed.has_value()) {
                remove_rest_success = false;
                return false;
            }
            return true;
        };

        detail::for_each_index_until(m_indices, remove_from_rest);
        if (remove_rest_success) {
            commit();
            return result;
        }

        rollback();
        return decltype(result)();
    }

    void commit() {
        detail::for_each_index(m_indices, [](auto& idx) {
            idx.commit();
            return true;
        });
    }

    void rollback() {
        detail::for_each_index(m_indices, [](auto& idx) {
            idx.rollback();
            return true;
        });
    }

    /**
     * 根据ID查找记录
     * @param id 对象ID
     * @return 迭代器
     */
    auto find_by_object_id(object_id_type id) {
        return std::get<0>(m_indices).find(id);
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
        if constexpr (std::is_same_v<Tag, by_object_id_tag>) {
            return std::get<0>(m_indices);
        } else {
            constexpr size_t index =
                mc::database::detail::tag_index_of<Tag, indices_def>::value + 1;
            return std::get<index>(m_indices);
        }
    }

private:
    /**
     * 生成新的对象ID
     * @return 新的对象ID
     */
    object_id_type generate_id() {
        return m_next_id.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * 根据IndexDef选择合适的索引创建方法
     */
    indices_tuple_type make_indices(const alloc_type& alloc = alloc_type()) {
        if constexpr (std::is_same_v<IndexDef, no_indices>) {
            // 只使用默认的对象ID索引
            return std::make_tuple(detail::make_object_id_index<object_type>(alloc));
        } else {
            // 使用默认的对象ID索引加上用户定义的索引
            return detail::make_indices_with_user_indices_impl<object_type, indices_def>(
                std::make_index_sequence<std::tuple_size_v<indices_def>>{}, alloc);
        }
    }

    indices_tuple_type          m_indices;
    std::atomic<object_id_type> m_next_id; ///< 下一个可用的对象ID
};

} // namespace mc::database

#endif // MC_DATABASE_TABLE_H