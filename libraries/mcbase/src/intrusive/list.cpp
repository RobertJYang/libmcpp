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

#include <mc/intrusive/list.h>

namespace mc::intrusive::detail {

list_hook_state* list_core::head() noexcept
{
    return m_head;
}

const list_hook_state* list_core::head() const noexcept
{
    return m_head;
}

list_hook_state* list_core::tail() noexcept
{
    return m_tail;
}

const list_hook_state* list_core::tail() const noexcept
{
    return m_tail;
}

list_hook_state* list_core::next(list_hook_state* node) noexcept
{
    return node == nullptr ? nullptr : static_cast<list_hook_state*>(node->next);
}

const list_hook_state* list_core::next(const list_hook_state* node) noexcept
{
    return node == nullptr ? nullptr : static_cast<const list_hook_state*>(node->next);
}

list_hook_state* list_core::prev(list_hook_state* node) noexcept
{
    return node == nullptr ? nullptr : static_cast<list_hook_state*>(node->prev);
}

const list_hook_state* list_core::prev(const list_hook_state* node) noexcept
{
    return node == nullptr ? nullptr : static_cast<const list_hook_state*>(node->prev);
}

void list_core::push_front(list_hook_state* node) noexcept
{
    node->prev = nullptr;
    node->next = m_head;

    if (m_head != nullptr) {
        m_head->prev = node;
    } else {
        m_tail = node;
    }

    m_head = node;
}

void list_core::push_back(list_hook_state* node) noexcept
{
    node->prev = m_tail;
    node->next = nullptr;

    if (m_tail != nullptr) {
        m_tail->next = node;
    } else {
        m_head = node;
    }

    m_tail = node;
}

void list_core::pop_front() noexcept
{
    if (m_head == nullptr) {
        return;
    }
    auto* old_head = m_head;
    m_head         = next(m_head);
    if (m_head != nullptr) {
        m_head->prev = nullptr;
    } else {
        m_tail = nullptr;
    }
    reset_hook(old_head);
}

void list_core::pop_back() noexcept
{
    if (m_tail == nullptr) {
        return;
    }
    auto* old_tail = m_tail;
    m_tail         = prev(m_tail);
    if (m_tail != nullptr) {
        m_tail->next = nullptr;
    } else {
        m_head = nullptr;
    }
    reset_hook(old_tail);
}

void list_core::insert_before(list_hook_state* pos, list_hook_state* node) noexcept
{
    if (pos == nullptr) {
        push_back(node);
        return;
    }

    if (pos == m_head) {
        push_front(node);
        return;
    }

    auto* prev_node = prev(pos);
    node->prev      = prev_node;
    node->next      = pos;
    prev_node->next = node;
    pos->prev       = node;
}

list_hook_state* list_core::erase(list_hook_state* node) noexcept
{
    if (node == nullptr) {
        return nullptr;
    }

    auto* prev_node = prev(node);
    auto* next_node = next(node);

    if (prev_node != nullptr) {
        prev_node->next = next_node;
    } else {
        m_head = next_node;
    }

    if (next_node != nullptr) {
        next_node->prev = prev_node;
    } else {
        m_tail = prev_node;
    }

    reset_hook(node);
    return next_node;
}

void list_core::splice(list_hook_state* pos, list_core& other) noexcept
{
    if (other.empty()) {
        return;
    }

    auto* other_head = other.m_head;
    auto* other_tail = other.m_tail;

    // Detach all nodes from other
    other.m_head = nullptr;
    other.m_tail = nullptr;

    if (pos == nullptr) {
        // Splice before end (= append after tail)
        other_head->prev = m_tail;
        if (m_tail != nullptr) {
            m_tail->next = other_head;
        } else {
            m_head = other_head;
        }
        m_tail = other_tail;
    } else if (pos == m_head) {
        // Splice before head (= prepend)
        other_tail->next = m_head;
        m_head->prev     = other_tail;
        m_head           = other_head;
    } else {
        // Splice before pos
        auto* pos_prev = prev(pos);
        pos_prev->next = other_head;
        other_head->prev = pos_prev;
        other_tail->next = pos;
        pos->prev        = other_tail;
    }
}

void list_core::clear() noexcept
{
    auto* current = m_head;
    while (current != nullptr) {
        auto* next_node = next(current);
        reset_hook(current);
        current = next_node;
    }

    m_head = nullptr;
    m_tail = nullptr;
}

bool list_core::empty() const noexcept
{
    return m_head == nullptr;
}

void list_core::reset_hook(list_hook_state* node) noexcept
{
    node->prev = nullptr;
    node->next = nullptr;
}

slist_hook_state* slist_core::head() noexcept
{
    return m_head;
}

const slist_hook_state* slist_core::head() const noexcept
{
    return m_head;
}

slist_hook_state* slist_core::next(slist_hook_state* node) noexcept
{
    return node == nullptr ? nullptr : static_cast<slist_hook_state*>(node->next);
}

const slist_hook_state* slist_core::next(const slist_hook_state* node) noexcept
{
    return node == nullptr ? nullptr : static_cast<const slist_hook_state*>(node->next);
}

void slist_core::push_front(slist_hook_state* node) noexcept
{
    node->next = m_head;
    m_head     = node;
}

void slist_core::pop_front() noexcept
{
    if (m_head == nullptr) {
        return;
    }
    auto* old_head = m_head;
    m_head         = next(m_head);
    reset_hook(old_head);
}

void slist_core::erase(slist_hook_state* node) noexcept
{
    if (node == nullptr) {
        return;
    }
    if (m_head == node) {
        m_head = next(node);
        reset_hook(node);
        return;
    }
    // O(n) search for predecessor
    auto* prev = m_head;
    while (prev != nullptr && next(prev) != node) {
        prev = next(prev);
    }
    if (prev != nullptr) {
        prev->next = node->next;
        reset_hook(node);
    }
}

void slist_core::clear_and_dispose(slist_dispose_fn fn, void* ctx)
{
    auto* current = m_head;
    while (current != nullptr) {
        auto* next_node = next(current);
        reset_hook(current);
        fn(current, ctx);
        current = next_node;
    }
    m_head = nullptr;
}

void slist_core::clear() noexcept
{
    auto* current = m_head;
    while (current != nullptr) {
        auto* next_node = next(current);
        reset_hook(current);
        current = next_node;
    }

    m_head = nullptr;
}

void slist_core::reverse() noexcept
{
    slist_hook_state* previous = nullptr;
    auto*             current  = m_head;

    while (current != nullptr) {
        auto* next_node = next(current);
        current->next   = previous;
        previous        = current;
        current         = next_node;
    }

    m_head = previous;
}

bool slist_core::empty() const noexcept
{
    return m_head == nullptr;
}

void slist_core::reset_hook(slist_hook_state* node) noexcept
{
    node->next = nullptr;
}

} // namespace mc::intrusive::detail
