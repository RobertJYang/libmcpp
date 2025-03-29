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

#include <mc/database/iterator.h>
#include <mc/database/key.h>
#include <mc/database/key_extractor.h>
#include <mc/exception.h>
#include <mc/im/radix_tree.h>

namespace mc::database {

/**
 * 基础表接口
 * 用于反射表信息
 */
class table_factory {
public:
    virtual ~table_factory() = default;

    /**
     * 获取表类型信息
     * @return 类型信息
     */
    virtual const std::type_info& get_type() const = 0;

    /**
     * 获取表名
     * @return 表名
     */
    virtual std::string_view name() const = 0;
};

/**
 * 降序排序标记类
 */
struct descending_order {
    static constexpr bool value = false;
};

/**
 * 索引实现
 * @tparam object_type 对象类型
 * @tparam KeyExtractor 键提取器类型
 */
template <typename ObjectType, typename KeyExtractor>
class index {
public:
    // 索引相关类型定义
    using key_extractor_type = KeyExtractor;
    using object_type        = ObjectType;

    // radix树配置
    using tree_config   = mc::im::tree_config<object_type*, std::allocator<char>>;
    using tree_type     = mc::im::radix_tree<tree_config>;
    using raw_iterator  = typename tree_type::iterator;
    using txn_type      = mc::im::transaction<tree_config>;
    using self_type     = index<object_type, KeyExtractor>;
    using iterator_type = iterator<self_type>;

    // radix_tree 的倒序实现还有问题，暂时只支持从小到大排序
    static constexpr bool is_sort_great = true;

    /**
     * 构造函数
     * @param name 索引名称
     * @param field_names 索引字段名
     * @param idx_num 索引编号
     * @param extractor 键提取器
     * @param unique 是否唯一索引
     */
    index(std::string name, std::vector<std::string> field_names, int idx_num,
          const key_extractor_type& extractor, bool unique)
        : m_name(std::move(name)), m_field_names(std::move(field_names)), m_idx_num(idx_num),
          m_extractor(extractor), m_unique(unique), m_txn(std::make_unique<txn_type>()) {
        int key_count = static_cast<int>(m_field_names.size());
        m_key.init(key_count, unique);
        m_key1.init(key_count, unique);
    }

    // 实现index_base接口
    std::string_view name() const {
        return m_name;
    }

    const std::vector<std::string>& field_names() const {
        return m_field_names;
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
        make_object_key(m_key, obj);

        auto key_view     = m_key.key();
        auto [_, updated] = m_txn->insert(key_view, const_cast<object_type*>(&obj));

        if (updated) {
            MC_THROW(mc::invalid_arg_exception, "索引键冲突: ${key}", ("key", key_view));
            return false;
        }

        m_txn->commit();
        return true;
    }

    bool update(const object_type& old_obj, const object_type& new_obj) {
        make_object_key(m_key, old_obj);
        make_object_key(m_key1, new_obj);

        auto old_key = m_key.key();
        auto new_key = m_key1.key();

        if (old_key != new_key) {
            auto old = m_txn->remove(old_key);
            if (!old.has_value()) {
                MC_THROW(mc::invalid_arg_exception, "源索引不存在: ${key}", ("key", old_key));
                return false;
            }

            auto update = m_txn->insert(new_key, const_cast<object_type*>(&new_obj));
            if (update.second) {
                MC_THROW(mc::invalid_arg_exception, "新索引键冲突: ${key}", ("key", new_key));
                return false;
            }
        } else {
            auto [it, found] = m_txn->find(old_key);
            if (!found) {
                MC_THROW(mc::invalid_arg_exception, "源索引不存在: ${key}", ("key", old_key));
                return false;
            }
            it->second = const_cast<object_type*>(&new_obj);
        }

        m_txn->commit();
        return true;
    }

    auto remove(const object_type& obj) {
        make_object_key(m_key, obj);

        auto key_view = m_key.key();
        auto it       = m_txn->remove(key_view);
        m_txn->commit();
        return it;
    }

    template <typename... KeyTypes>
    auto remove(const KeyTypes&... keys) {
        make_keys(m_key, keys...);

        auto key_view = m_key.key();
        auto ret      = m_txn->remove(key_view);
        m_txn->commit();
        return ret;
    }

    void clear() {
        m_txn = std::make_unique<txn_type>();
    }

    iterator_type begin() {
        return make_iterator(m_txn->root().begin(), m_key);
    }

    iterator_type end() {
        return make_iterator(m_txn->root().end(), m_key);
    }

    mdb_key* default_key() {
        return &m_key;
    }

private:
    void make_object_key(mdb_key& key, const object_type& obj) {
        make_key_internal(key, obj.get_id(), [&](mdb_key& key) {
            m_extractor.extract_key(key, obj);
        });
    }

    template <typename... KeyTypes>
    void make_keys(mdb_key& key, const KeyTypes&... keys) {
        make_key_internal(key, 0, [&](mdb_key& key) {
            m_extractor.append_key(key, keys...);
        });
    }

    /**
     * 使用键提取器从对象生成键
     * @param key 键对象
     * @param obj 源对象
     */
    template <typename F>
    void make_key_internal(mdb_key& key, uint32_t id, F&& extract_key) {
        key.reset();

        // 如果是唯一索引，直接使用提取器提取键
        if (key.is_unique()) {
            extract_key(key);
            return;
        }

        // 非唯一索引需要添加对象ID作为附加键
        // 先提取普通键
        extract_key(key);

        if (key.len() > 255) {
            MC_THROW(mc::invalid_arg_exception, "非唯一索引Key长度不允许超过255字节");
        }

        // 添加ID作为附加键
        auto* buf = key.buffer();
        if (id != 0) {
            // 根据排序方向处理ID
            if constexpr (!is_sort_great) {
                // 索引从大到小排列时，id也要倒序处理
                buf->write_uint32(~id);
            } else {
                buf->write_uint32(id);
            }
        }
    }

    iterator_type make_iterator(raw_iterator it, const mdb_key& key) {
        return iterator_type(it, key.is_compound_key(), key.is_unique(), key.key().length(),
                             key.key_count(), key.key_num());
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
    std::string               m_name;
    std::vector<std::string>  m_field_names;
    int                       m_idx_num;
    key_extractor_type        m_extractor;
    bool                      m_unique;
    std::unique_ptr<txn_type> m_txn;

    // 缓存的键
    mdb_key m_key;
    mdb_key m_key1;
};

/**
 * 创建内存索引
 * @param name 索引名称
 * @param idx_num 索引编号
 * @param extractor 键提取器
 * @param unique 是否唯一索引
 * @return 索引指针
 */
template <typename ObjectType, typename KeyExtractor>
static auto make_index(std::string name, int idx_num, const KeyExtractor& extractor,
                       bool unique = true) {
    auto field_names = mc::string::split(name, '|');
    return std::make_unique<index<ObjectType, KeyExtractor>>(
        std::move(name), std::move(field_names), idx_num, extractor, unique);
}

} // namespace mc::database

#endif // MC_DATABASE_INDEX_H