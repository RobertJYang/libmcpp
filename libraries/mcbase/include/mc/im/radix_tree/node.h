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
#include <mc/im/ref_list.h>
#include <mc/memory.h>
#include <memory>
#include <optional>
#include <vector>

namespace mc::im {

/**
 * 树配置模板类，集中管理树的各种类型参数
 * @tparam ValueType 叶子节点存储的数据类型
 * @tparam Allocator 内存分配器类型
 * @tparam IsLess 是否使用小于比较器，true为升序（默认），false为降序
 */
template <typename ValueType = void*, typename Allocator = std::allocator<char>, bool IsLess = true>
struct tree_config {
    using value_type              = ValueType;
    using leaf_type               = std::optional<ValueType>;
    using allocator_type          = Allocator;
    static constexpr bool is_less = IsLess;
};

// 默认配置
using default_tree_config = tree_config<>;

// 前向声明node类，用于edge中的引用
template <typename Config = default_tree_config>
class node;

template <typename Config = default_tree_config>
using walk_fn = std::function<bool(key_view, const typename Config::value_type&)>;

/**
 * 边结构，表示从一个节点到另一个节点的连接
 */
template <typename Node>
class edge {
public:
    using ref_ptr_type = typename Node::ref_ptr_type;

    // 构造函数实现
    edge(uint8_t label, ref_ptr_type node) : m_label(label), m_node(std::move(node))
    {}

    ~edge()
    {
        m_node.reset();
    }

    friend bool operator==(const edge& lhs, const edge& rhs)
    {
        return lhs.m_label == rhs.m_label && lhs.m_node == rhs.m_node;
    }

    friend bool operator!=(const edge& lhs, const edge& rhs)
    {
        return !(lhs == rhs);
    }

    uint8_t      m_label;
    ref_ptr_type m_node;
};

template <typename Node>
struct edge_less {
    bool operator()(const edge<Node>& a, const edge<Node>& b) const
    {
        return a.m_label < b.m_label;
    }

    bool operator()(const edge<Node>& a, uint8_t b) const
    {
        return a.m_label < b;
    }

    bool operator()(uint8_t a, const edge<Node>& b) const
    {
        return a < b.m_label;
    }

    bool operator()(uint8_t a, uint8_t b) const
    {
        return a < b;
    }

    // 添加对key_view和key_buffer的比较支持
    bool operator()(const key_view& a, const key_view& b) const
    {
        return std::less<std::string_view>()(a, b);
    }

    bool operator()(const key_buffer<>& a, const key_view& b) const
    {
        return (*this)(key_view(a.data(), a.size()), b);
    }

    bool operator()(const key_view& a, const key_buffer<>& b) const
    {
        return (*this)(a, key_view(b.data(), b.size()));
    }

    bool operator()(const key_buffer<>& a, const key_buffer<>& b) const
    {
        return (*this)(key_view(a.data(), a.size()), key_view(b.data(), b.size()));
    }
};

template <typename Node>
struct edge_greater {
    bool operator()(const edge<Node>& a, const edge<Node>& b) const
    {
        return a.m_label > b.m_label;
    }

    bool operator()(const edge<Node>& a, uint8_t b) const
    {
        return a.m_label > b;
    }

    bool operator()(uint8_t a, const edge<Node>& b) const
    {
        return a > b.m_label;
    }

    bool operator()(uint8_t a, uint8_t b) const
    {
        return a > b;
    }

    // 添加对key_view和key_buffer的比较支持
    bool operator()(const key_view& a, const key_view& b) const
    {
        return std::greater<std::string_view>()(a, b);
    }

    bool operator()(const key_buffer<>& a, const key_view& b) const
    {
        return (*this)(key_view(a.data(), a.size()), b);
    }

    bool operator()(const key_view& a, const key_buffer<>& b) const
    {
        return (*this)(a, key_view(b.data(), b.size()));
    }

    bool operator()(const key_buffer<>& a, const key_buffer<>& b) const
    {
        return (*this)(key_view(a.data(), a.size()), key_view(b.data(), b.size()));
    }
};

/**
 * 边的集合类型
 */
template <typename Node>
using edges =
    std::vector<edge<Node>,
                typename std::allocator_traits<typename Node::allocator_type>::template rebind_alloc<edge<Node>>>;

/**
 * 节点类，表示基数树中的一个节点
 * 继承自 enable_shared_from_this
 * @tparam Config 树配置类型
 */
template <typename Config>
class node : public enable_shared_from_this<node<Config>> {
public:
    // 从配置中提取类型
    using leaf_type              = typename Config::leaf_type;
    using allocator_type         = typename Config::allocator_type;
    static constexpr bool IsLess = Config::is_less;

    using alloc_traits = std::allocator_traits<allocator_type>;
    using alloc_type   = typename alloc_traits::template rebind_alloc<node>;
    using pointer_type = typename alloc_type::pointer;
    using key_type     = key_buffer<typename alloc_traits::template rebind_alloc<char>>;
    using edge_type    = edge<node>;
    using edges_type   = edges<node>;
    using ref_ptr_type = shared_ptr<node, default_deleter<node>, pointer_type>;
    using list_type    = ref_list<node, pointer_type>;
    using compare_type = std::conditional_t<IsLess, edge_less<node>, edge_greater<node>>;

    // 默认构造函数
    node()
    {}

    // 基本构造函数
    explicit node(leaf_type leaf) : m_leaf(leaf)
    {}

    // 带前缀构造，使用key_view
    node(leaf_type leaf, key_view prefix, edges_type edges = {})
        : m_leaf(leaf), m_prefix(prefix), m_edges(std::move(edges)), m_version(0)
    {}

    // 带前缀构造，使用key_type右值引用
    node(leaf_type leaf, key_type prefix, edges_type edges = {})
        : m_leaf(leaf), m_prefix(std::move(prefix)), m_edges(std::move(edges)), m_version(0)
    {}

    // 带分配器的构造函数
    node(const allocator_type& alloc, leaf_type leaf) : m_leaf(leaf), m_prefix(alloc), m_edges(alloc), m_alloc(alloc)
    {}

    // 带分配器和前缀的构造函数
    node(const allocator_type& alloc, leaf_type leaf, key_view prefix, edges_type edges = {})
        : m_leaf(leaf), m_prefix(alloc, prefix), m_edges(alloc, std::move(edges)), m_version(0), m_alloc(alloc)
    {}

    // 带分配器和前缀的构造函数（前缀为右值引用）
    node(const allocator_type& alloc, leaf_type leaf, key_type prefix, edges_type edges = {})
        : m_leaf(leaf), m_prefix(alloc, std::move(prefix)), m_edges(alloc, std::move(edges)), m_version(0),
          m_alloc(alloc)
    {}

    ~node()
    {
        m_edges.clear();
    }

    // 判断节点是否为叶子节点
    bool is_leaf() const
    {
        return m_leaf.has_value();
    }

    // 添加边
    void add_edge(const edge_type& e)
    {
        auto it = std::upper_bound(m_edges.begin(), m_edges.end(), e, compare_type());
        m_edges.insert(it, e);
    }

    // 替换边
    void replace_edge(const edge_type& e)
    {
        auto [idx, node_ptr] = get_edge(e.m_label);
        if (idx >= 0) {
            m_edges[idx] = e;
        }
    }

    // 查找边
    std::pair<int, ref_ptr_type> get_edge(uint8_t label) const
    {
        auto it = std::lower_bound(m_edges.begin(), m_edges.end(), label, compare_type());
        if (it != m_edges.end() && it->m_label == label) {
            return {static_cast<int>(std::distance(m_edges.begin(), it)), it->m_node};
        }
        return {-1, nullptr};
    }

    // 删除边
    void del_edge(uint8_t label)
    {
        auto [idx, node_ptr] = get_edge(label);
        if (idx >= 0) {
            m_edges.erase(m_edges.begin() + idx);
        }
    }

    // 查找键
    leaf_type get(key_view k) const
    {
        auto prefix_len = longest_prefix(m_prefix, k);
        if (prefix_len != m_prefix.size()) {
            return std::nullopt;
        } else if (prefix_len == k.size()) {
            return m_leaf;
        }

        auto [idx, child] = get_edge(k[prefix_len]);
        if (idx >= 0) {
            key_view remaining = k.substr(prefix_len);
            return child->get(remaining);
        }

        return std::nullopt;
    }

    // 遍历树
    void walk(walk_fn<Config>&& fn) const
    {
        recursive_walk(this, std::forward<walk_fn<Config>>(fn));
    }

    // 遍历指定前缀的子树
    void walk_prefix(key_view prefix, walk_fn<Config>&& fn) const
    {
        // 使用longest_prefix函数获取公共前缀长度
        size_t common_len = longest_prefix(m_prefix, prefix);
        size_t prefix_len = m_prefix.size();
        size_t p_len      = prefix.size();

        // 如果查询前缀是节点前缀的前缀，遍历所有节点
        if (common_len == p_len) {
            recursive_walk(this, std::forward<walk_fn<Config>>(fn));
        } else if (common_len == prefix_len) {
            auto [idx, child] = get_edge(prefix[prefix_len]);
            if (idx >= 0) {
                key_view remaining = prefix.substr(prefix_len);
                child->walk_prefix(remaining, std::forward<walk_fn<Config>>(fn));
            }
        }
    }

    friend bool operator==(const node& lhs, const node& rhs)
    {
        return lhs.m_leaf == rhs.m_leaf && lhs.m_prefix == rhs.m_prefix && lhs.m_edges == rhs.m_edges;
    }

    friend bool operator!=(const node& lhs, const node& rhs)
    {
        return !(lhs == rhs);
    }

private:
    // 递归遍历
    static bool recursive_walk(const node* n, walk_fn<Config>&& fn)
    {
        if (n->is_leaf()) {
            if (!fn(n->m_prefix, n->m_leaf.value())) {
                return false;
            }
        }

        // 遍历所有子节点
        for (const auto& e : n->m_edges) {
            if (!recursive_walk(e.m_node.get(), std::forward<walk_fn<Config>>(fn))) {
                return false;
            }
        }

        return true;
    }

public:
    leaf_type    m_leaf;
    key_type     m_prefix;
    edges_type   m_edges;
    int          m_version = 0;
    ref_ptr_type m_next;
    ref_ptr_type m_prev;
    alloc_type   m_alloc;
};

// 常用类型别名，简化使用
using default_node = node<default_tree_config>;
using reverse_node = node<tree_config<void, std::allocator<char>, false>>;

template <typename Node>
using node_list = typename Node::list_type;

} // namespace mc::im

#endif // MC_IM_NODE_H