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

#include "metric_table.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <new>

namespace mc::metric::detail {
namespace {

constexpr std::size_t max_probe_distance = 128;

// 把任意非 0 的 metric_id 映射为可用的"非 0"占位；理论上 fnv1a 几乎不会 0，但保险
inline std::uint64_t normalize_id(metric_id_t id) noexcept
{
    return id == 0 ? 0xFFFFFFFFFFFFFFFFULL : id;
}

inline std::uint64_t align_up(std::uint64_t v, std::uint64_t a) noexcept
{
    return (v + (a - 1U)) & ~(a - 1U);
}

inline std::size_t clamp_string_len(std::size_t n) noexcept
{
    return n > arena_max_string ? arena_max_string : n;
}

} // namespace

void metric_table::initialize(void* base, const table_init_params& params) noexcept
{
    auto* hdr = reinterpret_cast<metric_table_header*>(base);
    std::memset(static_cast<void*>(hdr), 0, sizeof(*hdr));
    hdr->magic.store(metric_table_magic, std::memory_order_relaxed);
    hdr->version.store(metric_table_version, std::memory_order_relaxed);
    hdr->capacity        = params.capacity;
    hdr->slots_off       = align_up(sizeof(metric_table_header), alignof(metric_slot));
    hdr->arena_off       = hdr->slots_off + params.capacity * sizeof(metric_slot);
    hdr->arena_size      = params.arena_size;
    hdr->hist_arena_off  = hdr->arena_off + params.arena_size;
    hdr->hist_arena_size = params.hist_arena_size;
    hdr->arena_used.store(0, std::memory_order_relaxed);
    hdr->hist_arena_used.store(0, std::memory_order_relaxed);
    hdr->registered.store(0, std::memory_order_relaxed);
    hdr->overflow_drops.store(0, std::memory_order_relaxed);

    auto* slots = reinterpret_cast<metric_slot*>(reinterpret_cast<std::uint8_t*>(base) + hdr->slots_off);
    for (std::uint64_t i = 0; i < params.capacity; ++i) {
        new (&slots[i]) metric_slot{};
    }
}

std::uint32_t metric_table::_arena_alloc(std::uint64_t size, std::uint64_t align) noexcept
{
    auto*         hdr = header();
    std::uint64_t old = hdr->arena_used.load(std::memory_order_acquire);
    while (true) {
        const std::uint64_t aligned = align_up(old, align);
        const std::uint64_t next    = aligned + size;
        if (next > hdr->arena_size) {
            return 0;
        }
        if (hdr->arena_used.compare_exchange_weak(old, next, std::memory_order_release, std::memory_order_acquire)) {
            // 偏移为 0 留作"未分配"哨兵；偏移 +1 存入字段，读出时减 1
            return static_cast<std::uint32_t>(aligned + 1U);
        }
    }
}

std::uint32_t metric_table::_hist_arena_alloc(std::uint64_t size, std::uint64_t align) noexcept
{
    auto*         hdr = header();
    std::uint64_t old = hdr->hist_arena_used.load(std::memory_order_acquire);
    while (true) {
        const std::uint64_t aligned = align_up(old, align);
        const std::uint64_t next    = aligned + size;
        if (next > hdr->hist_arena_size) {
            return 0;
        }
        if (hdr->hist_arena_used.compare_exchange_weak(old, next, std::memory_order_release,
                                                       std::memory_order_acquire)) {
            return static_cast<std::uint32_t>(aligned + 1U);
        }
    }
}

std::uint32_t metric_table::_store_string(mc::string_view s, std::uint16_t& out_len) noexcept
{
    if (s.empty()) {
        out_len = 0;
        return 0;
    }
    const std::size_t len = clamp_string_len(s.size());
    const auto        off = _arena_alloc(len, 1);
    if (off == 0) {
        out_len = 0;
        return 0;
    }
    std::memcpy(arena() + (off - 1), s.data(), len);
    out_len = static_cast<std::uint16_t>(len);
    return off;
}

std::uint32_t metric_table::_store_descriptor(const descriptor& d) noexcept
{
    const auto rec_off = _arena_alloc(sizeof(descriptor_record), alignof(descriptor_record));
    if (rec_off == 0) {
        return 0;
    }
    auto* rec = reinterpret_cast<descriptor_record*>(arena() + (rec_off - 1));
    std::memset(rec, 0, sizeof(*rec));
    rec->kind        = static_cast<std::uint8_t>(d.k);
    rec->label_count = static_cast<std::uint8_t>(d.labels.size());
    rec->name_off    = _store_string(d.name, rec->name_len);
    rec->unit_off    = _store_string(d.unit, rec->unit_len);
    rec->help_off    = _store_string(d.help, rec->help_len);
    if (rec->name_off == 0 && !d.name.empty()) {
        return 0;
    }
    for (std::size_t i = 0; i < d.labels.size(); ++i) {
        rec->label_key_off[i]   = _store_string(d.labels.at(i).key, rec->label_key_len[i]);
        rec->label_value_off[i] = _store_string(d.labels.at(i).value, rec->label_value_len[i]);
        if (rec->label_key_off[i] == 0 && !d.labels.at(i).key.empty()) {
            return 0;
        }
    }
    return rec_off;
}

metric_slot* metric_table::upsert(const descriptor& d, std::uint16_t writer_endpoint,
                                  std::uint32_t writer_instance, bool& created) noexcept
{
    created = false;
    if (!is_valid()) {
        return nullptr;
    }
    auto*      hdr = header();
    const auto cap = hdr->capacity;
    if (cap == 0) {
        return nullptr;
    }
    const auto         mask      = cap - 1U;
    const auto         id        = normalize_id(d.signature());
    const auto         hash      = id;
    metric_slot* const slots_ptr = slots();

    for (std::size_t probe = 0; probe < max_probe_distance; ++probe) {
        const auto   idx    = (hash + probe) & mask;
        metric_slot& slot   = slots_ptr[idx];
        const auto   cur_id = slot.metric_id.load(std::memory_order_acquire);
        if (cur_id == id) {
            // 命中已有槽：若新一任 owner 接管，更新 (eid, instance) 并清 stale；
            // body / descriptor 保留以延续 counter 累计语义
            if (writer_endpoint != 0) {
                const auto cur_eid  = slot.owner_endpoint.load(std::memory_order_acquire);
                const auto cur_inst = slot.writer_instance.load(std::memory_order_acquire);
                if (cur_eid != writer_endpoint || cur_inst != writer_instance) {
                    slot.owner_endpoint.store(writer_endpoint, std::memory_order_release);
                    slot.writer_instance.store(writer_instance, std::memory_order_release);
                    slot.stale.store(0, std::memory_order_release);
                }
            }
            return &slot;
        }
        if (cur_id == 0) {
            std::uint64_t expected = 0;
            if (!slot.metric_id.compare_exchange_strong(expected, id, std::memory_order_acq_rel,
                                                        std::memory_order_acquire)) {
                if (expected == id) {
                    return &slot;
                }
                continue;
            }

            // 我们独占了这个槽。注意：body/hist_data_off/updates/stale/descriptor_off
            // 在表初始化时已被 memset 为 0，且 slot 永不回收（metric_id 单向 0->id）。
            // 因此这里只写入"本 metric 特有"的字段，避免与并发的 add()/observe()
            // 竞争 —— CAS 成功后另一线程立即可看到 metric_id 并开始 fetch_add，
            // 此时若我们再 store(body, 0) 就会清掉对方的增量。
            slot.kind_byte.store(static_cast<std::uint8_t>(d.k), std::memory_order_relaxed);
            slot.owner_endpoint.store(writer_endpoint, std::memory_order_relaxed);
            slot.writer_instance.store(writer_instance, std::memory_order_relaxed);

            const auto desc_off = _store_descriptor(d);
            slot.descriptor_off.store(desc_off, std::memory_order_release);

            if (desc_off == 0) {
                hdr->overflow_drops.fetch_add(1, std::memory_order_relaxed);
            } else {
                hdr->registered.fetch_add(1, std::memory_order_relaxed);
            }
            created = true;
            return &slot;
        }
    }

    hdr->overflow_drops.fetch_add(1, std::memory_order_relaxed);
    return nullptr;
}

std::size_t metric_table::prune_stale_writers(const liveness_probe& probe) noexcept
{
    if (!is_valid() || !probe) return 0;
    const auto    cap          = capacity();
    auto* const   slots_ptr    = slots();
    std::size_t   newly_marked = 0;
    for (std::uint64_t i = 0; i < cap; ++i) {
        auto& slot = slots_ptr[i];
        if (slot.metric_id.load(std::memory_order_acquire) == 0) continue;
        const auto eid = slot.owner_endpoint.load(std::memory_order_acquire);
        if (eid == 0) continue;
        if (slot.stale.load(std::memory_order_acquire) != 0) continue;
        const auto inst = slot.writer_instance.load(std::memory_order_acquire);
        if (!probe(eid, inst)) {
            slot.stale.store(1, std::memory_order_release);
            ++newly_marked;
        }
    }
    return newly_marked;
}

const metric_slot* metric_table::find(metric_id_t id) const noexcept
{
    if (!is_valid()) {
        return nullptr;
    }
    const auto* hdr = header();
    const auto  cap = hdr->capacity;
    if (cap == 0) {
        return nullptr;
    }
    const auto        mask      = cap - 1U;
    const auto        norm_id   = normalize_id(id);
    const auto        hash      = norm_id;
    const auto* const slots_ptr = slots();
    for (std::size_t probe = 0; probe < max_probe_distance; ++probe) {
        const auto idx    = (hash + probe) & mask;
        const auto cur_id = slots_ptr[idx].metric_id.load(std::memory_order_acquire);
        if (cur_id == norm_id) {
            return &slots_ptr[idx];
        }
        if (cur_id == 0) {
            return nullptr;
        }
    }
    return nullptr;
}

hist_data_layout* metric_table::ensure_hist_data(metric_slot* slot, const descriptor& d) noexcept
{
    if (slot == nullptr) {
        return nullptr;
    }
    auto existing = slot->hist_data_off.load(std::memory_order_acquire);
    if (existing != 0) {
        return reinterpret_cast<hist_data_layout*>(hist_arena() + (existing - 1U));
    }

    const auto bc = static_cast<std::uint32_t>(d.bucket_count);
    const auto data_size =
        sizeof(hist_data_layout) + sizeof(double) * bc + sizeof(std::atomic<std::uint64_t>) * (bc + 1U);
    const auto off = _hist_arena_alloc(data_size, alignof(hist_data_layout));
    if (off == 0) {
        header()->overflow_drops.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }

    auto* data = reinterpret_cast<hist_data_layout*>(hist_arena() + (off - 1U));
    std::memset(static_cast<void*>(data), 0, data_size);
    data->bucket_count = bc;
    auto* bounds       = reinterpret_cast<double*>(reinterpret_cast<std::uint8_t*>(data) + sizeof(hist_data_layout));
    for (std::uint32_t i = 0; i < bc; ++i) {
        bounds[i] = d.buckets != nullptr ? d.buckets[i] : 0.0;
    }
    new (&data->count) std::atomic<std::uint64_t>(0);
    new (&data->sum_bits) std::atomic<std::uint64_t>(0);
    auto* counts = reinterpret_cast<std::atomic<std::uint64_t>*>(reinterpret_cast<std::uint8_t*>(data) +
                                                                 sizeof(hist_data_layout) + sizeof(double) * bc);
    for (std::uint32_t i = 0; i <= bc; ++i) {
        new (&counts[i]) std::atomic<std::uint64_t>(0);
    }

    std::uint32_t expected = 0;
    if (!slot->hist_data_off.compare_exchange_strong(expected, off, std::memory_order_acq_rel)) {
        // 已被并发线程发布；复用对方的，不释放 arena（lock-free bump 无回收）
        return reinterpret_cast<hist_data_layout*>(hist_arena() + (expected - 1U));
    }
    return data;
}

void metric_table::slot_to_sample(const metric_slot* slot, sample& out) const
{
    out = sample{};
    if (slot == nullptr) {
        return;
    }
    out.id             = slot->metric_id.load(std::memory_order_acquire);
    out.k              = static_cast<kind>(slot->kind_byte.load(std::memory_order_acquire));
    out.owner_endpoint = slot->owner_endpoint.load(std::memory_order_acquire);
    out.stale          = slot->stale.load(std::memory_order_acquire) != 0;

    const auto desc_off = slot->descriptor_off.load(std::memory_order_acquire);
    if (desc_off != 0) {
        const auto* rec = reinterpret_cast<const descriptor_record*>(arena() + (desc_off - 1U));
        if (rec->name_off != 0 && rec->name_len > 0) {
            out.name.assign(reinterpret_cast<const char*>(arena()) + (rec->name_off - 1U), rec->name_len);
        }
        if (rec->unit_off != 0 && rec->unit_len > 0) {
            out.unit.assign(reinterpret_cast<const char*>(arena()) + (rec->unit_off - 1U), rec->unit_len);
        }
        if (rec->help_off != 0 && rec->help_len > 0) {
            out.help.assign(reinterpret_cast<const char*>(arena()) + (rec->help_off - 1U), rec->help_len);
        }
        for (std::uint8_t i = 0; i < rec->label_count && i < arena_max_labels; ++i) {
            std::string k;
            std::string v;
            if (rec->label_key_off[i] != 0) {
                k.assign(reinterpret_cast<const char*>(arena()) + (rec->label_key_off[i] - 1U), rec->label_key_len[i]);
            }
            if (rec->label_value_off[i] != 0) {
                v.assign(reinterpret_cast<const char*>(arena()) + (rec->label_value_off[i] - 1U),
                         rec->label_value_len[i]);
            }
            out.labels.emplace_back(std::move(k), std::move(v));
        }
    }

    switch (out.k) {
        case kind::counter:
        case kind::up_down_counter:
        case kind::gauge: {
            out.i_value       = slot->body.load(std::memory_order_relaxed);
            std::int64_t bits = out.i_value;
            std::memcpy(&out.d_value, &bits, sizeof(out.d_value));
            break;
        }
        case kind::histogram: {
            const auto hist_off = slot->hist_data_off.load(std::memory_order_acquire);
            if (hist_off != 0) {
                const auto* data     = reinterpret_cast<const hist_data_layout*>(hist_arena() + (hist_off - 1U));
                out.hist_total_count = data->count.load(std::memory_order_relaxed);
                std::uint64_t sb     = data->sum_bits.load(std::memory_order_relaxed);
                std::memcpy(&out.hist_sum, &sb, sizeof(out.hist_sum));
                const auto  bc     = data->bucket_count;
                const auto* bounds = reinterpret_cast<const double*>(reinterpret_cast<const std::uint8_t*>(data) +
                                                                     sizeof(hist_data_layout));
                const auto* counts = reinterpret_cast<const std::atomic<std::uint64_t>*>(
                    reinterpret_cast<const std::uint8_t*>(data) + sizeof(hist_data_layout) + sizeof(double) * bc);
                std::uint64_t cumulative = 0;
                for (std::uint32_t i = 0; i < bc; ++i) {
                    cumulative += counts[i].load(std::memory_order_relaxed);
                    out.hist_buckets.push_back({bounds[i], cumulative});
                }
                cumulative += counts[bc].load(std::memory_order_relaxed);
                out.hist_buckets.push_back({std::numeric_limits<double>::infinity(), cumulative});
            }
            break;
        }
        case kind::none:
        default:
            break;
    }
}

void metric_table::collect_all(snapshot& out) const
{
    if (!is_valid()) {
        return;
    }
    const auto  cap       = capacity();
    const auto* slots_ptr = slots();
    for (std::uint64_t i = 0; i < cap; ++i) {
        const auto id = slots_ptr[i].metric_id.load(std::memory_order_acquire);
        if (id == 0) {
            continue;
        }
        const auto desc_off = slots_ptr[i].descriptor_off.load(std::memory_order_acquire);
        if (desc_off == 0) {
            continue; // 已分配但 arena 满，descriptor 未发布
        }
        sample s;
        slot_to_sample(&slots_ptr[i], s);
        out.push_back(std::move(s));
    }
}

} // namespace mc::metric::detail
