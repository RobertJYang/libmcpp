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

#include <mc/memory/fixed_size_pool.h>

#include <algorithm>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <vector>

namespace mc::memory {
namespace {

constexpr bool is_power_of_two(std::size_t n) noexcept
{
    return n != 0 && (n & (n - 1)) == 0;
}

constexpr std::size_t s_max_empty_slabs = 1;

std::size_t align_up(std::size_t n, std::size_t alignment) noexcept
{
    return (n + alignment - 1) & ~(alignment - 1);
}

} // namespace

fixed_size_pool::tls_cache_registry::~tls_cache_registry()
{
    while (head != nullptr) {
        auto* current = head;
        head          = current->thread_next;
        if (current->owner != nullptr) {
            current->owner->release_local_cache_on_thread_exit(*current);
        }
        delete current;
    }
}

fixed_size_pool::fixed_size_pool(std::size_t object_size, std::size_t alignment)
    : fixed_size_pool(object_size, alignment, options{})
{}

fixed_size_pool::fixed_size_pool(std::size_t object_size, std::size_t alignment, const options& opts) : m_options(opts)
{
    if (object_size == 0) {
        object_size = 1;
    }
    if (alignment == 0 || !is_power_of_two(alignment)) {
        throw std::invalid_argument("fixed_size_pool: alignment 必须为 2 的幂且非 0");
    }

    if (m_options.initial_slab_slots == 0) {
        m_options.initial_slab_slots = 1;
    }
    if (m_options.max_slab_slots == 0) {
        m_options.max_slab_slots = m_options.initial_slab_slots;
    }
    if (m_options.max_slab_slots < m_options.initial_slab_slots) {
        m_options.max_slab_slots = m_options.initial_slab_slots;
    }

    m_has_local_cache = m_options.local_cache_capacity != 0;
    if (m_has_local_cache) {
        m_local_hard_limit = m_options.local_cache_capacity;
    }

    m_object_size    = object_size;
    m_alignment      = std::max(alignment, alignof(slot_header));
    m_payload_offset = align_up(sizeof(slot_header), alignment);
    m_slot_bytes     = m_payload_offset + object_size;
    if (m_slot_bytes < object_size || m_slot_bytes < m_payload_offset) {
        throw std::invalid_argument("fixed_size_pool: 槽位大小溢出");
    }
    m_stride = align_up(m_slot_bytes, m_alignment);
    if (m_stride < m_slot_bytes || m_stride < sizeof(slot_header)) {
        throw std::invalid_argument("fixed_size_pool: 槽位步长无效");
    }

    m_segment_slots = m_options.max_slab_slots;
    if (m_options.max_total_slots != 0) {
        m_segment_slots = std::min(m_segment_slots, m_options.max_total_slots);
    }
    m_segment_slots      = std::max<std::size_t>(1, m_segment_slots);
    m_next_segment_slots = std::min(std::max<std::size_t>(1, m_options.initial_slab_slots), m_segment_slots);

    m_segment_table.reserve(8);
    m_segment_free_indices.reserve(8);
}

fixed_size_pool::~fixed_size_pool()
{
    shutdown_registered_caches();
    for (auto& seg : m_segment_table) {
        if (seg.allocation_base != nullptr) {
            std::free(seg.allocation_base);
            seg.allocation_base = nullptr;
        }
    }
}

fixed_size_pool::local_cache& fixed_size_pool::create_and_register_local_cache(tls_cache_registry& registry)
{
    auto* cache        = new local_cache();
    cache->owner       = this;
    cache->thread_next = registry.head;
    registry.head      = cache;

    std::lock_guard<std::mutex> lock(m_mutex);
    register_local_cache_locked(*cache);
    return *cache;
}

fixed_size_pool::local_cache& fixed_size_pool::acquire_local_cache()
{
    auto& last_cache = tls_last_cache();
    if (last_cache.owner == this && last_cache.cache != nullptr && last_cache.cache->owner == this) {
        return *last_cache.cache;
    }

    auto&  registry = tls_registry();
    auto** current  = &registry.head;
    while (*current != nullptr) {
        auto* cache = *current;
        if (cache->owner == nullptr) {
            *current = cache->thread_next;
            if (last_cache.cache == cache) {
                last_cache.owner = nullptr;
                last_cache.cache = nullptr;
            }
            delete cache;
            continue;
        }

        if (cache->owner == this) {
            last_cache.owner = this;
            last_cache.cache = cache;
            return *cache;
        }
        current = &((*current)->thread_next);
    }

    auto& cache      = create_and_register_local_cache(registry);
    last_cache.owner = this;
    last_cache.cache = &cache;
    return cache;
}

void fixed_size_pool::register_local_cache_locked(local_cache& cache) noexcept
{
    if (cache.registered) {
        return;
    }

    cache.owner_next    = m_registered_caches;
    m_registered_caches = &cache;
    cache.registered    = true;
}

void fixed_size_pool::unregister_local_cache_locked(local_cache& cache) noexcept
{
    if (!cache.registered) {
        return;
    }

    auto** current = &m_registered_caches;
    while (*current != nullptr) {
        if (*current == &cache) {
            *current = cache.owner_next;
            break;
        }
        current = &((*current)->owner_next);
    }

    cache.owner_next = nullptr;
    cache.registered = false;
}

std::size_t fixed_size_pool::acquire_segment_index_locked()
{
    if (!m_segment_free_indices.empty()) {
        const auto idx = m_segment_free_indices.back();
        m_segment_free_indices.pop_back();
        return idx;
    }

    m_segment_table.emplace_back();
    return m_segment_table.size() - 1;
}

void fixed_size_pool::allocate_segment_locked(std::size_t capacity)
{
    if (capacity == 0) {
        return;
    }

    const auto raw_bytes = align_up(m_stride * capacity, m_alignment);
    if (raw_bytes < m_stride || raw_bytes / m_stride < capacity) {
        throw std::overflow_error("fixed_size_pool: segment 大小溢出");
    }

    void* storage = std::aligned_alloc(m_alignment, raw_bytes);
    if (storage == nullptr) {
        throw std::bad_alloc();
    }

    const std::size_t segment_index = acquire_segment_index_locked();
    auto&             seg           = m_segment_table[segment_index];
    seg.allocation_base             = storage;
    seg.capacity                    = capacity;
    seg.central_count               = capacity;

    auto* base = static_cast<unsigned char*>(storage);
    for (std::size_t i = 0; i < capacity; ++i) {
        auto* slot                = reinterpret_cast<slot_header*>(base + i * m_stride);
        slot->segment_index       = static_cast<std::uint32_t>(segment_index);
        slot->next                = m_central_fresh_free_list;
        m_central_fresh_free_list = slot;
    }

    m_total_slots += capacity;
    m_central_fresh_slots += capacity;

    if (m_next_segment_slots < m_segment_slots) {
        const auto doubled   = m_next_segment_slots > std::numeric_limits<std::size_t>::max() / 2
                                   ? std::numeric_limits<std::size_t>::max()
                                   : m_next_segment_slots << 1;
        m_next_segment_slots = std::min(std::max(doubled, capacity), m_segment_slots);
    }
}

void fixed_size_pool::reserve_locked(std::size_t free_slots)
{
    while (central_free_slots() < free_slots) {
        if (m_options.max_total_slots != 0) {
            if (m_total_slots >= m_options.max_total_slots) {
                throw std::bad_alloc();
            }
        }
        auto capacity = std::max(m_next_segment_slots, free_slots - central_free_slots());
        capacity      = std::min(capacity, m_segment_slots);
        if (m_options.max_total_slots != 0) {
            capacity = std::min(capacity, m_options.max_total_slots - m_total_slots);
        }
        if (capacity == 0) {
            throw std::bad_alloc();
        }
        allocate_segment_locked(capacity);
    }
}

fixed_size_pool::slot_header* fixed_size_pool::pop_central_reused_locked() noexcept
{
    if (m_central_reused_free_list == nullptr) {
        return nullptr;
    }

    auto* slot                 = m_central_reused_free_list;
    m_central_reused_free_list = slot->next;
    --m_central_reused_slots;
    return slot;
}

fixed_size_pool::slot_header* fixed_size_pool::pop_central_fresh_locked() noexcept
{
    if (m_central_fresh_free_list == nullptr) {
        return nullptr;
    }

    auto* slot                = m_central_fresh_free_list;
    m_central_fresh_free_list = slot->next;
    --m_central_fresh_slots;
    return slot;
}

void fixed_size_pool::push_central_reused_locked(slot_header* slot) noexcept
{
    slot->next                 = m_central_reused_free_list;
    m_central_reused_free_list = slot;
    ++m_central_reused_slots;
}

void fixed_size_pool::push_central_fresh_locked(slot_header* slot) noexcept
{
    slot->next                = m_central_fresh_free_list;
    m_central_fresh_free_list = slot;
    ++m_central_fresh_slots;
}

void fixed_size_pool::rebuild_central_counts_locked() noexcept
{
    for (auto& seg : m_segment_table) {
        seg.central_count = 0;
    }

    for (auto* slot = m_central_reused_free_list; slot != nullptr; slot = slot->next) {
        const auto idx = static_cast<std::size_t>(slot->segment_index);
        if (idx < m_segment_table.size() && m_segment_table[idx].allocation_base != nullptr) {
            ++m_segment_table[idx].central_count;
        }
    }

    for (auto* slot = m_central_fresh_free_list; slot != nullptr; slot = slot->next) {
        const auto idx = static_cast<std::size_t>(slot->segment_index);
        if (idx < m_segment_table.size() && m_segment_table[idx].allocation_base != nullptr) {
            ++m_segment_table[idx].central_count;
        }
    }
}

std::size_t fixed_size_pool::drain_local_cache_locked(local_cache& cache, std::size_t keep_count) noexcept
{
    auto drain_list = [](slot_header*& local_head, std::size_t& local_count, std::size_t keep,
                         slot_header*& central_head, std::size_t& central_count) noexcept {
        if (local_head == nullptr || local_count <= keep) {
            return std::size_t{0};
        }

        const auto released = local_count - keep;
        if (keep == 0) {
            auto* tail = local_head;
            while (tail->next != nullptr) {
                tail = tail->next;
            }
            tail->next   = central_head;
            central_head = local_head;
            central_count += released;
            local_head  = nullptr;
            local_count = 0;
            return released;
        }

        auto* kept_tail = local_head;
        for (std::size_t index = 1; index < keep; ++index) {
            kept_tail = kept_tail->next;
        }

        auto* released_head = kept_tail->next;
        kept_tail->next     = nullptr;

        auto* released_tail = released_head;
        while (released_tail->next != nullptr) {
            released_tail = released_tail->next;
        }
        released_tail->next = central_head;
        central_head        = released_head;
        central_count += released;
        local_count = keep;
        return released;
    };

    const auto keep_reused = std::min(cache.fast_reused_count, keep_count);
    auto released = drain_list(cache.fast_reused_list, cache.fast_reused_count, keep_reused, m_central_reused_free_list,
                               m_central_reused_slots);
    const auto keep_fresh =
        keep_count > keep_reused ? std::min(cache.fast_fresh_count, keep_count - keep_reused) : std::size_t{0};
    released += drain_list(cache.fast_fresh_list, cache.fast_fresh_count, keep_fresh, m_central_fresh_free_list,
                           m_central_fresh_slots);
    return released;
}

void fixed_size_pool::refill_local_cache(local_cache& cache)
{
    if (!m_has_local_cache) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    reserve_locked(1);

    const auto target_batch = std::max<std::size_t>(1, m_options.local_cache_capacity);
    while (local_cached_slots(cache) < target_batch && m_central_reused_free_list != nullptr) {
        auto* slot             = pop_central_reused_locked();
        slot->next             = cache.fast_reused_list;
        cache.fast_reused_list = slot;
        ++cache.fast_reused_count;
    }

    while (local_cached_slots(cache) < target_batch && m_central_fresh_free_list != nullptr) {
        auto* slot            = pop_central_fresh_locked();
        slot->next            = cache.fast_fresh_list;
        cache.fast_fresh_list = slot;
        ++cache.fast_fresh_count;
    }
}

void fixed_size_pool::flush_local_cache(local_cache& cache, std::size_t keep_count)
{
    if (local_cached_slots(cache) <= keep_count) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    drain_local_cache_locked(cache, keep_count);
}

fixed_size_pool::slot_header* fixed_size_pool::acquire_slot_slow(local_cache& cache, bool* reused)
{
    if (reused != nullptr) {
        *reused = false;
    }

    if (m_has_local_cache && local_cached_slots(cache) == 0) {
        refill_local_cache(cache);
    }

    if (m_has_local_cache) {
        if (cache.fast_reused_list != nullptr) {
            auto* slot             = cache.fast_reused_list;
            cache.fast_reused_list = slot->next;
            --cache.fast_reused_count;
            if (reused != nullptr) {
                *reused = true;
            }
            return slot;
        }
        if (cache.fast_fresh_list != nullptr) {
            auto* slot            = cache.fast_fresh_list;
            cache.fast_fresh_list = slot->next;
            --cache.fast_fresh_count;
            return slot;
        }
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    reserve_locked(1);
    if (auto* slot = pop_central_reused_locked()) {
        if (reused != nullptr) {
            *reused = true;
        }
        return slot;
    }
    if (auto* slot = pop_central_fresh_locked()) {
        return slot;
    }
    throw std::bad_alloc();
}

void fixed_size_pool::deallocate_slow(slot_header* slot, local_cache& cache) noexcept
{
    if (!m_has_local_cache) {
        std::lock_guard<std::mutex> lock(m_mutex);
        push_central_reused_locked(slot);
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    drain_local_cache_locked(cache, 0);
}

void fixed_size_pool::release_local_cache_on_thread_exit(local_cache& cache) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (cache.registered) {
        drain_local_cache_locked(cache, 0);
        unregister_local_cache_locked(cache);
    }
    cache.owner             = nullptr;
    cache.fast_reused_list  = nullptr;
    cache.fast_reused_count = 0;
    cache.fast_fresh_list   = nullptr;
    cache.fast_fresh_count  = 0;
}

void fixed_size_pool::shutdown_registered_caches() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto*                       current = m_registered_caches;
    m_registered_caches                 = nullptr;
    while (current != nullptr) {
        auto* next = current->owner_next;
        if (current->registered) {
            drain_local_cache_locked(*current, 0);
        }
        current->owner             = nullptr;
        current->fast_reused_list  = nullptr;
        current->fast_reused_count = 0;
        current->fast_fresh_list   = nullptr;
        current->fast_fresh_count  = 0;
        current->owner_next        = nullptr;
        current->registered        = false;
        current                    = next;
    }
}

void fixed_size_pool::reserve(std::size_t slot_count)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    reserve_locked(slot_count);
}

void* fixed_size_pool::allocate()
{
    auto& cache = acquire_local_cache();
    if (MC_LIKELY(cache.fast_reused_list != nullptr)) {
        auto* slot             = cache.fast_reused_list;
        cache.fast_reused_list = slot->next;
        --cache.fast_reused_count;
        return payload_from_slot(slot);
    }
    if (MC_LIKELY(cache.fast_fresh_list != nullptr)) {
        auto* slot            = cache.fast_fresh_list;
        cache.fast_fresh_list = slot->next;
        --cache.fast_fresh_count;
        return payload_from_slot(slot);
    }
    return payload_from_slot(acquire_slot_slow(cache));
}

fixed_size_pool::allocation_result fixed_size_pool::allocate_with_status()
{
    auto& cache = acquire_local_cache();
    if (MC_LIKELY(cache.fast_reused_list != nullptr)) {
        auto* slot             = cache.fast_reused_list;
        cache.fast_reused_list = slot->next;
        --cache.fast_reused_count;
        return {
            payload_from_slot(slot),
            true,
        };
    }
    if (MC_LIKELY(cache.fast_fresh_list != nullptr)) {
        auto* slot            = cache.fast_fresh_list;
        cache.fast_fresh_list = slot->next;
        --cache.fast_fresh_count;
        return {
            payload_from_slot(slot),
            false,
        };
    }

    bool  reused = false;
    auto* slot   = acquire_slot_slow(cache, &reused);
    return {
        payload_from_slot(slot),
        reused,
    };
}

void fixed_size_pool::deallocate(void* item) noexcept
{
    if (item == nullptr) {
        return;
    }

    auto& cache = acquire_local_cache();
    auto* slot  = slot_from_payload(item);
    if (MC_UNLIKELY(!m_has_local_cache)) {
        deallocate_slow(slot, cache);
        return;
    }

    slot->next             = cache.fast_reused_list;
    cache.fast_reused_list = slot;
    ++cache.fast_reused_count;
    if (MC_UNLIKELY(local_cached_slots(cache) > local_hard_limit())) {
        deallocate_slow(slot, cache);
    }
}

fixed_size_pool::clear_result fixed_size_pool::clear_locked(bool keep_one_empty_slab, std::size_t keep_free_slots)
{
    rebuild_central_counts_locked();

    std::size_t reclaimable_empty_segments = 0;
    for (const auto& seg : m_segment_table) {
        if (seg.allocation_base != nullptr && seg.central_count == seg.capacity) {
            ++reclaimable_empty_segments;
        }
    }

    std::vector<std::size_t> segments_to_release;
    std::size_t              scheduled_release_slots  = 0;
    std::size_t              remaining_empty_segments = reclaimable_empty_segments;
    std::size_t              remaining_free_slots     = central_free_slots();

    for (std::size_t i = 0; i < m_segment_table.size(); ++i) {
        auto& seg = m_segment_table[i];
        if (seg.allocation_base == nullptr || seg.central_count != seg.capacity) {
            continue;
        }

        const bool over_empty_limit = !keep_one_empty_slab || remaining_empty_segments > s_max_empty_slabs;
        const bool keeps_enough_free =
            remaining_free_slots >= seg.capacity && (remaining_free_slots - seg.capacity) >= keep_free_slots;
        if (over_empty_limit && keeps_enough_free) {
            segments_to_release.push_back(i);
            remaining_free_slots -= seg.capacity;
            scheduled_release_slots += seg.capacity;
            --remaining_empty_segments;
        }
    }

    if (segments_to_release.empty()) {
        return {
            0,
            m_total_slots,
        };
    }

    std::vector<unsigned char> release_mark(m_segment_table.size(), 0);
    for (const auto idx : segments_to_release) {
        if (idx < release_mark.size()) {
            release_mark[idx] = 1;
        }
    }

    slot_header* rebuilt_reused       = nullptr;
    std::size_t  rebuilt_reused_slots = 0;
    auto*        slot                 = m_central_reused_free_list;
    while (slot != nullptr) {
        auto* const next = slot->next;
        const auto  idx  = static_cast<std::size_t>(slot->segment_index);
        if (idx >= release_mark.size() || release_mark[idx] == 0) {
            slot->next     = rebuilt_reused;
            rebuilt_reused = slot;
            ++rebuilt_reused_slots;
        }
        slot = next;
    }
    m_central_reused_free_list = rebuilt_reused;
    m_central_reused_slots     = rebuilt_reused_slots;

    slot_header* rebuilt_fresh       = nullptr;
    std::size_t  rebuilt_fresh_slots = 0;
    slot                             = m_central_fresh_free_list;
    while (slot != nullptr) {
        auto* const next = slot->next;
        const auto  idx  = static_cast<std::size_t>(slot->segment_index);
        if (idx >= release_mark.size() || release_mark[idx] == 0) {
            slot->next    = rebuilt_fresh;
            rebuilt_fresh = slot;
            ++rebuilt_fresh_slots;
        }
        slot = next;
    }
    m_central_fresh_free_list = rebuilt_fresh;
    m_central_fresh_slots     = rebuilt_fresh_slots;

    for (const auto idx : segments_to_release) {
        if (idx >= m_segment_table.size()) {
            continue;
        }
        auto& seg = m_segment_table[idx];
        if (seg.allocation_base != nullptr) {
            std::free(seg.allocation_base);
        }
        seg.allocation_base = nullptr;
        seg.capacity        = 0;
        seg.central_count   = 0;
        m_segment_free_indices.push_back(idx);
    }

    m_total_slots -= scheduled_release_slots;
    return {
        scheduled_release_slots,
        m_total_slots,
    };
}

std::size_t fixed_size_pool::trim(std::size_t keep_free_slots)
{
    auto& cache = acquire_local_cache();
    if (m_has_local_cache) {
        flush_local_cache(cache, 0);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    return clear_locked(true, keep_free_slots).released_slots;
}

fixed_size_pool::clear_result fixed_size_pool::clear()
{
    auto& cache = acquire_local_cache();
    if (m_has_local_cache) {
        flush_local_cache(cache, 0);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    return clear_locked(false, 0);
}

std::vector<void*> fixed_size_pool::snapshot_reused_slots() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::size_t reused_slots = m_central_reused_slots;
    for (auto* cache = m_registered_caches; cache != nullptr; cache = cache->owner_next) {
        reused_slots += cache->fast_reused_count;
    }

    std::vector<void*> payloads;
    payloads.reserve(reused_slots);

    for (auto* slot = m_central_reused_free_list; slot != nullptr; slot = slot->next) {
        payloads.push_back(payload_from_slot(slot));
    }

    for (auto* cache = m_registered_caches; cache != nullptr; cache = cache->owner_next) {
        for (auto* slot = cache->fast_reused_list; slot != nullptr; slot = slot->next) {
            payloads.push_back(payload_from_slot(slot));
        }
    }

    return payloads;
}

void fixed_size_pool::for_each_reused_payload_locked(const std::function<void(void*)>& fn)
{
    auto& cache = acquire_local_cache();
    if (m_has_local_cache) {
        flush_local_cache(cache, 0);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto* slot = m_central_reused_free_list; slot != nullptr; slot = slot->next) {
        fn(payload_from_slot(slot));
    }

    for (auto* cache = m_registered_caches; cache != nullptr; cache = cache->owner_next) {
        for (auto* slot = cache->fast_reused_list; slot != nullptr; slot = slot->next) {
            fn(payload_from_slot(slot));
        }
    }
}

} // namespace mc::memory
