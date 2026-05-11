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

#ifndef MC_RUNTIME_INCLUDE_THREAD_POOL_IMPL_H
#define MC_RUNTIME_INCLUDE_THREAD_POOL_IMPL_H

#include <mc/common.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include <boost/asio/detail/scheduler.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>

#include <mc/runtime/thread_pool.h>
#include <mc/sync/small_mutex.h>

namespace mc::runtime::detail {

constexpr uint16_t thread_pool_null_index    = 0xFFFF;
constexpr uint64_t thread_pool_index_mask    = 0xFFFF;
constexpr uint64_t thread_pool_tag_increment = 1ULL << 16;
constexpr uint16_t thread_pool_max_recursion_depth = 32;

enum class thread_pool_worker_state {
    Running,
    Idle,
    Notified,
};

struct predicate_ref {
    void* context         = nullptr;
    bool (*invoke)(void*) = nullptr;

    bool operator()() const
    {
        return invoke(context);
    }
};

template <typename Predicate>
predicate_ref make_predicate_ref(Predicate& predicate)
{
    return {
        &predicate,
        [](void* context) {
        return (*static_cast<Predicate*>(context))();
    },
    };
}

struct thread_pool_impl;

struct thread_pool_shard {
    using io_context          = boost::asio::io_context;
    using executor_work_guard = boost::asio::executor_work_guard<io_context::executor_type>;

    explicit thread_pool_shard(thread_pool_impl& impl_) noexcept : impl(impl_)
    {}

    thread_pool_impl&                     impl;
    std::unique_ptr<io_context>          ctx;
    std::unique_ptr<executor_work_guard> work;
    std::atomic<uint16_t>                next{thread_pool_null_index};
    std::atomic<thread_pool_worker_state> state{thread_pool_worker_state::Running};
    std::atomic<bool>                    in_stack{false};
    uint16_t                             id{thread_pool_null_index};
    uint16_t                             recursion_depth{0};
};

class thread_pool_scoped_recursion_guard {
public:
    explicit thread_pool_scoped_recursion_guard(thread_pool_shard* shard) : m_shard(shard)
    {
        if (m_shard->recursion_depth < thread_pool_max_recursion_depth) {
            m_shard->recursion_depth++;
            m_can_schedule = true;
        }
    }

    ~thread_pool_scoped_recursion_guard()
    {
        if (m_can_schedule) {
            m_shard->recursion_depth--;
        }
    }

    bool can_schedule() const
    {
        return m_can_schedule;
    }

private:
    thread_pool_shard* m_shard;
    bool               m_can_schedule{false};
};

using thread_pool_wakeup_handle = std::shared_ptr<void>;

struct thread_pool_impl : boost::asio::execution_context {
    using io_context          = boost::asio::io_context;
    using executor_work_guard = boost::asio::executor_work_guard<io_context::executor_type>;

    thread_pool_impl(thread_pool& owner, std::size_t num_threads, mc::string_view name);
    ~thread_pool_impl() = default;

    static thread_pool_shard* current_shard() noexcept;
    static thread_pool*       current_pool() noexcept;

    static thread_pool& get_pool(const thread_pool::executor_type& executor) noexcept;

    static boost::asio::io_context::executor_type get_native_io_executor(thread_pool_shard* shard);
    static boost::asio::io_context::executor_type get_native_io_executor(thread_pool& pool, std::size_t idx);
    static boost::asio::io_context::executor_type get_native_io_executor(const thread_pool::executor_type& executor);

    static void on_work_started(thread_pool& pool) noexcept;
    static void on_work_finished(thread_pool& pool) noexcept;
    static void on_work_started(const thread_pool::executor_type& executor) noexcept;
    static void on_work_finished(const thread_pool::executor_type& executor) noexcept;

    static void notify_shard(thread_pool_shard* shard) noexcept;
    static thread_pool_wakeup_handle schedule_shard_wakeup(thread_pool_shard* shard,
                                                           std::chrono::steady_clock::time_point deadline);
    static void cancel_shard_wakeup(const thread_pool_wakeup_handle& handle) noexcept;

    template <typename Predicate>
    static void poll_until(thread_pool& pool, thread_pool_shard* shard, Predicate&& predicate)
    {
        auto pred = std::forward<Predicate>(predicate);
        pool.m_impl->poll_until(shard, make_predicate_ref(pred));
    }

    void dispatch_task(thread_pool_task task);
    void post_task(thread_pool_task task);
    void defer_task(thread_pool_task task);
    bool running_in_this_thread() const noexcept;

    void           start();
    void           stop();
    void           join();
    bool           stopped() const;
    std::size_t    shard_count() const;
    std::size_t    poll_one();
    mc::string_view get_name() const noexcept;
    void           run();

private:
    friend class thread_pool;
    friend class thread_pool::executor_type;

    static boost::asio::detail::scheduler& get_or_create_scheduler(boost::asio::execution_context& ctx);

    void               schedule_global(thread_pool_task task, bool is_continuation);
    void               wake_up_workers();
    void               worker_loop(thread_pool_shard* shard);
    void               push_idle(thread_pool_shard* shard);
    thread_pool_shard* acquire_idle_worker();
    bool               has_work(thread_pool_shard* shard);
    bool               poll_tasks(thread_pool_shard* shard);
    void               poll_until(thread_pool_shard* shard, predicate_ref pred);
    void               try_sleep_until(thread_pool_shard* shard, predicate_ref pred);
    void               try_sleep(thread_pool_shard* shard);
    void               notify_shard_impl(thread_pool_shard* shard) noexcept;
    thread_pool_wakeup_handle schedule_shard_wakeup_impl(thread_pool_shard* shard,
                                                         std::chrono::steady_clock::time_point deadline);
    void cancel_shard_wakeup_impl(const thread_pool_wakeup_handle& handle) noexcept;

    thread_pool*                    m_owner;
    boost::asio::detail::scheduler& m_scheduler;
    std::size_t                     m_num_threads;
    mc::string                      m_name;
    std::vector<std::unique_ptr<thread_pool_shard>> m_shards;
    std::vector<std::thread>                     m_threads;
    std::atomic<uint64_t>                        m_idle_head;
    std::atomic<int64_t>                         m_pending_tasks{0};
    mc::sync::small_mutex                        m_attach_mutex;
};

} // namespace mc::runtime::detail

#endif // MC_RUNTIME_INCLUDE_THREAD_POOL_IMPL_H
