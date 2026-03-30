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

#include <mc/runtime/any_executor.h>
#include <mc/runtime/runtime_context.h>

#include "runtime/include/any_executor_access.h"
#include "runtime/include/thread_pool_impl.h"

namespace mc::runtime {

namespace {

template <typename T, typename = void>
struct has_bound_pool : std::false_type {};

template <typename T>
struct has_bound_pool<T, std::void_t<decltype(std::declval<T&>().bound_pool(nullptr))>> : std::true_type {};

template <typename T>
inline constexpr bool has_bound_pool_v = has_bound_pool<T>::value;

template <typename T, typename = void>
struct has_running_in_this_thread : std::false_type {};

template <typename T>
struct has_running_in_this_thread<T, std::void_t<decltype(std::declval<const T&>().running_in_this_thread())>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_running_in_this_thread_v = has_running_in_this_thread<T>::value;

template <typename Executor>
const Executor& as(const void* storage)
{
    return *static_cast<const Executor*>(storage);
}

template <typename Executor>
Executor& as(void* storage)
{
    return *static_cast<Executor*>(storage);
}

template <typename Executor>
constexpr detail::any_executor_backend_kind get_backend_kind();

template <>
constexpr detail::any_executor_backend_kind get_backend_kind<immediate_executor>()
{
    return detail::any_executor_backend_kind::immediate;
}

template <>
constexpr detail::any_executor_backend_kind get_backend_kind<thread_pool::executor_type>()
{
    return detail::any_executor_backend_kind::thread_pool;
}

template <>
constexpr detail::any_executor_backend_kind get_backend_kind<runtime_strand>()
{
    return detail::any_executor_backend_kind::runtime_strand;
}

template <>
constexpr detail::any_executor_backend_kind get_backend_kind<runtime_executor>()
{
    return detail::any_executor_backend_kind::runtime_executor;
}

} // namespace

template <typename Executor>
const any_executor::executor_ops& any_executor::ops()
{
    static_assert(any_executor::fits_inline_v<Executor>, "Executor storage too large for any_executor");

    static const any_executor::executor_ops ops = {
        get_backend_kind<Executor>(),
        // destroy
        [](void* storage) noexcept {
        as<Executor>(storage).~Executor();
    },
        // copy
        [](const void* src, void* dst) {
        new (dst) Executor(as<Executor>(src));
    },
        // move
        [](void* src, void* dst) noexcept {
        new (dst) Executor(std::move(as<Executor>(src)));
        as<Executor>(src).~Executor();
    },
        // equal
        [](const void* lhs, const void* rhs) noexcept {
        return as<Executor>(lhs) == as<Executor>(rhs);
    },
        // post
        [](const void* storage, any_executor::function&& f) {
        as<Executor>(storage).post(std::move(f), std::allocator<void>{});
    },
        // defer
        [](const void* storage, any_executor::function&& f) {
        as<Executor>(storage).defer(std::move(f), std::allocator<void>{});
    },
        // dispatch
        [](const void* storage, any_executor::function&& f) {
        as<Executor>(storage).dispatch(std::move(f), std::allocator<void>{});
    },
        // on_work_started
        [](const void* storage) noexcept {
        if constexpr (std::is_same_v<Executor, thread_pool::executor_type>) {
            detail::thread_pool_impl::on_work_started(as<Executor>(storage));
        } else {
            as<Executor>(storage).on_work_started();
        }
    },
        // on_work_finished
        [](const void* storage) noexcept {
        if constexpr (std::is_same_v<Executor, thread_pool::executor_type>) {
            detail::thread_pool_impl::on_work_finished(as<Executor>(storage));
        } else {
            as<Executor>(storage).on_work_finished();
        }
    },
        // running_in_this_thread
        [](const void* storage) noexcept {
        if constexpr (has_running_in_this_thread_v<Executor>) {
            return as<Executor>(storage).running_in_this_thread();
        }
        return false;
    },
        // bound_pool
        [](void* storage, thread_pool* pool) noexcept {
        if constexpr (has_bound_pool_v<Executor>) {
            as<Executor>(storage).bound_pool(pool);
        } else {
            MC_UNUSED(storage);
            MC_UNUSED(pool);
        }
    },
        // get_bound_pool
        [](const void* storage) noexcept -> thread_pool* {
        if constexpr (has_bound_pool_v<Executor>) {
            return as<Executor>(storage).get_bound_pool();
        }
        MC_UNUSED(storage);
        return nullptr;
    },
    };

    return ops;
}

template <typename Executor>
void any_executor::emplace(Executor executor)
{
    new (storage()) Executor(std::move(executor));
    m_ops = &ops<Executor>();
}

any_executor::any_executor()
{
    emplace(immediate_executor());
}

any_executor::any_executor(immediate_executor executor)
{
    emplace(std::move(executor));
}

any_executor::any_executor(thread_pool::executor_type executor)
{
    emplace(std::move(executor));
}

any_executor::any_executor(runtime_strand executor)
{
    emplace(std::move(executor));
}

any_executor::any_executor(runtime_executor executor)
{
    emplace(std::move(executor));
}

any_executor::any_executor(const any_executor& other) : m_ops(other.m_ops)
{
    if (m_ops) {
        m_ops->copy(other.storage(), storage());
    }
}

any_executor::any_executor(any_executor&& other) noexcept : m_ops(other.m_ops)
{
    if (m_ops) {
        m_ops->move(other.storage(), storage());
        other.m_ops = nullptr;
    }
}

any_executor& any_executor::operator=(const any_executor& other)
{
    if (this == &other) {
        return *this;
    }
    if (m_ops) {
        m_ops->destroy(storage());
    }
    m_ops = other.m_ops;
    if (m_ops) {
        m_ops->copy(other.storage(), storage());
    }
    return *this;
}

any_executor& any_executor::operator=(any_executor&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    if (m_ops) {
        m_ops->destroy(storage());
    }
    m_ops = other.m_ops;
    if (m_ops) {
        m_ops->move(other.storage(), storage());
        other.m_ops = nullptr;
    }
    return *this;
}

any_executor::~any_executor()
{
    if (m_ops) {
        m_ops->destroy(storage());
    }
}

bool any_executor::valid() const noexcept
{
    return m_ops != nullptr;
}

bool any_executor::operator==(const any_executor& other) const noexcept
{
    if (m_ops != other.m_ops) {
        return false;
    }
    if (!m_ops) {
        return true;
    }
    return m_ops->equal(storage(), other.storage());
}

bool any_executor::operator!=(const any_executor& other) const noexcept
{
    return !(*this == other);
}

void any_executor::on_work_started() const noexcept
{
    if (m_ops) {
        m_ops->on_work_started(storage());
    }
}

void any_executor::on_work_finished() const noexcept
{
    if (m_ops) {
        m_ops->on_work_finished(storage());
    }
}

bool any_executor::running_in_this_thread() const noexcept
{
    return m_ops ? m_ops->running_in_this_thread(storage()) : false;
}

any_executor& any_executor::bound_pool(thread_pool* pool) noexcept
{
    if (m_ops) {
        m_ops->bound_pool(storage(), pool);
    }
    return *this;
}

thread_pool* any_executor::get_bound_pool() const noexcept
{
    return m_ops ? m_ops->get_bound_pool(storage()) : nullptr;
}

boost::asio::io_context::executor_type detail::any_executor_access::get_native_io_executor(const any_executor& executor)
{
    auto& io_pool              = runtime::get_io_context();
    auto  select_pool_executor = [](thread_pool& pool) {
        if (detail::thread_pool_impl::current_pool() == &pool) {
            return detail::thread_pool_impl::get_native_io_executor(detail::thread_pool_impl::current_shard());
        }

        static std::atomic<std::size_t> round_robin{0};
        auto                            idx = round_robin.fetch_add(1, std::memory_order_relaxed) % pool.shard_count();
        return detail::thread_pool_impl::get_native_io_executor(pool, idx);
    };

    if (!executor.m_ops) {
        return select_pool_executor(io_pool);
    }

    switch (executor.m_ops->kind) {
        case detail::any_executor_backend_kind::immediate:
            return select_pool_executor(io_pool);
        case detail::any_executor_backend_kind::thread_pool: {
            const auto& pool_executor = as<thread_pool::executor_type>(executor.storage());
            auto&       pool          = detail::thread_pool_impl::get_pool(pool_executor);

            if (detail::thread_pool_impl::current_pool() == &pool) {
                return detail::thread_pool_impl::get_native_io_executor(detail::thread_pool_impl::current_shard());
            }

            static std::atomic<std::size_t> thread_pool_round_robin{0};
            auto idx = thread_pool_round_robin.fetch_add(1, std::memory_order_relaxed) % pool.shard_count();
            return detail::thread_pool_impl::get_native_io_executor(pool, idx);
        }
        case detail::any_executor_backend_kind::runtime_executor: {
            const auto& runtime_exec = as<runtime_executor>(executor.storage());

            if (auto* bound_pool = runtime_exec.get_bound_pool()) {
                static std::atomic<std::size_t> bound_round_robin{0};
                auto idx = bound_round_robin.fetch_add(1, std::memory_order_relaxed) % bound_pool->shard_count();
                return detail::thread_pool_impl::get_native_io_executor(*bound_pool, idx);
            }

            if (auto* shard = detail::thread_pool_impl::current_shard()) {
                return detail::thread_pool_impl::get_native_io_executor(shard);
            }

            auto&                           pool = runtime_exec.get_runtime_context().io();
            static std::atomic<std::size_t> round_robin{0};
            auto idx = round_robin.fetch_add(1, std::memory_order_relaxed) % pool.shard_count();
            return detail::thread_pool_impl::get_native_io_executor(pool, idx);
        }
        case detail::any_executor_backend_kind::runtime_strand: {
            const auto& strand = as<runtime_strand>(executor.storage());
            if (strand.running_in_this_thread()) {
                if (auto* shard = detail::thread_pool_impl::current_shard()) {
                    return detail::thread_pool_impl::get_native_io_executor(shard);
                }
            }

            if (auto* bound_pool = strand.get_bound_pool()) {
                static std::atomic<std::size_t> bound_round_robin{0};
                auto idx = bound_round_robin.fetch_add(1, std::memory_order_relaxed) % bound_pool->shard_count();
                return detail::thread_pool_impl::get_native_io_executor(*bound_pool, idx);
            }

            return select_pool_executor(io_pool);
        }
    }

    return select_pool_executor(io_pool);
}

} // namespace mc::runtime
