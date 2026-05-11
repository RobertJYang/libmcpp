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
    return m_head.get();
}

const list_hook_state* list_core::head() const noexcept
{
    return m_head.get();
}

list_hook_state* list_core::tail() noexcept
{
    return m_tail.get();
}

const list_hook_state* list_core::tail() const noexcept
{
    return m_tail.get();
}

list_hook_state* list_core::next(list_hook_state* node) noexcept
{
    if (node == nullptr) {
        return nullptr;
    }
    return node->next.get();
}

const list_hook_state* list_core::next(const list_hook_state* node) const noexcept
{
    if (node == nullptr) {
        return nullptr;
    }
    return node->next.get();
}

list_hook_state* list_core::prev(list_hook_state* node) noexcept
{
    if (node == nullptr) {
        return nullptr;
    }
    return node->prev.get();
}

const list_hook_state* list_core::prev(const list_hook_state* node) const noexcept
{
    if (node == nullptr) {
        return nullptr;
    }
    return node->prev.get();
}

void list_core::push_front(list_hook_state* node) noexcept
{
    node->prev.reset();
    node->next = m_head;

    if (auto* old_head = head(); old_head != nullptr) {
        old_head->prev = node;
    } else {
        m_tail = node;
    }

    m_head = node;
}

void list_core::push_back(list_hook_state* node) noexcept
{
    node->prev = m_tail;
    node->next.reset();

    if (auto* old_tail = tail(); old_tail != nullptr) {
        old_tail->next = node;
    } else {
        m_head = node;
    }

    m_tail = node;
}

void list_core::pop_front() noexcept
{
    auto* old_head = head();
    if (old_head == nullptr) {
        return;
    }
    auto* new_head = next(old_head);
    m_head         = new_head;
    if (new_head != nullptr) {
        new_head->prev.reset();
    } else {
        m_tail.reset();
    }
    reset_hook(old_head);
}

void list_core::pop_back() noexcept
{
    auto* old_tail = tail();
    if (old_tail == nullptr) {
        return;
    }
    auto* new_tail = prev(old_tail);
    m_tail         = new_tail;
    if (new_tail != nullptr) {
        new_tail->next.reset();
    } else {
        m_head.reset();
    }
    reset_hook(old_tail);
}

void list_core::insert_before(list_hook_state* pos, list_hook_state* node) noexcept
{
    if (pos == nullptr) {
        push_back(node);
        return;
    }

    if (pos == head()) {
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

    auto* other_head = other.head();
    auto* other_tail = other.tail();

    other.m_head.reset();
    other.m_tail.reset();

    if (pos == nullptr) {
        other_head->prev = m_tail;
        if (auto* old_tail = tail(); old_tail != nullptr) {
            old_tail->next = other_head;
        } else {
            m_head = other_head;
        }
        m_tail = other_tail;
    } else if (pos == head()) {
        other_tail->next = m_head;
        head()->prev     = other_tail;
        m_head           = other_head;
    } else {
        auto* pos_prev   = prev(pos);
        pos_prev->next   = other_head;
        other_head->prev = pos_prev;
        other_tail->next = pos;
        pos->prev        = other_tail;
    }
}

void list_core::clear() noexcept
{
    auto* current = head();
    while (current != nullptr) {
        auto* next_node = next(current);
        reset_hook(current);
        current = next_node;
    }

    m_head.reset();
    m_tail.reset();
}

bool list_core::empty() const noexcept
{
    return m_head.is_null();
}

void list_core::reset_hook(list_hook_state* node) noexcept
{
    node->prev.reset();
    node->next.reset();
}

slist_hook_state* slist_core::head() noexcept
{
    return m_head.get();
}

const slist_hook_state* slist_core::head() const noexcept
{
    return m_head.get();
}

slist_hook_state* slist_core::next(slist_hook_state* node) noexcept
{
    if (node == nullptr) {
        return nullptr;
    }
    return node->next.get();
}

const slist_hook_state* slist_core::next(const slist_hook_state* node) const noexcept
{
    if (node == nullptr) {
        return nullptr;
    }
    return node->next.get();
}

void slist_core::push_front(slist_hook_state* node) noexcept
{
    node->next = m_head;
    m_head     = node;
}

void slist_core::pop_front() noexcept
{
    auto* old_head = head();
    if (old_head == nullptr) {
        return;
    }
    m_head = next(old_head);
    reset_hook(old_head);
}

void slist_core::erase(slist_hook_state* node) noexcept
{
    if (node == nullptr) {
        return;
    }
    if (head() == node) {
        m_head = next(node);
        reset_hook(node);
        return;
    }
    auto* prev_node = head();
    while (prev_node != nullptr && next(prev_node) != node) {
        prev_node = next(prev_node);
    }
    if (prev_node != nullptr) {
        prev_node->next = node->next;
        reset_hook(node);
    }
}

void slist_core::clear_and_dispose(slist_dispose_fn fn, void* ctx)
{
    auto* current = head();
    while (current != nullptr) {
        auto* next_node = next(current);
        reset_hook(current);
        fn(current, ctx);
        current = next_node;
    }
    m_head.reset();
}

void slist_core::clear() noexcept
{
    auto* current = head();
    while (current != nullptr) {
        auto* next_node = next(current);
        reset_hook(current);
        current = next_node;
    }

    m_head.reset();
}

void slist_core::reverse() noexcept
{
    slist_hook_state* previous = nullptr;
    auto*             current  = head();

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
    return m_head.is_null();
}

void slist_core::reset_hook(slist_hook_state* node) noexcept
{
    node->next.reset();
}

} // namespace mc::intrusive::detail
