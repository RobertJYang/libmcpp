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

#include <dict/include/entry_storage.h>
#include <dict/include/index.h>

#include <algorithm>
#include <new>

namespace mc::dict_types {
namespace {

constexpr std::size_t s_max_load_numerator   = 7;
constexpr std::size_t s_max_load_denominator = 8;
constexpr std::size_t s_inline_bucket_count  = MC_DICT_BUCKET_COUNT;

std::size_t normalize_bucket_count(std::size_t bucket_count) noexcept
{
    std::size_t normalized = s_inline_bucket_count;
    while (normalized < bucket_count) {
        normalized <<= 1;
    }
    return normalized;
}

void clear_slots(entry** slots, std::size_t bucket_count) noexcept
{
    std::fill_n(slots, bucket_count, nullptr);
}

std::size_t required_bucket_count(std::size_t entry_count) noexcept
{
    const auto scaled = (entry_count * s_max_load_denominator + s_max_load_numerator - 1) / s_max_load_numerator;
    return std::max(s_inline_bucket_count, scaled);
}

} // namespace

dict_index::dict_index()
{
    clear_slots(m_inline_slots, s_inline_bucket_count);
}

dict_index::~dict_index() = default;

void dict_order_cache::rebuild(const entry_list& entries)
{
    m_entries.clear();
    m_indices.clear();
    m_entries.reserve(entries.size());
    m_indices.reserve(entries.size());
    int index = 0;
    for (const auto& item : entries) {
        auto* current = const_cast<entry*>(&item);
        m_entries.push_back(current);
        m_indices.emplace(current, index);
        ++index;
    }
}

const entry* dict_order_cache::at(std::size_t index) const noexcept
{
    return index < m_entries.size() ? m_entries[index] : nullptr;
}

int dict_order_cache::index_of(const entry* item) const noexcept
{
    auto it = m_indices.find(item);
    return it == m_indices.end() ? -1 : it->second;
}

std::size_t dict_order_cache::size() const noexcept
{
    return m_entries.size();
}

entry* dict_index::find(mc::string_view key) const
{
    if (m_size == 0) {
        return nullptr;
    }
    return find(key, key_hash{}(key));
}

entry* dict_index::find(mc::string_view key, std::size_t hash_code) const
{
    if (m_size == 0) {
        return nullptr;
    }

    std::size_t index    = ideal_index(hash_code);
    std::size_t distance = 0;

    while (true) {
        auto* current = m_slots[index];
        if (current == nullptr) {
            return nullptr;
        }

        const auto current_distance = probe_distance(index, current->hash_code);
        if (current_distance < distance) {
            return nullptr;
        }

        if (current->hash_code == hash_code && key_equal{}(key, *current)) {
            return current;
        }

        index = (index + 1) & (m_bucket_count - 1);
        ++distance;
    }
}

entry* dict_index::find(const variant& key) const
{
    if (m_size == 0) {
        return nullptr;
    }

    const std::size_t hash_code = key_hash{}(key);
    std::size_t       index     = ideal_index(hash_code);
    std::size_t       distance  = 0;

    while (true) {
        auto* current = m_slots[index];
        if (current == nullptr) {
            return nullptr;
        }

        const auto current_distance = probe_distance(index, current->hash_code);
        if (current_distance < distance) {
            return nullptr;
        }

        if (current->hash_code == hash_code && key_equal{}(key, *current)) {
            return current;
        }

        index = (index + 1) & (m_bucket_count - 1);
        ++distance;
    }
}

void dict_index::insert(entry& item)
{
    maybe_grow();
    insert_existing(item);
    ++m_size;
}

void dict_index::erase(entry& item) noexcept
{
    if (m_size == 0) {
        return;
    }

    std::size_t index    = ideal_index(item.hash_code);
    std::size_t distance = 0;

    while (true) {
        auto* current = m_slots[index];
        if (current == nullptr) {
            return;
        }

        const auto current_distance = probe_distance(index, current->hash_code);
        if (current_distance < distance) {
            return;
        }

        if (current == &item) {
            erase_at(index);
            --m_size;
            maybe_shrink();
            return;
        }

        index = (index + 1) & (m_bucket_count - 1);
        ++distance;
    }
}

void dict_index::clear() noexcept
{
    if (m_bucket_count != s_inline_bucket_count) {
        m_dynamic_slots.reset();
        m_slots        = m_inline_slots;
        m_bucket_count = s_inline_bucket_count;
    }

    clear_slots(m_slots, m_bucket_count);
    m_size = 0;
}

void dict_index::reserve(std::size_t entry_count)
{
    const auto target_bucket_count = normalize_bucket_count(required_bucket_count(entry_count));
    if (target_bucket_count > m_bucket_count) {
        rehash(target_bucket_count);
    }
}

std::size_t dict_index::bucket_count() const noexcept
{
    return m_bucket_count;
}

void dict_index::maybe_grow()
{
    if ((m_size + 1) * s_max_load_denominator <= m_bucket_count * s_max_load_numerator) {
        return;
    }

    rehash(m_bucket_count << 1);
}

void dict_index::maybe_shrink()
{
    if (m_bucket_count == s_inline_bucket_count || m_size * 4 > m_bucket_count) {
        return;
    }

    rehash(m_bucket_count >> 1);
}

void dict_index::rehash(std::size_t new_bucket_count)
{
    const std::size_t target_bucket_count = normalize_bucket_count(new_bucket_count);
    if (target_bucket_count == m_bucket_count) {
        return;
    }

    entry**                   old_slots        = m_slots;
    const std::size_t         old_bucket_count = m_bucket_count;
    auto                      old_dynamic      = std::move(m_dynamic_slots);
    std::unique_ptr<entry*[]> new_dynamic_slots;
    entry**                   new_slots = m_inline_slots;

    if (target_bucket_count > s_inline_bucket_count) {
        new_dynamic_slots = std::make_unique<entry*[]>(target_bucket_count);
        new_slots         = new_dynamic_slots.get();
    }

    clear_slots(new_slots, target_bucket_count);

    m_dynamic_slots = std::move(new_dynamic_slots);
    m_slots         = new_slots;
    m_bucket_count  = target_bucket_count;

    for (std::size_t i = 0; i < old_bucket_count; ++i) {
        if (old_slots[i] != nullptr) {
            insert_existing(*old_slots[i]);
        }
    }
}

void dict_index::insert_existing(entry& item) noexcept
{
    std::size_t index    = ideal_index(item.hash_code);
    std::size_t distance = 0;
    auto*       current  = &item;

    while (true) {
        auto*& slot = m_slots[index];
        if (slot == nullptr) {
            slot = current;
            return;
        }

        const auto slot_distance = probe_distance(index, slot->hash_code);
        if (slot_distance < distance) {
            std::swap(slot, current);
            distance = slot_distance;
        }

        index = (index + 1) & (m_bucket_count - 1);
        ++distance;
    }
}

void dict_index::erase_at(std::size_t slot_index) noexcept
{
    auto index = slot_index;

    while (true) {
        const auto next_index = (index + 1) & (m_bucket_count - 1);
        auto*      next_entry = m_slots[next_index];
        if (next_entry == nullptr || probe_distance(next_index, next_entry->hash_code) == 0) {
            m_slots[index] = nullptr;
            return;
        }

        m_slots[index] = next_entry;
        index          = next_index;
    }
}

std::size_t dict_index::ideal_index(std::size_t hash_code) const noexcept
{
    return hash_code & (m_bucket_count - 1);
}

std::size_t dict_index::probe_distance(std::size_t slot_index, std::size_t hash_code) const noexcept
{
    return (slot_index + m_bucket_count - ideal_index(hash_code)) & (m_bucket_count - 1);
}

data_t::data_t() : index(std::make_unique<dict_index>())
{}

data_t::~data_t()
{
    if (index) {
        index->clear();
    }

    entries.clear_and_dispose([this](entry* p) {
        destroy_entry(p);
    });
}

void data_t::clear_index() noexcept
{
    index.reset();
}

entry* data_t::create_entry(variant key, variant value)
{
    return entry_storage_create(std::move(key), std::move(value));
}

void data_t::destroy_entry(entry* item) noexcept
{
    if (item == nullptr) {
        return;
    }
    entry_storage_destroy(item);
}

void data_t::reserve(std::size_t count)
{
    index->reserve(count);
    entry_storage_reserve(count);
}

std::size_t data_t::index_bucket_count() const
{
    return index == nullptr ? 0 : index->bucket_count();
}

const entry* data_t::entry_at_index(std::size_t index) const
{
    if (order_cache == nullptr) {
        order_cache = std::make_unique<dict_order_cache>();
        order_cache->rebuild(entries);
    }

    return order_cache->at(index);
}

int data_t::entry_index(const entry* item) const
{
    if (item == nullptr) {
        return -1;
    }

    if (order_cache == nullptr) {
        order_cache = std::make_unique<dict_order_cache>();
        order_cache->rebuild(entries);
    }

    return order_cache->index_of(item);
}

void data_t::invalidate_order_cache() const
{
    order_cache.reset();
}

std::size_t data_t::order_cache_size() const
{
    return order_cache == nullptr ? 0 : order_cache->size();
}

} // namespace mc::dict_types
