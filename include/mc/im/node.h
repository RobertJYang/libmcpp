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

#ifndef MC_IM_NODE_H
#define MC_IM_NODE_H

#include <algorithm>
#include <cstdint>
#include <functional>
#include <mc/im/key_buffer.h>
#include <mc/im/ref_base.h>
#include <mc/im/ref_list.h>
#include <mc/im/ref_ptr.h>
#include <memory>
#include <vector>

namespace mc::im {

// 前向声明node类，用于edge中的引用
template <typename Allocator = std::allocator<void>, bool IsLess = true>
class node;

using leaf_type       = void*;
using const_leaf_type = const void*;

using walk_fn = std::function<bool(const key_view&, const_leaf_type)>;

/**
 * 边结构，表示从一个节点到另一个节点的连接
 */
template <typename Node>
class edge {
public:
    using ref_ptr_type = typename Node::ref_ptr_type;

    // 构造函数实现
    edge(uint8_t label, ref_ptr_type node) : m_label(label), m_node(std::move(node)) {
    }

    ~edge() {
        m_node.reset();
    }

    friend bool operator==(const edge& lhs, const edge& rhs) {
        return lhs.m_label == rhs.m_label && lhs.m_node == rhs.m_node;
    }

    friend bool operator!=(const edge& lhs, const edge& rhs) {
        return !(lhs == rhs);
    }

    uint8_t      m_label;
    ref_ptr_type m_node;
};

template <typename Node>
struct edge_less {
    bool operator()(const edge<Node>& a, const edge<Node>& b) const {
        return a.m_label < b.m_label;
    }

    bool operator()(const edge<Node>& a, uint8_t b) const {
        return a.m_label < b;
    }

    bool operator()(uint8_t a, const edge<Node>& b) const {
        return a < b.m_label;
    }
};

template <typename Node>
struct edge_greater {
    bool operator()(const edge<Node>& a, const edge<Node>& b) const {
        return a.m_label > b.m_label;
    }

    bool operator()(const edge<Node>& a, uint8_t b) const {
        return a.m_label > b;
    }

    bool operator()(uint8_t a, const edge<Node>& b) const {
        return a > b.m_label;
    }
};

/**
 * 边的集合类型
 */
template <typename Node>
using edges =
    std::vector<edge<Node>, typename std::allocator_traits<
                                typename Node::allocator_type>::template rebind_alloc<edge<Node>>>;

/**
 * 节点类，表示基数树中的一个节点
 * 继承自 ref_base
 * @tparam Allocator 内存分配器类型
 * @tparam IsLess 是否使用小于比较器，true为升序（默认），false为降序
 */
template <typename Allocator, bool IsLess>
class node : public ref_base<node<Allocator, IsLess>> {
public:
    using alloc_traits   = std::allocator_traits<Allocator>;
    using allocator_type = typename alloc_traits::template rebind_alloc<node>;
    using pointer_type   = typename allocator_type::pointer;
    using key_type       = key_buffer<typename alloc_traits::template rebind_alloc<char>>;
    using edge_type      = edge<node>;
    using edges_type     = edges<node>;
    using ref_ptr_type   = ref_ptr<node, pointer_type>;
    using list_type      = ref_list<node, pointer_type>;
    using compare_type   = std::conditional_t<IsLess, edge_less<node>, edge_greater<node>>;

    // 默认构造函数
    node() {
    }

    // 基本构造函数
    explicit node(leaf_type leaf) : m_leaf(leaf) {
    }

    // 带前缀构造，使用key_view
    node(leaf_type leaf, const key_view& prefix, edges_type edges = {})
        : m_leaf(leaf), m_prefix(prefix), m_edges(std::move(edges)), m_version(0) {
    }

    // 带前缀构造，使用key_type右值引用
    node(leaf_type leaf, key_type prefix, edges_type edges = {})
        : m_leaf(leaf), m_prefix(std::move(prefix)), m_edges(std::move(edges)), m_version(0) {
    }

    // 带分配器的构造函数
    node(const Allocator& alloc, leaf_type leaf)
        : m_leaf(leaf), m_prefix(alloc), m_edges(alloc), m_alloc(alloc) {
    }

    // 带分配器和前缀的构造函数
    node(const Allocator& alloc, leaf_type leaf, const key_view& prefix, edges_type edges = {})
        : m_leaf(leaf), m_prefix(alloc, prefix), m_edges(alloc, std::move(edges)), m_version(0),
          m_alloc(alloc) {
    }

    // 带分配器和前缀的构造函数（前缀为右值引用）
    node(const Allocator& alloc, leaf_type leaf, key_type prefix, edges_type edges = {})
        : m_leaf(leaf), m_prefix(alloc, std::move(prefix)), m_edges(alloc, std::move(edges)),
          m_version(0), m_alloc(alloc) {
    }

    ~node() {
        m_edges.clear();
    }

    // 判断节点是否为叶子节点
    bool is_leaf() const {
        return m_leaf != nullptr;
    }

    // 添加边
    void add_edge(const edge_type& e) {
        auto it = std::upper_bound(m_edges.begin(), m_edges.end(), e, compare_type());
        m_edges.insert(it, e);
    }

    // 替换边
    void replace_edge(const edge_type& e) {
        auto [idx, node_ptr] = get_edge(e.m_label);
        if (idx >= 0) {
            m_edges[idx] = e;
        }
    }

    // 查找边
    std::pair<int, ref_ptr_type> get_edge(uint8_t label) const {
        // 使用二分查找
        compare_type compare;

        // 对不同排序方式使用不同的查找逻辑
        auto it = std::lower_bound(m_edges.begin(), m_edges.end(), label, compare);

        // 检查是否找到了匹配的边
        if (it != m_edges.end() && it->m_label == label) {
            return {static_cast<int>(std::distance(m_edges.begin(), it)), it->m_node};
        }

        return {-1, nullptr};
    }

    // 删除边
    void del_edge(uint8_t label) {
        auto [idx, node_ptr] = get_edge(label);
        if (idx >= 0) {
            m_edges.erase(m_edges.begin() + idx);
        }
    }

    // 查找键
    std::pair<leaf_type, bool> get(const key_view& k) const {
        // 如果前缀不匹配，则返回失败
        size_t prefix_len = m_prefix.size();
        size_t k_len      = k.size();

        // 前缀比查询键长，肯定不匹配
        if (prefix_len > k_len) {
            return {nullptr, false};
        }

        // 逐字节比较前缀
        for (size_t i = 0; i < prefix_len; i++) {
            if (m_prefix[i] != k[i]) {
                return {nullptr, false};
            }
        }

        // 如果前缀正好是整个键，且当前节点是叶子节点，则找到
        if (prefix_len == k_len) {
            return {m_leaf, m_leaf != nullptr};
        }

        // 尝试在子节点中查找剩余部分
        auto [idx, child] = get_edge(k[prefix_len]);
        if (idx >= 0) {
            key_view remaining = k.substr(prefix_len);
            return child->get(remaining);
        }

        return {nullptr, false};
    }

    // 遍历树
    void walk(const walk_fn& fn) const {
        recursive_walk(this, fn);
    }

    // 遍历指定前缀的子树
    void walk_prefix(const key_view& prefix, const walk_fn& fn) const {
        // 如果前缀不匹配，则返回
        size_t prefix_len = m_prefix.size();
        size_t p_len      = prefix.size();

        // 如果节点前缀比查询前缀长，检查节点前缀是否以查询前缀开头
        if (prefix_len > p_len) {
            for (size_t i = 0; i < p_len; i++) {
                if (m_prefix[i] != prefix[i]) {
                    return;
                }
            }
            // 节点前缀以查询前缀开头，遍历所有节点
            recursive_walk(this, fn);
            return;
        }

        // 逐字节比较前缀
        for (size_t i = 0; i < prefix_len; i++) {
            if (m_prefix[i] != prefix[i]) {
                return;
            }
        }

        // 如果节点前缀是查询前缀的前缀，继续查找
        if (prefix_len == p_len) {
            // 前缀完全匹配，遍历所有节点
            recursive_walk(this, fn);
        } else {
            // 继续在子节点中查找
            auto [idx, child] = get_edge(prefix[prefix_len]);
            if (idx >= 0) {
                key_view remaining = prefix.substr(prefix_len);
                child->walk_prefix(remaining, fn);
            }
        }
    }

    // 设置链接（在目标节点之后）
    void set_links_after(ref_ptr_type pos) {
        node* next = pos->m_next.get();

        this->m_prev = pos;
        if (next) {
            next->m_prev = ref_ptr_type(this);
            this->m_next = ref_ptr_type(next);
        } else {
            this->m_next = nullptr;
        }

        pos->m_next = ref_ptr_type(this);
    }

    // 设置链接（在目标节点之前）
    void set_links_before(ref_ptr_type pos) {
        node* prev = pos->m_prev.get();

        this->m_next = pos;
        if (prev) {
            prev->m_next = ref_ptr_type(this);
            this->m_prev = ref_ptr_type(prev);
        } else {
            this->m_prev = nullptr;
        }

        pos->m_prev = ref_ptr_type(this);
    }

    friend bool operator==(const node& lhs, const node& rhs) {
        return lhs.m_leaf == rhs.m_leaf && lhs.m_prefix == rhs.m_prefix &&
               lhs.m_edges == rhs.m_edges;
    }

    friend bool operator!=(const node& lhs, const node& rhs) {
        return !(lhs == rhs);
    }

private:
    // 递归遍历
    static bool recursive_walk(const node* n, const walk_fn& fn) {
        if (n->is_leaf()) {
            if (!fn(n->m_prefix, n->m_leaf)) {
                return false;
            }
        }

        // 遍历所有子节点
        for (const auto& e : n->m_edges) {
            if (!recursive_walk(e.m_node.get(), fn)) {
                return false;
            }
        }

        return true;
    }

public:
    leaf_type      m_leaf = nullptr;
    key_type       m_prefix;
    edges_type     m_edges;
    size_t         m_version = 0;
    ref_ptr_type   m_next;
    ref_ptr_type   m_prev;
    allocator_type m_alloc;
};

// 常用类型别名，简化使用
using default_node = node<std::allocator<void>, true>;  // 默认升序
using reverse_node = node<std::allocator<void>, false>; // 降序节点

template <typename Node>
using node_list = typename Node::list_type;

} // namespace mc::im

#endif // MC_IM_NODE_H