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

#ifndef MC_IM_RADIX_TREE_H
#define MC_IM_RADIX_TREE_H

#include <cstdint>
#include <functional>
#include <mc/exception.h>
#include <mc/im/node.h>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace mc::im {

// 前向声明
template <typename Config>
class transaction;

/**
 * 不可变基数树
 * 该树结构确保不会修改已有的数据结构，所有修改操作都会创建新节点
 * 设计为可在共享内存中使用
 * @tparam Config 树配置类型
 */
template <typename Config = default_tree_config>
class radix_tree {
public:
    // 从配置中提取类型
    using leaf_type              = typename Config::leaf_type;
    using value_type             = typename Config::value_type;
    using allocator_type         = typename Config::allocator_type;
    static constexpr bool IsLess = Config::is_less;

    using node_type = node<Config>;
    using node_ptr  = typename node_type::ref_ptr_type;

    // 迭代器相关类型定义
    using key_type  = key_buffer<>;
    using item_type = std::pair<key_type, value_type>;

    /**
     * 基数树迭代器
     * 支持深度优先遍历树中的所有键值对
     * 轻量级设计，减少复制成本
     */
    class iterator {
    public:
        // 标准迭代器类型定义
        using iterator_category = std::forward_iterator_tag;
        using value_type        = item_type;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const value_type*;
        using reference         = const value_type&;

        // 默认构造函数（end迭代器）
        iterator() : m_is_end(true) {
        }

        // 构造函数（begin迭代器）
        explicit iterator(const node_ptr& root) : m_is_end(false) {
            if (!root) {
                m_is_end = true;
                return;
            }

            // 初始化第一个元素
            if (findNext(root)) {
                m_current_item.second = m_current_node->m_leaf.value();
            } else {
                m_is_end = true;
            }
        }

        // 前置递增
        iterator& operator++() {
            if (m_is_end) {
                return *this;
            }

            // 检查当前节点是否还有未处理的边
            if (m_current_edge_index < m_current_node->m_edges.size()) {
                // 保存当前状态并进入下一个边
                const auto& edge = m_current_node->m_edges[m_current_edge_index++];

                // 保存当前路径信息
                m_path.push_back({m_current_node, m_current_edge_index - 1});

                // 继续搜索下一个节点
                if (findNext(edge.m_node)) {
                    m_current_item.second = m_current_node->m_leaf.value();
                    return *this;
                }
            }

            // 回溯到上一个有未处理边的节点
            while (!m_path.empty()) {
                auto [node, idx] = m_path.back();
                m_path.pop_back();

                // 回退键缓冲区
                size_t last_node_prefix_len = node->m_edges[idx].m_node->m_prefix.size();
                MC_ASSERT(m_key_buffer.size() >= last_node_prefix_len,
                          "key_buffer size is less than last_node_prefix_len");
                m_key_buffer.resize(m_key_buffer.size() - last_node_prefix_len);

                // 检查当前节点是否还有未处理的边
                if (idx + 1 < node->m_edges.size()) {
                    m_current_node       = node;
                    m_current_edge_index = idx + 1;

                    // 处理下一条边
                    const auto& edge = node->m_edges[m_current_edge_index++];
                    m_path.push_back({node, m_current_edge_index - 1});

                    // 继续搜索
                    if (findNext(edge.m_node)) {
                        m_current_item.second = m_current_node->m_leaf.value();
                        return *this;
                    }
                }
            }

            // 如果没有更多节点，迭代结束
            m_is_end = true;
            return *this;
        }

        // 后置递增
        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        // 解引用操作符，返回键值对
        reference operator*() const {
            // 懒惰计算 - 仅在需要时构建完整键
            if (!m_is_key_valid) {
                m_current_item.first = m_key_buffer;
                m_is_key_valid       = true;
            }
            return m_current_item;
        }

        // 箭头操作符
        pointer operator->() const {
            // 懒惰计算 - 仅在需要时构建完整键
            if (!m_is_key_valid) {
                m_current_item.first = m_key_buffer;
                m_is_key_valid       = true;
            }
            return &m_current_item;
        }

        // 等于比较
        bool operator==(const iterator& other) const {
            if (m_is_end && other.m_is_end) {
                return true;
            }
            if (m_is_end != other.m_is_end) {
                return false;
            }
            return m_current_node == other.m_current_node &&
                   m_current_edge_index == other.m_current_edge_index;
        }

        // 不等于比较
        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

    private:
        using path_item = std::pair<node_ptr, size_t>; // 节点和边索引

        bool                   m_is_end             = false;
        node_ptr               m_current_node       = nullptr;
        size_t                 m_current_edge_index = 0;
        key_buffer<>           m_key_buffer;
        std::vector<path_item> m_path;
        mutable item_type      m_current_item;
        mutable bool           m_is_key_valid = false;

        // 查找下一个叶子节点
        bool findNext(const node_ptr& node) {
            if (!node) {
                return false;
            }

            // 追加当前节点的前缀
            m_key_buffer.append(node->m_prefix);

            // 设置当前状态
            m_current_node       = node;
            m_current_edge_index = 0;
            m_is_key_valid       = false;

            // 如果当前节点是叶子节点，返回它
            if (node->is_leaf()) {
                return true;
            }

            // 递归查找第一个叶子节点
            if (!node->m_edges.empty()) {
                const auto& edge = node->m_edges[m_current_edge_index++];
                m_path.push_back({node, m_current_edge_index - 1});

                if (findNext(edge.m_node)) {
                    return true;
                }

                // 如果没有找到叶子节点，回溯并清理
                m_path.pop_back();
            }

            // 没有找到叶子节点
            return false;
        }
    };

    /**
     * 创建一个空的不可变基数树
     */
    radix_tree(const allocator_type& alloc = allocator_type())
        : m_root(allocate_ref<node_type>(alloc, std::nullopt)), m_size(0), m_allocator(alloc) {
    }

    /**
     * 使用现有节点创建不可变基数树
     * @param root 根节点
     * @param size 树的大小
     */
    radix_tree(node_ptr root, size_t size);

    ~radix_tree() {
        m_root.reset();
    }

    /**
     * 返回树中元素的数量
     * @return 元素数量
     */
    size_t size() const;

    /**
     * 获取根节点
     * @return 根节点的指针
     */
    const node_type root() const;

    /**
     * 查找键对应的值
     * @param key 查找的键
     * @return 值的可选实例
     */
    leaf_type get(const key_view& key) const;

    /**
     * 返回指向第一个元素的迭代器
     * @return 起始迭代器
     */
    iterator begin() const {
        return iterator(m_root);
    }

    /**
     * 返回指向最后一个元素之后的迭代器
     * @return 结束迭代器
     */
    iterator end() const {
        return iterator();
    }

    /**
     * 检查树是否为空
     * @return 如果树为空返回true
     */
    bool empty() const {
        return m_size == 0;
    }

    allocator_type get_allocator() const {
        return m_allocator;
    }

    // 允许事务类访问私有成员
    friend class transaction<Config>;

private:
    node_ptr       m_root;
    size_t         m_size = 0;
    allocator_type m_allocator;
};

// 模板函数实现

template <typename Config>
radix_tree<Config>::radix_tree(node_ptr root, size_t size) : m_root(std::move(root)), m_size(size) {
}

template <typename Config>
size_t radix_tree<Config>::size() const {
    return m_size;
}

template <typename Config>
const typename radix_tree<Config>::node_type radix_tree<Config>::root() const {
    return m_root;
}

template <typename Config>
typename radix_tree<Config>::leaf_type radix_tree<Config>::get(const key_view& key) const {
    if (!m_root) {
        return std::nullopt;
    }
    return m_root->get(key);
}

} // namespace mc::im

#endif // MC_IM_RADIX_TREE_H