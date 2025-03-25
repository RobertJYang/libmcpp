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

#include <mc/im/raw_iterator.h>
#include <mc/im/node.h>
#include <mc/im/iradix.h>

namespace mc::im {

raw_iterator::raw_iterator() : m_tree(nullptr) {}

void raw_iterator::init(const iradix_tree* tree) {
    m_tree = tree;
    m_stack.clear();
    if (m_tree && m_tree->root()) {
        new_stack(std::shared_ptr<node>(const_cast<node*>(m_tree->root())), {});
    }
}

bool raw_iterator::next() {
    return next_node();
}

const std::vector<uint8_t>& raw_iterator::key() const {
    static const std::vector<uint8_t> empty;
    if (m_stack.empty()) {
        return empty;
    }
    return m_stack.back().key;
}

void* raw_iterator::leaf() const {
    if (m_stack.empty()) {
        return nullptr;
    }
    return m_stack.back().n->m_leaf;
}

bool raw_iterator::seek_prefix(const std::vector<uint8_t>& prefix) {
    if (!m_tree || !m_tree->root()) {
        return false;
    }

    m_stack.clear();
    std::shared_ptr<node> n = std::shared_ptr<node>(const_cast<node*>(m_tree->root()));
    std::vector<uint8_t> key;

    while (n) {
        if (n->m_leaf) {
            if (key.size() >= prefix.size() && 
                std::equal(prefix.begin(), prefix.end(), key.begin())) {
                new_stack(n, key);
                return true;
            }
        }

        bool found = false;
        for (const auto& edge : n->m_edges) {
            if (edge.m_label == prefix[key.size()]) {
                key.push_back(edge.m_label);
                n = edge.m_node;
                found = true;
                break;
            }
        }

        if (!found) {
            break;
        }
    }

    return false;
}

void raw_iterator::new_stack(std::shared_ptr<node> n, const std::vector<uint8_t>& key) {
    t_stack stack;
    stack.n = n;
    stack.key = key;
    stack.i = 0;
    m_stack.push_back(stack);
}

void raw_iterator::pop_stack() {
    if (!m_stack.empty()) {
        m_stack.pop_back();
    }
}

bool raw_iterator::next_node() {
    while (!m_stack.empty()) {
        auto& stack = m_stack.back();
        if (stack.i < stack.n->m_edges.size()) {
            const auto& edge = stack.n->m_edges[stack.i++];
            std::vector<uint8_t> new_key = stack.key;
            new_key.push_back(edge.m_label);
            new_stack(edge.m_node, new_key);
            return true;
        }
        pop_stack();
    }
    return false;
}

} // namespace mc::im 