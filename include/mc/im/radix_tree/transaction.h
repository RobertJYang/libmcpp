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

#ifndef MC_IM_TRANSACTION_H
#define MC_IM_TRANSACTION_H

#include <mc/exception.h>
#include <mc/im/radix_tree/node.h>
#include <mc/im/radix_tree/node_pool.h>
#include <optional>
#include <vector>

namespace mc::im {

namespace detail {
/**
 * 事务保存点，用于支持回滚操作
 */
template <typename Config>
class save_point {
public:
    using allocator_type = typename Config::allocator_type;
    using node_type      = node<Config>;
    using node_ptr       = typename node_type::ref_ptr_type;
    using node_list      = typename node_type::list_type;
    using pool_type      = node_pool<Config>;

    save_point(const allocator_type& alloc = allocator_type())
        : m_node_pool(alloc), m_size(0), m_version(0) {
    }

    save_point(node_ptr root, size_t size, const allocator_type& alloc)
        : m_node_pool(alloc), m_root(std::move(root)), m_size(size), m_version(0) {
    }

    save_point(const save_point& other)            = delete;
    save_point& operator=(const save_point& other) = delete;

    save_point(save_point&& other) noexcept
        : m_node_pool(std::move(other.m_node_pool)), m_root(std::move(other.m_root)),
          m_size(other.m_size), m_version(other.m_version) {
    }

    save_point& operator=(save_point&& other) noexcept {
        if (this != &other) {
            m_node_pool = std::move(other.m_node_pool);
            m_root      = std::move(other.m_root);
            m_size      = other.m_size;
            m_version   = other.m_version;
        }
        return *this;
    }

    /**
     * 初始化保存点
     * @param free_list 空闲列表
     */
    void init(node_list* free_list) {
        m_node_pool.init_node_pool(free_list);
    }

    /**
     * 设置版本号
     * @param pre_version 前一个版本
     * @param version 当前版本
     */
    void set_version(int pre_version, int version) {
        m_node_pool.set_version(pre_version, version);
        m_version = version;
    }

    /**
     * 提交保存点
     */
    void commit() {
        m_node_pool.commit();
    }

    /**
     * 回滚保存点
     */
    void rollback() {
        m_node_pool.rollback();
    }

    pool_type m_node_pool;
    node_ptr  m_root;
    size_t    m_size;
    size_t    m_version;
};
} // namespace detail

/**
 * 事务对象，用于操作不可变基数树
 */
template <typename Config = default_tree_config>
class transaction {
public:
    // 从配置中提取类型
    using leaf_type              = typename Config::leaf_type;
    using value_type             = typename Config::value_type;
    using allocator_type         = typename Config::allocator_type;
    static constexpr bool IsLess = Config::is_less;

    using node_type       = node<Config>;
    using node_ptr        = typename node_type::ref_ptr_type;
    using node_list       = typename node_type::list_type;
    using edges_type      = typename node_type::edges_type;
    using edge_type       = typename node_type::edge_type;
    using tree_type       = radix_tree<Config>;
    using pool_type       = node_pool<Config>;
    using save_point_type = detail::save_point<Config>;

    /**
     * 默认构造函数
     */
    transaction(const allocator_type& alloc = allocator_type())
        : m_tree(alloc), m_def_save_point(alloc) {
    }

    /**
     * 为给定 tree 创建新的事务
     * @param tree 给定的树
     */
    transaction(tree_type tree) : m_tree(tree), m_def_save_point(tree.get_allocator()) {
    }

    ~transaction() {
        rollback();
        m_free_list.clear();
    }

    transaction(const transaction&)            = delete;
    transaction& operator=(const transaction&) = delete;
    transaction(transaction&&)                 = delete;
    transaction& operator=(transaction&&)      = delete;

    /**
     * 获取空闲列表长度
     * @return 空闲列表长度
     */
    node_list& free_list() {
        return m_free_list;
    }

    int save_point() {
        if (!m_current_sp) {
            m_def_save_point.init(&m_free_list);
            m_def_save_point.m_root = m_tree.m_root;
            m_def_save_point.m_size = m_tree.m_size;
            m_def_save_point.set_version(m_version, m_version + 1);
            m_current_sp = &m_def_save_point;
            m_lock_db    = 0;
        } else {
            auto tmp_root = m_tree.m_root;
            m_save_points.emplace_back(tmp_root, m_tree.m_size, m_tree.get_allocator());
            auto& sp = m_save_points.back();
            sp.init(&m_free_list);
            sp.set_version(m_version, m_version + m_save_points.size() + 1);
            m_current_sp = &sp;
        }
        return current_save_point();
    }

    /**
     * 插入或更新数据
     * @param k 键
     * @param v 值
     * @return 返回更新前的值和是否更新的标志
     */
    std::pair<leaf_type, bool> insert(key_view k, value_type v) {
        auto [new_node, old_value, updated] = insert(m_tree.m_root, k, k, std::move(v));
        if (new_node) {
            m_tree.m_root = new_node;
        }
        if (!updated) {
            m_tree.m_size++;
        }

        return {old_value, updated};
    }

    /**
     * 删除节点
     * @param k 键
     * @return 返回被删除的值和是否删除成功的标志
     */
    leaf_type remove(key_view k) {
        auto [new_root, leaf] = delete_key(m_tree.m_root, k);
        if (new_root) {
            m_tree.m_root = new_root;
        }

        if (leaf.has_value()) {
            m_tree.m_size--;
            return leaf;
        }

        return std::nullopt;
    }

    /**
     * 获取树的根节点
     * @return 树
     */
    tree_type& root() {
        return m_tree;
    }

    /**
     * 查找数据
     * @param k 键
     * @return 查找到的值（如果有）
     */
    leaf_type get(key_view k) {
        return m_tree.get(k);
    }

    /**
     * 查找键对应的迭代器
     * @param k 键
     * @return 包含迭代器和是否找到的标志
     */
    std::pair<typename tree_type::iterator, bool> find(key_view k) {
        auto it = m_tree.find(k);
        return {it, it != m_tree.end()};
    }

    /**
     * 删除指定迭代器位置的元素
     * @param it 迭代器
     * @return 是否成功删除
     */
    bool erase(const typename tree_type::iterator& it) {
        if (it != m_tree.end()) {
            remove(it.key());
            return true;
        }
        return false;
    }

    /**
     * 提交事务
     * @return 树
     */
    tree_type& commit() {
        if (m_current_sp) {
            m_version = m_current_sp->m_version;
            for (auto it = m_save_points.rbegin(); it != m_save_points.rend(); it++) {
                it->commit();
            }
            m_def_save_point.commit();

            // 重新初始化保存点
            m_save_points.clear();
            m_current_sp = nullptr;
        }

        m_lock_db = 0;
        return m_tree;
    }

    /**
     * 检查是否有未提交的修改
     * @return 如果有未提交的修改则返回true
     */
    bool dirty() const {
        return m_current_sp != nullptr;
    }

    /**
     * 锁定数据库，防止垃圾回收
     */
    void lock_db() {
        m_lock_db++;
    }

    /**
     * 解锁数据库，允许垃圾回收
     */
    void unlock_db() {
        if (m_lock_db > 0) {
            m_lock_db--;
        }
    }

    /**
     * 获取最后保存点ID，-1表示没有保存点
     * @return 保存点ID
     */
    int current_save_point() const {
        return m_save_points.size() - 1;
    }

    /**
     * 回滚到指定保存点
     * @param save_point_id 保存点ID
     */
    void rollback(int save_point_id = -1) {
        if (!m_current_sp) {
            return;
        }

        m_lock_db          = 0;
        save_point_type* s = nullptr;
        int              i = m_save_points.size();
        for (i--; i >= 0; i--) {
            s = &m_save_points[i];
            s->rollback();
            if (i == save_point_id) {
                break;
            }
        }
        if (i == -1) {
            m_tree.m_root = m_def_save_point.m_root;
            m_tree.m_size = m_def_save_point.m_size;
            m_def_save_point.rollback();
            m_current_sp = nullptr;
            m_save_points.clear();
            return;
        }
        m_tree.m_root = s->m_root;
        m_tree.m_size = s->m_size;
        if (i == 0) {
            m_current_sp = &m_def_save_point;
            m_save_points.clear();
        } else {
            m_current_sp = &m_save_points[i - 1];
            m_save_points.resize(i);
        }
    }

private:
    tree_type                    m_tree;
    size_t                       m_version    = 0;
    int                          m_lock_db    = 0;
    save_point_type*             m_current_sp = nullptr;
    node_list                    m_free_list;
    save_point_type              m_def_save_point;
    std::vector<save_point_type> m_save_points;

    /**
     * 复制节点以进行写操作
     * @param n 需要复制的节点
     * @return 复制后的新节点
     */
    node_ptr write_node(const node_ptr& n) {
        if (!m_current_sp) {
            save_point();
        }

        return m_current_sp->m_node_pool.write_node(n, m_lock_db);
    }

    /**
     * 创建新节点
     */
    node_ptr new_node(leaf_type leaf, key_view prefix, const edges_type& edges = {}) {
        return m_current_sp->m_node_pool.new_node(leaf, prefix, edges);
    }

    /**
     * 合并子节点，如果子节点只有一个边且不是叶子节点
     */
    void merge_child(node_ptr& n) {
        auto child = n->m_edges[0].m_node;

        n->m_prefix += child->m_prefix;
        n->m_leaf  = child->m_leaf;
        n->m_edges = std::move(child->m_edges);
    }

    /**
     * 递归插入节点
     */
    std::tuple<node_ptr, leaf_type, bool> insert(const node_ptr& n, key_view k, key_view search,
                                                 value_type v) {
        // 如果搜索到了末尾，更新叶子节点
        if (search.empty()) {
            leaf_type old_leaf   = n->m_leaf;
            bool      did_update = old_leaf.has_value();

            auto new_n    = write_node(n);
            new_n->m_leaf = std::move(v);

            return {new_n, old_leaf, did_update};
        }

        // 查找匹配的边
        uint8_t first_char = search[0];
        auto [idx, child]  = n->get_edge(first_char);

        // 如果没有匹配的边，创建新边
        if (!child) {
            node_ptr new_n     = write_node(n);
            auto     new_child = new_node(std::move(v), search);
            new_n->add_edge(edge_type(first_char, new_child));
            return {new_n, std::nullopt, false};
        }

        // 计算公共前缀长度
        size_t prefix_len = longest_prefix(search, child->m_prefix);

        // 如果前缀完全匹配，继续在子节点上递归
        if (prefix_len == child->m_prefix.size()) {
            auto remaining                       = search.substr(prefix_len);
            auto [new_child, old_value, updated] = insert(child, k, remaining, std::move(v));

            if (!new_child) {
                return {nullptr, old_value, updated};
            }

            auto new_n                 = write_node(n);
            new_n->m_edges[idx].m_node = new_child;
            return {new_n, old_value, updated};
        }

        // 如果搜索键是子节点前缀的子集，需要分裂节点
        auto new_n      = write_node(n);
        auto split_node = new_node(std::nullopt, search.substr(0, prefix_len));
        new_n->replace_edge(edge_type(search[0], split_node));

        auto new_child      = write_node(child);
        new_child->m_prefix = child->m_prefix.substr(prefix_len);
        split_node->add_edge(edge_type(new_child->m_prefix[0], new_child));

        auto remaining = search.substr(prefix_len);
        if (remaining.empty()) {
            // 如果搜索键为空，则将叶子节点设置为v
            split_node->m_leaf = std::move(v);
        } else {
            // 否则，在分裂节点中添加新的边
            split_node->add_edge(edge_type(remaining[0], new_node(std::move(v), remaining)));
        }

        return {new_n, std::nullopt, false};
    }

    /**
     * 递归删除节点
     */
    std::pair<node_ptr, leaf_type> delete_key(const node_ptr& n, key_view search) {
        if (search.empty()) {
            if (!n->m_leaf.has_value()) {
                return {nullptr, std::nullopt};
            }

            auto old_leaf = n->m_leaf;
            auto new_n    = write_node(n);
            new_n->m_leaf = std::nullopt;

            if (n != m_tree.m_root && new_n->m_edges.size() == 1) {
                merge_child(new_n);
            }

            return {new_n, old_leaf};
        }

        // 查找匹配的边
        uint8_t first_char = search[0];
        auto [idx, child]  = n->get_edge(first_char);
        if (!child || !has_prefix(search, child->m_prefix)) {
            return {nullptr, std::nullopt};
        }

        key_view remaining     = search.substr(child->m_prefix.size());
        auto [new_child, leaf] = delete_key(child, remaining);
        if (!new_child) {
            return {nullptr, std::nullopt};
        }

        auto new_n = write_node(n);
        if (!new_child->m_leaf && new_child->m_edges.empty()) {
            new_n->del_edge(first_char);
            if (n != m_tree.m_root && new_n->m_edges.size() == 1 && !new_n->is_leaf()) {
                merge_child(new_n);
            }
        } else {
            new_n->m_edges[idx].m_node = new_child;
        }
        return {new_n, leaf};
    }
};

} // namespace mc::im

#endif // MC_IM_TRANSACTION_H