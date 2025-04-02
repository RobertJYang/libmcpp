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
#include <mc/database/table_op.h>
#include <mc/database/transaction.h>
#include <mc/exception.h>
#include <mc/im/radix_tree.h>
#include <mc/im/ref_ptr.h>
#include <mc/reflect.h>

namespace mc::database {
/**
 * 使用反射获取对象属性值
 * @tparam T 对象类型
 * @param obj 对象
 * @param key 属性名
 * @return 属性值的可选包装
 */
template <typename T>
std::optional<mc::variant> get_property(const T& obj, std::string_view key) {
    // 使用反射框架中的缓存机制获取属性
    return mc::reflect::get_property(obj, key);
}

/**
 * 使用反射获取对象属性值
 * @tparam T 对象类型
 * @param obj 对象
 * @param key 属性名
 * @return 属性值的可选包装
 */
template <typename T>
std::optional<mc::variant> set_property(T& obj, std::string_view key, const mc::variant& value) {
    // 使用反射框架中的缓存机制获取属性
    return mc::reflect::set_property(obj, key, value);
}

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

/**
 * 判断键抽取器是否为成员键
 */
template <typename KeyExtractor, typename ObjectType, typename MemberType, typename = void>
struct has_member_key : std::false_type {};

template <typename KeyExtractor, typename ObjectType, typename MemberType>
struct has_member_key<
    KeyExtractor, ObjectType, MemberType,
    std::void_t<
        decltype(std::declval<KeyExtractor>().template get_member_name<ObjectType, MemberType>())>>
    : std::true_type {};

/**
 * 获取成员键名
 */
template <typename KeyExtractor, typename ObjectType, typename MemberType>
std::string get_member_name() {
    if constexpr (has_member_key<KeyExtractor, ObjectType, MemberType>::value) {
        return KeyExtractor::template get_member_name<ObjectType, MemberType>();
    }
    return "";
}

/**
 * 判断键抽取器是否为成员函数键
 */
template <typename KeyExtractor, typename ObjectType, typename ReturnType, typename = void>
struct has_member_function_key : std::false_type {};

template <typename KeyExtractor, typename ObjectType, typename ReturnType>
struct has_member_function_key<
    KeyExtractor, ObjectType, ReturnType,
    std::void_t<decltype(std::declval<KeyExtractor>()
                             .template get_member_function_name<ObjectType, ReturnType>())>>
    : std::true_type {};

/**
 * 获取成员函数键名
 */
template <typename KeyExtractor, typename ObjectType, typename ReturnType>
std::string get_member_function_name() {
    if constexpr (has_member_function_key<KeyExtractor, ObjectType, ReturnType>::value) {
        return KeyExtractor::template get_member_function_name<ObjectType, ReturnType>();
    }
    return "";
}

/**
 * 判断键抽取器是否为复合键
 */
template <typename KeyExtractor, typename ObjectType, typename = void>
struct has_composite_key : std::false_type {};

template <typename KeyExtractor, typename ObjectType>
struct has_composite_key<
    KeyExtractor, ObjectType,
    std::void_t<decltype(std::declval<KeyExtractor>().template is_composite_key<ObjectType>())>>
    : std::true_type {};

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
    explicit table(uint32_t table_id = 0, const alloc_type& alloc = alloc_type())
        : m_indices(make_indices(alloc)), m_next_id(1),
          m_table_id(table_id ? table_id : transaction::alloc_table_id()) {
    }

    ~table() {
    }

    /**
     * 向表中添加一条记录
     * @param obj 记录对象
     * @return 成功返回true，失败返回false
     */
    object_ptr_type add(const object_type& obj, transaction* txn = nullptr) {
        auto obj_ptr = object_type::create(obj);
        return add(obj_ptr, txn);
    }

    /**
     * 向表中添加一条记录（接受引用计数指针）
     * @param obj_ptr 记录对象的引用计数指针
     * @return 成功返回true，失败返回false
     */
    object_ptr_type add(object_ptr_type obj_ptr, transaction* txn = nullptr) {
        // 如果对象没有有效ID，则分配新ID
        if (!obj_ptr->has_valid_id()) {
            obj_ptr->set_object_id(generate_id());
        }

        // 是否需要事务管理
        bool need_txn = (txn != nullptr);

        // 如果使用事务，先创建资源对象
        std::shared_ptr<db_resource> resource;
        if (need_txn) {
            resource = std::make_shared<table_add_resource<table>>(m_table_id, *this, obj_ptr,
                                                                   alloc_savepoint(txn));
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
        if (!success) {
            rollback();
            return nullptr;
        }

        if (!need_txn) {
            commit();
        } else {
            txn->add_resource(resource);
        }
        return obj_ptr;
    }

    object_ptr_type update(object_ptr_type old_obj_ptr, const object_type& new_obj,
                           transaction* txn = nullptr) {
        auto new_obj_ptr = object_type::create(new_obj);
        new_obj_ptr->set_object_id(old_obj_ptr->get_object_id());
        return update(old_obj_ptr, new_obj_ptr, txn);
    }

    /**
     * 更新表中的记录
     * @param old_obj 旧记录
     * @param new_obj 新记录
     * @return 成功返回true，失败返回false
     */
    object_ptr_type update(object_ptr_type old_obj_ptr, object_ptr_type new_obj_ptr,
                           transaction* txn = nullptr) {
        // 是否需要事务管理
        bool need_txn = (txn != nullptr);

        // 如果使用事务，先创建资源对象
        std::shared_ptr<db_resource> resource;
        if (need_txn) {
            resource = std::make_shared<table_update_resource<table>>(
                m_table_id, *this, old_obj_ptr, new_obj_ptr, alloc_savepoint(txn));
        }

        bool success      = true;
        auto update_index = [&](auto& idx) -> bool {
            bool result = idx.update(*old_obj_ptr, new_obj_ptr);
            if (!result) {
                success = false;
            }
            return result;
        };

        // 更新所有索引，任一失败即中断
        detail::for_each_index_until(m_indices, update_index);

        if (!success) {
            rollback();
            return nullptr;
        }

        if (!need_txn) {
            commit();
        } else {
            txn->add_resource(resource);
        }
        return new_obj_ptr;
    }

    /**
     * 删除表中的记录
     * @param obj 要删除的记录
     * @return 删除的记录数量
     */
    bool remove(object_ptr_type obj_ptr, transaction* txn = nullptr) {
        // 是否需要事务管理
        bool need_txn = (txn != nullptr);

        // 如果使用事务，创建资源对象
        std::shared_ptr<db_resource> resource;
        if (need_txn) {
            resource = std::make_shared<table_remove_resource<table>>(m_table_id, *this, obj_ptr,
                                                                      alloc_savepoint(txn));
        }

        // 从剩余索引删除
        bool success = true;
        auto remove  = [&](auto& idx) -> bool {
            auto removed = idx.remove(*obj_ptr);
            if (!removed.has_value()) {
                success = false;
            }
            return true;
        };

        detail::for_each_index_until(m_indices, remove);
        if (!success) {
            rollback();
            return false;
        }

        if (!need_txn) {
            commit();
        } else {
            txn->add_resource(resource);
        }
        return true;
    }

    void commit() {
        detail::for_each_index(m_indices, [](auto& idx) {
            idx.commit();
            return true;
        });
        m_txn_savepoint_id   = -1;
        m_index_savepoint_id = -1;
    }

    void rollback() {
        detail::for_each_index(m_indices, [](auto& idx) {
            idx.rollback();
            return true;
        });
        m_txn_savepoint_id   = -1;
        m_index_savepoint_id = -1;
    }

    void rollback_to(int32_t savepoint_id) {
        detail::for_each_index(m_indices, [savepoint_id](auto& idx) {
            idx.rollback_to(savepoint_id);
            return true;
        });
        m_txn_savepoint_id = std::get<0>(m_indices).last_savepoint_id();
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
     * @tparam I 索引序号
     * @return 迭代器
     */
    template <size_t I, typename... KeyTypes>
    auto find(const KeyTypes&... keys) {
        return std::get<I>(m_indices).find(keys...);
    }

    /**
     * 根据键查找记录
     * @param keys 键值
     * @tparam I 索引序号
     * @return 迭代器
     */
    template <typename Tag, typename... KeyTypes>
    auto find(const KeyTypes&... keys) {
        return std::get<Tag>(m_indices).find(keys...);
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

    int alloc_savepoint(transaction* txn) {
        auto sp = txn->last_savepoint_id();
        if (m_txn_savepoint_id != sp) {
            detail::for_each_index(m_indices, [&](auto& idx) {
                m_index_savepoint_id = idx.alloc_save_point();
                return true;
            });
            m_txn_savepoint_id = sp;
        }
        return m_index_savepoint_id;
    }

    /**
     * 获取表ID
     * @return 表ID
     */
    uint32_t get_table_id() const {
        return m_table_id;
    }

    /**
     * 通过键值对查询对象，如果ObjectType支持反射则使用反射机制
     * @tparam DictType 字典类型，可以是mc::variant_object或std::map等
     * @param query 查询条件，键值对字典
     * @return 符合查询条件的对象列表
     */
    template <typename DictType>
    std::vector<object_ptr_type> query_by_dict(const DictType& query) const {
        if constexpr (!mc::reflect::is_reflectable<object_type>()) {
            MC_THROW(mc::exception, "对象类型不支持反射，无法通过字典查询");
        } else {
            return query_by_dict_impl(query);
        }
    }

    /**
     * 通过键值对查询对象，返回第一个匹配的对象
     * @tparam DictType 字典类型，可以是mc::variant_object或std::map等
     * @param query 查询条件，键值对字典
     * @return 符合查询条件的第一个对象，如果未找到则返回nullptr
     */
    template <typename DictType>
    object_ptr_type query_one_by_dict(const DictType& query) const {
        auto results = query_by_dict(query);
        if (results.empty()) {
            return nullptr;
        }
        return results[0];
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

    /**
     * 内部实现方法
     * @tparam DictType 字典类型，可以是mc::variant_object或std::map等
     * @param query 查询条件，键值对字典
     * @return 符合查询条件的对象列表
     */
    template <typename DictType>
    std::vector<object_ptr_type> query_by_dict_impl(const DictType& query) const {
        std::vector<object_ptr_type> results;

        // 如果查询条件为空，返回空结果
        if (query.empty()) {
            return results;
        }

        // 查询策略：
        // 1. 首先检查是否有某个查询条件可以利用索引
        // 2. 如果有，使用该索引获取候选结果集
        // 3. 然后对候选结果集应用其他查询条件进行过滤
        // 4. 如果没有索引可用，则进行全表扫描

        // 记录是否使用了索引
        bool has_used_index = false;

        // 尝试使用索引进行查询
        for (const auto& kv : query) {
            const auto& key   = kv.key;
            const auto& value = kv.value;

            // 尝试使用索引查询
            if (try_query_by_index(key, value, results)) {
                has_used_index = true;

                // 应用其他条件进行过滤
                filter_results(results, query, key);

                break; // 找到一个可用索引就退出
            }
        }

        // 如果没有使用索引，则进行全表扫描
        if (!has_used_index) {
            // 获取主索引（通常是第一个索引）
            auto& primary_index = std::get<0>(m_indices);

            // 遍历所有对象
            for (auto it = primary_index.begin(); !it.is_end(); ++it) {
                object_ptr_type obj_ptr = object_type::create(*it);

                // 检查是否满足所有查询条件
                if (match_all_conditions(obj_ptr, query)) {
                    results.push_back(obj_ptr);
                }
            }
        }

        return results;
    }

    /**
     * 根据其他条件过滤结果集
     * @tparam DictType 字典类型
     * @param results 结果集
     * @param query 所有查询条件
     * @param skip_key 要跳过的键（已经用于索引查询）
     */
    template <typename DictType>
    void filter_results(std::vector<object_ptr_type>& results, const DictType& query,
                        const std::string& skip_key) const {
        // 如果只有一个查询条件，已经通过索引筛选过了，不需要再过滤
        if (query.size() <= 1) {
            return;
        }

        // 过滤不符合其他条件的结果
        auto it = results.begin();
        while (it != results.end()) {
            bool match = true;

            // 检查所有其他条件
            for (const auto& kv : query) {
                const auto& key   = kv.key;
                const auto& value = kv.value;

                // 跳过已经用于索引查询的键
                if (key == skip_key) {
                    continue;
                }

                // 检查对象属性值是否匹配查询条件
                auto obj_value = get_property(**it, key);
                if (!obj_value.has_value() || obj_value.value() != value) {
                    match = false;
                    break;
                }
            }

            // 如果不匹配所有条件，从结果集中移除
            if (!match) {
                it = results.erase(it);
            } else {
                ++it;
            }
        }
    }

    /**
     * 检查对象是否匹配所有查询条件
     * @tparam DictType 字典类型
     * @param obj_ptr 对象指针
     * @param query 查询条件
     * @return 是否匹配所有条件
     */
    template <typename DictType>
    bool match_all_conditions(const object_ptr_type& obj_ptr, const DictType& query) const {
        for (const auto& kv : query) {
            const auto& key   = kv.key;
            const auto& value = kv.value;

            // 通过反射获取对象属性值
            auto obj_value = get_property(*obj_ptr, key);

            // 如果属性不存在或值不匹配，返回false
            if (!obj_value.has_value() || obj_value.value() != value) {
                return false;
            }
        }

        return true;
    }

    /**
     * 尝试使用索引进行查询
     * @tparam ValueType 值类型
     * @param key 键名
     * @param value 值
     * @param results 结果列表
     * @return 是否使用了索引
     */
    template <typename ValueType>
    bool try_query_by_index(const std::string& key, const ValueType& value,
                            std::vector<object_ptr_type>& results) const {
        // 定义缺失的变量
        constexpr bool   HasIndices  = !std::is_same_v<IndexDef, no_indices>;
        constexpr size_t index_count = std::tuple_size_v<indices_def>;

        // 根据不同的索引类型进行查询
        // 检查是否有匹配的索引，通过查找索引定义中的成员键名
        if constexpr (HasIndices) {
            // 优先尝试使用唯一索引
            if (try_query_index<0>(key, value, results)) {
                return true;
            }

            // 尝试其他索引
            if constexpr (index_count > 1) {
                if (try_query_index<1>(key, value, results)) {
                    return true;
                }
            }

            if constexpr (index_count > 2) {
                if (try_query_index<2>(key, value, results)) {
                    return true;
                }
            }

            // 可以根据需要添加更多索引的查询
        }

        return false;
    }

    /**
     * 使用指定索引进行查询
     * @tparam I 索引位置
     * @tparam ValueType 值类型
     * @param key 键名
     * @param value 值
     * @param results 结果列表
     * @return 是否使用了索引
     */
    template <size_t I, typename ValueType>
    bool try_query_index(const std::string& key, const ValueType& value,
                         std::vector<object_ptr_type>& results) const {
        using index_type         = std::tuple_element_t<I, indices_tuple_type>;
        using key_extractor_type = typename index_type::key_extractor_type;

        // 判断索引的键抽取器中是否包含指定的键
        if constexpr (detail::has_member_key<key_extractor_type, object_type, std::string>::value) {
            // 对于使用成员变量作为键的索引
            if (detail::get_member_name<key_extractor_type, object_type, std::string>() == key) {
                if constexpr (std::is_convertible_v<ValueType, std::string>) {
                    auto& index = get<I>();
                    auto  it    = index.find(value);

                    // 收集所有匹配的对象
                    while (!it.is_end() &&
                           get_property(*it, key).value_or(mc::variant()) == value) {
                        results.push_back(object_type::create(*it));
                        ++it;
                    }

                    return !results.empty();
                }
            }
        } else if constexpr (detail::has_member_function_key<key_extractor_type, object_type,
                                                             int>::value) {
            // 对于使用成员函数作为键的索引
            if (detail::get_member_function_name<key_extractor_type, object_type, int>() == key) {
                if constexpr (std::is_convertible_v<ValueType, int>) {
                    auto& index = get<I>();
                    auto  it    = index.find(value);

                    // 收集所有匹配的对象
                    while (!it.is_end() &&
                           get_property(*it, key).value_or(mc::variant()) == value) {
                        results.push_back(object_type::create(*it));
                        ++it;
                    }

                    return !results.empty();
                }
            }
        } else if constexpr (detail::has_composite_key<key_extractor_type, object_type>::value) {
            // 对于复合键，检查是否包含指定的键
            // 复合键的处理比较复杂，这里只是一个示例实现
            // 在真实场景中可能需要根据具体的复合键结构进行更复杂的处理
            return false;
        }

        return false;
    }

    indices_tuple_type          m_indices;
    std::atomic<object_id_type> m_next_id;        ///< 下一个可用的对象ID
    uint32_t                    m_table_id = {0}; ///< 表ID

    int32_t m_txn_savepoint_id   = {-1}; ///< 事务保存点ID
    int32_t m_index_savepoint_id = {-1}; ///< 索引保存点ID
};

} // namespace mc::database

#endif // MC_DATABASE_TABLE_H