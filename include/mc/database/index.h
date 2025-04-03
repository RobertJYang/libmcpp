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

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <vector>

#include <mc/database/index_tag.h>
#include <mc/database/iterator.h>
#include <mc/database/key.h>
#include <mc/database/key_extractor.h>
#include <mc/database/object.h>
#include <mc/exception.h>
#include <mc/im/radix_tree.h>

namespace mc::database {

/**
 * 索引实现
 * @tparam ObjectType 对象类型
 * @tparam KeyExtractor 键提取器类型
 * @tparam IsUnique 是否唯一索引
 * @tparam Tag 标签类型
 */
template <typename ObjectType, typename KeyExtractor, bool IsUnique = true, typename Tag = void>
class index {
    static_assert(std::is_base_of_v<object_base<ObjectType>, ObjectType>,
                  "ObjectType必须继承自object_base");

public:
    // 索引相关类型定义
    using key_extractor_type        = KeyExtractor;
    using object_type               = ObjectType;
    using object_ptr_type           = mc::im::ref_ptr<object_type>;
    using alloc_type                = typename ObjectType::alloc_type;
    using tag_type                  = Tag;
    static constexpr bool is_unique = IsUnique;

    // radix树配置
    using tree_config    = mc::im::tree_config<object_ptr_type, alloc_type>;
    using tree_type      = mc::im::radix_tree<tree_config>;
    using raw_iterator   = typename tree_type::iterator;
    using txn_type       = mc::im::transaction<tree_config>;
    using self_type      = index<object_type, KeyExtractor, IsUnique, Tag>;
    using iterator_type  = iterator<self_type>;
    using object_id_type = typename ObjectType::object_id_type;

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
                   const alloc_type&         alloc     = alloc_type())
        : m_extractor(extractor), m_txn(std::make_unique<txn_type>(alloc)) {
        m_key.init(key_count, is_unique);
        m_key1.init(key_count, is_unique);
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

    bool add(const object_type& obj) {
        auto obj_ptr = object_type::create(obj);
        return add(obj_ptr);
    }

    bool add(object_ptr_type obj_ptr) {
        make_object_key(m_key, *obj_ptr);

        auto key_view     = m_key.key();
        auto [_, updated] = m_txn->insert(key_view, obj_ptr);

        if (updated) {
            MC_THROW(mc::invalid_arg_exception, "索引键冲突: ${key}", ("key", key_view));
            return false;
        }

        m_tree = m_txn->root();
        return true;
    }

    bool update(const object_type& old_obj, const object_type& new_obj) {
        auto obj_ptr = object_type::create(new_obj);
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
                MC_THROW(mc::invalid_arg_exception, "源索引不存在: ${key}", ("key", old_key));
                return false;
            }

            auto [_, update] = m_txn->insert(new_key, new_obj_ptr);
            if (update) {
                MC_THROW(mc::invalid_arg_exception, "目标索引键冲突: ${key}", ("key", new_key));
                return false;
            }
        } else {
            auto [_, update] = m_txn->insert(new_key, new_obj_ptr);
            if (!update) {
                MC_THROW(mc::invalid_arg_exception, "源索引不存在: ${key}", ("key", old_key));
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
        m_txn->clear();
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
    iterator_type find_by_key_internal(const mdb_key& key) {
        auto& root     = m_txn->root();
        auto  key_view = key.key();
        auto  it       = root.lower_bound(key_view);

        if (it == root.end() || !mc::im::has_prefix(it->first, key_view)) {
            return iterator_type();
        }

        return make_iterator(it, key);
    }

    // 索引基本属性
    key_extractor_type        m_extractor;
    std::unique_ptr<txn_type> m_txn;
    tree_type                 m_tree;

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
    return std::make_unique<index<ObjectType, KeyExtractor, IsUnique, Tag>>(extractor, alloc);
}

} // namespace mc::database

#endif // MC_DATABASE_INDEX_H