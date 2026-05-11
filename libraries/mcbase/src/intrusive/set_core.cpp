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
    return resolve(m_root);
}

const set_hook_state* set_core::root() const noexcept
{
    return resolve(m_root);
}

set_hook_state* set_core::resolve(const offset_ptr<set_hook_state>& ref) const noexcept
{
    return ref.get();
}

// --- Rotation helpers ---

void set_core::rotate_left(set_hook_state* node, set_hook_state*& root_node) noexcept
{
    auto* child      = resolve(node->right);
    auto* child_left = resolve(child->left);
    node->right      = child->left;
    if (child_left != nullptr) {
        child_left->parent = node;
    }
    child->parent = node->parent;
    auto* parent  = resolve(node->parent);
    if (parent == nullptr) {
        root_node = child;
    } else if (node == resolve(parent->left)) {
        parent->left = child;
    } else {
        parent->right = child;
    }
    child->left  = node;
    node->parent = child;
}

void set_core::rotate_right(set_hook_state* node, set_hook_state*& root_node) noexcept
{
    auto* child       = resolve(node->left);
    auto* child_right = resolve(child->right);
    node->left        = child->right;
    if (child_right != nullptr) {
        child_right->parent = node;
    }
    child->parent = node->parent;
    auto* parent  = resolve(node->parent);
    if (parent == nullptr) {
        root_node = child;
    } else if (node == resolve(parent->right)) {
        parent->right = child;
    } else {
        parent->left = child;
    }
    child->right = node;
    node->parent = child;
}

// --- Rebalance after insert (CLRS-style) ---

void set_core::rebalance_after_insert(set_hook_state* node, set_hook_state*& root_node) noexcept
{
    while (node != root_node) {
        auto* parent = resolve(node->parent);
        if (parent == nullptr || !parent->is_red) {
            break;
        }
        auto* grand = resolve(parent->parent);
        if (grand == nullptr) {
            break;
        }
        if (parent == resolve(grand->left)) {
            auto* uncle = resolve(grand->right);
            if (uncle != nullptr && uncle->is_red) {
                // Case 1: uncle is red — recolor
                parent->is_red = false;
                uncle->is_red  = false;
                grand->is_red  = true;
                node           = grand;
            } else {
                if (node == resolve(parent->right)) {
                    // Case 2: left-rotate to convert to case 3
                    node = parent;
                    rotate_left(node, root_node);
                    parent = resolve(node->parent);
                    grand  = resolve(parent->parent);
                }
                // Case 3: recolor + right-rotate
                parent->is_red = false;
                grand->is_red  = true;
                rotate_right(grand, root_node);
            }
        } else {
            auto* uncle = resolve(grand->left);
            if (uncle != nullptr && uncle->is_red) {
                parent->is_red = false;
                uncle->is_red  = false;
                grand->is_red  = true;
                node           = grand;
            } else {
                if (node == resolve(parent->left)) {
                    node = parent;
                    rotate_right(node, root_node);
                    parent = resolve(node->parent);
                    grand  = resolve(parent->parent);
                }
                parent->is_red = false;
                grand->is_red  = true;
                rotate_left(grand, root_node);
            }
        }
    }
    if (root_node != nullptr) {
        root_node->is_red = false;
    }
}

// --- Transplant ---

void set_core::transplant(set_hook_state* u, set_hook_state* v, set_hook_state*& root_node) noexcept
{
    auto* parent = resolve(u->parent);
    if (parent == nullptr) {
        root_node = v;
    } else if (u == resolve(parent->left)) {
        parent->left = v;
    } else {
        parent->right = v;
    }
    if (v != nullptr) {
        v->parent = parent;
    }
}

// --- Rebalance after erase (CLRS-style) ---

void set_core::rebalance_after_erase(set_hook_state* node, set_hook_state* parent, set_hook_state*& root_node) noexcept
{
    while (node != root_node && (node == nullptr || !node->is_red)) {
        if (parent == nullptr) {
            break;
        }
        if (node == resolve(parent->left)) {
            auto* sibling = resolve(parent->right);
            if (sibling == nullptr) {
                break;
            }
            if (sibling->is_red) {
                sibling->is_red = false;
                parent->is_red  = true;
                rotate_left(parent, root_node);
                sibling = resolve(parent->right);
                if (sibling == nullptr) {
                    break;
                }
            }
            auto* sibling_left  = resolve(sibling->left);
            auto* sibling_right = resolve(sibling->right);
            if ((sibling_left == nullptr || !sibling_left->is_red) &&
                (sibling_right == nullptr || !sibling_right->is_red)) {
                sibling->is_red = true;
                node            = parent;
                parent          = resolve(parent->parent);
            } else {
                if (sibling_right == nullptr || !sibling_right->is_red) {
                    if (sibling_left != nullptr) {
                        sibling_left->is_red = false;
                    }
                    sibling->is_red = true;
                    rotate_right(sibling, root_node);
                    sibling       = resolve(parent->right);
                    sibling_right = resolve(sibling->right);
                }
                sibling->is_red = parent->is_red;
                parent->is_red  = false;
                if (sibling_right != nullptr) {
                    sibling_right->is_red = false;
                }
                rotate_left(parent, root_node);
                break;
            }
        } else {
            auto* sibling = resolve(parent->left);
            if (sibling == nullptr) {
                break;
            }
            if (sibling->is_red) {
                sibling->is_red = false;
                parent->is_red  = true;
                rotate_right(parent, root_node);
                sibling = resolve(parent->left);
                if (sibling == nullptr) {
                    break;
                }
            }
            auto* sibling_left  = resolve(sibling->left);
            auto* sibling_right = resolve(sibling->right);
            if ((sibling_right == nullptr || !sibling_right->is_red) &&
                (sibling_left == nullptr || !sibling_left->is_red)) {
                sibling->is_red = true;
                node            = parent;
                parent          = resolve(parent->parent);
            } else {
                if (sibling_left == nullptr || !sibling_left->is_red) {
                    if (sibling_right != nullptr) {
                        sibling_right->is_red = false;
                    }
                    sibling->is_red = true;
                    rotate_left(sibling, root_node);
                    sibling      = resolve(parent->left);
                    sibling_left = resolve(sibling->left);
                }
                sibling->is_red = parent->is_red;
                parent->is_red  = false;
                if (sibling_left != nullptr) {
                    sibling_left->is_red = false;
                }
                rotate_right(parent, root_node);
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
    node->parent.reset();
    node->left.reset();
    node->right.reset();
    node->is_red = true;
}

// --- Insert ---

set_hook_state* set_core::insert(set_hook_state* node, set_compare_fn cmp, const void* ctx) noexcept
{
    reset_hook(node);

    auto*           root_node = root();
    set_hook_state* parent    = nullptr;
    set_hook_state* current   = root_node;

    while (current != nullptr) {
        parent = current;
        if (cmp(node, current, ctx)) {
            current = resolve(current->left);
        } else if (cmp(current, node, ctx)) {
            current = resolve(current->right);
        } else {
            // Duplicate
            return current;
        }
    }

    node->parent = parent;
    if (parent == nullptr) {
        root_node = node;
    } else if (cmp(node, parent, ctx)) {
        parent->left = node;
    } else {
        parent->right = node;
    }

    rebalance_after_insert(node, root_node);
    m_root = root_node;
    return nullptr;
}

// --- Find ---

const set_hook_state* set_core::find(const void* key, set_compare_fn cmp, const void* ctx) const noexcept
{
    auto* current = root();
    while (current != nullptr) {
        if (cmp(key, current, ctx)) {
            current = resolve(current->left);
        } else if (cmp(current, key, ctx)) {
            current = resolve(current->right);
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
    const set_hook_state* current = root();
    while (current != nullptr) {
        if (!cmp(current, key, ctx)) {
            result  = current;
            current = resolve(current->left);
        } else {
            current = resolve(current->right);
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
    const set_hook_state* current = root();
    while (current != nullptr) {
        if (cmp(key, current, ctx)) {
            result  = current;
            current = resolve(current->left);
        } else {
            current = resolve(current->right);
        }
    }
    return result;
}

// --- Erase ---

void set_core::erase(set_hook_state* node) noexcept
{
    auto* root_node = root();
    auto* y         = node;
    auto* x         = static_cast<set_hook_state*>(nullptr);
    auto* x_par     = static_cast<set_hook_state*>(nullptr);
    bool  y_orig    = y->is_red;

    if (node->left.is_null()) {
        x     = resolve(node->right);
        x_par = resolve(node->parent);
        transplant(node, x, root_node);
    } else if (node->right.is_null()) {
        x     = resolve(node->left);
        x_par = resolve(node->parent);
        transplant(node, x, root_node);
    } else {
        // Find successor (leftmost in right subtree)
        y = resolve(node->right);
        while (!y->left.is_null()) {
            y = resolve(y->left);
        }
        y_orig = y->is_red;
        x      = resolve(y->right);

        if (resolve(y->parent) == node) {
            x_par = y;
        } else {
            x_par = resolve(y->parent);
            transplant(y, x, root_node);
            y->right                  = node->right;
            resolve(y->right)->parent = y;
        }
        transplant(node, y, root_node);
        y->left                  = node->left;
        resolve(y->left)->parent = y;
        y->is_red                = node->is_red;
    }

    if (!y_orig && x_par != nullptr) {
        rebalance_after_erase(x, x_par, root_node);
    }

    reset_hook(node);
    m_root = root_node;
}

// --- Clear ---

void set_core::clear() noexcept
{
    // Iterative post-order traversal to reset all hooks
    auto* current = root();
    while (current != nullptr) {
        auto* left  = resolve(current->left);
        auto* right = resolve(current->right);
        if (left != nullptr) {
            current = left;
        } else if (right != nullptr) {
            current = right;
        } else {
            auto* p = resolve(current->parent);
            if (p != nullptr) {
                if (resolve(p->left) == current) {
                    p->left.reset();
                } else {
                    p->right.reset();
                }
            }
            reset_hook(current);
            current = p;
        }
    }
    m_root.reset();
}

void set_core::clear_and_dispose(set_dispose_fn fn, void* ctx)
{
    auto* current = root();
    while (current != nullptr) {
        auto* left  = resolve(current->left);
        auto* right = resolve(current->right);
        if (left != nullptr) {
            current = left;
        } else if (right != nullptr) {
            current = right;
        } else {
            auto* p = resolve(current->parent);
            if (p != nullptr) {
                if (resolve(p->left) == current) {
                    p->left.reset();
                } else {
                    p->right.reset();
                }
            }
            auto* to_dispose = current;
            reset_hook(to_dispose);
            current = p;
            fn(to_dispose, ctx);
        }
    }
    m_root.reset();
}

// --- Swap ---

void set_core::swap(set_core& other) noexcept
{
    auto tmp     = m_root;
    m_root       = other.m_root;
    other.m_root = tmp;
    // Re-parent
    if (auto* root_node = root(); root_node != nullptr) {
        root_node->parent.reset();
    }
    if (auto* other_root = other.root(); other_root != nullptr) {
        other_root->parent.reset();
    }
}

// --- Iteration ---

set_hook_state* set_core::begin() noexcept
{
    auto* node = root();
    if (node == nullptr) {
        return nullptr;
    }
    while (auto* left = resolve(node->left)) {
        node = left;
    }
    return node;
}

const set_hook_state* set_core::begin() const noexcept
{
    auto* node = root();
    if (node == nullptr) {
        return nullptr;
    }
    while (auto* left = resolve(node->left)) {
        node = left;
    }
    return node;
}

set_hook_state* set_core::last() noexcept
{
    auto* node = root();
    if (node == nullptr) {
        return nullptr;
    }
    while (auto* right = resolve(node->right)) {
        node = right;
    }
    return node;
}

const set_hook_state* set_core::last() const noexcept
{
    auto* node = root();
    if (node == nullptr) {
        return nullptr;
    }
    while (auto* right = resolve(node->right)) {
        node = right;
    }
    return node;
}

set_hook_state* set_core::next(set_hook_state* node) noexcept
{
    if (node == nullptr) {
        return nullptr;
    }
    if (auto* right = resolve(node->right); right != nullptr) {
        node = right;
        while (auto* left = resolve(node->left)) {
            node = left;
        }
        return node;
    }
    auto* p = resolve(node->parent);
    while (p != nullptr && node == resolve(p->right)) {
        node = p;
        p    = resolve(p->parent);
    }
    return p;
}

const set_hook_state* set_core::next(const set_hook_state* node) const noexcept
{
    return const_cast<set_core*>(this)->next(const_cast<set_hook_state*>(node));
}

set_hook_state* set_core::prev(set_hook_state* node) noexcept
{
    if (node == nullptr) {
        return nullptr;
    }
    if (auto* left = resolve(node->left); left != nullptr) {
        node = left;
        while (auto* right = resolve(node->right)) {
            node = right;
        }
        return node;
    }
    auto* p = resolve(node->parent);
    while (p != nullptr && node == resolve(p->left)) {
        node = p;
        p    = resolve(p->parent);
    }
    return p;
}

const set_hook_state* set_core::prev(const set_hook_state* node) const noexcept
{
    return const_cast<set_core*>(this)->prev(const_cast<set_hook_state*>(node));
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
