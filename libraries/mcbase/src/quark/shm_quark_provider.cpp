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

/**
 * @file shm_quark_provider.cpp
 * @brief 共享内存后端 quark provider 实现
 */

#include "include/shm_quark_provider.h"

#include <cstring>

#include "securec.h"
#include <mc/log/log.h>
#include <mc/quark.h>
#include <mc/shm/region.h>

namespace mc::quark_detail {

namespace {

constexpr const char* k_root_name = "mc.quark";

} // namespace

std::size_t shm_quark_provider::_next_pow2(std::size_t value) noexcept
{
    std::size_t result = 8U;
    while (result < value) {
        result <<= 1U;
    }
    return result;
}

shm_quark_provider::shm_quark_provider(mc::shm::shm_runtime& runtime, std::size_t bucket_count,
                                       std::size_t soft_max_count, std::size_t arena_size) noexcept
    : m_runtime(&runtime)
{
    if (!runtime.is_valid()) {
        return;
    }

    m_allocator = runtime.user_arena();
    if (!m_allocator.is_valid()) {
        return;
    }

    auto&      region   = runtime.region();
    const auto existing = region.find_root(mc::string_view(k_root_name));

    if (existing.has_value()) {
        m_control = region.ptr_from_offset<shm_quark_control>(existing->offset);
        if (m_control == nullptr || m_control->magic.load(std::memory_order_acquire) != shm_quark_control::magic_value
            || m_control->initialized.load(std::memory_order_acquire) != 1U) {
            m_control = nullptr;
            return;
        }
    } else {
        const std::size_t bc = _next_pow2(bucket_count == 0U ? 8U : bucket_count);
        const std::size_t sm = soft_max_count == 0U ? 1U : soft_max_count;
        const std::size_t as = arena_size == 0U ? 64U * 1024U : arena_size;

        void* control_mem = m_allocator.allocate(sizeof(shm_quark_control), alignof(shm_quark_control));
        if (control_mem == nullptr) {
            return;
        }
        m_control                    = ::new (control_mem) shm_quark_control();
        m_control->bucket_count      = static_cast<std::uint32_t>(bc);
        m_control->soft_max_count    = static_cast<std::uint32_t>(sm);
        m_control->records_capacity  = static_cast<std::uint32_t>(bc);
        m_control->arena_size        = as;

        if (!_initialize_control_locked()) {
            m_allocator.deallocate(control_mem);
            m_control = nullptr;
            return;
        }

        const std::uint64_t off = region.offset_of(m_control);
        if (!region.upsert_root(mc::string_view(k_root_name), off, sizeof(shm_quark_control),
                                mc::shm::root_kind::opaque)) {
            m_control = nullptr;
            return;
        }
    }

    if (m_control != nullptr) {
        _build_local_set_locked();
    }
}

shm_quark_provider::~shm_quark_provider() = default;

bool shm_quark_provider::is_valid() const noexcept
{
    return m_control != nullptr && m_set != nullptr;
}

bool shm_quark_provider::_initialize_control_locked() noexcept
{
    using bucket_type = mc::intrusive::detail::unordered_bucket;
    using record_ptr  = mc::intrusive::offset_ptr<quark_record>;

    void* buckets_mem = m_allocator.allocate(sizeof(bucket_type) * m_control->bucket_count, alignof(bucket_type));
    if (buckets_mem == nullptr) {
        return false;
    }
    auto* buckets = static_cast<bucket_type*>(buckets_mem);
    for (std::uint32_t i = 0U; i < m_control->bucket_count; ++i) {
        ::new (buckets + i) bucket_type();
    }
    m_control->buckets = buckets;

    void* records_mem = m_allocator.allocate(sizeof(record_ptr) * m_control->records_capacity, alignof(record_ptr));
    if (records_mem == nullptr) {
        m_allocator.deallocate(buckets_mem);
        return false;
    }
    auto* records = static_cast<record_ptr*>(records_mem);
    for (std::uint32_t i = 0U; i < m_control->records_capacity; ++i) {
        ::new (records + i) record_ptr();
    }
    m_control->records_by_id = records;

    void* arena_mem = m_allocator.allocate(m_control->arena_size, alignof(std::max_align_t));
    if (arena_mem == nullptr) {
        m_allocator.deallocate(buckets_mem);
        m_allocator.deallocate(records_mem);
        return false;
    }
    m_control->string_arena = static_cast<char*>(arena_mem);

    auto* empty_record_mem = m_allocator.allocate(sizeof(quark_record), alignof(quark_record));
    if (empty_record_mem == nullptr) {
        m_allocator.deallocate(buckets_mem);
        m_allocator.deallocate(records_mem);
        m_allocator.deallocate(arena_mem);
        return false;
    }
    auto* empty_record  = ::new (empty_record_mem) quark_record();
    empty_record->id    = mc::quark::empty_id;
    empty_record->size  = 0U;
    empty_record->hash  = fnv1a32(mc::string_view());
    char* empty_str     = _store_string_locked(mc::string_view());
    empty_record->data  = empty_str;
    records[mc::quark::empty_id] = empty_record;

    m_control->live_count.store(1U, std::memory_order_release);
    m_control->next_id.store(2U, std::memory_order_release);
    m_control->magic.store(shm_quark_control::magic_value, std::memory_order_release);
    m_control->version.store(shm_quark_control::version_value, std::memory_order_release);
    m_control->initialized.store(1U, std::memory_order_release);
    return true;
}

void shm_quark_provider::_build_local_set_locked()
{
    auto* buckets = m_control->buckets.get();
    if (buckets == nullptr) {
        return;
    }
    m_set = std::make_unique<set_type>(set_type::bucket_traits(buckets, m_control->bucket_count),
                                       view_hasher{}, view_equal{});

    if (m_control->live_count.load(std::memory_order_acquire) == 1U) {
        bool empty_already_linked = false;
        for (std::uint32_t i = 0U; i < m_control->bucket_count; ++i) {
            if (buckets[i].head.get() != nullptr) {
                empty_already_linked = true;
                break;
            }
        }
        if (!empty_already_linked) {
            auto* empty_record = m_control->records_by_id.get()[mc::quark::empty_id].get();
            if (empty_record != nullptr) {
                m_set->insert(*empty_record);
            }
        }
    }
}

mc::quark::id_type shm_quark_provider::intern(mc::string_view value)
{
    if (!is_valid()) {
        return mc::quark::invalid_id;
    }
    if (value.empty()) {
        return mc::quark::empty_id;
    }

    const auto hash = fnv1a32(value);

    {
        mc::shm::ipc_shared_lock_guard read_guard(m_control->lock);
        auto                           it = m_set->find(value, view_hasher{}, view_equal{});
        if (it != m_set->end()) {
            return it->id;
        }
    }

    mc::shm::ipc_unique_lock_guard write_guard(m_control->lock);

    auto it = m_set->find(value, view_hasher{}, view_equal{});
    if (it != m_set->end()) {
        return it->id;
    }

    return _create_record_locked(value, hash);
}

mc::quark::id_type shm_quark_provider::lookup(mc::string_view value) noexcept
{
    if (!is_valid()) {
        return mc::quark::invalid_id;
    }
    if (value.empty()) {
        return mc::quark::empty_id;
    }

    mc::shm::ipc_shared_lock_guard read_guard(m_control->lock);
    auto                           it = m_set->find(value, view_hasher{}, view_equal{});
    return it != m_set->end() ? it->id : mc::quark::invalid_id;
}

const quark_record* shm_quark_provider::resolve(mc::quark::id_type id) noexcept
{
    if (!is_valid()) {
        return nullptr;
    }
    if (id == mc::quark::invalid_id) {
        return nullptr;
    }
    const auto next = m_control->next_id.load(std::memory_order_acquire);
    if (id >= next) {
        return nullptr;
    }
    return m_control->records_by_id.get()[id].get();
}

std::size_t shm_quark_provider::size() const noexcept
{
    if (m_control == nullptr) {
        return 0U;
    }
    return static_cast<std::size_t>(m_control->live_count.load(std::memory_order_acquire));
}

std::size_t shm_quark_provider::bucket_count() const noexcept
{
    return m_control == nullptr ? 0U : m_control->bucket_count;
}

quark_record* shm_quark_provider::_allocate_record_locked() noexcept
{
    void* mem = m_allocator.allocate(sizeof(quark_record), alignof(quark_record));
    if (mem == nullptr) {
        return nullptr;
    }
    return ::new (mem) quark_record();
}

char* shm_quark_provider::_store_string_locked(mc::string_view value) noexcept
{
    const std::uint64_t need = static_cast<std::uint64_t>(value.size()) + 1U;
    const std::uint64_t cur  = m_control->arena_used.load(std::memory_order_acquire);
    if (cur + need > m_control->arena_size) {
        return nullptr;
    }
    char* arena  = m_control->string_arena.get();
    char* target = arena + cur;
    if (value.size() > 0U) {
        (void)memcpy_s(target, m_control->arena_size - cur, value.data(), value.size());
    }
    target[value.size()] = '\0';
    m_control->arena_used.store(cur + need, std::memory_order_release);
    return target;
}

void shm_quark_provider::_emit_overflow_warning_locked()
{
    if (!m_overflow_warned) {
        wlog("mc::quark/shm soft max count ${limit} exceeded; lookup will degrade to O(N), "
             "consider raising quark_max_count",
             ("limit", m_control->soft_max_count));
        m_overflow_warned = true;
    }
}

mc::quark::id_type shm_quark_provider::_create_record_locked(mc::string_view value, std::uint32_t hash) noexcept
{
    const auto new_id = m_control->next_id.load(std::memory_order_acquire);
    if (new_id >= m_control->records_capacity) {
        _emit_overflow_warning_locked();
        return mc::quark::invalid_id;
    }

    char* str = _store_string_locked(value);
    if (str == nullptr) {
        _emit_overflow_warning_locked();
        return mc::quark::invalid_id;
    }

    quark_record* record = _allocate_record_locked();
    if (record == nullptr) {
        return mc::quark::invalid_id;
    }
    record->id   = new_id;
    record->size = static_cast<std::uint32_t>(value.size());
    record->hash = hash;
    record->data = str;

    m_set->insert(*record);
    m_control->records_by_id.get()[new_id] = record;
    m_control->next_id.store(new_id + 1U, std::memory_order_release);
    const auto live = m_control->live_count.fetch_add(1U, std::memory_order_acq_rel) + 1U;

    if (live == static_cast<std::uint64_t>(m_control->soft_max_count) + 1U) {
        _emit_overflow_warning_locked();
    }

    return new_id;
}

} // namespace mc::quark_detail
