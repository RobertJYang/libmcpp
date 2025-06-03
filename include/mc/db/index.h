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

#ifndef MC_DATABASE_INDEX_H
#define MC_DATABASE_INDEX_H

#include <mc/db/common.h>
#include <mc/db/index_tag.h>
#include <mc/db/iterator.h>
#include <mc/db/key.h>
#include <mc/db/key_extractor.h>
#include <mc/db/table_base.h>
#include <mc/exception.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace mc::db {

/**
 * 索引实现
 * @tparam ObjectType 对象类型
 * @tparam KeyExtractor 键提取器类型
 * @tparam IsUnique 是否唯一索引
 * @tparam Tag 标签类型
 */
template <typename ObjectType, typename KeyExtractor, bool IsUnique = true, typename Tag = void,
          typename Allocator = std::allocator<char>>
class index : public index_base<ObjectType, Allocator> {
    static_assert(std::is_base_of_v<object_base, ObjectType>, "ObjectType必须继承自object_base");

public:
    // 索引相关类型定义
    using key_extractor_type        = KeyExtractor;
    using object_type               = ObjectType;
    using object_ptr_type           = mc::ref_ptr<object_type>;
    using alloc_type                = Allocator;
    using tag_type                  = Tag;
    static constexpr bool is_unique = IsUnique;

    // radix树配置
    using tree_config   = mc::im::tree_config<object_ptr_type, alloc_type>;
    using tree_type     = mc::im::radix_tree<tree_config>;
    using raw_iterator  = typename tree_type::iterator;
    using txn_type      = mc::im::transaction<tree_config>;
    using self_type     = index<object_type, KeyExtractor, IsUnique, Tag, Allocator>;
    using iterator_type = iterator<self_type>;

    static constexpr int  key_count       = key_extractor_type::key_count;
    static constexpr bool is_compound_key = key_extractor_type::is_compound_key;

    // radix_tree 的倒序实现还有问题，暂时只支持从小到大排序
    static constexpr bool is_sort_great = true;

    /**
     * 构造函数
     * @param extractor 键提取器
     * @param alloc 内存分配器
     */
    explicit index(const key_extractor_type& extractor = key_extractor_type(),
                   const alloc_type&         alloc     = alloc_type(),
                   uint8_t                   index_id  = 0,
                   table_base*               table     = nullptr)
        : m_extractor(extractor), m_txn(std::make_unique<txn_type>(alloc)),
          m_index_id(index_id), m_table(table) {
        m_key.init(key_count, is_unique);
        m_key1.init(key_count, is_unique);
    }

    void set_table(table_base* table) {
        m_table = table;
    }

    table_base* get_table() const override {
        return m_table;
    }

    iterator_type find(const object_type& obj) {
        make_object_key(m_key, obj);
        return find_by_key_internal(m_key);
    }

    template <typename... KeyType>
    iterator_type find(const KeyType&... keys) {
        make_keys(m_key, keys...);
        return find_by_key_internal(m_key);
    }

    template <typename... KeyType>
    std::pair<iterator_type, iterator_type> equal_range(const KeyType&... keys) {
        make_keys(m_key, keys...);

        auto key_view = m_key.key();
        auto first    = find_by_key_internal(m_key);
        if (first.is_end()) {
            return std::make_pair(iterator_type(), iterator_type());
        }

        auto second = first.to_next_prefix(key_view);
        return std::make_pair(first, second);
    }

    /**
     * 返回大于等于给定键的第一个元素的迭代器
     * @param key 键
     * @return 迭代器
     */
    template <typename... KeyType>
    iterator_type lower_bound(const KeyType&... keys) {
        make_keys(m_key, keys...);

        auto first = find_by_key_internal(m_key, true);
        if (first.is_end()) {
            return iterator_type();
        }

        return first;
    }

    bool add(const object_type& obj) {
        auto obj_ptr = mc::make_ref<object_type>(obj);
        return add(obj_ptr);
    }

    bool add(object_ptr_type obj_ptr) {
        make_object_key(m_key, *obj_ptr);

        auto key_view     = m_key.key();
        auto [_, updated] = m_txn->insert(key_view, obj_ptr);

        if (updated) {
            MC_THROW(mc::invalid_arg_exception, "表 ${table} 索引键冲突: ${key}(${name})",
                     ("table", table_name())("name", index_name())("key", get_key_values(*obj_ptr)));
            return false;
        }

        m_tree = m_txn->root();
        return true;
    }

    bool update(const object_type& old_obj, const object_type& new_obj) {
        auto obj_ptr = mc::make_ref<object_type>(new_obj);
        return update(old_obj, obj_ptr);
    }

    bool update(const object_type& old_obj, object_ptr_type new_obj_ptr) {
        make_object_key(m_key, old_obj);
        make_object_key(m_key1, *new_obj_ptr);

        auto old_key = m_key.key();
        auto new_key = m_key1.key();

        if (old_key != new_key) {
            auto old = m_txn->remove(old_key);
            if (!old.has_value()) {
                MC_THROW(mc::invalid_arg_exception, "表 ${table} 源索引不存在: ${key}(${name})",
                         ("table", table_name())("name", index_name())("key", get_key_values(old_obj)));
                return false;
            }

            auto [_, update] = m_txn->insert(new_key, new_obj_ptr);
            if (update) {
                MC_THROW(mc::invalid_arg_exception, "表 ${table} 目标索引键冲突: ${key}(${name})",
                         ("table", table_name())("name", index_name())("key", get_key_values(*new_obj_ptr)));
                return false;
            }
        } else {
            auto [_, update] = m_txn->insert(new_key, new_obj_ptr);
            if (!update) {
                MC_THROW(mc::invalid_arg_exception, "表 ${table} 源索引不存在: ${key}(${name})",
                         ("table", table_name())("name", index_name())("key", get_key_values(old_obj)));
                return false;
            }
        }

        m_tree = m_txn->root();
        return true;
    }

    auto remove(const object_type& obj) {
        make_object_key(m_key, obj);

        auto key_view = m_key.key();
        auto it       = m_txn->remove(key_view);
        m_tree        = m_txn->root();
        return it;
    }

    template <typename... KeyTypes>
    auto remove(const KeyTypes&... keys) {
        make_keys(m_key, keys...);

        auto key_view = m_key.key();
        auto ret      = m_txn->remove(key_view);
        m_tree        = m_txn->root();
        return ret;
    }

    /**
     * 清空索引
     */
    void clear() {
        m_txn  = std::make_unique<txn_type>(alloc_type());
        m_tree = m_txn->root();
    }

    /**
     * 获取索引的起始迭代器
     * @return 起始迭代器
     */
    iterator_type begin() {
        return make_iterator(m_txn->root().begin(), m_key);
    }

    /**
     * 获取索引的起始迭代器（常量版本）
     * @return 起始迭代器
     */
    iterator_type begin() const {
        return iterator_type(m_txn->root().begin(), m_key.key().length(), m_key.key_num());
    }

    /**
     * 获取索引的结束迭代器
     * @return 结束迭代器
     */
    iterator_type end() {
        return iterator_type();
    }

    /**
     * 提交当前事务
     */
    void commit() {
        m_txn->commit();
        m_tree = m_txn->root();
    }

    /**
     * 回滚当前事务
     */
    void rollback() {
        m_txn->rollback();
        m_tree = m_txn->root();
    }

    int32_t last_savepoint_id() const {
        return m_txn->current_save_point();
    }

    int32_t alloc_save_point() {
        return m_txn->save_point();
    }

    void rollback_to(int32_t savepoint_id) {
        m_txn->rollback(savepoint_id);
    }

    void lock_db() {
        m_txn->lock_db();
    }

    void unlock_db() {
        m_txn->unlock_db();
    }

    object_ptr_type raw_find(const mc::variant& value) override {
        make_keys(m_key, value);

        auto& root     = m_txn->root();
        auto  key_view = m_key.key();
        if constexpr (is_unique) {
            auto it = root.find(key_view);
            if (it == root.end()) {
                return nullptr;
            }

            return it->second;
        }

        raw_iterator it = root.lower_bound(key_view);
        make_object_key(m_key1, *it->second);
        if (it->first != m_key1.key()) {
            raw_iterator();
        }

        return it->second;
    }

    raw_iterator raw_lower_bound(const mc::variant& value) override {
        make_keys(m_key, value);

        auto& root     = m_txn->root();
        auto  key_view = m_key.key();
        auto  it       = root.lower_bound(key_view);

        return it;
    }

    raw_iterator raw_upper_bound(const mc::variant& value) override {
        make_keys(m_key, value);

        auto& root     = m_txn->root();
        auto  key_view = m_key.key();
        auto  it       = root.upper_bound(key_view);

        if (it == root.end()) {
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

    raw_iterator raw_begin() const override {
        return m_txn->root().begin();
    }

    bool empty() const {
        return m_txn->root().empty();
    }

    size_t size() const {
        return m_txn->root().size();
    }

    std::string index_name() const {
        if constexpr (is_field_tag_v<Tag>) {
            return mc::string::join(Tag::get_field_names(), ",");
        } else {
            auto names = m_extractor.get_field_names();
            if (names.empty()) {
                return "index_" + std::to_string(m_index_id);
            }

            return mc::string::join(names, ",");
        }
    }

    std::string_view table_name() const {
        return m_table ? m_table->get_table_name() : "anonymous_table";
    }

    mc::variant get_key_values(const object_type& obj) {
        return m_extractor(obj);
    }

private:
    void make_object_key(mdb_key& key, const object_type& obj) {
        make_key_internal<is_unique>(key, obj.get_object_id(), [&](mdb_key& key) {
            m_extractor.extract_key(key, obj);
        });
    }

    template <typename... KeyTypes>
    void make_keys(mdb_key& key, const KeyTypes&... keys) {
        make_key_internal<is_unique>(key, 0, [&](mdb_key& key) {
            m_extractor.append_key(key, keys...);
        });
    }

    void make_keys(mdb_key& key, const mc::variant& value) {
        make_key_internal<is_unique>(key, 0, [&](mdb_key& key) {
            m_extractor.append_variant(key, value);
        });
    }

    /**
     * 使用键提取器从对象生成键
     * @param key 键对象
     * @param obj 源对象
     */
    template <bool KeyIsUnique = false, typename F>
    void make_key_internal(mdb_key& key, object_id_type id, F&& extract_key) {
        key.reset();

        // 如果是唯一索引，直接使用提取器提取键
        if constexpr (KeyIsUnique) {
            extract_key(key);
            return;
        }

        // 非唯一索引需要添加对象ID作为附加键
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

    iterator_type make_iterator(raw_iterator it, const mdb_key& key) {
        return iterator_type(it, key.key().length(), key.key_num());
    }

    /**
     * 内部查找实现
     * @param key 键
     * @return 迭代器
     */
    iterator_type find_by_key_internal(const mdb_key& key, bool is_lower_bound = false) {
        auto& root     = m_txn->root();
        auto  key_view = key.key();
        auto  it       = root.lower_bound(key_view);

        if (it == root.end() || !mc::im::has_prefix(it->first, key_view)) {
            return iterator_type();
        }

        if (is_lower_bound) {
            return make_iterator(it, mdb_key());
        }

        return make_iterator(it, key);
    }

    // 索引基本属性
    key_extractor_type        m_extractor;
    std::unique_ptr<txn_type> m_txn;
    tree_type                 m_tree;
    uint8_t                   m_index_id;
    table_base*               m_table{nullptr};

    // 缓存的键
    mdb_key m_key;
    mdb_key m_key1;
};

/**
 * 创建内存索引
 * @param extractor 键提取器
 * @param alloc 内存分配器
 * @tparam IsUnique 是否唯一索引
 * @tparam Tag 标签类型
 * @return 索引指针
 */
template <typename ObjectType, bool IsUnique, typename KeyExtractor, typename Tag = void,
          typename Alloc = std::allocator<ObjectType>>
static auto make_index(const KeyExtractor& extractor = KeyExtractor(),
                       const Alloc&        alloc     = Alloc()) {
    return std::make_unique<index<ObjectType, KeyExtractor, IsUnique, Tag, Alloc>>(extractor,
                                                                                   alloc);
}

} // namespace mc::db

#endif // MC_DATABASE_INDEX_H