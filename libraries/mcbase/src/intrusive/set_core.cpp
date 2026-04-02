/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <mc/intrusive/detail/set_core.h>

namespace mc::intrusive::detail {

set_hook_state* set_core::root() noexcept
{
    return m_root;
}

const set_hook_state* set_core::root() const noexcept
{
    return m_root;
}

// --- Rotation helpers ---

void set_core::rotate_left(set_hook_state* node, set_hook_state*& root) noexcept
{
    auto* child = node->right;
    node->right = child->left;
    if (child->left != nullptr) {
        child->left->parent = node;
    }
    child->parent = node->parent;
    if (node->parent == nullptr) {
        root = child;
    } else if (node == node->parent->left) {
        node->parent->left = child;
    } else {
        node->parent->right = child;
    }
    child->left  = node;
    node->parent = child;
}

void set_core::rotate_right(set_hook_state* node, set_hook_state*& root) noexcept
{
    auto* child = node->left;
    node->left = child->right;
    if (child->right != nullptr) {
        child->right->parent = node;
    }
    child->parent = node->parent;
    if (node->parent == nullptr) {
        root = child;
    } else if (node == node->parent->right) {
        node->parent->right = child;
    } else {
        node->parent->left = child;
    }
    child->right = node;
    node->parent = child;
}

// --- Rebalance after insert (CLRS-style) ---

void set_core::rebalance_after_insert(set_hook_state* node, set_hook_state*& root) noexcept
{
    while (node != root && node->parent->is_red) {
        auto* parent = node->parent;
        auto* grand  = parent->parent;
        if (grand == nullptr) {
            break;
        }
        if (parent == grand->left) {
            auto* uncle = grand->right;
            if (uncle != nullptr && uncle->is_red) {
                // Case 1: uncle is red — recolor
                parent->is_red = false;
                uncle->is_red  = false;
                grand->is_red  = true;
                node           = grand;
            } else {
                if (node == parent->right) {
                    // Case 2: left-rotate to convert to case 3
                    node   = parent;
                    rotate_left(node, root);
                    parent = node->parent;
                    grand  = parent->parent;
                }
                // Case 3: recolor + right-rotate
                parent->is_red = false;
                grand->is_red  = true;
                rotate_right(grand, root);
            }
        } else {
            auto* uncle = grand->left;
            if (uncle != nullptr && uncle->is_red) {
                parent->is_red = false;
                uncle->is_red  = false;
                grand->is_red  = true;
                node           = grand;
            } else {
                if (node == parent->left) {
                    node   = parent;
                    rotate_right(node, root);
                    parent = node->parent;
                    grand  = parent->parent;
                }
                parent->is_red = false;
                grand->is_red  = true;
                rotate_left(grand, root);
            }
        }
    }
    root->is_red = false;
}

// --- Transplant ---

void set_core::transplant(set_hook_state* u, set_hook_state* v, set_hook_state*& root) noexcept
{
    if (u->parent == nullptr) {
        root = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }
    if (v != nullptr) {
        v->parent = u->parent;
    }
}

// --- Rebalance after erase (CLRS-style) ---

void set_core::rebalance_after_erase(set_hook_state* node, set_hook_state* parent,
                                     set_hook_state*& root) noexcept
{
    while (node != root && (node == nullptr || !node->is_red)) {
        if (node == parent->left) {
            auto* sibling = parent->right;
            if (sibling == nullptr) {
                break;
            }
            if (sibling->is_red) {
                sibling->is_red = false;
                parent->is_red  = true;
                rotate_left(parent, root);
                sibling = parent->right;
                if (sibling == nullptr) {
                    break;
                }
            }
            if ((sibling->left == nullptr || !sibling->left->is_red) &&
                (sibling->right == nullptr || !sibling->right->is_red)) {
                sibling->is_red = true;
                node            = parent;
                parent          = parent->parent;
            } else {
                if (sibling->right == nullptr || !sibling->right->is_red) {
                    if (sibling->left != nullptr) {
                        sibling->left->is_red = false;
                    }
                    sibling->is_red = true;
                    rotate_right(sibling, root);
                    sibling = parent->right;
                }
                sibling->is_red = parent->is_red;
                parent->is_red  = false;
                if (sibling->right != nullptr) {
                    sibling->right->is_red = false;
                }
                rotate_left(parent, root);
                break;
            }
        } else {
            auto* sibling = parent->left;
            if (sibling == nullptr) {
                break;
            }
            if (sibling->is_red) {
                sibling->is_red = false;
                parent->is_red  = true;
                rotate_right(parent, root);
                sibling = parent->left;
                if (sibling == nullptr) {
                    break;
                }
            }
            if ((sibling->right == nullptr || !sibling->right->is_red) &&
                (sibling->left == nullptr || !sibling->left->is_red)) {
                sibling->is_red = true;
                node            = parent;
                parent          = parent->parent;
            } else {
                if (sibling->left == nullptr || !sibling->left->is_red) {
                    if (sibling->right != nullptr) {
                        sibling->right->is_red = false;
                    }
                    sibling->is_red = true;
                    rotate_left(sibling, root);
                    sibling = parent->left;
                }
                sibling->is_red = parent->is_red;
                parent->is_red  = false;
                if (sibling->left != nullptr) {
                    sibling->left->is_red = false;
                }
                rotate_right(parent, root);
                break;
            }
        }
    }
    if (node != nullptr) {
        node->is_red = false;
    }
}

void set_core::reset_hook(set_hook_state* node) noexcept
{
    node->parent = nullptr;
    node->left   = nullptr;
    node->right  = nullptr;
    node->is_red = true;
}

// --- Insert ---

set_hook_state* set_core::insert(set_hook_state* node, set_compare_fn cmp, const void* ctx) noexcept
{
    reset_hook(node);

    set_hook_state* parent = nullptr;
    set_hook_state* current = m_root;

    while (current != nullptr) {
        parent = current;
        if (cmp(node, current, ctx)) {
            current = current->left;
        } else if (cmp(current, node, ctx)) {
            current = current->right;
        } else {
            // Duplicate
            return current;
        }
    }

    node->parent = parent;
    if (parent == nullptr) {
        m_root = node;
    } else if (cmp(node, parent, ctx)) {
        parent->left = node;
    } else {
        parent->right = node;
    }

    rebalance_after_insert(node, m_root);
    return nullptr;
}

// --- Find ---

const set_hook_state* set_core::find(const void* key, set_compare_fn cmp, const void* ctx) const noexcept
{
    auto* current = m_root;
    while (current != nullptr) {
        if (cmp(key, current, ctx)) {
            current = current->left;
        } else if (cmp(current, key, ctx)) {
            current = current->right;
        } else {
            return current;
        }
    }
    return nullptr;
}

// --- Lower bound ---

set_hook_state* set_core::lower_bound(const void* key, set_compare_fn cmp, const void* ctx) noexcept
{
    return const_cast<set_hook_state*>(const_cast<const set_core*>(this)->lower_bound(key, cmp, ctx));
}

const set_hook_state* set_core::lower_bound(const void* key, set_compare_fn cmp, const void* ctx) const noexcept
{
    const set_hook_state* result  = nullptr;
    const set_hook_state* current = m_root;
    while (current != nullptr) {
        if (!cmp(current, key, ctx)) {
            result  = current;
            current = current->left;
        } else {
            current = current->right;
        }
    }
    return result;
}

// --- Upper bound ---

set_hook_state* set_core::upper_bound(const void* key, set_compare_fn cmp, const void* ctx) noexcept
{
    return const_cast<set_hook_state*>(const_cast<const set_core*>(this)->upper_bound(key, cmp, ctx));
}

const set_hook_state* set_core::upper_bound(const void* key, set_compare_fn cmp, const void* ctx) const noexcept
{
    const set_hook_state* result  = nullptr;
    const set_hook_state* current = m_root;
    while (current != nullptr) {
        if (cmp(key, current, ctx)) {
            result  = current;
            current = current->left;
        } else {
            current = current->right;
        }
    }
    return result;
}

// --- Erase ---

void set_core::erase(set_hook_state* node) noexcept
{
    auto* y      = node;
    auto* x      = static_cast<set_hook_state*>(nullptr);
    auto* x_par  = static_cast<set_hook_state*>(nullptr);
    bool  y_orig = y->is_red;

    if (node->left == nullptr) {
        x = node->right;
        transplant(node, node->right, m_root);
        x_par = node->parent;
    } else if (node->right == nullptr) {
        x = node->left;
        transplant(node, node->left, m_root);
        x_par = node->parent;
    } else {
        // Find successor (leftmost in right subtree)
        y     = node->right;
        while (y->left != nullptr) {
            y = y->left;
        }
        y_orig = y->is_red;
        x      = y->right;

        if (y->parent == node) {
            x_par = y;
        } else {
            transplant(y, y->right, m_root);
            y->right         = node->right;
            y->right->parent = y;
            x_par            = y->parent;
        }
        transplant(node, y, m_root);
        y->left         = node->left;
        y->left->parent = y;
        y->is_red       = node->is_red;
    }

    if (!y_orig && x_par != nullptr) {
        rebalance_after_erase(x, x_par, m_root);
    }

    reset_hook(node);
}

// --- Clear ---

void set_core::clear() noexcept
{
    // Iterative post-order traversal to reset all hooks
    auto* current = m_root;
    while (current != nullptr) {
        if (current->left != nullptr) {
            current = current->left;
        } else if (current->right != nullptr) {
            current = current->right;
        } else {
            auto* p = current->parent;
            if (p != nullptr) {
                if (p->left == current) {
                    p->left = nullptr;
                } else {
                    p->right = nullptr;
                }
            }
            reset_hook(current);
            current = p;
        }
    }
    m_root = nullptr;
}

void set_core::clear_and_dispose(set_dispose_fn fn, void* ctx)
{
    auto* current = m_root;
    while (current != nullptr) {
        if (current->left != nullptr) {
            current = current->left;
        } else if (current->right != nullptr) {
            current = current->right;
        } else {
            auto* p = current->parent;
            if (p != nullptr) {
                if (p->left == current) {
                    p->left = nullptr;
                } else {
                    p->right = nullptr;
                }
            }
            auto* to_dispose = current;
            reset_hook(to_dispose);
            current = p;
            fn(to_dispose, ctx);
        }
    }
    m_root = nullptr;
}

// --- Swap ---

void set_core::swap(set_core& other) noexcept
{
    auto* tmp       = m_root;
    m_root          = other.m_root;
    other.m_root    = tmp;
    // Re-parent
    if (m_root != nullptr) {
        m_root->parent = nullptr;
    }
    if (other.m_root != nullptr) {
        other.m_root->parent = nullptr;
    }
}

// --- Iteration ---

set_hook_state* set_core::begin() noexcept
{
    if (m_root == nullptr) {
        return nullptr;
    }
    auto* node = m_root;
    while (node->left != nullptr) {
        node = node->left;
    }
    return node;
}

const set_hook_state* set_core::begin() const noexcept
{
    if (m_root == nullptr) {
        return nullptr;
    }
    auto* node = m_root;
    while (node->left != nullptr) {
        node = node->left;
    }
    return node;
}

set_hook_state* set_core::last() noexcept
{
    if (m_root == nullptr) {
        return nullptr;
    }
    auto* node = m_root;
    while (node->right != nullptr) {
        node = node->right;
    }
    return node;
}

const set_hook_state* set_core::last() const noexcept
{
    if (m_root == nullptr) {
        return nullptr;
    }
    auto* node = m_root;
    while (node->right != nullptr) {
        node = node->right;
    }
    return node;
}

set_hook_state* set_core::next(set_hook_state* node) noexcept
{
    if (node == nullptr) {
        return nullptr;
    }
    if (node->right != nullptr) {
        node = node->right;
        while (node->left != nullptr) {
            node = node->left;
        }
        return node;
    }
    auto* p = node->parent;
    while (p != nullptr && node == p->right) {
        node = p;
        p    = p->parent;
    }
    return p;
}

const set_hook_state* set_core::next(const set_hook_state* node) noexcept
{
    return next(const_cast<set_hook_state*>(node));
}

set_hook_state* set_core::prev(set_hook_state* node) noexcept
{
    if (node == nullptr) {
        return nullptr;
    }
    if (node->left != nullptr) {
        node = node->left;
        while (node->right != nullptr) {
            node = node->right;
        }
        return node;
    }
    auto* p = node->parent;
    while (p != nullptr && node == p->left) {
        node = p;
        p    = p->parent;
    }
    return p;
}

const set_hook_state* set_core::prev(const set_hook_state* node) noexcept
{
    return prev(const_cast<set_hook_state*>(node));
}

std::size_t set_core::count_all() const noexcept
{
    std::size_t count = 0;
    for (auto* n = begin(); n != nullptr; n = next(n)) {
        ++count;
    }
    return count;
}

} // namespace mc::intrusive::detail
