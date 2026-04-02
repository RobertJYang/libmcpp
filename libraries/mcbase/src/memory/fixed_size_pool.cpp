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

struct fixed_size_pool::slab {
    void*                 storage{nullptr};
    std::size_t           capacity{0};
    std::size_t           central_count{0};
    std::unique_ptr<slab> next;

    ~slab()
    {
        if (storage != nullptr) {
            std::free(storage);
        }
    }
};

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

    m_object_size            = object_size;
    m_alignment              = std::max(alignment, alignof(slot_header));
    const auto min_slot_size = std::max(sizeof(slot_header), object_size);
    m_payload_offset         = 0;
    m_stride                 = align_up(min_slot_size, m_alignment);
    if (m_stride < min_slot_size || m_stride < sizeof(slot_header)) {
        throw std::invalid_argument("fixed_size_pool: 槽位步长无效");
    }

    if (m_options.max_total_slots != 0) {
        m_next_slab_slots = std::min(m_options.initial_slab_slots, m_options.max_total_slots);
    } else {
        m_next_slab_slots = m_options.initial_slab_slots;
    }
}

fixed_size_pool::~fixed_size_pool()
{
    shutdown_registered_caches();
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

void* fixed_size_pool::allocate_fast(local_cache& cache)
{
    auto* slot = cache.fast_free_list;
    if (MC_LIKELY(slot != nullptr)) {
        cache.fast_free_list = slot->next;
        slot->next           = nullptr;
        --cache.fast_free_count;
        return payload_from_slot(slot);
    }
    return allocate_slow(cache);
}

void fixed_size_pool::deallocate_fast(void* item, local_cache& cache) noexcept
{
    auto* slot = slot_from_payload(item);
    if (MC_UNLIKELY(!m_has_local_cache)) {
        deallocate_slow(slot, cache);
        return;
    }

    slot->next           = cache.fast_free_list;
    cache.fast_free_list = slot;
    ++cache.fast_free_count;
    if (MC_UNLIKELY(cache.fast_free_count > local_hard_limit())) {
        deallocate_slow(slot, cache);
    }
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

std::size_t fixed_size_pool::next_slab_capacity(std::size_t required_slots) const noexcept
{
    auto capacity = std::max(m_next_slab_slots, required_slots);
    return std::min(capacity, m_options.max_slab_slots);
}

void fixed_size_pool::allocate_slab_locked(std::size_t capacity)
{
    if (capacity == 0) {
        return;
    }

    const auto raw_bytes = align_up(m_stride * capacity, m_alignment);
    if (raw_bytes < m_stride || raw_bytes / m_stride < capacity) {
        throw std::overflow_error("fixed_size_pool: slab 大小溢出");
    }

    void* storage = std::aligned_alloc(m_alignment, raw_bytes);
    if (storage == nullptr) {
        throw std::bad_alloc();
    }

    auto new_slab           = std::make_unique<slab>();
    new_slab->storage       = storage;
    new_slab->capacity      = capacity;
    new_slab->central_count = capacity;

    auto* base = static_cast<unsigned char*>(storage);
    for (std::size_t i = 0; i < capacity; ++i) {
        auto* slot          = reinterpret_cast<slot_header*>(base + i * m_stride);
        slot->next          = m_central_free_list;
        m_central_free_list = slot;
    }

    m_total_slots += capacity;
    m_central_free_slots += capacity;

    if (m_next_slab_slots < m_options.max_slab_slots) {
        const auto doubled = m_next_slab_slots > std::numeric_limits<std::size_t>::max() / 2
                                 ? std::numeric_limits<std::size_t>::max()
                                 : m_next_slab_slots << 1;
        m_next_slab_slots  = std::min(std::max(doubled, capacity), m_options.max_slab_slots);
    }

    new_slab->next = std::move(m_slabs);
    m_slabs        = std::move(new_slab);
}

void fixed_size_pool::reserve_locked(std::size_t free_slots)
{
    while (m_central_free_slots < free_slots) {
        const auto needed   = free_slots - m_central_free_slots;
        auto       capacity = next_slab_capacity(needed);
        if (m_options.max_total_slots != 0) {
            if (m_total_slots >= m_options.max_total_slots) {
                throw std::bad_alloc();
            }
            capacity = std::min(capacity, m_options.max_total_slots - m_total_slots);
        }
        if (capacity == 0) {
            throw std::bad_alloc();
        }
        allocate_slab_locked(capacity);
    }
}

fixed_size_pool::slab* fixed_size_pool::find_slab_by_slot_locked(const slot_header* slot) noexcept
{
    for (auto* current = m_slabs.get(); current != nullptr; current = current->next.get()) {
        if (slot_belongs_to_slab(slot, current)) {
            return current;
        }
    }
    return nullptr;
}

bool fixed_size_pool::slot_belongs_to_slab(const slot_header* slot, const slab* owner) const noexcept
{
    if (slot == nullptr || owner == nullptr || owner->storage == nullptr) {
        return false;
    }

    const auto* slot_bytes = reinterpret_cast<const unsigned char*>(slot);
    const auto* slab_begin = static_cast<const unsigned char*>(owner->storage);
    const auto* slab_end   = slab_begin + owner->capacity * m_stride;
    return slot_bytes >= slab_begin && slot_bytes < slab_end;
}

fixed_size_pool::slot_header* fixed_size_pool::pop_central_locked()
{
    if (m_central_free_list == nullptr) {
        return nullptr;
    }

    auto* slot          = m_central_free_list;
    m_central_free_list = slot->next;
    slot->next          = nullptr;
    --m_central_free_slots;
    return slot;
}

void fixed_size_pool::push_central_locked(slot_header* slot) noexcept
{
    slot->next          = m_central_free_list;
    m_central_free_list = slot;
    ++m_central_free_slots;
}

void fixed_size_pool::rebuild_central_counts_locked() noexcept
{
    for (auto* current = m_slabs.get(); current != nullptr; current = current->next.get()) {
        current->central_count = 0;
    }

    for (auto* slot = m_central_free_list; slot != nullptr; slot = slot->next) {
        auto* owner = find_slab_by_slot_locked(slot);
        if (owner != nullptr) {
            ++owner->central_count;
        }
    }
}

std::size_t fixed_size_pool::drain_local_cache_locked(local_cache& cache, std::size_t keep_count) noexcept
{
    auto* list = cache.fast_free_list;
    if (list == nullptr) {
        cache.fast_free_count = 0;
        return 0;
    }

    const auto released_count = cache.fast_free_count;
    cache.fast_free_list      = nullptr;
    cache.fast_free_count     = 0;

    if (keep_count == 0) {
        auto* released_tail = list;
        for (auto* current = list; current != nullptr; current = current->next) {
            released_tail = current;
        }
        released_tail->next = m_central_free_list;
        m_central_free_list = list;
        m_central_free_slots += released_count;
        return released_count;
    }

    slot_header* kept_head     = nullptr;
    slot_header* kept_tail     = nullptr;
    slot_header* released_head = list;
    std::size_t  kept          = 0;

    while (released_head != nullptr && kept < keep_count) {
        if (kept_head == nullptr) {
            kept_head = released_head;
        }
        kept_tail     = released_head;
        released_head = released_head->next;
        ++kept;
    }

    if (kept_tail != nullptr) {
        kept_tail->next = nullptr;
    }

    cache.fast_free_list  = kept_head;
    cache.fast_free_count = kept;

    if (released_head == nullptr) {
        return 0;
    }

    slot_header* released_tail = released_head;
    std::size_t  released      = 1;
    while (released_tail->next != nullptr) {
        released_tail = released_tail->next;
        ++released;
    }

    released_tail->next = m_central_free_list;
    m_central_free_list = released_head;
    m_central_free_slots += released;
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
    const auto needed       = target_batch > cache.fast_free_count ? target_batch - cache.fast_free_count : 0;
    if (needed == 0) {
        return;
    }

    slot_header* batch_head = nullptr;
    slot_header* batch_tail = nullptr;
    std::size_t  refilled   = 0;
    while (refilled < needed && m_central_free_list != nullptr) {
        auto* slot          = m_central_free_list;
        m_central_free_list = slot->next;
        slot->next          = batch_head;
        batch_head          = slot;
        if (batch_tail == nullptr) {
            batch_tail = slot;
        }
        ++refilled;
    }

    if (refilled != 0) {
        batch_tail->next     = cache.fast_free_list;
        cache.fast_free_list = batch_head;
        cache.fast_free_count += refilled;
        m_central_free_slots -= refilled;
    }
}

void fixed_size_pool::flush_local_cache(local_cache& cache, std::size_t keep_count)
{
    if (cache.fast_free_count <= keep_count) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    drain_local_cache_locked(cache, keep_count);
}

void* fixed_size_pool::allocate_slow(local_cache& cache)
{
    if (m_has_local_cache && cache.fast_free_list == nullptr) {
        refill_local_cache(cache);
    }

    if (m_has_local_cache) {
        auto* slot = cache.fast_free_list;
        if (slot != nullptr) {
            cache.fast_free_list = slot->next;
            slot->next           = nullptr;
            --cache.fast_free_count;
            return payload_from_slot(slot);
        }
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    reserve_locked(1);
    auto* slot = pop_central_locked();
    if (slot == nullptr) {
        if (m_options.max_total_slots != 0 && m_total_slots >= m_options.max_total_slots) {
            throw std::bad_alloc();
        }

        auto capacity = next_slab_capacity(1);
        if (m_options.max_total_slots != 0) {
            capacity = std::min(capacity, m_options.max_total_slots - m_total_slots);
        }
        if (capacity == 0) {
            throw std::bad_alloc();
        }

        allocate_slab_locked(capacity);
        slot = pop_central_locked();
    }
    if (slot == nullptr) {
        throw std::bad_alloc();
    }
    return payload_from_slot(slot);
}

void fixed_size_pool::deallocate_slow(slot_header* slot, local_cache& cache) noexcept
{
    if (!m_has_local_cache) {
        std::lock_guard<std::mutex> lock(m_mutex);
        push_central_locked(slot);
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
    cache.owner           = nullptr;
    cache.fast_free_list  = nullptr;
    cache.fast_free_count = 0;
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
        current->owner           = nullptr;
        current->fast_free_list  = nullptr;
        current->fast_free_count = 0;
        current->owner_next      = nullptr;
        current->registered      = false;
        current                  = next;
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
    return allocate_fast(cache);
}

void fixed_size_pool::deallocate(void* item) noexcept
{
    if (item == nullptr) {
        return;
    }

    auto& cache = acquire_local_cache();
    deallocate_fast(item, cache);
}

std::size_t fixed_size_pool::trim(std::size_t keep_free_slots)
{
    auto& cache = acquire_local_cache();
    if (m_has_local_cache) {
        flush_local_cache(cache, 0);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    rebuild_central_counts_locked();

    std::size_t reclaimable_empty_slabs = 0;
    for (auto* current = m_slabs.get(); current != nullptr; current = current->next.get()) {
        if (current->central_count == current->capacity) {
            ++reclaimable_empty_slabs;
        }
    }

    std::vector<slab*> slabs_to_release;
    std::size_t        scheduled_release_slots = 0;
    std::size_t        remaining_empty_slabs   = reclaimable_empty_slabs;
    std::size_t        remaining_free_slots    = m_central_free_slots;

    for (auto* current = m_slabs.get(); current != nullptr; current = current->next.get()) {
        if (current->central_count != current->capacity) {
            continue;
        }

        const bool over_empty_limit = remaining_empty_slabs > s_max_empty_slabs;
        const bool keeps_enough_free =
            remaining_free_slots >= current->capacity && (remaining_free_slots - current->capacity) >= keep_free_slots;
        if (over_empty_limit && keeps_enough_free) {
            slabs_to_release.push_back(current);
            remaining_free_slots -= current->capacity;
            scheduled_release_slots += current->capacity;
            --remaining_empty_slabs;
        }
    }

    if (slabs_to_release.empty()) {
        return 0;
    }

    auto should_release = [&slabs_to_release](const slab* candidate) {
        return std::find(slabs_to_release.begin(), slabs_to_release.end(), candidate) != slabs_to_release.end();
    };

    slot_header* rebuilt_central = nullptr;
    auto*        slot            = m_central_free_list;
    while (slot != nullptr) {
        auto* next  = slot->next;
        auto* owner = find_slab_by_slot_locked(slot);
        if (owner == nullptr || !should_release(owner)) {
            slot->next      = rebuilt_central;
            rebuilt_central = slot;
        }
        slot = next;
    }
    m_central_free_list = rebuilt_central;

    auto* cursor = &m_slabs;
    while (*cursor != nullptr) {
        if (!should_release(cursor->get())) {
            cursor = &((*cursor)->next);
            continue;
        }

        auto released = std::move(*cursor);
        *cursor       = std::move(released->next);
    }

    m_total_slots -= scheduled_release_slots;
    m_central_free_slots -= scheduled_release_slots;
    return scheduled_release_slots;
}

} // namespace mc::memory
