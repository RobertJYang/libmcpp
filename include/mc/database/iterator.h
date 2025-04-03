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

#ifndef MC_DATABASE_ITERATOR_H
#define MC_DATABASE_ITERATOR_H

#include <iterator>
#include <mc/exception.h>
#include <string_view>
#include <type_traits>

namespace mc::database {

// 前向声明
template <typename ObjectType, typename KeyExtractor, bool IsUnique, typename Tag>
class index;

/**
 * 索引迭代器
 * 用于遍历索引中的数据
 * @tparam IndexType 索引类型
 */
template <typename IndexType>
class iterator {
public:
    using object_type    = typename IndexType::object_type;
    using raw_iterator   = typename IndexType::raw_iterator;
    using key_type       = typename IndexType::key_extractor_type;
    using object_id_type = typename IndexType::object_id_type;
    using value_type     = object_type;
    using pointer        = const object_type*;
    using reference      = const object_type&;

    static constexpr bool is_sort_great   = IndexType::is_sort_great;
    static constexpr bool is_unique       = IndexType::is_unique;
    static constexpr bool is_compound_key = IndexType::is_compound_key;
    static constexpr int  field_count     = IndexType::key_count;

    /**
     * 构造函数
     * @param it 原始迭代器
     * @param prefix_len 前缀长度
     * @param key_field_count 键字段数量
     */
    explicit iterator(raw_iterator it = raw_iterator(), size_t prefix_len = 0,
                      size_t key_field_count = 0)
        : m_iterator(it), m_prefix_len(prefix_len), m_key_field_count(key_field_count),
          m_is_end(it == raw_iterator()) {
        if (!do_next()) {
            m_is_end = true;
        }
    }

    // 标准迭代器接口

    // 前置递增
    iterator& operator++() {
        if (m_is_end) {
            return *this;
        }

        ++m_iterator;
        if (!do_next()) {
            m_is_end = true;
        }

        return *this;
    }

    // 后置递增
    iterator operator++(int) {
        iterator tmp = *this;
        ++(*this);
        return tmp;
    }

    // 解引用
    reference operator*() const {
        if (m_is_end) {
            throw std::out_of_range("迭代器指向末尾");
        }
        return *m_iterator->second;
    }

    // 成员访问
    pointer operator->() const {
        if (m_is_end) {
            throw std::out_of_range("迭代器指向末尾");
        }
        return &*m_iterator->second;
    }

    // 相等比较
    bool operator==(const iterator& other) const {
        if (m_is_end && other.m_is_end) {
            return true;
        }

        if (m_is_end != other.m_is_end) {
            return false;
        }

        return m_iterator == other.m_iterator;
    }

    // 不等比较
    bool operator!=(const iterator& other) const {
        return !(*this == other);
    }

    /**
     * 检查是否已到达末尾
     * @return 如果到达末尾则返回true
     */
    bool is_end() const {
        return m_is_end;
    }

    /**
     * 获取当前对象
     * @return 当前对象
     */
    reference get() const {
        return *m_iterator->second;
    }

    /**
     * 获取键视图
     * @return 键视图
     */
    std::string_view key() const {
        if (m_is_end) {
            return {};
        }
        return m_iterator.key();
    }

    // 跳到下一个前缀的位置，用于equal_range场景只给定部分key时使用
    iterator to_next_prefix(std::string_view key_view) {
        if (m_is_end) {
            return *this;
        }

        auto next_it = m_iterator;
        next_it.to_next_prefix(key_view);
        return iterator(next_it, m_prefix_len, m_key_field_count);
    }

private:
    /**
     * 获取下一个匹配条件的对象
     * 实现与Go版doNext类似的逻辑
     * @return 下一个对象指针，如果没有则返回nullptr
     */
    bool do_next() {
        if (m_iterator == raw_iterator()) {
            return false;
        }

        std::string_view key = m_iterator.key();
        size_t           n   = key.length();
        // 前缀完全匹配或没有前缀要求
        if (n == m_prefix_len || m_prefix_len == 0) {
            return true;
        }

        if (n < m_prefix_len) {
            return false;
        }

        // 处理非唯一键的情况
        if constexpr (!is_unique) {
            // 检查键长度（确保有足够空间存储ID）
            if (n - m_prefix_len < 4) {
                return false;
            }

            if constexpr (!is_compound_key) {
                return check_unique_key<is_compound_key>(key, n);
            } else {
                if (!check_unique_key<is_compound_key>(key, n)) {
                    return false;
                }
            }
        }

        // 处理组合键的情况
        if constexpr (is_compound_key) {
            if (check_compound_key<is_compound_key>(key, n)) {
                return true;
            }
        }

        return false;
    }

    template <bool IsCompound>
    bool check_compound_key(std::string_view key, int n);

    template <>
    bool check_compound_key<true>(std::string_view key, int n) {
        size_t count = 0;
        size_t i     = m_prefix_len;

        // 解析子键
        while (i < n) {
            i += static_cast<uint8_t>(key[i]) + 1;
            count++;
        }

        // 检查字段数量是否符合预期
        if (i == n && count + m_key_field_count == field_count) {
            return true;
        }

        return false;
    }

    template <bool IsCompound>
    bool check_unique_key(std::string_view& key, size_t& n);

    template <>
    bool check_unique_key<true>(std::string_view& key, size_t& n) {
        if (!check_id(key.data() + n - sizeof(object_id_type))) {
            return false;
        }

        n   = n - sizeof(object_id_type);
        key = key.substr(0, n);
        return true;
    }

    template <>
    bool check_unique_key<false>(std::string_view& key, size_t& n) {
        // 非组合键必须精确匹配长度
        if (m_prefix_len + sizeof(object_id_type) != n) {
            return false;
        }

        return check_id(key.data() + n - sizeof(object_id_type));
    }

    bool check_id(const void* p_id) const {
        object_id_type id = *static_cast<const object_id_type*>(p_id);
        if constexpr (!is_sort_great) {
            id = ~id; // 索引从大到小排列时，id需要转换回来
        }
        id = mc::ntoh(id);
        return m_iterator->second->get_object_id() == id;
    }

    raw_iterator m_iterator;
    size_t       m_prefix_len;
    size_t       m_key_field_count;
    bool         m_is_end;

    // 声明mem_index为友元类
    template <typename, typename, bool, typename>
    friend class index;
};

} // namespace mc::database

#endif // MC_DATABASE_ITERATOR_H