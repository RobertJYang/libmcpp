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

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

#include <mc/im/radix_tree.h>

namespace mc::database {

/**
 * 索引迭代器
 * 用于遍历索引中的数据
 * @tparam RadixIterator 底层radix树迭代器类型
 */
template <typename Index>
class iterator {
public:
    // 标准迭代器类型定义
    using iterator_category = std::forward_iterator_tag;
    using object_type       = typename Index::object_type;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const object_type*;
    using reference         = const object_type&;
    using raw_iterator      = typename Index::raw_iterator;

    /**
     * 默认构造函数 - 生成end迭代器
     */
    iterator() : m_is_end(true) {
    }

    /**
     * 构造函数
     * @param iter 底层radix树迭代器
     * @param is_compound_key 是否是复合键
     * @param is_unique 是否是唯一键
     * @param prefix_len 前缀长度
     * @param field_count 字段数量
     * @param key_field_count 键字段数量
     * @param is_sort_great 是否是大到小排序
     */
    iterator(raw_iterator iter, bool is_compound_key, bool is_unique, size_t prefix_len,
             size_t field_count, size_t key_field_count, bool is_sort_great)
        : m_iterator(std::move(iter)), m_is_end(false), m_is_compound_key(is_compound_key),
          m_is_unique(is_unique), m_prefix_len(prefix_len), m_field_count(field_count),
          m_key_field_count(key_field_count), m_is_sort_great(is_sort_great) {
        if (m_iterator == raw_iterator()) {
            m_is_end = true;
            return;
        }

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
        return m_iterator->second;
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
    iterator to_next_prefix(std::string_view key) {
        if (m_is_end) {
            return *this;
        }

        auto next_it       = m_iterator;
        next_it.to_next_prefix(key);
        return iterator(next_it, m_is_compound_key, m_is_unique, m_prefix_len, m_field_count,
                        m_key_field_count, m_is_sort_great);
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

        auto key = m_iterator.key();
        auto n   = key.length();
        // 前缀完全匹配或没有前缀要求
        if (n == m_prefix_len || m_prefix_len == 0) {
            return true;
        }

        if (n > m_prefix_len) {
            // 处理非唯一键的情况
            if (!m_is_unique) {
                // 检查键长度（确保有足够空间存储ID）
                if (n - m_prefix_len < 4) {
                    return false;
                }

                // 非组合键必须精确匹配长度
                if (!m_is_compound_key && m_prefix_len + 4 != n) {
                    return false;
                }

                // 提取并检查ID
                uint32_t id = 0;
                memcpy(&id, key.data() + (n - 4), 4);
                if (m_is_sort_great) {
                    // 索引从大到小排列时，id需要转换回来
                    id = ~id;
                }

                if (m_iterator->second->get_id() != id) {
                    return false;
                }

                if (!m_is_compound_key) {
                    return true;
                }

                n   = n - 4;
                key = std::string_view(key.data(), n);
            }

            // 处理组合键的情况
            if (m_is_compound_key) {
                size_t field_count = 0;
                size_t i           = m_prefix_len;

                // 解析子键
                while (i < n) {
                    i += static_cast<uint8_t>(key[i]) + 1;
                    field_count++;
                }

                // 检查字段数量是否符合预期
                if (i == n && field_count + m_key_field_count == m_field_count) {
                    return true;
                }
            }
        }

        return false;
    }

    raw_iterator m_iterator;
    bool         m_is_end;
    bool         m_is_compound_key{false};
    bool         m_is_unique{false};
    size_t       m_prefix_len{0};
    size_t       m_field_count{0};
    size_t       m_key_field_count{0};
    bool         m_is_sort_great{false};

    // 声明mem_index为友元类
    template <typename O, typename K, bool L>
    friend class index;
};

} // namespace mc::database

#endif // MC_DATABASE_ITERATOR_H