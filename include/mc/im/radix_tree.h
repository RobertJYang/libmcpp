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
#include <mc/im/radix_tree/node.h>
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

    /**
     * 路径项结构，用于记录遍历路径中的节点信息
     */
    struct path_item {
        node_ptr node;        // 当前节点
        size_t   edge_index;  // 下一个要访问的边索引
        size_t   prefix_size; // 节点前缀在键缓冲区中的大小
    };
    using path_type = std::vector<path_item>;

    // 前向声明
    class const_iterator;
    class iterator;

    /**
     * 创建一个空的不可变基数树
     */
    radix_tree(const allocator_type& alloc = allocator_type())
        : m_root(mc::allocate_ref<node_type>(alloc, std::nullopt)), m_size(0), m_allocator(alloc) {
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
    leaf_type get(key_view key) const;

    /**
     * 查找具有特定键的元素
     * @param key 要查找的键
     * @return 指向找到元素的迭代器，如果未找到则返回end()
     */
    iterator find(key_view key);

    /**
     * 查找具有特定键的元素（const版本）
     * @param key 要查找的键
     * @return 指向找到元素的常量迭代器，如果未找到则返回end()
     */
    const_iterator find(key_view key) const {
        return const_cast<radix_tree*>(this)->find(key);
    }

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

    /**
     * 返回指向大于等于给定键的第一个元素的迭代器
     * @param key 键值
     * @return 指向大于等于key的第一个元素的迭代器，如果没有则返回end()
     */
    iterator lower_bound(key_view key);

    /**
     * 返回指向大于等于给定键的第一个元素的迭代器（const版本）
     * @param key 键值
     * @return 指向大于等于key的第一个元素的常量迭代器，如果没有则返回end()
     */
    const_iterator lower_bound(key_view key) const {
        return const_cast<radix_tree*>(this)->lower_bound(key);
    }

    /**
     * 返回指向大于给定键的第一个元素的迭代器
     * @param key 键值
     * @return 指向大于key的第一个元素的迭代器，如果没有则返回end()
     */
    iterator upper_bound(key_view key);

    /**
     * 返回指向大于给定键的第一个元素的迭代器（const版本）
     * @param key 键值
     * @return 指向大于key的第一个元素的常量迭代器，如果没有则返回end()
     */
    const_iterator upper_bound(key_view key) const {
        return const_cast<radix_tree*>(this)->upper_bound(key);
    }

private:
    node_ptr       m_root;
    size_t         m_size = 0;
    allocator_type m_allocator;

    /**
     * 创建一个迭代器并设置其状态
     * @param it_node 迭代器的当前节点
     * @param it_key_buf 迭代器的键缓冲区
     * @param it_path 迭代器的路径
     * @return 设置好状态的迭代器
     */
    iterator make_iterator(node_ptr n, key_buffer<>&& key_buf, path_type&& path);
};

} // namespace mc::im

// 包含迭代器和实现文件
#include <mc/im/radix_tree/radix_tree_impl.h>
#include <mc/im/radix_tree/radix_tree_iterator.h>
#include <mc/im/radix_tree/transaction.h>

#endif // MC_IM_RADIX_TREE_H