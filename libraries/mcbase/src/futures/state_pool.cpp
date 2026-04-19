/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <mc/futures/state_pool.h>
#include <mc/memory/fixed_size_pool.h>

#include <algorithm>
#include <atomic>
#include <cstdint>

namespace mc::memory {

// 显式实例化定义：与 state_pool.h 中 extern template 声明配对；MC_API 保证符号从库中导出
template class MC_API mc::memory::shared_ptr<mc::futures::state_base>;

} // namespace mc::memory

namespace mc::futures {

namespace detail {

bool pooled_state_try_release(state_base* ptr)
{
    if (!ptr) {
        return false;
    }
    auto& pool = state_pool::instance();
    return pool.try_release_to_pool(ptr);
}

void recover_state_slot_to_pool(void* ptr)
{
    MC_ASSERT(ptr != nullptr, "recover_state_slot_to_pool 需要有效槽位");
    auto* fallback = new (ptr) state_base(state_base::executor_type{});
    fallback->bind_cached_state_destroy([](state_base* state) noexcept {
        state->~state_base();
    });
    const bool released = pooled_state_try_release(fallback);
    MC_ASSERT(released, "恢复后的 state 槽位必须能够归还到池中");
}

} // namespace detail

namespace {

std::unique_ptr<mc::memory::fixed_size_pool> make_state_base_pool()
{
    mc::memory::fixed_size_pool::options options;
    options.local_cache_capacity = 32;
    const auto alignment         = std::max(static_cast<std::size_t>(8), alignof(state_base));
    return std::make_unique<mc::memory::fixed_size_pool>(sizeof(state_base), alignment, options);
}

} // namespace

class state_pool::impl {
public:
    impl();
    ~impl();

    state_pool::pool_stats get_stats() const;

    void                             destroy_cached_states();
    void                             clear_all_pools();
    state_pool::acquire_state_result try_acquire_state(std::size_t state_size);
    bool                             try_release_state_to_pool(state_base* ptr);

private:
    std::atomic_size_t                           m_cached_state_count{0};
    std::unique_ptr<mc::memory::fixed_size_pool> m_state_base_pool;
};

state_pool::impl::impl() : m_state_base_pool(make_state_base_pool())
{}

state_pool::impl::~impl()
{
    destroy_cached_states();
}

state_pool::pool_stats state_pool::impl::get_stats() const
{
    return {
        m_cached_state_count.load(std::memory_order_acquire),
        1U,
    };
}

void state_pool::impl::destroy_cached_states()
{
    const auto payloads = m_state_base_pool->snapshot_reused_slots();
    for (auto* payload : payloads) {
        static_cast<state_base*>(payload)->destroy_cached_state_object();
    }
}

void state_pool::impl::clear_all_pools()
{
    destroy_cached_states();
    m_state_base_pool = make_state_base_pool();
    m_cached_state_count.store(0, std::memory_order_release);
}

state_pool::acquire_state_result state_pool::impl::try_acquire_state(std::size_t state_size)
{
    MC_UNUSED(state_size);
    const auto result = m_state_base_pool->allocate_with_status();
    if (result.reused) {
        const auto previous = m_cached_state_count.fetch_sub(1, std::memory_order_acq_rel);
        MC_ASSERT(previous != 0, "state_pool cached state 计数下溢");
    }

    return {
        result.ptr,
        result.reused,
    };
}

bool state_pool::impl::try_release_state_to_pool(state_base* ptr)
{
    if (!ptr) {
        return false;
    }

    m_state_base_pool->deallocate(ptr);
    m_cached_state_count.fetch_add(1, std::memory_order_acq_rel);
    return true;
}

state_pool::state_pool() : m_pimpl(std::make_unique<impl>())
{}

state_pool::~state_pool() = default;

state_pool& state_pool::instance()
{
    return mc::singleton<state_pool>::instance_with_creator([]() {
        return new state_pool();
    });
}

state_pool::pool_stats state_pool::get_stats() const
{
    return m_pimpl->get_stats();
}

void state_pool::clear_all_pools()
{
    m_pimpl->clear_all_pools();
}

state_pool::acquire_state_result state_pool::try_acquire_state(std::size_t state_size)
{
    return m_pimpl->try_acquire_state(state_size);
}

bool state_pool::try_release_to_pool(state_base* ptr)
{
    return m_pimpl->try_release_state_to_pool(ptr);
}

} // namespace mc::futures
