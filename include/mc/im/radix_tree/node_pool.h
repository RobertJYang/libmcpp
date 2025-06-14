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

#ifndef MC_IM_NODE_POOL_H
#define MC_IM_NODE_POOL_H

#include <mc/im/radix_tree/node.h>
#include <memory>

namespace mc::im {

/**
 * 节点池管理类，负责节点的分配和回收
 */
template <typename Config = default_tree_config>
class node_pool {
public:
    // 从配置中提取类型
    using leaf_type              = typename Config::leaf_type;
    using allocator_type         = typename Config::allocator_type;
    static constexpr bool IsLess = Config::is_less;

    using node_type  = node<Config>;
    using node_ptr   = typename node_type::ref_ptr_type;
    using node_list  = typename node_type::list_type;
    using edges_type = typename node_type::edges_type;

    /**
     * 默认构造函数
     * @param alloc 内存分配器
     */
    explicit node_pool(const allocator_type& alloc);

    node_pool(const node_pool& other)            = delete;
    node_pool& operator=(const node_pool& other) = delete;

    node_pool(node_pool&& other) noexcept {
        m_free_list    = std::move(other.m_free_list);
        m_tx_old_nodes = std::move(other.m_tx_old_nodes);
        m_tx_new_nodes = std::move(other.m_tx_new_nodes);
        m_tx_tmp_nodes = std::move(other.m_tx_tmp_nodes);
        m_allocator    = std::move(other.m_allocator);
    }

    node_pool& operator=(node_pool&& other) noexcept {
        if (this != &other) {
            m_free_list    = std::move(other.m_free_list);
            m_tx_old_nodes = std::move(other.m_tx_old_nodes);
            m_tx_new_nodes = std::move(other.m_tx_new_nodes);
            m_tx_tmp_nodes = std::move(other.m_tx_tmp_nodes);
            m_allocator    = std::move(other.m_allocator);
        }
        return *this;
    }

    /**
     * 初始化节点池
     * @param free_list 空闲节点列表
     */
    void init_node_pool(node_list* free_list);

    /**
     * 设置版本号
     * @param pre_version 前一个版本号
     * @param version 当前版本号
     */
    void set_version(int pre_version, int version);

    /**
     * 提交操作，将修改永久化
     */
    void commit();

    /**
     * 回滚操作，撤销修改
     */
    void rollback();

    /**
     * 创建可写节点
     * @param n 原节点
     * @return 可写节点
     */
    node_ptr write_node(const node_ptr& n, int lock_db);

    /**
     * 创建新节点
     * @param leaf 叶子节点的值
     * @param prefix 前缀
     * @param edges 边
     * @return 新节点
     */
    node_ptr new_node(leaf_type leaf, key_view prefix, edges_type edges = {});

    /**
     * 移除节点
     * @param n 要移除的节点
     * @param lock_db 锁定数据库标志
     */
    void remove_node(node_ptr n, int lock_db);

    /**
     * 获取内存分配器
     * @return 内存分配器
     */
    allocator_type get_allocator() const;

    int m_version;
    int m_pre_version;

private:
    node_list*     m_free_list;
    node_list      m_tx_old_nodes;
    node_list      m_tx_new_nodes;
    node_list      m_tx_tmp_nodes;
    allocator_type m_allocator; // 内存分配器

    /**
     * 释放节点列表
     * @param l 节点列表
     */
    void free_node_list(node_list* l);

    /**
     * 修复节点列表
     * @param l 节点列表
     */
    void fix_node_list(node_list* l);

    /**
     * 释放单个节点
     * @param n 节点
     */
    void free_node(node_ptr n);

    /**
     * 修剪节点的边数组容量，减少内存浪费
     * @param node 需要修剪的节点
     */
    inline void trim_node_edges(const node_ptr& node) {
        auto&  edges    = node->m_edges;
        size_t edge_len = edges.size();
        size_t edge_cap = edges.capacity();

        if (edge_len == 0 || edge_cap == 0) {
            edges.reserve(0);
            return;
        }

        double waste       = edge_cap - edge_len;
        double waste_ratio = waste / edge_cap;
        if (waste_ratio > 0.5) {
            edges.reserve(edge_len);
        }
    }
};

// 模板函数实现

template <typename Config>
node_pool<Config>::node_pool(const allocator_type& alloc)
    : m_version(0), m_pre_version(0), m_free_list(nullptr), m_tx_old_nodes(), m_tx_new_nodes(),
      m_tx_tmp_nodes(), m_allocator(alloc) {
}

template <typename Config>
void node_pool<Config>::init_node_pool(node_list* free_list) {
    m_version     = 0;
    m_pre_version = 0;
    m_free_list   = free_list;
    m_tx_old_nodes.clear();
    m_tx_new_nodes.clear();
    m_tx_tmp_nodes.clear();
}

template <typename Config>
void node_pool<Config>::set_version(int pre_version, int version) {
    m_version     = version;
    m_pre_version = pre_version;
}

template <typename Config>
void node_pool<Config>::commit() {
    fix_node_list(&m_tx_new_nodes);
    free_node_list(&m_tx_tmp_nodes);
    free_node_list(&m_tx_old_nodes);
}

template <typename Config>
void node_pool<Config>::rollback() {
    free_node_list(&m_tx_new_nodes);
    free_node_list(&m_tx_tmp_nodes);
    fix_node_list(&m_tx_old_nodes);
}

template <typename Config>
void node_pool<Config>::free_node_list(node_list* l) {
    while (l->len() > 0) {
        auto node = l->front();
        l->remove(node);

        // 仅仅只有当前节点被引用，则释放节点会缓存池
        if (node->ref_count() == 1) {
            free_node(node);
        }
    }
}

template <typename Config>
void node_pool<Config>::fix_node_list(node_list* l) {
    size_t list_len = l->len();

    if (list_len == 0) {
        return;
    }

    auto node = l->front();
    for (size_t i = 0; i < list_len && node; i++) {
        trim_node_edges(node);
        node = node->m_next;
    }

    l->clear();
}

template <typename Config>
void node_pool<Config>::free_node(node_ptr n) {
    n->m_leaf.reset();
    n->m_edges.clear();
    n->m_prefix.clear();
    n->m_version = 0;
    m_free_list->push_back(std::move(n));
}

template <typename Config>
void node_pool<Config>::remove_node(node_ptr n, int lock_db) {
    if (lock_db == 0 && n->m_version == m_version) {
        return;
    }

    if (n->m_version == m_version) {
        auto removed = m_tx_new_nodes.remove(n);
        m_tx_tmp_nodes.push_back(std::move(removed));
    } else if (n->m_version <= m_pre_version) {
        m_tx_old_nodes.push_back(std::move(n));
    }
}

template <typename Config>
typename node_pool<Config>::node_ptr node_pool<Config>::write_node(const node_ptr& n, int lock_db) {
    if (lock_db == 0 && n->m_version == m_version) {
        return n;
    }

    auto nc = new_node(n->m_leaf, n->m_prefix, n->m_edges);

    if (n->m_version == m_version) {
        auto removed = m_tx_new_nodes.remove(n);
        m_tx_tmp_nodes.push_back(removed);
    } else if (n->m_version <= m_pre_version) {
        m_tx_old_nodes.push_back(n);
    }

    return nc;
}

template <typename Config>
typename node_pool<Config>::node_ptr node_pool<Config>::new_node(leaf_type leaf, key_view prefix,
                                                                 edges_type edges) {
    node_ptr n;

    if (m_free_list && m_free_list->len() > 0) {
        n = m_free_list->front();
        m_free_list->remove(n);

        n->m_leaf   = leaf;
        n->m_prefix = prefix;
        n->m_edges  = std::move(edges);
    } else {
        n = mc::allocate_shared<node_type>(m_allocator, leaf, prefix, std::move(edges));
    }

    n->m_version = m_version;
    m_tx_new_nodes.push_back(n);

    return n;
}

template <typename Config>
typename node_pool<Config>::allocator_type node_pool<Config>::get_allocator() const {
    return m_allocator;
}

} // namespace mc::im

#endif // MC_IM_NODE_POOL_H