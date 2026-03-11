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

#ifndef MC_IM_RADIX_TREE_IMPL_H
#define MC_IM_RADIX_TREE_IMPL_H

namespace mc::im {

// radix_tree 实现部分

template <typename Config>
radix_tree<Config>::radix_tree(node_ptr root, size_t size)
    : m_root(std::move(root)), m_size(size)
{
}

template <typename Config>
size_t radix_tree<Config>::size() const
{
    return m_size;
}

template <typename Config>
const typename radix_tree<Config>::node_type radix_tree<Config>::root() const
{
    return m_root;
}

template <typename Config>
typename radix_tree<Config>::leaf_type radix_tree<Config>::get(key_view key) const
{
    if (!m_root) {
        return std::nullopt;
    }
    return m_root->get(key);
}

template <typename Config>
typename radix_tree<Config>::iterator radix_tree<Config>::find(key_view key)
{
    if (!m_root || m_size == 0) {
        return end();
    }

    if (key.empty()) {
        return begin();
    }

    path_type    path;
    key_buffer<> key_buf;

    key_buf.append(m_root->m_prefix);
    path.push_back({m_root, 0, 0});

    using compare_type = typename node_type::compare_type;
    compare_type compare;

    // 深度优先遍历树，寻找第一个大于等于key的节点
    size_t key_pos = 0; // 当前匹配到key的位置
    while (!path.empty()) {
        auto& current = path.back();
        auto  n       = current.node;
        auto& edges   = n->m_edges;

        // 如果当前节点已经处理完毕，回溯
        key_pos = current.prefix_size;
        if (current.edge_index >= edges.size() || key_pos >= key.size()) {
            break;
        }

        // 找到第一个大于等于key当前位置的边
        auto s  = edges.begin() + current.edge_index;
        auto it = std::lower_bound(s, edges.end(), key[key_pos], compare);
        if (it == edges.end()) {
            break;
        }

        // 更新当前路径节点的下一个要检查的边索引
        current.edge_index = std::distance(edges.begin(), it) + 1;

        // 准备下一级节点
        auto& edge = *it;
        key_buf.resize(current.prefix_size);
        key_buf.append(edge.m_node->m_prefix);
        path.push_back({edge.m_node, 0, key_buf.size()});

        if (edge.m_node->m_prefix == key.substr(key_pos)) {
            if (!edge.m_node->is_leaf()) {
                break;
            }
            return make_iterator(edge.m_node, std::move(key_buf), std::move(path));
        }
    }

    return end();
}

template <typename Config>
typename radix_tree<Config>::iterator radix_tree<Config>::lower_bound(key_view key)
{
    if (!m_root || m_size == 0) {
        return end();
    }

    if (key.empty()) {
        return begin();
    }

    path_type    path;
    key_buffer<> key_buf;

    key_buf.append(m_root->m_prefix);
    path.push_back({m_root, 0, 0});

    using compare_type = typename node_type::compare_type;
    compare_type compare;

    // 深度优先遍历树，寻找第一个大于等于key的节点
    size_t key_pos = 0; // 当前匹配到key的位置
    while (!path.empty()) {
        auto& current = path.back();
        auto  n       = current.node;
        auto& edges   = n->m_edges;

        // 如果当前节点已经处理完毕，回溯
        if (current.edge_index >= edges.size()) {
            key_buf.resize(current.prefix_size);
            path.pop_back();
            continue;
        }

        // 计算当前应该比较的key位置
        key_pos = current.prefix_size;

        // 找到第一个大于等于key当前位置的边
        auto s = edges.begin() + current.edge_index;
        auto it =
            key_pos < key.size()
                ? std::lower_bound(s, edges.end(), static_cast<uint8_t>(key[key_pos]), compare)
                : s; // 如果key已经用完，从当前位置开始

        if (it == edges.end()) {
            // 没有找到合适的边，需要回溯
            key_buf.resize(current.prefix_size);
            path.pop_back();
            continue;
        }

        // 更新当前路径节点的下一个要检查的边索引
        current.edge_index = std::distance(edges.begin(), it) + 1;

        // 准备下一级节点
        auto& edge = *it;
        key_buf.resize(current.prefix_size);
        key_buf.append(edge.m_node->m_prefix);
        path.push_back({edge.m_node, 0, key_buf.size()});

        // 如果找到叶子节点且键满足要求，返回迭代器
        if (edge.m_node->is_leaf() && (key_buf == key || !compare(key_buf, key))) {
            return make_iterator(edge.m_node, std::move(key_buf), std::move(path));
        }
    }

    return end();
}

template <typename Config>
typename radix_tree<Config>::iterator radix_tree<Config>::upper_bound(key_view key)
{
    if (!m_root || m_size == 0) {
        return end();
    }

    path_type    path;
    key_buffer<> key_buf;

    key_buf.append(m_root->m_prefix);
    path.push_back({m_root, 0, 0});

    using compare_type = typename node_type::compare_type;
    compare_type compare;

    // 深度优先遍历树，寻找第一个大于key的节点
    size_t key_pos = 0; // 当前匹配到key的位置
    while (!path.empty()) {
        auto& current = path.back();
        auto  n       = current.node;
        auto& edges   = n->m_edges;

        // 如果当前节点已经处理完毕，回溯
        if (current.edge_index >= edges.size()) {
            key_buf.resize(current.prefix_size);
            path.pop_back();
            continue;
        }

        // 计算当前应该比较的key位置
        key_pos = current.prefix_size;

        // 找到第一个大于等于key当前位置的边
        auto s  = edges.begin() + current.edge_index;
        auto it = key_pos < key.size() ? std::lower_bound(s, edges.end(), key[key_pos], compare)
                                       : s; // 如果key已经用完，从当前位置开始

        if (it == edges.end()) {
            // 没有找到合适的边，需要回溯
            key_buf.resize(current.prefix_size);
            path.pop_back();
            continue;
        }

        // 更新当前路径节点的下一个要检查的边索引
        current.edge_index = std::distance(edges.begin(), it) + 1;

        // 准备下一级节点
        auto& edge = *it;
        key_buf.resize(current.prefix_size);
        key_buf.append(edge.m_node->m_prefix);
        path.push_back({edge.m_node, 0, key_buf.size()});

        // 如果找到叶子节点且键严格大于给定键，返回迭代器
        if (edge.m_node->is_leaf() && compare(key, key_buf)) {
            return make_iterator(edge.m_node, std::move(key_buf), std::move(path));
        }
    }

    return end();
}

// 创建一个迭代器并设置其状态
template <typename Config>
typename radix_tree<Config>::iterator
radix_tree<Config>::make_iterator(node_ptr n, key_buffer<>&& key_buf, path_type&& path)
{
    iterator result;
    result.m_is_end       = false;
    result.m_current_node = n;
    result.m_key_buffer   = std::move(key_buf);
    result.m_path         = std::move(path);
    result.update_current_item();
    return result;
}

} // namespace mc::im

#endif // MC_IM_RADIX_TREE_IMPL_H