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

#include <boost/asio/bind_allocator.hpp>
#include <boost/asio/defer.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/recycling_allocator.hpp>
#include <boost/asio/steady_timer.hpp>

#include <mc/common.h>
#include <mc/log/log.h>

#include "runtime/include/thread_pool_impl.h"

namespace mc::runtime {
namespace detail {
thread_local thread_pool_shard* t_current_shard = nullptr;
}

namespace {

struct shard_scope {
    explicit shard_scope(detail::thread_pool_shard* shard) : m_previous(detail::t_current_shard)
    {
        detail::t_current_shard = shard;
    }

    ~shard_scope()
    {
        detail::t_current_shard = m_previous;
    }

private:
    detail::thread_pool_shard* m_previous;
};

mc::string make_thread_name(const mc::string& base_name, std::size_t index)
{
    mc::string suffix = "-" + mc::to_string(index);
    mc::string tname  = base_name;

    if (tname.length() + suffix.length() > 15) {
        int keep_len = 15 - static_cast<int>(suffix.length()) - 2;
        if (keep_len > 0) {
            mc::string_view prefix = tname.view().substr(0, static_cast<std::size_t>(keep_len));
            tname.clear();
            tname.append(prefix.data(), prefix.size());
            tname.append("..");
        } else {
            tname = tname.substr(0, 15 - suffix.length());
        }
    }

    tname += suffix;
    return tname;
}

std::unique_ptr<detail::thread_pool_shard> make_shard(detail::thread_pool_impl& impl, uint16_t id)
{
    auto shard  = std::make_unique<detail::thread_pool_shard>(impl);
    shard->ctx  = std::make_unique<detail::thread_pool_impl::io_context>();
    shard->work = std::make_unique<detail::thread_pool_impl::executor_work_guard>(
        boost::asio::make_work_guard(shard->ctx->get_executor()));
    shard->next.store(detail::thread_pool_null_index, std::memory_order_relaxed);
    shard->state.store(detail::thread_pool_worker_state::Running, std::memory_order_relaxed);
    shard->id = id;
    return shard;
}

class global_task_operation final : public boost::asio::detail::scheduler_operation {
public:
    explicit global_task_operation(detail::thread_pool_task task)
        : boost::asio::detail::scheduler_operation(&global_task_operation::do_complete), m_task(std::move(task))
    {}

private:
    static void do_complete(void* owner, boost::asio::detail::scheduler_operation* base,
                            const boost::system::error_code&, std::size_t)
    {
        std::unique_ptr<global_task_operation> operation(static_cast<global_task_operation*>(base));
        if (owner != nullptr) {
            operation->m_task();
        }
    }

    detail::thread_pool_task m_task;
};

void empty_cb()
{}

using native_wakeup_timer = boost::asio::steady_timer;

} // namespace

namespace detail {

boost::asio::detail::scheduler& thread_pool_impl::get_or_create_scheduler(boost::asio::execution_context& ctx)
{
    if (boost::asio::has_service<boost::asio::detail::scheduler>(ctx)) {
        return boost::asio::use_service<boost::asio::detail::scheduler>(ctx);
    }

    auto& scheduler = boost::asio::make_service<boost::asio::detail::scheduler>(ctx, false);
    scheduler.init_task();
    scheduler.work_started();
    return scheduler;
}

thread_pool_impl::thread_pool_impl(thread_pool& owner, std::size_t num_threads, mc::string_view name)
    : boost::asio::execution_context(), m_owner(&owner), m_scheduler(get_or_create_scheduler(*this)),
      m_num_threads(num_threads), m_name(name), m_idle_head(thread_pool_null_index)
{
    MC_ASSERT_THROW(m_num_threads < thread_pool_null_index, mc::runtime_exception, "线程数不能超过 {}",
                    thread_pool_null_index - 1);

    if (m_num_threads == 0) {
        return;
    }

    m_shards.reserve(m_num_threads);
    for (std::size_t i = 0; i < m_num_threads; ++i) {
        m_shards.push_back(make_shard(*this, static_cast<uint16_t>(i)));
    }
}

thread_pool_shard* thread_pool_impl::current_shard() noexcept
{
    return t_current_shard;
}

thread_pool* thread_pool_impl::current_pool() noexcept
{
    return t_current_shard ? t_current_shard->impl.m_owner : nullptr;
}

thread_pool& thread_pool_impl::get_pool(const thread_pool::executor_type& executor) noexcept
{
    return *executor.m_impl->m_owner;
}

boost::asio::io_context::executor_type thread_pool_impl::get_native_io_executor(thread_pool_shard* shard)
{
    return shard->ctx->get_executor();
}

boost::asio::io_context::executor_type thread_pool_impl::get_native_io_executor(thread_pool& pool, std::size_t idx)
{
    if (!pool.m_impl || pool.m_impl->m_shards.empty()) {
        throw std::runtime_error("没有可用的 Worker 分片");
    }
    return pool.m_impl->m_shards[idx % pool.m_impl->m_shards.size()]->ctx->get_executor();
}

boost::asio::io_context::executor_type
thread_pool_impl::get_native_io_executor(const thread_pool::executor_type& executor)
{
    auto* current = current_shard();
    if (current && &current->impl == executor.m_impl.get()) {
        return current->ctx->get_executor();
    }

    static std::atomic<std::size_t> round_robin{0};
    auto                            idx = round_robin.fetch_add(1, std::memory_order_relaxed);
    return get_native_io_executor(*executor.m_impl->m_owner, idx);
}

void thread_pool_impl::on_work_started(thread_pool& pool) noexcept
{
    pool.m_impl->m_scheduler.work_started();
}

void thread_pool_impl::on_work_finished(thread_pool& pool) noexcept
{
    pool.m_impl->m_scheduler.work_finished();
}

void thread_pool_impl::on_work_started(const thread_pool::executor_type& executor) noexcept
{
    executor.m_impl->m_scheduler.work_started();
}

void thread_pool_impl::on_work_finished(const thread_pool::executor_type& executor) noexcept
{
    executor.m_impl->m_scheduler.work_finished();
}

void thread_pool_impl::notify_shard(thread_pool_shard* shard) noexcept
{
    if (shard) {
        shard->impl.notify_shard_impl(shard);
    }
}

thread_pool_wakeup_handle thread_pool_impl::schedule_shard_wakeup(thread_pool_shard* shard,
                                                                  std::chrono::steady_clock::time_point deadline)
{
    return shard->impl.schedule_shard_wakeup_impl(shard, deadline);
}

void thread_pool_impl::cancel_shard_wakeup(const thread_pool_wakeup_handle& handle) noexcept
{
    if (!handle) {
        return;
    }

    auto timer = std::static_pointer_cast<native_wakeup_timer>(handle);
    timer->cancel();
}

void thread_pool_impl::dispatch_task(thread_pool_task task)
{
    auto* current = current_shard();
    if (current && &current->impl == this) {
        boost::asio::dispatch(current->ctx->get_executor(), std::move(task));
        return;
    }

    if (m_pending_tasks.load(std::memory_order_relaxed) == 0) {
        if (auto* shard = acquire_idle_worker()) {
            boost::asio::dispatch(shard->ctx->get_executor(), std::move(task));
            return;
        }
    }

    schedule_global(std::move(task), false);
}

void thread_pool_impl::post_task(thread_pool_task task)
{
    if (m_pending_tasks.load(std::memory_order_relaxed) == 0) {
        if (auto* shard = acquire_idle_worker()) {
            boost::asio::post(shard->ctx->get_executor(), std::move(task));
            return;
        }

        auto* current = current_shard();
        if (current && &current->impl == this) {
            boost::asio::post(current->ctx->get_executor(), std::move(task));
            return;
        }
    }

    schedule_global(std::move(task), false);
}

void thread_pool_impl::defer_task(thread_pool_task task)
{
    auto* current = current_shard();
    if (current && &current->impl == this) {
        boost::asio::defer(current->ctx->get_executor(), std::move(task));
        return;
    }

    if (m_pending_tasks.load(std::memory_order_relaxed) == 0) {
        if (auto* shard = acquire_idle_worker()) {
            boost::asio::post(shard->ctx->get_executor(), std::move(task));
            return;
        }
    }

    schedule_global(std::move(task), true);
}

bool thread_pool_impl::running_in_this_thread() const noexcept
{
    auto* current = current_shard();
    return current && &current->impl == this;
}

void thread_pool_impl::schedule_global(thread_pool_task task, bool is_continuation)
{
    auto* operation = new global_task_operation(std::move(task));
    m_pending_tasks.fetch_add(1, std::memory_order_relaxed);
    m_scheduler.post_immediate_completion(operation, is_continuation);
    wake_up_workers();
}

void thread_pool_impl::start()
{
    if (!m_threads.empty()) {
        return;
    }

    m_threads.reserve(m_num_threads);
    for (std::size_t i = 0; i < m_num_threads; ++i) {
        m_threads.emplace_back([this, i]() {
            const mc::string tname = make_thread_name(m_name, i);
            set_current_thread_name(tname.view());
            dlog("${name} 线程 ${idx} 启动", ("name", m_name)("idx", i));
            try {
                worker_loop(m_shards[i].get());
            } catch (const std::exception& e) {
                elog("${name} 线程 ${idx} 异常: ${error}", ("name", m_name)("idx", i)("error", e.what()));
            }
            dlog("${name} 线程 ${idx} 结束", ("name", m_name)("idx", i));
        });
    }
}

void thread_pool_impl::stop()
{
    if (stopped()) {
        return;
    }

    m_scheduler.stop();
    for (auto& shard : m_shards) {
        shard->work.reset();
        shard->ctx->stop();
    }
}

void thread_pool_impl::join()
{
    for (auto& thread : m_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_threads.clear();
}

bool thread_pool_impl::stopped() const
{
    return m_scheduler.stopped();
}

std::size_t thread_pool_impl::shard_count() const
{
    return m_shards.size();
}

std::size_t thread_pool_impl::poll_one()
{
    boost::system::error_code ec;
    return m_scheduler.poll_one(ec);
}

mc::string_view thread_pool_impl::get_name() const noexcept
{
    return m_name;
}

void thread_pool_impl::push_idle(thread_pool_shard* shard)
{
    uint64_t old_head = m_idle_head.load(std::memory_order_relaxed);
    uint64_t new_head = 0;

    do {
        uint16_t old_index = static_cast<uint16_t>(old_head & thread_pool_index_mask);
        shard->next.store(old_index, std::memory_order_relaxed);
        uint64_t new_tag = (old_head & ~thread_pool_index_mask) + thread_pool_tag_increment;
        new_head         = new_tag | shard->id;
    } while (!m_idle_head.compare_exchange_weak(old_head, new_head, std::memory_order_release,
                                                std::memory_order_relaxed));
}

thread_pool_shard* thread_pool_impl::acquire_idle_worker()
{
    uint64_t old_head = m_idle_head.load(std::memory_order_acquire);

    while (true) {
        uint16_t old_index = static_cast<uint16_t>(old_head & thread_pool_index_mask);
        if (old_index == thread_pool_null_index) {
            return nullptr;
        }

        thread_pool_shard* node       = m_shards[old_index].get();
        uint16_t           next_index = node->next.load(std::memory_order_relaxed);
        uint64_t           new_tag    = (old_head & ~thread_pool_index_mask) + thread_pool_tag_increment;
        uint64_t           new_head   = new_tag | next_index;

        if (m_idle_head.compare_exchange_weak(old_head, new_head, std::memory_order_acquire,
                                              std::memory_order_acquire)) {
            node->in_stack.store(false, std::memory_order_release);

            auto expected = thread_pool_worker_state::Idle;
            if (node->state.compare_exchange_strong(expected, thread_pool_worker_state::Notified,
                                                    std::memory_order_release, std::memory_order_relaxed)) {
                return node;
            }

            old_head = m_idle_head.load(std::memory_order_acquire);
        }
    }
}

bool thread_pool_impl::has_work(thread_pool_shard* shard)
{
    if (m_pending_tasks.load(std::memory_order_relaxed) > 0) {
        return true;
    }

    return shard->ctx->poll() > 0;
}

bool thread_pool_impl::poll_tasks(thread_pool_shard* shard)
{
    auto& ctx = *shard->ctx;
    bool  ran_local = ctx.poll() > 0;

    boost::system::error_code ec;
    bool                      ran_global = m_scheduler.poll_one(ec) > 0;
    if (ran_global) {
        int64_t old_val = m_pending_tasks.load(std::memory_order_relaxed);
        while (old_val > 0) {
            if (m_pending_tasks.compare_exchange_weak(old_val, old_val - 1, std::memory_order_relaxed)) {
                break;
            }
        }
    }

    return ran_local || ran_global;
}

void thread_pool_impl::worker_loop(thread_pool_shard* shard)
{
    shard_scope scope(shard);
    auto        stop_pred = [this]() {
        return m_scheduler.stopped();
    };
    poll_until(shard, make_predicate_ref(stop_pred));
}

void thread_pool_impl::wake_up_workers()
{
    if (m_shards.empty()) {
        return;
    }

    if (auto* shard = acquire_idle_worker()) {
        boost::asio::post(shard->ctx->get_executor(),
                          boost::asio::bind_allocator(boost::asio::recycling_allocator<void>(), empty_cb));
    }
}

void thread_pool_impl::notify_shard_impl(thread_pool_shard* shard) noexcept
{
    if (!shard) {
        return;
    }

    boost::asio::post(shard->ctx->get_executor(),
                      boost::asio::bind_allocator(boost::asio::recycling_allocator<void>(), empty_cb));
}

thread_pool_wakeup_handle thread_pool_impl::schedule_shard_wakeup_impl(thread_pool_shard* shard,
                                                                       std::chrono::steady_clock::time_point deadline)
{
    auto timer = std::make_shared<native_wakeup_timer>(shard->ctx->get_executor(), deadline);
    timer->async_wait([](const boost::system::error_code&) {
    });
    return timer;
}

void thread_pool_impl::cancel_shard_wakeup_impl(const thread_pool_wakeup_handle& handle) noexcept
{
    cancel_shard_wakeup(handle);
}

void thread_pool_impl::poll_until(thread_pool_shard* shard, predicate_ref pred)
{
    auto& ctx = *shard->ctx;

    while (!pred()) {
        if (m_scheduler.stopped()) {
            break;
        }

        bool ran_tasks = poll_tasks(shard);
        if (ctx.stopped()) {
            ctx.restart();
        }

        if (!ran_tasks) {
            try_sleep_until(shard, pred);
        }
    }
}

void thread_pool_impl::try_sleep_until(thread_pool_shard* shard, predicate_ref pred)
{
    if (pred()) {
        return;
    }

    shard->state.store(thread_pool_worker_state::Idle, std::memory_order_release);
    if (!shard->in_stack.exchange(true, std::memory_order_acq_rel)) {
        push_idle(shard);
    }

    if (pred() || has_work(shard) || shard->state.load(std::memory_order_acquire) == thread_pool_worker_state::Notified) {
        shard->state.store(thread_pool_worker_state::Running, std::memory_order_relaxed);
        return;
    }

    try {
        shard->ctx->run_one();
    } catch (...) {
    }

    shard->state.store(thread_pool_worker_state::Running, std::memory_order_relaxed);
}

void thread_pool_impl::try_sleep(thread_pool_shard* shard)
{
    auto always_false = []() {
        return false;
    };
    try_sleep_until(shard, make_predicate_ref(always_false));
}

void thread_pool_impl::run()
{
    thread_pool_shard* shard = nullptr;
    {
        std::lock_guard<mc::sync::small_mutex> lock(m_attach_mutex);
        if (t_current_shard != nullptr && &t_current_shard->impl == this) {
            shard = t_current_shard;
        } else {
            MC_ASSERT_THROW(m_shards.size() < thread_pool_null_index, mc::runtime_exception, "线程数不能超过 ${max}",
                            ("max", thread_pool_null_index - 1));

            auto new_shard = make_shard(*this, static_cast<uint16_t>(m_shards.size()));
            shard          = new_shard.get();
            m_shards.push_back(std::move(new_shard));
            dlog("外部线程附加到 ${name}，shard ${idx}", ("name", m_name)("idx", shard->id));
        }
    }

    dlog("${name} 外部线程 shard ${idx} 开始运行", ("name", m_name)("idx", shard->id));
    try {
        worker_loop(shard);
    } catch (const std::exception& e) {
        elog("${name} 外部线程 shard ${idx} 异常: ${error}", ("name", m_name)("idx", shard->id)("error", e.what()));
    }
    dlog("${name} 外部线程 shard ${idx} 结束", ("name", m_name)("idx", shard->id));
}

} // namespace detail

thread_pool::thread_pool(std::size_t num_threads, mc::string_view name)
    : m_impl(std::make_shared<detail::thread_pool_impl>(*this, num_threads, name))
{}

thread_pool::~thread_pool()
{
    stop();
    join();
}

thread_pool::executor_type thread_pool::get_executor() noexcept
{
    return executor_type(m_impl);
}

void thread_pool::start()
{
    m_impl->start();
}

void thread_pool::stop()
{
    m_impl->stop();
}

void thread_pool::join()
{
    m_impl->join();
}

bool thread_pool::stopped() const
{
    return m_impl->stopped();
}

std::size_t thread_pool::shard_count() const
{
    return m_impl->shard_count();
}

void thread_pool::run()
{
    m_impl->run();
}

std::size_t thread_pool::poll_one()
{
    return m_impl->poll_one();
}

mc::string_view thread_pool::get_name() const noexcept
{
    return m_impl->get_name();
}

void thread_pool::executor_type::dispatch_task(detail::thread_pool_task task) const
{
    m_impl->dispatch_task(std::move(task));
}

void thread_pool::executor_type::post_task(detail::thread_pool_task task) const
{
    m_impl->post_task(std::move(task));
}

void thread_pool::executor_type::defer_task(detail::thread_pool_task task) const
{
    m_impl->defer_task(std::move(task));
}

bool thread_pool::executor_type::running_in_this_thread() const noexcept
{
    return m_impl->running_in_this_thread();
}

} // namespace mc::runtime