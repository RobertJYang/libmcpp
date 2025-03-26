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
    using key_type             = key_buffer<>;
    using map_value_type       = std::pair<key_view, value_type&>;
    using const_map_value_type = std::pair<const key_view, const value_type&>;

    // 前向声明
    class const_iterator;
    class iterator;

    /**
     * 基数树常量迭代器
     * 支持深度优先遍历树中的所有键值对
     * 轻量级设计，减少复制成本
     */
    class const_iterator {
    public:
        // 标准迭代器类型定义
        using iterator_category = std::forward_iterator_tag;
        using value_type        = const_map_value_type;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const value_type*;
        using reference         = const value_type&;

        // 构造节点路径记录项
        struct path_item {
            node_ptr node;        // 当前节点
            size_t   edge_index;  // 下一个要访问的边索引
            size_t   prefix_size; // 节点前缀在键缓冲区中的大小
        };

        // 默认构造函数（end迭代器）
        const_iterator() : m_is_end(true) {
        }

        // 构造函数（begin迭代器）
        explicit const_iterator(const node_ptr& root) : m_is_end(false) {
            if (!root) {
                m_is_end = true;
                return;
            }

            // 初始化DFS堆栈
            if (initialize_stack(root)) {
                update_current_item();
            } else {
                m_is_end = true;
            }
        }

        // 前置递增
        const_iterator& operator++() {
            if (m_is_end || m_path.empty()) {
                m_is_end = true;
                return *this;
            }

            // 当前节点已处理完毕，回退到上一级
            m_current_node = nullptr;

            // 继续深度优先搜索
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
            // 都是末尾迭代器
            if (m_is_end && other.m_is_end) {
                return true;
            }

            // 一个是末尾，一个不是
            if (m_is_end != other.m_is_end) {
                return false;
            }

            // 两个都不是末尾，比较当前节点
            return m_current_node == other.m_current_node;
        }

        // 不等比较操作符
        bool operator!=(const const_iterator& other) const {
            return !(*this == other);
        }

    protected:
        // 初始化DFS堆栈
        bool initialize_stack(const node_ptr& root) {
            if (!root) {
                return false;
            }

            // 清空堆栈和键缓冲区
            m_path.clear();
            m_key_buffer.clear();

            // 记录当前路径前缀大小
            size_t prev_size = m_key_buffer.size();

            // 添加根节点到堆栈
            m_key_buffer.append(root->m_prefix);
            m_path.push_back({root, 0, prev_size});

            return find_first_leaf();
        }

        // 查找第一个叶子节点
        bool find_first_leaf() {
            while (!m_path.empty()) {
                auto& current = m_path.back();

                // 如果当前节点是叶子节点，成功找到
                if (current.node->is_leaf()) {
                    m_current_node = current.node;
                    return true;
                }

                // 如果已经处理完所有子节点，回溯
                if (current.edge_index >= current.node->m_edges.size()) {
                    // 恢复键缓冲区大小
                    m_key_buffer.resize(current.prefix_size);
                    m_path.pop_back();
                    continue;
                }

                // 处理下一个子节点
                const auto& edge      = current.node->m_edges[current.edge_index++];
                size_t      prev_size = m_key_buffer.size();

                // 添加子节点的前缀到键缓冲区
                m_key_buffer.append(edge.m_node->m_prefix);
                m_path.push_back({edge.m_node, 0, prev_size});
            }

            return false;
        }

        // 前进到下一个叶子节点
        bool advance_to_next_leaf() {
            // 回溯并寻找下一个要处理的边
            while (!m_path.empty()) {
                auto& current = m_path.back();

                // 如果当前节点的边已经全部处理完毕
                if (current.edge_index >= current.node->m_edges.size()) {
                    // 恢复键缓冲区大小并回溯
                    m_key_buffer.resize(current.prefix_size);
                    m_path.pop_back();
                    continue;
                }

                // 处理下一个边
                const auto& edge      = current.node->m_edges[current.edge_index++];
                size_t      prev_size = m_key_buffer.size();

                // 添加边目标节点的前缀
                m_key_buffer.append(edge.m_node->m_prefix);
                m_path.push_back({edge.m_node, 0, prev_size});

                // 如果是叶子节点，找到了下一个元素
                if (edge.m_node->is_leaf()) {
                    m_current_node = edge.m_node;
                    return true;
                }

                // 如果不是叶子节点，继续向下搜索
                if (edge.m_node->m_edges.empty()) {
                    // 如果没有子节点，回溯
                    m_key_buffer.resize(prev_size);
                    m_path.pop_back();
                } else {
                    // 否则寻找这个子树中的第一个叶子节点
                    if (find_first_leaf()) {
                        return true;
                    }
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

        bool                   m_is_end       = false;
        node_ptr               m_current_node = nullptr;
        key_buffer<>           m_key_buffer;
        std::vector<path_item> m_path;
        union {
            const_map_value_type m_current_item;
            map_value_type       m_mutable_current_item;
        };

        friend class iterator;
    };

    /**
     * 可变迭代器
     * 不可变基数树原则上不允许通过迭代器修改元素值，但这里提供一个可变迭代器，
     * 允许修改元素值但不允许修改键，这么做只是出于性能考虑，安全的修改值的办法
     * 是通过transaction::update()方法，在事务中修改值，保证不可变基数树的不可变特性。
     */
    class iterator : public const_iterator {
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
     * 返回指向第一个元素的迭代器（const版本）
     * @return 常量起始迭代器
     */
    const_iterator cbegin() const {
        return const_iterator(m_root);
    }

    /**
     * 返回指向最后一个元素之后的迭代器（const版本）
     * @return 常量结束迭代器
     */
    const_iterator cend() const {
        return const_iterator();
    }

    /**
     * 返回指向第一个元素的迭代器（非const版本）
     * @return 起始迭代器
     */
    iterator begin() {
        return iterator(m_root);
    }

    /**
     * 返回指向最后一个元素之后的迭代器（非const版本）
     * @return 结束迭代器
     */
    iterator end() {
        return iterator();
    }

    /**
     * 返回指向第一个元素的迭代器（const版本）
     * @return 常量起始迭代器
     */
    const_iterator begin() const {
        return const_iterator(m_root);
    }

    /**
     * 返回指向最后一个元素之后的迭代器（const版本）
     * @return 常量结束迭代器
     */
    const_iterator end() const {
        return const_iterator();
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