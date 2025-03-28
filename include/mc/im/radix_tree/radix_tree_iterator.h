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

#ifndef MC_IM_RADIX_TREE_ITERATOR_H
#define MC_IM_RADIX_TREE_ITERATOR_H

#include <cstdint>
#include <utility>
#include <vector>

namespace mc::im {

// 前置声明
template <typename Config>
class radix_tree;

template <typename Config>
class radix_tree<Config>::const_iterator {
public:
    // 标准迭代器类型定义
    using iterator_category = std::forward_iterator_tag;
    using value_type        = const_map_value_type;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const value_type*;
    using reference         = const value_type&;

    // 默认构造函数（end迭代器）
    const_iterator() : m_is_end(true) {
    }

    // 构造函数（begin迭代器）
    explicit const_iterator(const node_ptr& root) : m_is_end(false) {
        if (!root) {
            m_is_end = true;
            return;
        }

        auto* n      = root.get();
        m_key_buffer = n->m_prefix;
        m_path.push_back({n, 0, 0});
        if (n->m_leaf) {
            m_current_node = n;
            update_current_item();
        } else if (advance_to_next_leaf()) {
            update_current_item();
        } else {
            m_is_end = true;
        }
    }

    // 复制构造函数
    const_iterator(const const_iterator& other) {
        m_is_end       = other.m_is_end;
        m_current_node = other.m_current_node;
        m_key_buffer   = other.m_key_buffer;
        m_path         = other.m_path;
        update_current_item();
    }

    // 复制赋值运算符
    const_iterator& operator=(const const_iterator& other) {
        if (this != &other) {
            m_is_end       = other.m_is_end;
            m_current_node = other.m_current_node;
            m_key_buffer   = other.m_key_buffer;
            m_path         = other.m_path;
            update_current_item();
        }
        return *this;
    }

    // 移动构造函数
    const_iterator(const_iterator&& other) noexcept {
        m_is_end       = other.m_is_end;
        m_current_node = other.m_current_node;
        m_key_buffer   = std::move(other.m_key_buffer);
        m_path         = std::move(other.m_path);
        update_current_item();

        other.m_is_end       = true;
        other.m_current_node = nullptr;
    }

    // 移动赋值运算符
    const_iterator& operator=(const_iterator&& other) noexcept {
        if (this != &other) {
            m_is_end       = other.m_is_end;
            m_current_node = other.m_current_node;
            m_key_buffer   = std::move(other.m_key_buffer);
            m_path         = std::move(other.m_path);
            update_current_item();

            other.m_is_end       = true;
            other.m_current_node = nullptr;
        }
        return *this;
    }

    // 前置递增
    const_iterator& operator++() {
        if (m_is_end || m_path.empty()) {
            m_is_end = true;
            return *this;
        }

        m_current_node = nullptr;
        if (advance_to_next_leaf()) {
            update_current_item();
        } else {
            m_is_end = true;
        }

        return *this;
    }

    // 后置递增
    const_iterator operator++(int) {
        const_iterator tmp = *this;
        ++(*this);
        return tmp;
    }

    // 解引用操作符
    reference operator*() const {
        if (m_is_end) {
            throw std::out_of_range("迭代器指向了末尾");
        }
        return m_current_item;
    }

    // 箭头操作符
    pointer operator->() const {
        if (m_is_end) {
            throw std::out_of_range("迭代器指向了末尾");
        }
        return &m_current_item;
    }

    // 相等比较操作符
    bool operator==(const const_iterator& other) const {
        if (m_is_end && other.m_is_end) {
            return true;
        }

        if (m_is_end != other.m_is_end) {
            return false;
        }

        return m_current_node == other.m_current_node;
    }

    // 不等比较操作符
    bool operator!=(const const_iterator& other) const {
        return !(*this == other);
    }

    // 跳到下一个前缀
    void to_next_prefix(std::string_view key) {
        if (m_is_end || !mc::im::has_prefix(m_key_buffer, key)) {
            return;
        }

        while (!m_path.empty()) {
            auto& current = m_path.back();

            if (current.edge_index >= current.node->m_edges.size()) {
                m_key_buffer.resize(current.prefix_size);
                m_path.pop_back();
                continue;
            }

            // 处理下一个边
            const auto& edge = current.node->m_edges[current.edge_index++];
            m_key_buffer.resize(current.prefix_size);
            m_key_buffer.append(edge.m_node->m_prefix);
            if (mc::im::has_prefix(m_key_buffer, key)) {
                m_key_buffer.resize(current.prefix_size);
                m_path.pop_back();
                continue;
            }

            m_path.push_back({edge.m_node.get(), 0, m_key_buffer.size()});
            if (edge.m_node->is_leaf()) {
                m_current_node = edge.m_node.get();
                update_current_item();
                return;
            }
        }
        m_is_end = true;
    }

    friend class radix_tree<Config>; // 添加友元声明，允许radix_tree访问protected成员

protected:
    // 前进到下一个叶子节点
    bool advance_to_next_leaf() {
        while (!m_path.empty()) {
            auto& current = m_path.back();

            if (current.edge_index >= current.node->m_edges.size()) {
                m_key_buffer.resize(current.prefix_size);
                m_path.pop_back();
                continue;
            }

            // 处理下一个边
            const auto& edge = current.node->m_edges[current.edge_index++];
            m_key_buffer.resize(current.prefix_size);
            m_key_buffer.append(edge.m_node->m_prefix);
            m_path.push_back({edge.m_node.get(), 0, m_key_buffer.size()});
            if (edge.m_node->is_leaf()) {
                m_current_node = edge.m_node.get();
                return true;
            }
        }

        return false;
    }

    // 更新当前项
    void update_current_item() {
        if (m_current_node && m_current_node->is_leaf()) {
            // 为了让m_current_item的值和m_current_node->m_leaf.value()绑定，
            // 因为C++引用不能重绑定，这里用了一个挫办法，利用placement
            // new重绑定m_current_item的值
            m_current_item.~value_type();
            new (&m_current_item) value_type(m_key_buffer, m_current_node->m_leaf.value());
        }
    }

    bool         m_is_end       = false;
    node_type*   m_current_node = nullptr;
    key_buffer<> m_key_buffer;
    path_type    m_path;
    union {
        const_map_value_type m_current_item;
        map_value_type       m_mutable_current_item;
    };

    friend class iterator;
};

template <typename Config>
class radix_tree<Config>::iterator : public radix_tree<Config>::const_iterator {
public:
    // 标准迭代器类型定义
    using iterator_category = std::forward_iterator_tag;
    using value_type        = map_value_type;
    using difference_type   = std::ptrdiff_t;
    using pointer           = value_type*;
    using reference         = value_type&;

    // 默认构造函数（end迭代器）
    iterator() : const_iterator() {
    }

    // 构造函数（begin迭代器）
    explicit iterator(const node_ptr& root) : const_iterator(root) {
    }

    // 复制构造函数
    iterator(const iterator& other) : const_iterator(other) {
    }

    // 复制赋值运算符
    iterator& operator=(const iterator& other) {
        if (this != &other) {
            const_iterator::operator=(other);
        }
        return *this;
    }

    // 移动构造函数
    iterator(iterator&& other) noexcept : const_iterator(std::move(other)) {
    }

    // 移动赋值运算符
    iterator& operator=(iterator&& other) noexcept {
        if (this != &other) {
            const_iterator::operator=(std::move(other));
        }
        return *this;
    }

    // 前置递增
    iterator& operator++() {
        const_iterator::operator++();
        return *this;
    }

    // 后置递增
    iterator operator++(int) {
        iterator tmp = *this;
        ++(*this);
        return tmp;
    }

    // 解引用操作符
    reference operator*() const {
        if (this->m_is_end) {
            throw std::out_of_range("迭代器指向了末尾");
        }
        return const_cast<reference>(this->m_mutable_current_item);
    }

    // 箭头操作符
    pointer operator->() const {
        if (this->m_is_end) {
            throw std::out_of_range("迭代器指向了末尾");
        }
        return const_cast<pointer>(&this->m_mutable_current_item);
    }

    /**
     * 获取当前项的键
     * @return 键
     */
    key_view key() const {
        if (this->m_is_end) {
            throw std::out_of_range("迭代器指向了末尾");
        }
        return this->m_key_buffer;
    }
};

} // namespace mc::im

#endif // MC_IM_RADIX_TREE_ITERATOR_H