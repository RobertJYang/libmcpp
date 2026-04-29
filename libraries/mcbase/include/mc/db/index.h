/*
 * Copyright (c) 2024-2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MC_DATABASE_INDEX_H
#define MC_DATABASE_INDEX_H

#include <mc/db/common.h>
#include <mc/db/index_tag.h>
#include <mc/db/iterator.h>
#include <mc/db/key.h>
#include <mc/db/key_extractor.h>
#include <mc/db/local_storage_engine.h>
#include <mc/db/storage_engine.h>
#include <mc/db/table_base.h>
#include <mc/exception.h>
#include <mc/string_utils.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace mc::db {

/**
 * 索引实现
 *
 * db::index 退化为：
 *   - key_extractor + index_id 元数据
 *   - 通过 m_engine->op(m_index_id, ...) 转发所有读写
 *
 * 兼容 standalone 用法：
 *   - 默认 Engine = local_storage_engine<Object, 1, Allocator>
 *   - 构造时自动创建 owned engine（IndexCount = 1，即一棵 radix_tree）
 *   - 现有 make_index<>() 工厂返回的 unique_ptr<index<...>> 直接可 add/find/...
 *
 * Table 模式：
 *   - db::table 在构造期把所有 index 创建完毕后，调 set_engine(engine, idx_id)
 *     注入外部共享 engine，丢弃 owned 实例
 *
 * @tparam ObjectType   对象类型
 * @tparam KeyExtractor 键提取器
 * @tparam IsUnique     是否唯一索引
 * @tparam Tag          标签类型
 * @tparam Allocator    内存分配器
 * @tparam Engine       存储引擎类型
 */
template <typename ObjectType, typename KeyExtractor, bool IsUnique = true, typename Tag = void,
          typename Allocator = std::allocator<char>, typename Engine = local_storage_engine<ObjectType, 1U, Allocator>>
class index : public index_base<ObjectType, Allocator, Engine> {
    static_assert(std::is_base_of_v<mc::object_base, ObjectType>, "ObjectType必须继承自mc::object_base");

public:
    using key_extractor_type        = KeyExtractor;
    using object_type               = ObjectType;
    using object_ptr_type           = mc::shared_ptr<object_type>;
    using alloc_type                = Allocator;
    using tag_type                  = Tag;
    using engine_type               = Engine;
    static constexpr bool is_unique = IsUnique;

    using raw_iterator  = engine_raw_iterator_t<Engine>;
    using self_type     = index<object_type, KeyExtractor, IsUnique, Tag, Allocator, Engine>;
    using iterator_type = iterator<self_type>;

    static constexpr int  key_count       = key_extractor_type::key_count;
    static constexpr bool is_compound_key = key_extractor_type::is_compound_key;

    // radix_tree 的倒序实现还有问题，暂时只支持从小到大排序
    static constexpr bool is_sort_great = true;

    explicit index(const key_extractor_type& extractor = key_extractor_type(), const alloc_type& alloc = alloc_type(),
                   uint8_t index_id = 0, table_base* table = nullptr)
        : m_extractor(extractor), m_owned_engine(std::make_unique<Engine>(alloc)), m_engine(m_owned_engine.get()),
          m_index_id(index_id), m_table(table)
    {
        m_key.init(key_count, is_unique);
        m_key1.init(key_count, is_unique);
    }

    /**
     * Table 模式构造：不创建 owned engine，等待 set_engine() 注入
     */
    struct table_mode_t {};
    static constexpr table_mode_t table_mode{};

    index(table_mode_t, const key_extractor_type& extractor, uint8_t index_id, table_base* table)
        : m_extractor(extractor), m_owned_engine(nullptr), m_engine(nullptr), m_index_id(index_id), m_table(table)
    {
        m_key.init(key_count, is_unique);
        m_key1.init(key_count, is_unique);
    }

    /**
     * Table 模式：注入外部 engine 并丢弃 owned 实例
     */
    void set_engine(Engine& engine, std::size_t index_id)
    {
        m_owned_engine.reset();
        m_engine   = &engine;
        m_index_id = static_cast<uint8_t>(index_id);
    }

    void set_table(table_base* table)
    {
        m_table = table;
    }

    table_base* get_table() const override
    {
        return m_table;
    }

    iterator_type find(const object_type& obj)
    {
        make_object_key(m_key, obj);
        return find_by_key_internal(m_key);
    }

    bool contains_key(const object_type& obj)
    {
        make_object_key(m_key, obj);
        return m_engine->contains(m_index_id, m_key.key());
    }

    template <typename... KeyType>
    iterator_type find(const KeyType&... keys)
    {
        make_keys(m_key, keys...);
        return find_by_key_internal(m_key);
    }

    template <typename... KeyType>
    bool contains_key(const KeyType&... keys)
    {
        make_keys(m_key, keys...);
        return m_engine->contains(m_index_id, m_key.key());
    }

    template <typename... KeyType>
    std::pair<iterator_type, iterator_type> equal_range(const KeyType&... keys)
    {
        make_keys(m_key, keys...);

        auto key_view = m_key.key();
        auto first    = find_by_key_internal(m_key);
        if (first.is_end()) {
            return std::make_pair(iterator_type(), iterator_type());
        }

        auto second = first.to_next_prefix(key_view);
        return std::make_pair(first, second);
    }

    template <typename... KeyType>
    iterator_type lower_bound(const KeyType&... keys)
    {
        make_keys(m_key, keys...);

        auto first = find_by_key_internal(m_key, true);
        if (first.is_end()) {
            return iterator_type();
        }

        return first;
    }

    bool add(const object_type& obj)
    {
        auto obj_ptr = mc::make_shared<object_type>(obj);
        return add(obj_ptr);
    }

    bool add(object_ptr_type obj_ptr)
    {
        make_object_key(m_key, *obj_ptr);

        auto key_view     = m_key.key();
        auto [_, updated] = m_engine->insert(m_index_id, key_view, obj_ptr);
        if (updated) {
            MC_THROW(mc::invalid_arg_exception, "表 ${table} 索引键冲突: ${key}(${name})",
                     ("table", table_name())("name", index_name())("key", get_key_values(*obj_ptr)));
            return false;
        }
        return true;
    }

    bool update(const object_type& old_obj, const object_type& new_obj)
    {
        auto obj_ptr = mc::make_shared<object_type>(new_obj);
        return update(old_obj, obj_ptr);
    }

    bool update(const object_type& old_obj, object_ptr_type new_obj_ptr)
    {
        make_object_key(m_key, old_obj);
        make_object_key(m_key1, *new_obj_ptr);

        auto old_key = m_key.key();
        auto new_key = m_key1.key();

        if (old_key != new_key) {
            auto old = m_engine->remove(m_index_id, old_key);
            if (!old.has_value()) {
                MC_THROW(mc::invalid_arg_exception, "表 ${table} 源索引不存在: ${key}(${name})",
                         ("table", table_name())("name", index_name())("key", get_key_values(old_obj)));
                return false;
            }

            auto [_, updated] = m_engine->insert(m_index_id, new_key, new_obj_ptr);
            if (updated) {
                MC_THROW(mc::invalid_arg_exception, "表 ${table} 目标索引键冲突: ${key}(${name})",
                         ("table", table_name())("name", index_name())("key", get_key_values(*new_obj_ptr)));
                return false;
            }
        } else {
            auto [_, updated] = m_engine->insert(m_index_id, new_key, new_obj_ptr);
            if (!updated) {
                MC_THROW(mc::invalid_arg_exception, "表 ${table} 源索引不存在: ${key}(${name})",
                         ("table", table_name())("name", index_name())("key", get_key_values(old_obj)));
                return false;
            }
        }

        return true;
    }

    auto remove(const object_type& obj)
    {
        make_object_key(m_key, obj);

        auto key_view = m_key.key();
        return m_engine->remove(m_index_id, key_view);
    }

    template <typename... KeyTypes>
    auto remove(const KeyTypes&... keys)
    {
        make_keys(m_key, keys...);

        auto key_view = m_key.key();
        return m_engine->remove(m_index_id, key_view);
    }

    /**
     * 清空索引：仅清当前 index 对应的子树
     */
    void clear()
    {
        m_engine->clear(m_index_id);
    }

    iterator_type begin()
    {
        return make_iterator(m_engine->begin(m_index_id), m_key);
    }

    iterator_type begin() const
    {
        return iterator_type(m_engine->begin(m_index_id), m_key.key().length(), m_key.key_num());
    }

    iterator_type end()
    {
        return iterator_type();
    }

    void commit()
    {
        m_engine->commit();
    }

    void rollback()
    {
        m_engine->rollback();
    }

    int32_t last_savepoint_id() const
    {
        return m_engine->current_save_point();
    }

    int32_t alloc_save_point()
    {
        return m_engine->save_point();
    }

    void rollback_to(int32_t savepoint_id)
    {
        m_engine->rollback_to(savepoint_id);
    }

    void lock_db()
    {
        m_engine->lock_db();
    }

    void unlock_db()
    {
        m_engine->unlock_db();
    }

    object_ptr_type raw_find(const mc::variant& value) override
    {
        make_keys(m_key, value);

        auto key_view = m_key.key();
        if constexpr (is_unique) {
            auto it = m_engine->find(m_index_id, key_view);
            if (it == m_engine->end(m_index_id)) {
                return nullptr;
            }

            return it->second;
        }

        raw_iterator it = m_engine->lower_bound(m_index_id, key_view);
        make_object_key(m_key1, *it->second);
        if (it->first != m_key1.key()) {
            raw_iterator();
        }

        return it->second;
    }

    raw_iterator raw_lower_bound(const mc::variant& value) override
    {
        make_keys(m_key, value);

        auto key_view = m_key.key();
        return m_engine->lower_bound(m_index_id, key_view);
    }

    raw_iterator raw_upper_bound(const mc::variant& value) override
    {
        make_keys(m_key, value);

        auto key_view = m_key.key();
        auto it       = m_engine->upper_bound(m_index_id, key_view);

        if (it == m_engine->end(m_index_id)) {
            return raw_iterator();
        }

        if constexpr (is_unique) {
            return it;
        }

        // 非唯一索引，key 后面会拼接对象 id，所以需要遍历到 key 不匹配为止
        for (; !it.is_end(); ++it) {
            if (!mc::im::has_prefix(it->first, key_view)) {
                return it;
            }
        }

        return it;
    }

    raw_iterator raw_begin() const override
    {
        return m_engine->begin(m_index_id);
    }

    bool empty() const
    {
        return m_engine->empty(m_index_id);
    }

    size_t size() const
    {
        return m_engine->size(m_index_id);
    }

    mc::string index_name() const
    {
        if constexpr (is_field_tag_v<Tag>) {
            return mc::strings::join(Tag::get_field_names(), ",");
        } else {
            auto names = m_extractor.get_field_names();
            if (names.empty()) {
                return "index_" + mc::to_string(m_index_id);
            }

            return mc::strings::join(names, ",");
        }
    }

    mc::string_view table_name() const
    {
        return m_table ? m_table->get_table_name() : "anonymous_table";
    }

    mc::variant get_key_values(const object_type& obj)
    {
        return m_extractor(obj);
    }

private:
    void make_object_key(mdb_key& key, const object_type& obj)
    {
        make_key_internal<is_unique>(key, obj.get_object_id(), [&](mdb_key& key) {
            m_extractor.extract_key(key, obj);
        });
    }

    template <typename... KeyTypes>
    void make_keys(mdb_key& key, const KeyTypes&... keys)
    {
        make_key_internal<is_unique>(key, 0, [&](mdb_key& key) {
            m_extractor.append_key(key, keys...);
        });
    }

    void make_keys(mdb_key& key, const mc::variant& value)
    {
        make_key_internal<is_unique>(key, 0, [&](mdb_key& key) {
            m_extractor.append_variant(key, value);
        });
    }

    template <bool KeyIsUnique = false, typename F>
    void make_key_internal(mdb_key& key, object_id_type id, F&& extract_key)
    {
        key.reset();

        if constexpr (KeyIsUnique) {
            extract_key(key);
            return;
        }

        extract_key(key);

        if (key.len() > 255) {
            MC_THROW(mc::invalid_arg_exception, "非唯一索引Key长度不允许超过255字节");
        }

        if (id != 0) {
            if constexpr (!is_sort_great) {
                key.write_value(~id); // 索引从大到小排列时，id也要倒序处理
            } else {
                key.write_value(id);
            }
        }
    }

    iterator_type make_iterator(raw_iterator it, const mdb_key& key)
    {
        return iterator_type(it, key.key().length(), key.key_num());
    }

    iterator_type find_by_key_internal(const mdb_key& key, bool is_lower_bound = false)
    {
        auto key_view = key.key();
        auto it       = m_engine->lower_bound(m_index_id, key_view);

        if (it == m_engine->end(m_index_id) || !mc::im::has_prefix(it->first, key_view)) {
            return iterator_type();
        }

        if (is_lower_bound) {
            return make_iterator(it, mdb_key());
        }

        return make_iterator(it, key);
    }

    key_extractor_type      m_extractor;
    std::unique_ptr<Engine> m_owned_engine; // standalone 模式持有；table 模式 set_engine 后置 null
    Engine*                 m_engine{nullptr};
    uint8_t                 m_index_id;
    table_base*             m_table{nullptr};

    mdb_key m_key;
    mdb_key m_key1;
};

/**
 * 创建独立索引（standalone 模式）
 *
 * 返回的 index 自带 owned engine（IndexCount = 1），可直接 add/find/...
 * 适用于 mcbase test_index 等单测场景。生产代码应使用 db::table。
 */
template <typename ObjectType, bool IsUnique, typename KeyExtractor, typename Tag = void,
          typename Alloc = std::allocator<ObjectType>>
static auto make_index(const KeyExtractor& extractor = KeyExtractor(), const Alloc& alloc = Alloc())
{
    return std::make_unique<index<ObjectType, KeyExtractor, IsUnique, Tag, Alloc>>(extractor, alloc);
}

} // namespace mc::db

#endif // MC_DATABASE_INDEX_H
