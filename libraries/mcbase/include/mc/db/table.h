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
#include <mc/db/local_storage_engine.h>
#include <mc/db/query/query.h>
#include <mc/db/storage_engine.h>
#include <mc/db/table_base.h>
#include <mc/db/table_op.h>
#include <mc/exception.h>
#include <mc/memory.h>
#include <mc/reflect.h>
#include <mc/string_utils.h>
#include <mc/traits.h>

#include <atomic>
#include <cstddef>
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
constexpr bool verify_indices_object_type_impl(std::index_sequence<Is...>)
{
    if constexpr (sizeof...(Is) <= 1) {
        return true;
    } else {
        using FirstType = typename std::tuple_element_t<0, Tuple>::object_type;
        return (std::is_same_v<typename std::tuple_element_t<Is, Tuple>::object_type, FirstType> && ...);
    }
}

/**
 * 检查一个索引元组中所有索引是否使用相同的对象类型
 */
template <typename ObjectType, typename Tuple>
constexpr bool verify_indices_object_type()
{
    return verify_indices_object_type_impl<ObjectType, Tuple>(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

/**
 * IndexDef 的总索引数：用户索引数 + 1（默认 object_id 索引）
 */
template <typename IndexDef>
inline constexpr std::size_t indices_count_v = std::tuple_size_v<typename IndexDef::indices> + 1U;

/**
 * 创建对象ID索引
 *
 * @tparam Engine 存储引擎类型
 *
 * 注意：`alloc` 是 per-index 的 allocator（已由调用方通过 derive_per_index_alloc 派生过）
 */
template <typename ObjectType, typename Alloc, typename Engine>
auto make_object_id_index(const Alloc& alloc = Alloc())
{
    return index<ObjectType, detail::object_id_key<ObjectType>, true, void, Alloc, Engine>(
        detail::object_id_key<ObjectType>{}, alloc, 0);
}

/**
 * 生成索引元组帮助类
 *
 * 索引在 table 模式下不再各自持 owned engine，统一通过 set_engine() 由 table
 * 的共享 engine 注入。这里只负责把 N 个 index 实例就位并指定 index_id。
 */
template <typename ObjectType, typename IndexTuple, typename Alloc, typename Engine, typename IndexSeq>
struct user_indices_builder;

template <typename ObjectType, typename IndexTuple, typename Alloc, typename Engine, std::size_t... Is>
struct user_indices_builder<ObjectType, IndexTuple, Alloc, Engine, std::index_sequence<Is...>> {
    using object_id_index_type = index<ObjectType, detail::object_id_key<ObjectType>, true, void, Alloc, Engine>;

    static auto build()
    {
        return std::make_tuple(
            object_id_index_type(typename object_id_index_type::table_mode_t{}, detail::object_id_key<ObjectType>{}, 0,
                                 nullptr),
            index<ObjectType, typename std::tuple_element_t<Is, IndexTuple>::key_extractor_type,
                  std::tuple_element_t<Is, IndexTuple>::is_unique,
                  typename std::tuple_element_t<Is, IndexTuple>::tag_type, Alloc, Engine>(
                typename index<ObjectType, typename std::tuple_element_t<Is, IndexTuple>::key_extractor_type,
                               std::tuple_element_t<Is, IndexTuple>::is_unique,
                               typename std::tuple_element_t<Is, IndexTuple>::tag_type, Alloc, Engine>::table_mode_t{},
                typename std::tuple_element_t<Is, IndexTuple>::key_extractor_type{}, static_cast<uint8_t>(Is + 1),
                nullptr)...);
    }
};

/**
 * 遍历索引元组并执行操作的辅助函数
 */
template <typename Tuple, typename Func, std::size_t... Is>
bool for_each_index_impl(Tuple& tuple, Func&& func, std::index_sequence<Is...>)
{
    return (func(std::get<Is>(tuple)) && ...);
}

template <typename Tuple, typename Func>
bool for_each_index(Tuple& tuple, Func&& func)
{
    return for_each_index_impl(tuple, std::forward<Func>(func), std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

/**
 * 遍历索引元组并执行操作，直到某个操作返回false
 */
template <typename Tuple, typename Func, std::size_t... Is>
void for_each_index_until_impl(Tuple& tuple, Func&& func, std::index_sequence<Is...>)
{
    (func(std::get<Is>(tuple)) && ...);
}

template <typename Tuple, typename Func>
void for_each_index_until(Tuple& tuple, Func&& func)
{
    for_each_index_until_impl(tuple, std::forward<Func>(func), std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

/**
 * 判断键抽取器是否为成员键
 */
template <typename KeyExtractor, typename ObjectType, typename MemberType, typename = void>
struct has_member_key : std::false_type {};

template <typename KeyExtractor, typename ObjectType, typename MemberType>
struct has_member_key<
    KeyExtractor, ObjectType, MemberType,
    std::void_t<decltype(std::declval<KeyExtractor>().template get_property_name<ObjectType, MemberType>())>>
    : std::true_type {};

/**
 * 获取成员键名
 */
template <typename KeyExtractor, typename ObjectType, typename MemberType>
mc::string get_property_name()
{
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
    std::void_t<decltype(std::declval<KeyExtractor>().template get_member_function_name<ObjectType, ReturnType>())>>
    : std::true_type {};

/**
 * 获取成员函数键名
 */
template <typename KeyExtractor, typename ObjectType, typename ReturnType>
mc::string get_member_function_name()
{
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
struct has_composite_key<KeyExtractor, ObjectType,
                         std::void_t<decltype(std::declval<KeyExtractor>().template is_composite_key<ObjectType>())>>
    : std::true_type {};

template <typename ObjectType, typename = void>
struct has_createor : std::false_type {};

template <typename ObjectType>
struct has_createor<ObjectType, std::void_t<decltype(ObjectType::create(std::declval<mc::variant>()))>>
    : std::true_type {};

} // namespace detail

/**
 * 表的实现，它是多个索引的组合
 * @tparam ObjectType 对象类型
 * @tparam IndexDef 索引定义类型，默认为 no_indices
 */
template <typename ObjectType, typename IndexDef = no_indices, typename Allocator = std::allocator<char>,
          typename Engine = local_storage_engine<ObjectType, detail::indices_count_v<IndexDef>, Allocator>>
class table : public table_base {
public:
    using object_type           = ObjectType;
    using indices_def           = typename IndexDef::indices;
    using object_ptr_type       = mc::shared_ptr<object_type>;
    using const_object_ptr_type = mc::shared_ptr<const object_type>;
    using engine_type           = Engine;
    using alloc_type            = typename Engine::allocator_type;
    using raw_iterator          = engine_raw_iterator_t<Engine>;

    static constexpr std::size_t index_count = detail::indices_count_v<IndexDef>;

    static_assert(detail::verify_indices_object_type<object_type, indices_def>(),
                  "All indices must use the same object type");

    using indices_tuple_type = std::conditional_t<
        std::is_same_v<IndexDef, no_indices>,
        std::tuple<index<ObjectType, detail::object_id_key<ObjectType>, true, void, alloc_type, Engine>>,
        decltype(detail::user_indices_builder<ObjectType, indices_def, alloc_type, Engine,
                                              std::make_index_sequence<std::tuple_size_v<indices_def>>>::build())>;

    using indices_array_type =
        std::array<index_base<object_type, alloc_type, Engine>*, std::tuple_size_v<indices_tuple_type>>;

    /**
     * 构造函数
     *
     * table 持有唯一的 m_engine，所有 index 通过 set_engine() 注入此引擎，
     *     不再 per-index 各自持事务。
     */
    table(mc::string_view name = mc::string_view(), uint32_t table_id = 0, const alloc_type& alloc = alloc_type())
        : m_engine(std::make_unique<Engine>(alloc, name)), m_indices(make_indices()),
          m_table_id(table_id ? table_id : transaction::alloc_table_id()), m_name(name)
    {
        size_t i = 0;
        detail::for_each_index(m_indices, [&i, this](auto& idx) {
            m_indices_array[i] = &idx;
            idx.set_engine(*m_engine, i);
            idx.set_table(this);
            ++i;
            return true;
        });
        if (m_name.empty()) {
            m_name = mc::strings::join("", "table_", mc::to_string(m_table_id), mc::pretty_name<object_type>());
        }
    }

    ~table()
    {}

    /**
     * 访问 table 持有的存储引擎实例。
     *
     * 用途：
     *   - mcengine 在 USE_SHM 路径上注入 shm_handle_extractor / reconstruct_fn；
     *   - 诊断 / 测试观察引擎内部状态。
     * 注意：返回的引用生命周期与 table 一致，调用方不应另行持有。
     */
    Engine& engine() noexcept
    {
        return *m_engine;
    }
    const Engine& engine() const noexcept
    {
        return *m_engine;
    }

    object_id_type generate_available_id()
    {
        std::lock_guard lock(m_mutex);
        return generate_available_id_internal();
    }

    /**
     * 向表中添加一条记录
     * @param obj 记录对象
     * @return 成功返回true，失败返回false
     */
    object_ptr_type add(const object_type& obj, transaction* txn = nullptr)
    {
        auto obj_ptr = mc::make_shared<object_type>(obj);
        return add(obj_ptr, txn);
    }

    /**
     * 向表中添加一条记录（接受引用计数指针）
     * @param obj_ptr 记录对象的引用计数指针
     * @return 成功返回true，失败返回false
     */
    object_ptr_type add(object_ptr_type obj_ptr, transaction* txn = nullptr)
    {
        std::lock_guard lock(m_mutex);

        // 如果对象没有有效ID，则分配新ID
        if (!obj_ptr->has_valid_id()) {
            obj_ptr->set_object_id(generate_available_id_internal());
        }

        // 是否需要事务管理
        bool need_txn = (txn != nullptr);

        // 如果使用事务，先创建资源对象
        std::shared_ptr<db_resource> resource;
        if (need_txn) {
            resource =
                std::make_shared<table_add_resource<table>>(m_table_id, *this, obj_ptr, alloc_savepoint_internal(txn));
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

    object_ptr_type update(object_ptr_type old_obj_ptr, const object_type& new_obj, transaction* txn = nullptr)
    {
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
    object_ptr_type update(object_ptr_type old_obj_ptr, object_ptr_type new_obj_ptr, transaction* txn = nullptr)
    {
        std::lock_guard lock(m_mutex);

        bool need_txn = (txn != nullptr);

        std::shared_ptr<db_resource> resource;
        if (need_txn) {
            resource = std::make_shared<table_update_resource<table>>(m_table_id, *this, old_obj_ptr, new_obj_ptr,
                                                                      alloc_savepoint_internal(txn));
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
            on_object_updated(const_cast<object_type&>(*old_obj_ptr), const_cast<object_type&>(*new_obj_ptr));
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
    bool remove(object_ptr_type obj_ptr, transaction* txn = nullptr)
    {
        std::lock_guard lock(m_mutex);

        bool need_txn = (txn != nullptr);

        // 如果使用事务，创建资源对象
        std::shared_ptr<db_resource> resource;
        if (need_txn) {
            resource = std::make_shared<table_remove_resource<table>>(m_table_id, *this, obj_ptr,
                                                                      alloc_savepoint_internal(txn));
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

    void commit()
    {
        std::lock_guard lock(m_mutex);

        commit_internal();
    }

    void rollback()
    {
        std::lock_guard lock(m_mutex);

        rollback_internal();
    }

    void rollback_to(int32_t savepoint_id)
    {
        std::lock_guard lock(m_mutex);

        m_engine->rollback_to(savepoint_id);
        m_txn_savepoint_id = m_engine->current_save_point();
    }

    struct lock_guard {
        lock_guard(table& t) : m_table(t)
        {
            m_table.lock_db();
        }
        ~lock_guard()
        {
            m_table.unlock_db();
        }

        table& m_table;
    };

    void lock_db()
    {
        std::lock_guard lock(m_mutex);

        m_engine->lock_db();
    }

    void unlock_db()
    {
        std::lock_guard lock(m_mutex);

        m_engine->unlock_db();
    }

    /**
     * 根据ID查找记录
     * @param id 对象ID
     * @return 迭代器
     */
    auto find_by_object_id(object_id_type id)
    {
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
    auto find(const KeyTypes&... keys)
    {
        std::lock_guard lock(m_mutex);

        return index_ref<I>().find(keys...);
    }

    /**
     * 根据键查找记录
     * @param keys 键值
     * @tparam I 索引序号
     * @return 迭代器
     */
    template <typename Tag, typename... KeyTypes>
    auto find(const KeyTypes&... keys)
    {
        std::lock_guard lock(m_mutex);

        return index_ref<Tag>().find(keys...);
    }

    object_ptr_type find_by_index(size_t index_id, const mc::variant& value)
    {
        std::lock_guard lock(m_mutex);

        if (index_id >= std::tuple_size_v<indices_tuple_type>) {
            return nullptr;
        }

        return m_indices_array[index_id]->raw_find(value);
    }

    raw_iterator lower_bound_by_index(size_t index_id, const mc::variant& value)
    {
        std::lock_guard lock(m_mutex);

        if (index_id >= std::tuple_size_v<indices_tuple_type>) {
            return raw_iterator();
        }

        return m_indices_array[index_id]->raw_lower_bound(value);
    }

    raw_iterator upper_bound_by_index(size_t index_id, const mc::variant& value)
    {
        std::lock_guard lock(m_mutex);

        if (index_id >= std::tuple_size_v<indices_tuple_type>) {
            return raw_iterator();
        }

        return m_indices_array[index_id]->raw_upper_bound(value);
    }

    raw_iterator begin(std::size_t index_id = 0)
    {
        std::lock_guard lock(m_mutex);

        if (index_id >= std::tuple_size_v<indices_tuple_type>) {
            return raw_iterator();
        }

        return m_indices_array[index_id]->raw_begin();
    }

    raw_iterator end(std::size_t index_id = 0)
    {
        std::lock_guard lock(m_mutex);

        if (index_id >= std::tuple_size_v<indices_tuple_type>) {
            return raw_iterator();
        }

        return m_indices_array[index_id]->raw_end();
    }

    /**
     * 统一的获取索引方法，既支持数字索引又支持标签类型
     * @tparam I 索引序号
     * @return 索引引用。注意：返回后不再持有表锁；运行时代码优先使用 with_index()。
     */
    template <size_t I>
    auto& get()
    {
        std::lock_guard lock(m_mutex);

        return index_ref<I>();
    }

    /**
     * 统一的获取索引方法，既支持数字索引又支持标签类型
     * @tparam Tag 标签类型
     * @return 索引引用。注意：返回后不再持有表锁；运行时代码优先使用 with_index()。
     */
    template <typename Tag, typename = std::enable_if_t<mc::db::is_tag_v<Tag>>>
    auto& get()
    {
        std::lock_guard lock(m_mutex);

        return index_ref<Tag>();
    }

    // 锁内访问索引，运行时代码优先使用这个入口。
    template <size_t I, typename Func>
    decltype(auto) with_index(Func&& func)
    {
        std::lock_guard lock(m_mutex);

        using result_type = std::invoke_result_t<Func, decltype(index_ref<I>())>;
        if constexpr (std::is_void_v<result_type>) {
            std::invoke(std::forward<Func>(func), index_ref<I>());
            return;
        } else {
            return std::invoke(std::forward<Func>(func), index_ref<I>());
        }
    }

    template <typename Tag, typename Func, typename = std::enable_if_t<mc::db::is_tag_v<Tag>>>
    decltype(auto) with_index(Func&& func)
    {
        std::lock_guard lock(m_mutex);

        using result_type = std::invoke_result_t<Func, decltype(index_ref<Tag>())>;
        if constexpr (std::is_void_v<result_type>) {
            std::invoke(std::forward<Func>(func), index_ref<Tag>());
            return;
        } else {
            return std::invoke(std::forward<Func>(func), index_ref<Tag>());
        }
    }

    int alloc_savepoint(transaction* txn)
    {
        std::lock_guard lock(m_mutex);

        return alloc_savepoint_internal(txn);
    }

    /**
     * 获取表ID
     * @return 表ID
     */
    uint32_t get_table_id() const override
    {
        return m_table_id;
    }

    /**
     * 设置表ID
     * @param id 表ID
     */
    void set_table_id(uint32_t id) override
    {
        m_table_id = id;
    }

    /**
     * 获取表名
     * @return 表名
     */
    mc::string_view get_table_name() const override
    {
        return m_name;
    }

    /**
     * 根据查询条件查找单个记录
     * @param builder 查询构建器
     * @return 找到的记录的可选包装
     */
    object_ptr_type find(const query_builder& builder)
    {
        return table_query<table<object_type, IndexDef>>(*this).query_one(builder);
    }

    object_ptr_type find(const condition& cond)
    {
        return find(query_builder(cond));
    }

    object_ptr_type find_object(const query_builder& builder)
    {
        return find(builder);
    }

    object_ptr_type find_object(const condition& cond)
    {
        return find(cond);
    }

    /**
     * 查询记录
     * @param builder 查询构建器
     * @param limit 限制返回的记录数量，0表示不限制
     * @return 查询结果
     */
    std::vector<object_ptr_type> query(const query_builder& builder, size_t limit = 0)
    {
        return table_query<table<object_type, IndexDef>>(*this).query(builder, limit);
    }

    std::vector<object_ptr_type> query_object(const query_builder& builder, size_t limit = 0)
    {
        return query(builder, limit);
    }

    /**
     * 查询记录
     * @param builder 查询构建器
     * @param handler 处理函数，返回false表示停止查询
     * @return 是否查询完成
     */
    template <typename Handler, typename = std::enable_if_t<std::is_invocable_r_v<bool, Handler, object_type&>>>
    bool query(const query_builder& builder, Handler&& handler)
    {
        return table_query<table<object_type, IndexDef>>(*this).query(builder, std::forward<Handler>(handler));
    }

    template <typename Handler, typename = std::enable_if_t<std::is_invocable_r_v<bool, Handler, object_type&>>>
    bool query_object(const query_builder& builder, Handler&& handler)
    {
        return query(builder, std::forward<Handler>(handler));
    }

    std::vector<object_ptr_type> all()
    {
        query_builder builder;
        return table_query<table<object_type, IndexDef>>(*this).query_all(builder);
    }

    /**
     * 高级更新方法，支持通过条件更新多个属性
     * @param condition 查询条件
     * @param values 要更新的值，支持多种数据源
     * @return 更新的记录数量
     */
    size_t update(const query_builder& condition, const mc::dict& values, transaction* txn = nullptr)
    {
        std::lock_guard lock(m_mutex);

        return update_internal(condition, values, txn);
    }

    size_t update(const query_builder& condition, const std::map<mc::string, variant>& values,
                  transaction* txn = nullptr)
    {
        std::lock_guard lock(m_mutex);

        return update_internal(condition, values, txn);
    }

    /**
     * 高级删除方法，支持通过条件删除多个记录
     * @param condition 查询条件
     * @return 删除的记录数量
     */
    size_t remove(const query_builder& condition, transaction* txn = nullptr)
    {
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

    void clear() override
    {
        std::lock_guard lock(m_mutex);

        auto& idx = std::get<0>(m_indices);
        for (auto it = idx.begin(); it != idx.end(); ++it) {
            on_object_removed(const_cast<object_type&>(*it));
        }

        m_engine->clear();
    }

    bool empty() const override
    {
        std::lock_guard lock(m_mutex);

        return std::get<0>(m_indices).empty();
    }

    size_t size() const override
    {
        std::lock_guard lock(m_mutex);

        return std::get<0>(m_indices).size();
    }

protected:
    void commit_internal()
    {
        m_engine->commit();
        m_txn_savepoint_id   = -1;
        m_index_savepoint_id = -1;
    }

    void rollback_internal()
    {
        m_engine->rollback();
        m_txn_savepoint_id   = -1;
        m_index_savepoint_id = -1;
    }

    int alloc_savepoint_internal(transaction* txn)
    {
        auto sp = txn->last_savepoint_id();
        if (m_txn_savepoint_id != sp) {
            m_index_savepoint_id = m_engine->save_point();
            m_txn_savepoint_id   = sp;
        }
        return m_index_savepoint_id;
    }

    object_ptr do_add_object(const mc::dict& var, transaction* txn) override
    {
        if constexpr (mc::reflect::is_reflectable<object_type>() && std::is_constructible_v<object_type>) {
            auto obj = mc::make_shared<object_type>();
            from_variant(var, *obj);
            return add(obj, txn).template static_pointer_cast<object_base>();
        } else {
            MC_UNUSED(txn);
        }

        return nullptr;
    }

private:
    template <size_t I>
    auto& index_ref()
    {
        return std::get<I>(m_indices);
    }

    template <typename Tag>
    auto& index_ref()
    {
        if constexpr (std::is_same_v<Tag, by_object_id_tag>) {
            return std::get<0>(m_indices);
        } else {
            constexpr size_t index = mc::db::detail::tag_index_of<Tag, indices_def>::value + 1;
            return std::get<index>(m_indices);
        }
    }

    object_id_type generate_available_id_internal()
    {
        auto& id_idx = std::get<0>(m_indices);
        for (;;) {
            auto id = generate_id();
            if (!id_idx.contains_key(id)) {
                return id;
            }
        }
    }

    // 从 dict 更新对象
    void update_object(object_type& obj, const dict& values)
    {
        for (auto& entry : values) {
            mc::reflect::set_property(obj, entry.key.get_string(), entry.value);
        }
    }

    // 从 map 更新对象
    void update_object(object_type& obj, const std::map<mc::string, variant>& values)
    {
        for (auto& entry : values) {
            mc::reflect::set_property(obj, entry.first, entry.second);
        }
    }

    template <typename T>
    size_t update_internal(const query_builder& condition, const T& values, transaction* txn)
    {
        size_t updated_count = 0;

        auto handler = [&](const object_type& obj) -> bool {
            if constexpr (mc::traits::is_copyable_v<object_type>) {
                auto new_obj = mc::make_shared<object_type>(obj);
                update_object(*new_obj, values);
                if (update(mc::shared_ptr<object_type>(const_cast<object_type*>(&obj)), new_obj, txn)) {
                    updated_count++;
                }
                return true;
            }
            // 非可复制类型，直接更新对象
            update_object(const_cast<object_type&>(obj), values);
            updated_count++;
            return true;
        };

        query(condition, handler);
        return updated_count;
    }

    /**
     * 根据 IndexDef 选择合适的索引元组构造方式
     */
    indices_tuple_type make_indices()
    {
        using object_id_idx_t = index<object_type, detail::object_id_key<object_type>, true, void, alloc_type, Engine>;
        if constexpr (std::is_same_v<IndexDef, no_indices>) {
            return std::make_tuple(object_id_idx_t(typename object_id_idx_t::table_mode_t{},
                                                   detail::object_id_key<object_type>{}, 0, nullptr));
        } else {
            return detail::user_indices_builder<object_type, indices_def, alloc_type, Engine,
                                                std::make_index_sequence<std::tuple_size_v<indices_def>>>::build();
        }
    }

    mutable std::recursive_mutex m_mutex;
    std::unique_ptr<Engine>      m_engine; ///< per-table 共享存储引擎
    indices_tuple_type           m_indices;
    indices_array_type           m_indices_array;
    uint32_t                     m_table_id = {0}; ///< 表ID
    mc::string                   m_name;           ///< 表名

    int32_t m_txn_savepoint_id   = {-1}; ///< 事务保存点ID
    int32_t m_index_savepoint_id = {-1}; ///< 索引保存点ID
};

} // namespace mc::db

#endif // MC_DATABASE_TABLE_H