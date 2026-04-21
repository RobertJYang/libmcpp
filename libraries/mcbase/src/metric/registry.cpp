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

#include <mc/metric/registry.h>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>

#include <stdlib.h>

#include <mc/shm/shm_runtime.h>

#include "metric_table.h"
#include "shm_layout.h"

namespace mc::metric {
namespace {

// 把 options 标准化：capacity 上调到 2 的幂；arena 至少 4 KiB，hist arena 至少 4 KiB
detail::table_init_params normalize(const registry_options& opts) noexcept
{
    detail::table_init_params p;
    p.capacity        = detail::round_up_pow2(opts.capacity == 0 ? 64 : opts.capacity);
    p.arena_size      = opts.arena_bytes < 4096 ? 4096 : opts.arena_bytes;
    p.hist_arena_size = opts.hist_arena_bytes < 4096 ? 4096 : opts.hist_arena_bytes;
    return p;
}

// 共用 base：counter / up_down 都返回原始 body 的 atomic<int64> 句柄
counter make_counter_handle(detail::metric_slot* slot) noexcept
{
    if (slot == nullptr) {
        return counter{};
    }
    return counter{&slot->body};
}

gauge make_gauge_handle(detail::metric_slot* slot) noexcept
{
    if (slot == nullptr) {
        return gauge{};
    }
    return gauge{&slot->body};
}

class table_registry final : public registry {
public:
    table_registry(void* table_base, std::uint16_t writer_endpoint,
                   std::uint32_t writer_instance, mc::shm::shm_runtime* runtime,
                   bool owns_heap_memory)
        : m_table(table_base),
          m_writer_endpoint(writer_endpoint),
          m_writer_instance(writer_instance),
          m_runtime(runtime),
          m_owns_heap_memory(owns_heap_memory)
    {}

    ~table_registry() override
    {
        if (m_owns_heap_memory && m_table.is_valid()) {
            std::free(reinterpret_cast<void*>(m_table.header()));
        }
    }

    counter counter_of(const descriptor& d) override
    {
        bool  created = false;
        auto* slot    = m_table.upsert(d, m_writer_endpoint, m_writer_instance, created);
        return make_counter_handle(slot);
    }

    counter up_down_counter_of(const descriptor& d) override
    {
        bool  created = false;
        auto* slot    = m_table.upsert(d, m_writer_endpoint, m_writer_instance, created);
        return make_counter_handle(slot);
    }

    gauge gauge_of(const descriptor& d) override
    {
        bool  created = false;
        auto* slot    = m_table.upsert(d, m_writer_endpoint, m_writer_instance, created);
        return make_gauge_handle(slot);
    }

    histogram histogram_of(const descriptor& d) override
    {
        bool  created = false;
        auto* slot    = m_table.upsert(d, m_writer_endpoint, m_writer_instance, created);
        if (slot == nullptr) {
            return histogram{};
        }
        auto* hd = m_table.ensure_hist_data(slot, d);
        return histogram{reinterpret_cast<histogram_data*>(hd)};
    }

    std::size_t registered_count() const noexcept override
    {
        return static_cast<std::size_t>(m_table.registered());
    }

    std::size_t overflow_count() const noexcept override
    {
        return static_cast<std::size_t>(m_table.overflow());
    }

    snapshot collect() const override
    {
        snapshot out;
        m_table.collect_all(out);
        return out;
    }

    std::size_t prune_stale_writers() override
    {
        if (m_runtime == nullptr) {
            return 0;
        }
        auto* rt = m_runtime;
        return m_table.prune_stale_writers([rt](std::uint16_t eid, std::uint32_t inst) {
            return rt->writer_instance_is_current(eid, static_cast<std::uint64_t>(inst));
        });
    }

private:
    detail::metric_table  m_table;
    std::uint16_t         m_writer_endpoint;
    std::uint32_t         m_writer_instance;
    mc::shm::shm_runtime* m_runtime;
    bool                  m_owns_heap_memory;
};

void table_init_thunk(void* mem, std::uint32_t /*self_tag*/) noexcept
{
    detail::metric_table::initialize(mem, detail::current_init_params());
}

} // namespace

std::unique_ptr<registry> registry::open_shm(mc::shm::shm_runtime& rt, const registry_options& opts)
{
    if (!rt.is_valid()) {
        return nullptr;
    }

    const auto params             = normalize(opts);
    detail::current_init_params() = params;

    mc::shm::shm_runtime::named_container_spec spec;
    spec.kind_code  = 'T';
    spec.name       = opts.name;
    spec.ctrl_size  = detail::compute_table_size(params.capacity, params.arena_size, params.hist_arena_size);
    spec.ctrl_align = alignof(detail::metric_table_header);
    spec.signature  = detail::compute_table_signature(params.capacity, params.arena_size, params.hist_arena_size);
    spec.init_fn    = &table_init_thunk;

    auto handle = rt.ensure_named_container(spec);
    if (!handle) {
        return nullptr;
    }

    return std::make_unique<table_registry>(handle.control, opts.owner_endpoint,
                                            opts.writer_instance, &rt,
                                            /*owns_heap_memory=*/false);
}

std::unique_ptr<registry> registry::open_heap(const registry_options& opts)
{
    const auto params = normalize(opts);
    const auto bytes  = detail::compute_table_size(params.capacity, params.arena_size, params.hist_arena_size);
    void*      raw    = nullptr;
    if (::posix_memalign(&raw, alignof(detail::metric_table_header), bytes) != 0 || raw == nullptr) {
        return nullptr;
    }
    detail::metric_table::initialize(raw, params);
    return std::make_unique<table_registry>(raw, opts.owner_endpoint, opts.writer_instance,
                                            /*runtime=*/nullptr, /*owns_heap_memory=*/true);
}

// ---------------------------------------------------------------------------
// 进程级默认 registry
// ---------------------------------------------------------------------------
namespace {

std::shared_ptr<registry>& default_registry_storage() noexcept
{
    static std::shared_ptr<registry> s_instance;
    return s_instance;
}

std::mutex& default_registry_mutex() noexcept
{
    static std::mutex s_mutex;
    return s_mutex;
}

std::shared_ptr<registry> ensure_default_registry()
{
    std::lock_guard<std::mutex> guard(default_registry_mutex());
    auto&                       slot = default_registry_storage();
    if (!slot) {
        slot = registry::open_heap(registry_options{});
    }
    return slot;
}

} // namespace

registry& default_registry()
{
    auto inst = ensure_default_registry();
    return *inst;
}

void install_default_registry(std::shared_ptr<registry> r)
{
    std::lock_guard<std::mutex> guard(default_registry_mutex());
    default_registry_storage() = std::move(r);
}

void reset_default_registry_for_test() noexcept
{
    std::lock_guard<std::mutex> guard(default_registry_mutex());
    default_registry_storage().reset();
}

} // namespace mc::metric
