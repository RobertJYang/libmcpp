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

#include <mc/intrusive/detail/hash_core.h>

#include <utility>
#include <vector>

namespace mc::intrusive::detail {

unordered_core::unordered_core(unordered_bucket* buckets, std::size_t bucket_count) noexcept
    : m_buckets(buckets), m_bucket_count(bucket_count), m_cached_begin(bucket_count)
{}

void unordered_core::reset_buckets(unordered_bucket* buckets, std::size_t bucket_count) noexcept
{
    m_buckets      = buckets;
    m_bucket_count = bucket_count;
    m_cached_begin = bucket_count;
}

unordered_bucket* unordered_core::bucket_at(std::size_t index) const noexcept
{
    auto* buckets = m_buckets.get();
    return buckets == nullptr ? nullptr : (buckets + index);
}

std::size_t unordered_core::bucket_count() const noexcept
{
    return m_bucket_count;
}

unordered_hook_state* unordered_core::bucket_head(std::size_t index) noexcept
{
    auto* bucket = bucket_at(index);
    return bucket == nullptr ? nullptr : bucket->head.get();
}

const unordered_hook_state* unordered_core::bucket_head(std::size_t index) const noexcept
{
    auto* bucket = bucket_at(index);
    return bucket == nullptr ? nullptr : bucket->head.get();
}

const unordered_hook_state* unordered_core::find(std::size_t index, std::size_t hash, const void* key,
                                                 unordered_match_fn match, const void* ctx) const noexcept
{
    auto* current = bucket_head(index);
    while (current != nullptr) {
        if (current->hash_value == hash && match(key, current, ctx)) {
            return current;
        }
        current = next(current);
    }

    return nullptr;
}

std::size_t unordered_core::count_all() const noexcept
{
    if (bucket_at(0) == nullptr && m_bucket_count != 0) {
        return 0;
    }
    if (m_bucket_count == 0) {
        return 0;
    }

    std::size_t count = 0;
    for (std::size_t i = 0; i < m_bucket_count; ++i) {
        auto* current = bucket_head(i);
        while (current != nullptr) {
            ++count;
            current = next(current);
        }
    }

    return count;
}

unordered_hook_state* unordered_core::next(unordered_hook_state* node) noexcept
{
    return node == nullptr ? nullptr : node->next.get();
}

const unordered_hook_state* unordered_core::next(const unordered_hook_state* node) const noexcept
{
    return node == nullptr ? nullptr : node->next.get();
}

void unordered_core::insert_front(std::size_t index, std::size_t hash, unordered_hook_state* node) noexcept
{
    auto* bucket = bucket_at(index);
    if (bucket == nullptr) {
        return;
    }

    node->hash_value = hash;
    node->prev.reset();
    node->next       = bucket->head;
    if (auto* old_head = bucket->head.get(); old_head != nullptr) {
        old_head->prev = node;
    }
    bucket->head = node;
    if (index < m_cached_begin) {
        m_cached_begin = index;
    }
}

bool unordered_core::erase(std::size_t index, unordered_hook_state* node) noexcept
{
    auto* bucket = bucket_at(index);
    if (bucket == nullptr || node == nullptr) {
        return false;
    }

    // Unlink via prev/next pointers directly — O(1)
    if (auto* prev_node = node->prev.get(); prev_node != nullptr) {
        prev_node->next = node->next;
    } else {
        bucket->head = node->next;
    }
    if (auto* next_node = node->next.get(); next_node != nullptr) {
        next_node->prev = node->prev;
    }
    // Check if cached_begin bucket became empty
    if (bucket->head.is_null() && index == m_cached_begin) {
        refresh_cached_begin();
    }
    reset_hook(node);
    return true;
}

void unordered_core::clear() noexcept
{
    if (m_bucket_count == 0) {
        return;
    }

    for (std::size_t i = 0; i < m_bucket_count; ++i) {
        auto* bucket  = bucket_at(i);
        auto* current = bucket_head(i);
        while (current != nullptr) {
            auto* next_node = next(current);
            reset_hook(current);
            current = next_node;
        }
        if (bucket != nullptr) {
            bucket->head.reset();
        }
    }
    m_cached_begin = m_bucket_count;
}

void unordered_core::reset_hook(unordered_hook_state* node) noexcept
{
    node->next.reset();
    node->prev.reset();
    node->hash_value = 0;
}

std::size_t unordered_core::cached_begin() const noexcept
{
    return m_cached_begin;
}

void unordered_core::refresh_cached_begin() noexcept
{
    for (std::size_t i = m_cached_begin; i < m_bucket_count; ++i) {
        auto* bucket = bucket_at(i);
        if (bucket != nullptr && !bucket->head.is_null()) {
            m_cached_begin = i;
            return;
        }
    }
    m_cached_begin = m_bucket_count;
}

unordered_iterator_state unordered_core::begin() const noexcept
{
    if (m_cached_begin >= m_bucket_count) {
        return {}; // empty table
    }
    return {m_cached_begin, const_cast<unordered_hook_state*>(bucket_head(m_cached_begin))};
}

unordered_iterator_state unordered_core::advance(const unordered_iterator_state& state) const noexcept
{
    if (state.current == nullptr) {
        return {};
    }
    // Try next node in same bucket
    if (auto* next_node = next(state.current); next_node != nullptr) {
        return {state.bucket_idx, const_cast<unordered_hook_state*>(next_node)};
    }
    // Scan subsequent buckets
    for (std::size_t i = state.bucket_idx + 1; i < m_bucket_count; ++i) {
        if (auto* head = bucket_head(i); head != nullptr) {
            return {i, const_cast<unordered_hook_state*>(head)};
        }
    }
    return {};
}

void unordered_core::clear_and_dispose(unordered_dispose_fn dispose_fn, void* ctx)
{
    if (m_bucket_count == 0) {
        return;
    }
    for (std::size_t i = 0; i < m_bucket_count; ++i) {
        auto* bucket  = bucket_at(i);
        auto* current = bucket_head(i);
        while (current != nullptr) {
            auto* next_node = next(current);
            reset_hook(current);
            dispose_fn(current, ctx);
            current = next_node;
        }
        if (bucket != nullptr) {
            bucket->head.reset();
        }
    }
    m_cached_begin = m_bucket_count;
}

void unordered_core::swap_rehash(unordered_core& other) noexcept
{
    // Collect all nodes from this with their hash values
    std::vector<std::pair<unordered_hook_state*, std::size_t>> this_nodes;
    for (std::size_t i = 0; i < m_bucket_count; ++i) {
        auto* current = bucket_head(i);
        while (current != nullptr) {
            this_nodes.emplace_back(current, current->hash_value);
            current = next(current);
        }
    }
    // Collect all nodes from other
    std::vector<std::pair<unordered_hook_state*, std::size_t>> other_nodes;
    for (std::size_t i = 0; i < other.m_bucket_count; ++i) {
        auto* current = other.bucket_head(i);
        while (current != nullptr) {
            other_nodes.emplace_back(current, current->hash_value);
            current = other.next(current);
        }
    }
    // Clear both
    clear();
    other.clear();
    // Re-insert other's nodes into this
    for (auto& [hook, h] : other_nodes) {
        const auto idx = h & (m_bucket_count - 1);
        insert_front(idx, h, hook);
    }
    // Re-insert this's nodes into other
    for (auto& [hook, h] : this_nodes) {
        const auto idx = h & (other.m_bucket_count - 1);
        other.insert_front(idx, h, hook);
    }
}

void unordered_core::rehash(unordered_bucket* new_buckets, std::size_t new_bucket_count) noexcept
{
    if (new_buckets == nullptr || new_bucket_count == 0) {
        clear();
        m_buckets      = new_buckets;
        m_bucket_count = new_bucket_count;
        m_cached_begin = new_bucket_count;
        return;
    }
    // Collect all nodes with their hash values before clearing resets them
    std::vector<std::pair<unordered_hook_state*, std::size_t>> nodes;
    for (std::size_t i = 0; i < m_bucket_count; ++i) {
        auto* current = bucket_head(i);
        while (current != nullptr) {
            nodes.emplace_back(current, current->hash_value);
            current = next(current);
        }
    }
    // Unlink all nodes (don't use clear() which would call reset_hook)
    for (auto& [hook, h] : nodes) {
        hook->next.reset();
        hook->prev.reset();
    }
    // Switch to new buckets
    m_buckets      = new_buckets;
    m_bucket_count = new_bucket_count;
    m_cached_begin = new_bucket_count;
    // Initialize new buckets
    for (std::size_t i = 0; i < m_bucket_count; ++i) {
        bucket_at(i)->head.reset();
    }
    // Re-insert all nodes into new buckets using stored hash
    for (auto& [hook, h] : nodes) {
        const auto idx    = h & (m_bucket_count - 1);
        auto*      bucket = bucket_at(idx);
        hook->prev.reset();
        hook->next        = bucket->head;
        if (auto* head = bucket->head.get(); head != nullptr) {
            head->prev = hook;
        }
        bucket->head = hook;
        if (idx < m_cached_begin) {
            m_cached_begin = idx;
        }
    }
}

} // namespace mc::intrusive::detail
