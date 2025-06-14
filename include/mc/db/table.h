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

#include <mc/db/index.h>
#include <mc/db/index_tag.h>
#include <mc/db/key.h>
#include <mc/db/key_extractor.h>
#include <mc/db/table_base.h>
#include <mc/db/table_op.h>
#include <mc/exception.h>
#include <mc/im/radix_tree.h>
#include <mc/memory.h>
#include <mc/reflect.h>
#include <mc/signal_slot.h>
#include <mc/traits.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace mc::db {

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
    return index<ObjectType, detail::object_id_key<ObjectType>, true, void, Alloc>(
        detail::object_id_key<ObjectType>{}, alloc, 0);
}

/**
 * 生成索引元组帮助类，包含用户定义的索引
 */
template <typename ObjectType, typename Tuple, std::size_t... Is, typename Alloc>
auto make_indices_with_user_indices_impl(std::index_sequence<Is...>, const Alloc& alloc) {
    return std::make_tuple(
        make_object_id_index<ObjectType, Alloc>(alloc),
        index<ObjectType, typename std::tuple_element_t<Is, Tuple>::key_extractor_type,
              std::tuple_element_t<Is, Tuple>::is_unique,
              typename std::tuple_element_t<Is, Tuple>::tag_type, Alloc>(
            typename std::tuple_element_t<Is, Tuple>::key_extractor_type{}, alloc, Is + 1)...);
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
    std::void_t<decltype(std::declval<KeyExtractor>()
                             .template get_property_name<ObjectType, MemberType>())>>
    : std::true_type {};

/**
 * 获取成员键名
 */
template <typename KeyExtractor, typename ObjectType, typename MemberType>
std::string get_property_name() {
    if constexpr (has_member_key<KeyExtractor, ObjectType, MemberType>::value) {
        return KeyExtractor::template get_property_name<ObjectType, MemberType>();
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

template <typename ObjectType, typename = void>
struct has_createor : std::false_type {};

template <typename ObjectType>
struct has_createor<ObjectType,
                    std::void_t<decltype(ObjectType::create(std::declval<mc::variant>()))>>
    : std::true_type {};

} // namespace detail

/**
 * 表的实现，它是多个索引的组合
 * @tparam ObjectType 对象类型
 * @tparam IndexDef 索引定义类型，默认为 no_indices
 */
template <typename ObjectType, typename IndexDef = no_indices,
          typename Allocator = std::allocator<char>>
class table : public table_base {
public:
    using object_type           = ObjectType;
    using indices_def           = typename IndexDef::indices;
    using object_ptr_type       = mc::shared_ptr<object_type>;
    using const_object_ptr_type = mc::shared_ptr<const object_type>;
    using index_map_config      = mc::im::tree_config<object_ptr_type, Allocator>;
    using index_txn_type        = mc::im::transaction<index_map_config>;
    using alloc_type            = typename index_txn_type::allocator_type;
    using tree_type             = typename index_txn_type::tree_type;
    using raw_iterator          = typename tree_type::iterator;

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

    using indices_array_type =
        std::array<index_base<object_type>*, std::tuple_size_v<indices_tuple_type>>;

    /**
     * 构造函数
     * @param alloc 内存分配器
     */
    table(std::string_view name = std::string_view(), uint32_t table_id = 0,
          const alloc_type& alloc = alloc_type())
        : m_indices(make_indices(alloc)),
          m_table_id(table_id ? table_id : transaction::alloc_table_id()), m_name(name) {
        size_t i = 0;
        detail::for_each_index(m_indices, [&i, this](auto& idx) {
            m_indices_array[i++] = &idx;
            idx.set_table(this);
            return true;
        });
        if (m_name.empty()) {
            m_name = mc::string::join("", "table_", std::to_string(m_table_id),
                                      mc::pretty_name<object_type>());
        }
    }

    ~table() {
    }

    /**
     * 向表中添加一条记录
     * @param obj 记录对象
     * @return 成功返回true，失败返回false
     */
    object_ptr_type add(const object_type& obj, transaction* txn = nullptr) {
        auto obj_ptr = mc::make_shared<object_type>(obj);
        return add(obj_ptr, txn);
    }

    /**
     * 向表中添加一条记录（接受引用计数指针）
     * @param obj_ptr 记录对象的引用计数指针
     * @return 成功返回true，失败返回false
     */
    object_ptr_type add(object_ptr_type obj_ptr, transaction* txn = nullptr) {
        std::lock_guard lock(m_mutex);

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
                                                                   alloc_savepoint_internal(txn));
        }

        try {
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
                rollback_internal();
                return nullptr;
            }
        } catch (...) {
            rollback_internal();
            throw;
        }

        if (!need_txn) {
            commit_internal();
            on_object_added(const_cast<object_type&>(*obj_ptr));
        } else {
            txn->add_resource(resource);
        }
        return obj_ptr;
    }

    object_ptr_type update(object_ptr_type old_obj_ptr, const object_type& new_obj,
                           transaction* txn = nullptr) {
        auto new_obj_ptr = mc::make_shared<object_type>(new_obj);
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
        std::lock_guard lock(m_mutex);

        bool need_txn = (txn != nullptr);

        std::shared_ptr<db_resource> resource;
        if (need_txn) {
            resource = std::make_shared<table_update_resource<table>>(
                m_table_id, *this, old_obj_ptr, new_obj_ptr, alloc_savepoint_internal(txn));
        }

        try {
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
                rollback_internal();
                return nullptr;
            }
        } catch (...) {
            rollback_internal();
            throw;
        }

        if (!need_txn) {
            commit_internal();
            on_object_updated(const_cast<object_type&>(*old_obj_ptr),
                              const_cast<object_type&>(*new_obj_ptr));
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
        std::lock_guard lock(m_mutex);

        bool need_txn = (txn != nullptr);

        // 如果使用事务，创建资源对象
        std::shared_ptr<db_resource> resource;
        if (need_txn) {
            resource = std::make_shared<table_remove_resource<table>>(
                m_table_id, *this, obj_ptr, alloc_savepoint_internal(txn));
        }

        // 从剩余索引删除
        try {
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
                rollback_internal();
                return false;
            }
        } catch (...) {
            rollback_internal();
            throw;
        }

        if (!need_txn) {
            commit_internal();
            on_object_removed(const_cast<object_type&>(*obj_ptr));
        } else {
            txn->add_resource(resource);
        }
        return true;
    }

    void commit() {
        std::lock_guard lock(m_mutex);

        commit_internal();
    }

    void rollback() {
        std::lock_guard lock(m_mutex);

        rollback_internal();
    }

    void rollback_to(int32_t savepoint_id) {
        std::lock_guard lock(m_mutex);

        detail::for_each_index(m_indices, [savepoint_id](auto& idx) {
            idx.rollback_to(savepoint_id);
            return true;
        });
        m_txn_savepoint_id = std::get<0>(m_indices).last_savepoint_id();
    }

    struct lock_guard {
        lock_guard(table& t) : m_table(t) {
            m_table.lock_db();
        }
        ~lock_guard() {
            m_table.unlock_db();
        }

        table& m_table;
    };

    void lock_db() {
        std::lock_guard lock(m_mutex);

        detail::for_each_index(m_indices, [](auto& idx) {
            idx.lock_db();
            return true;
        });
    }

    void unlock_db() {
        std::lock_guard lock(m_mutex);

        detail::for_each_index(m_indices, [](auto& idx) {
            idx.unlock_db();
            return true;
        });
    }

    /**
     * 根据ID查找记录
     * @param id 对象ID
     * @return 迭代器
     */
    auto find_by_object_id(object_id_type id) {
        std::lock_guard lock(m_mutex);

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
        std::lock_guard lock(m_mutex);

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
        std::lock_guard lock(m_mutex);

        return get<Tag>().find(keys...);
    }

    object_ptr_type find_by_index(size_t index_id, const mc::variant& value) {
        std::lock_guard lock(m_mutex);

        if (index_id >= std::tuple_size_v<indices_tuple_type>) {
            return nullptr;
        }

        return m_indices_array[index_id]->raw_find(value);
    }

    raw_iterator lower_bound_by_index(size_t index_id, const mc::variant& value) {
        std::lock_guard lock(m_mutex);

        if (index_id >= std::tuple_size_v<indices_tuple_type>) {
            return raw_iterator();
        }

        return m_indices_array[index_id]->raw_lower_bound(value);
    }

    raw_iterator upper_bound_by_index(size_t index_id, const mc::variant& value) {
        std::lock_guard lock(m_mutex);

        if (index_id >= std::tuple_size_v<indices_tuple_type>) {
            return raw_iterator();
        }

        return m_indices_array[index_id]->raw_upper_bound(value);
    }

    raw_iterator begin(std::size_t index_id = 0) {
        std::lock_guard lock(m_mutex);

        if (index_id >= std::tuple_size_v<indices_tuple_type>) {
            return raw_iterator();
        }

        return m_indices_array[index_id]->raw_begin();
    }

    raw_iterator end(std::size_t index_id = 0) {
        std::lock_guard lock(m_mutex);

        if (index_id >= std::tuple_size_v<indices_tuple_type>) {
            return raw_iterator();
        }

        return m_indices_array[index_id]->raw_end();
    }

    /**
     * 统一的获取索引方法，既支持数字索引又支持标签类型
     * @tparam I 索引序号
     * @return 索引引用
     */
    template <size_t I>
    auto& get() {
        std::lock_guard lock(m_mutex);

        return std::get<I>(m_indices);
    }

    /**
     * 统一的获取索引方法，既支持数字索引又支持标签类型
     * @tparam Tag 标签类型
     * @return 索引引用
     */
    template <typename Tag, typename = std::enable_if_t<mc::db::is_tag_v<Tag>>>
    auto& get() {
        std::lock_guard lock(m_mutex);

        if constexpr (std::is_same_v<Tag, by_object_id_tag>) {
            return std::get<0>(m_indices);
        } else {
            constexpr size_t index = mc::db::detail::tag_index_of<Tag, indices_def>::value + 1;
            return std::get<index>(m_indices);
        }
    }

    int alloc_savepoint(transaction* txn) {
        std::lock_guard lock(m_mutex);

        return alloc_savepoint_internal(txn);
    }

    /**
     * 获取表ID
     * @return 表ID
     */
    uint32_t get_table_id() const override {
        return m_table_id;
    }

    /**
     * 设置表ID
     * @param id 表ID
     */
    void set_table_id(uint32_t id) override {
        m_table_id = id;
    }

    /**
     * 获取表名
     * @return 表名
     */
    std::string_view get_table_name() const override {
        return m_name;
    }

    /**
     * 根据查询条件查找单个记录
     * @param builder 查询构建器
     * @return 找到的记录的可选包装
     */
    object_ptr_type find(const query_builder& builder) {
        return table_query<table<object_type, IndexDef>>(*this).query_one(builder);
    }

    /**
     * 查询记录
     * @param builder 查询构建器
     * @param limit 限制返回的记录数量，0表示不限制
     * @return 查询结果
     */
    std::vector<object_ptr_type> query(const query_builder& builder, size_t limit = 0) {
        return table_query<table<object_type, IndexDef>>(*this).query(builder, limit);
    }

    /**
     * 查询记录
     * @param builder 查询构建器
     * @param handler 处理函数，返回false表示停止查询
     * @return 是否查询完成
     */
    template <typename Handler,
              typename = std::enable_if_t<std::is_invocable_r_v<bool, Handler, object_type&>>>
    bool query(const query_builder& builder, Handler&& handler) {
        return table_query<table<object_type, IndexDef>>(*this).query(
            builder, std::forward<Handler>(handler));
    }

    std::vector<object_ptr_type> all() {
        query_builder builder;
        return table_query<table<object_type, IndexDef>>(*this).query_all(builder);
    }

    /**
     * 高级更新方法，支持通过条件更新多个属性
     * @param condition 查询条件
     * @param values 要更新的值，支持多种数据源
     * @return 更新的记录数量
     */
    size_t update(const query_builder& condition, const mc::dict& values,
                  transaction* txn = nullptr) {
        std::lock_guard lock(m_mutex);

        return update_internal(condition, values, txn);
    }

    size_t update(const query_builder& condition, const std::map<std::string, variant>& values,
                  transaction* txn = nullptr) {
        std::lock_guard lock(m_mutex);

        return update_internal(condition, values, txn);
    }

    /**
     * 高级删除方法，支持通过条件删除多个记录
     * @param condition 查询条件
     * @return 删除的记录数量
     */
    size_t remove(const query_builder& condition, transaction* txn = nullptr) {
        size_t removed_count = 0;

        auto handler = [&](object_type& obj) -> bool {
            if (remove(mc::shared_ptr<object_type>(const_cast<object_type*>(&obj)), txn)) {
                removed_count++;
            }
            return true;
        };

        query(condition, handler);
        return removed_count;
    }

    void clear() override {
        std::lock_guard lock(m_mutex);

        auto& idx = std::get<0>(m_indices);
        for (auto it = idx.begin(); it != idx.end(); ++it) {
            on_object_removed(const_cast<object_type&>(*it));
        }

        detail::for_each_index(m_indices, [](auto& idx) {
            idx.clear();
            return true;
        });
    }

    bool empty() const override {
        std::lock_guard lock(m_mutex);

        return std::get<0>(m_indices).empty();
    }

    size_t size() const override {
        std::lock_guard lock(m_mutex);

        return std::get<0>(m_indices).size();
    }

protected:
    void commit_internal() {
        detail::for_each_index(m_indices, [](auto& idx) {
            idx.commit();
            return true;
        });
        m_txn_savepoint_id   = -1;
        m_index_savepoint_id = -1;
    }

    void rollback_internal() {
        detail::for_each_index(m_indices, [](auto& idx) {
            idx.rollback();
            return true;
        });
        m_txn_savepoint_id   = -1;
        m_index_savepoint_id = -1;
    }

    int alloc_savepoint_internal(transaction* txn) {
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

    object_ptr do_add_object(const mc::dict& var, transaction* txn) override {
        if constexpr (mc::reflect::is_reflectable<object_type>() &&
                      std::is_constructible_v<object_type>) {
            auto obj = mc::make_shared<object_type>();
            from_variant(var, *obj);
            return add(obj, txn).template static_pointer_cast<object_base>();
        } else {
            MC_UNUSED(txn);
        }

        return nullptr;
    }

    size_t do_remove_object(const query_builder& condition, transaction* txn) override {
        if constexpr (mc::reflect::is_reflectable<object_type>()) {
            return remove(condition, txn);
        } else {
            MC_UNUSED(txn);
        }

        return 0;
    }

    object_ptr do_find_object(const query_builder& condition) override {
        if constexpr (mc::reflect::is_reflectable<object_type>()) {
            return find(condition).template static_pointer_cast<object_base>();
        }

        return nullptr;
    }

    bool do_query_object(const query_builder&        builder,
                         table_base::query_handler&& handler) override {
        if constexpr (mc::reflect::is_reflectable<object_type>()) {
            return query(builder, [handler = std::forward<table_base::query_handler>(handler)](
                                      object_type& obj) {
                return handler(obj);
            });
        }
        return false;
    }

    size_t do_update_object(const query_builder& condition, const mc::dict& values,
                            transaction* txn) override {
        if constexpr (mc::reflect::is_reflectable<object_type>()) {
            return update_internal(condition, values, txn);
        } else {
            MC_UNUSED(txn);
        }
        return 0;
    }

    size_t do_update_object(const query_builder&                  condition,
                            const std::map<std::string, variant>& values,
                            transaction*                          txn) override {
        if constexpr (mc::reflect::is_reflectable<object_type>()) {
            return update_internal(condition, values, txn);
        } else {
            MC_UNUSED(txn);
        }
        return 0;
    }

private:
    // 从 dict 更新对象
    void update_object(object_type& obj, const dict& values) {
        for (auto& entry : values) {
            mc::reflect::set_property(obj, entry.key.get_string(), entry.value);
        }
    }

    // 从 map 更新对象
    void update_object(object_type& obj, const std::map<std::string, variant>& values) {
        for (auto& entry : values) {
            mc::reflect::set_property(obj, entry.first, entry.second);
        }
    }

    template <typename T>
    size_t update_internal(const query_builder& condition, const T& values, transaction* txn) {
        size_t updated_count = 0;

        auto handler = [&](const object_type& obj) -> bool {
            if constexpr (mc::traits::is_copyable_v<object_type>) {
                auto new_obj = mc::make_shared<object_type>(obj);
                update_object(*new_obj, values);
                if (update(mc::shared_ptr<object_type>(const_cast<object_type*>(&obj)), new_obj,
                           txn)) {
                    updated_count++;
                }
                return true;
            } else {
                // 非可复制类型，直接更新对象
                update_object(const_cast<object_type&>(obj), values);
                updated_count++;
                return true;
            }
        };

        query(condition, handler);
        return updated_count;
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

    mutable std::recursive_mutex m_mutex;
    indices_tuple_type           m_indices;
    indices_array_type           m_indices_array;
    uint32_t                     m_table_id = {0}; ///< 表ID
    std::string                  m_name;           ///< 表名

    int32_t m_txn_savepoint_id   = {-1}; ///< 事务保存点ID
    int32_t m_index_savepoint_id = {-1}; ///< 索引保存点ID
};

} // namespace mc::db

#endif // MC_DATABASE_TABLE_H