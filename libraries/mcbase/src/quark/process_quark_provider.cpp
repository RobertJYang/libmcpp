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
 * @file process_quark_provider.cpp
 * @brief 进程内 quark 后端实现
 */

#include "include/process_quark_provider.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <utility>

#include "securec.h"
#include <mc/log/log.h>
#include <mc/quark.h>

namespace mc::quark_detail {

namespace {

constexpr double      k_rehash_load_factor   = 0.75;
constexpr std::size_t k_records_headroom     = 64U;

} // namespace

std::size_t process_quark_provider::_next_pow2(std::size_t value) noexcept
{
    std::size_t result = 8U;
    while (result < value) {
        result <<= 1U;
    }
    return result;
}

std::size_t process_quark_provider::_records_capacity_for(std::size_t soft_max_count) noexcept
{
    return soft_max_count + k_records_headroom + 2U;
}

process_quark_provider::process_quark_provider(std::size_t initial_buckets, std::size_t soft_max_count)
    : m_soft_max_count(soft_max_count == 0U ? 1U : soft_max_count),
      m_records_capacity(_records_capacity_for(soft_max_count == 0U ? 1U : soft_max_count)),
      m_bucket_count(_next_pow2(initial_buckets == 0U ? 8U : initial_buckets))
{
    m_buckets = std::make_unique<set_type::bucket_type[]>(m_bucket_count);
    m_set     = std::make_unique<set_type>(set_type::bucket_traits(m_buckets.get(), m_bucket_count),
                                           view_hasher{}, view_equal{});

    m_records_by_id.reserve(m_records_capacity);

    m_records_by_id.emplace_back(nullptr);

    auto empty_record  = std::make_unique<quark_record>();
    empty_record->id   = mc::quark::empty_id;
    empty_record->size = 0U;
    empty_record->hash = fnv1a32(mc::string_view());
    empty_record->data = "";
    m_set->insert(*empty_record);
    m_records_by_id.emplace_back(std::move(empty_record));

    m_records_published.store(m_records_by_id.size(), std::memory_order_release);
}

process_quark_provider::~process_quark_provider()
{
    if (m_set != nullptr) {
        m_set->clear();
    }
}

mc::quark::id_type process_quark_provider::intern(mc::string_view value)
{
    if (value.empty()) {
        return mc::quark::empty_id;
    }

    const auto hash = fnv1a32(value);

    {
        std::shared_lock<std::shared_mutex> read_lock(m_mutex);
        auto                                it = m_set->find(value, view_hasher{}, view_equal{});
        if (it != m_set->end()) {
            return it->id;
        }
    }

    std::unique_lock<std::shared_mutex> write_lock(m_mutex);

    auto it = m_set->find(value, view_hasher{}, view_equal{});
    if (it != m_set->end()) {
        return it->id;
    }

    return _create_record_locked(value, hash);
}

mc::quark::id_type process_quark_provider::lookup(mc::string_view value) noexcept
{
    if (value.empty()) {
        return mc::quark::empty_id;
    }

    std::shared_lock<std::shared_mutex> read_lock(m_mutex);
    auto                                it = m_set->find(value, view_hasher{}, view_equal{});
    return it != m_set->end() ? it->id : mc::quark::invalid_id;
}

const quark_record* process_quark_provider::resolve(mc::quark::id_type id) noexcept
{
    const auto published = m_records_published.load(std::memory_order_acquire);
    if (id == 0U || id >= published) {
        return nullptr;
    }
    return m_records_by_id[id].get();
}

std::size_t process_quark_provider::size() const noexcept
{
    return m_records_published.load(std::memory_order_acquire) - 1U;
}

std::size_t process_quark_provider::bucket_count() const noexcept
{
    std::shared_lock<std::shared_mutex> read_lock(m_mutex);
    return m_bucket_count;
}

std::size_t process_quark_provider::capacity() const noexcept
{
    return m_records_capacity - 2U;
}

const char* process_quark_provider::_store_string(mc::string_view value)
{
    const std::size_t need = value.size() + 1U;

    if (m_string_chunks.empty() || m_string_chunks.back().used + need > m_string_chunks.back().capacity) {
        const std::size_t cap = std::max(string_arena_chunk_size, need);
        string_chunk      chunk;
        chunk.data     = std::make_unique<char[]>(cap);
        chunk.used     = 0U;
        chunk.capacity = cap;
        m_string_chunks.emplace_back(std::move(chunk));
    }

    auto&       chunk  = m_string_chunks.back();
    char* const target = chunk.data.get() + chunk.used;
    if (value.size() > 0U) {
        (void)memcpy_s(target, chunk.capacity - chunk.used, value.data(), value.size());
    }
    target[value.size()] = '\0';
    chunk.used += need;
    return target;
}

void process_quark_provider::_emit_overflow_warning_locked()
{
    if (!m_overflow_warned) {
        wlog("mc::quark soft max count ${limit} exceeded; further intern returns invalid_id, "
             "consider raising meson option quark_max_count",
             ("limit", m_soft_max_count));
        m_overflow_warned = true;
    }
}

mc::quark::id_type process_quark_provider::_create_record_locked(mc::string_view value, std::uint32_t hash)
{
    if (m_records_by_id.size() >= m_records_capacity) {
        _emit_overflow_warning_locked();
        return mc::quark::invalid_id;
    }

    const auto new_id = static_cast<mc::quark::id_type>(m_records_by_id.size());

    auto record  = std::make_unique<quark_record>();
    record->id   = new_id;
    record->size = static_cast<std::uint32_t>(value.size());
    record->hash = hash;
    record->data = _store_string(value);

    m_set->insert(*record);
    m_records_by_id.emplace_back(std::move(record));
    m_records_published.store(m_records_by_id.size(), std::memory_order_release);

    const std::size_t live = m_records_by_id.size() - 1U;
    if (live == m_soft_max_count + 1U) {
        _emit_overflow_warning_locked();
    }

    _rehash_if_needed_locked();
    return new_id;
}

void process_quark_provider::_rehash_if_needed_locked()
{
    const std::size_t current_size = m_records_by_id.size() - 1U;
    if (static_cast<double>(current_size) <= static_cast<double>(m_bucket_count) * k_rehash_load_factor) {
        return;
    }

    const std::size_t new_bucket_count = m_bucket_count << 1U;
    _rebuild_set_locked(new_bucket_count);
}

void process_quark_provider::_rebuild_set_locked(std::size_t new_bucket_count)
{
    auto new_buckets = std::make_unique<set_type::bucket_type[]>(new_bucket_count);
    auto new_set     = std::make_unique<set_type>(set_type::bucket_traits(new_buckets.get(), new_bucket_count),
                                                  view_hasher{}, view_equal{});

    m_set->clear();

    for (std::size_t id = mc::quark::empty_id; id < m_records_by_id.size(); ++id) {
        if (m_records_by_id[id] != nullptr) {
            new_set->insert(*m_records_by_id[id]);
        }
    }

    m_buckets      = std::move(new_buckets);
    m_set          = std::move(new_set);
    m_bucket_count = new_bucket_count;
}

} // namespace mc::quark_detail
