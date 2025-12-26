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

#ifndef MC_RUNTIME_THREAD_POOL_H
#define MC_RUNTIME_THREAD_POOL_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include <boost/asio/bind_allocator.hpp>
#include <boost/asio/defer.hpp>
#include <boost/asio/detail/executor_op.hpp>
#include <boost/asio/detail/scheduler.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/execution.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>

#include <mc/common.h>
#include <mc/sync/mutex_box.h>
#include <mc/sync/small_mutex.h>

namespace mc::runtime {
constexpr uint16_t NULL_INDEX    = 0xFFFF;
constexpr uint64_t INDEX_MASK    = 0xFFFF;
constexpr uint64_t TAG_INCREMENT = 1ULL << 16;

class MC_API thread_pool : public boost::asio::execution_context {
public:
    using io_context = boost::asio::io_context;

    class executor_type;

    using strand = boost::asio::strand<executor_type>;

    // 最大递归深度，用于重新进入调度
    static constexpr uint16_t MAX_RECURSION_DEPTH = 32;

    enum class State {
        Running, // Worker 正在运行任务
        Idle,    // Worker 在栈中，等待睡眠或任务
        Notified // Worker 已被唤醒者选中
    };

    using executor_work_guard = boost::asio::executor_work_guard<io_context::executor_type>;
    struct shard_t {
        explicit shard_t(thread_pool& pool_) noexcept : pool(pool_) {
        }

        thread_pool&                         pool; // 所属线程池
        std::unique_ptr<io_context>          ctx;
        std::unique_ptr<executor_work_guard> work;

        std::atomic<uint16_t> next{NULL_INDEX};
        std::atomic<State>    state{State::Running};
        std::atomic<bool>     in_stack{false};
        uint16_t              id{NULL_INDEX};
        uint16_t              recursion_depth{0};
    };

    class executor_type {
    public:
        executor_type(thread_pool& ctx) noexcept : m_context(&ctx) {
        }
        executor_type(const executor_type&) = default;

        executor_type& operator=(const executor_type&) = default;
        executor_type(executor_type&&)                 = default;
        executor_type& operator=(executor_type&&)      = default;

        thread_pool& context() const noexcept {
            return *m_context;
        }

        void on_work_started() const noexcept {
            m_context->m_scheduler.work_started();
        }

        void on_work_finished() const noexcept {
            m_context->m_scheduler.work_finished();
        }

        template <typename Function, typename Allocator>
        void dispatch(Function&& f, const Allocator& a) const {
            // 1: 优先在当前线程执行
            auto* current_shard = thread_pool::get_current_shard();
            if (current_shard && &current_shard->pool == m_context) {
                boost::asio::dispatch(
                    current_shard->ctx->get_executor(),
                    boost::asio::bind_allocator(a, std::forward<Function>(f)));
                return;
            }

            // 2: 尝试直接将任务提交给空闲 Worker
            if (m_context->m_pending_tasks.load(std::memory_order_relaxed) == 0) {
                if (auto* shard = m_context->acquire_idle_worker()) {
                    boost::asio::dispatch(
                        shard->ctx->get_executor(),
                        boost::asio::bind_allocator(a, std::forward<Function>(f)));
                    return;
                }
            }

            // 3: 最后放到全局 Scheduler
            schedule_global(std::forward<Function>(f), a, false);
        }

        template <typename Function, typename Allocator>
        void post(Function&& f, const Allocator& a) const {
            // 仅当全局队列没有待处理任务时，才尝试直接投递到 shard
            // 这保证了全局队列任务的优先级和公平调度
            if (m_context->m_pending_tasks.load(std::memory_order_relaxed) == 0) {
                // 1: 优先尝试获取空闲 Worker
                if (auto* shard = m_context->acquire_idle_worker()) {
                    boost::asio::post(shard->ctx->get_executor(),
                                      boost::asio::bind_allocator(a, std::forward<Function>(f)));
                    return;
                }

                // 2: 没有空闲 Worker，但如果当前线程在某个 shard 上，投递到当前 shard
                auto* current_shard = thread_pool::get_current_shard();
                if (current_shard && &current_shard->pool == m_context) {
                    boost::asio::post(
                        current_shard->ctx->get_executor(),
                        boost::asio::bind_allocator(a, std::forward<Function>(f)));
                    return;
                }
            }

            // 3: 最后放到全局 Scheduler
            schedule_global(std::forward<Function>(f), a, false);
        }

        template <typename Function, typename Allocator>
        void defer(Function&& f, const Allocator& a) const {
            // 1: 优先在当前线程执行
            auto* current_shard = thread_pool::get_current_shard();
            if (current_shard && &current_shard->pool == m_context) {
                boost::asio::defer(
                    current_shard->ctx->get_executor(),
                    boost::asio::bind_allocator(a, std::forward<Function>(f)));
                return;
            }

            // 2: 尝试直接将任务提交给空闲 Worker
            if (m_context->m_pending_tasks.load(std::memory_order_relaxed) == 0) {
                if (auto* shard = m_context->acquire_idle_worker()) {
                    boost::asio::post(
                        shard->ctx->get_executor(),
                        boost::asio::bind_allocator(a, std::forward<Function>(f)));
                    return;
                }
            }

            // 3: 最后放到全局 Scheduler
            schedule_global(std::forward<Function>(f), a, true);
        }

        bool operator==(const executor_type& other) const noexcept {
            return m_context == other.m_context;
        }

        bool operator!=(const executor_type& other) const noexcept {
            return !(*this == other);
        }

        /**
         * @brief 检查当前线程是否在此 executor 上执行
         */
        bool running_in_this_thread() const noexcept {
            auto* current_shard = thread_pool::get_current_shard();
            return current_shard && &current_shard->pool == m_context;
        }

        operator boost::asio::any_io_executor() const {
            if (auto* current = thread_pool::get_current_shard()) {
                return current->ctx->get_executor();
            }
            return m_context->get_shard(m_context->m_wake_idx.fetch_add(1, std::memory_order_relaxed) % m_context->shard_count())
                .get_executor();
        }

        operator boost::asio::io_context::executor_type() const {
            if (auto* current = thread_pool::get_current_shard()) {
                return current->ctx->get_executor();
            }
            return m_context->get_shard(m_context->m_wake_idx.fetch_add(1, std::memory_order_relaxed) % m_context->shard_count())
                .get_executor();
        }

        // 兼容 Asio Concept: 声明这是一个 Executor
        using shape_type = std::size_t;
        using index_type = std::size_t;

        template <typename Function, typename Allocator>
        void execute(Function&& f, const Allocator& a) const {
            this->post(std::forward<Function>(f), a);
        }

    private:
        thread_pool* m_context;

        template <typename Function, typename Allocator>
        void schedule_global(Function&& f, const Allocator& a, bool is_continuation) const {
            using Op = boost::asio::detail::executor_op<
                typename std::decay<Function>::type,
                Allocator,
                boost::asio::detail::scheduler_operation>;

            typename Op::ptr p = {boost::asio::detail::addressof(a), Op::ptr::allocate(a), 0};
            p.p                = new (p.v) Op(std::forward<Function>(f), a);

            m_context->m_pending_tasks.fetch_add(1);
            m_context->m_scheduler.post_immediate_completion(p.p, is_continuation);
            p.v = p.p = 0;

            m_context->wake_up_workers();
        }
    };

    explicit thread_pool(std::size_t num_threads, const std::string& name = "pool");
    ~thread_pool();

    executor_type get_executor() noexcept;

    void start();
    void stop();
    void join();
    bool stopped() const;

    io_context& get_shard(std::size_t idx);
    std::size_t shard_count() const;

    static shard_t* get_current_shard();

    /// @brief 将当前线程附加到线程池并运行工作循环
    /// @note 进入时自动 attach，退出时自动 detach
    /// @note 阻塞直到线程池停止
    void run();

    class scoped_recursion_guard {
    public:
        explicit scoped_recursion_guard(shard_t* shard) : m_shard(shard) {
            if (m_shard->recursion_depth < MAX_RECURSION_DEPTH) {
                m_shard->recursion_depth++;
                m_can_schedule = true;
            }
        }

        ~scoped_recursion_guard() {
            if (m_can_schedule) {
                m_shard->recursion_depth--;
            }
        }

        bool can_schedule() const {
            return m_can_schedule;
        }

    private:
        shard_t* m_shard;
        bool     m_can_schedule{false};
    };

    std::size_t poll_one();

    void wake_up_workers();

    /// @brief 运行调度循环直到谓词返回 true 或线程池停止
    /// @param shard 当前 shard
    /// @param pred 谓词，返回 true 时退出循环
    /// @note 复用 worker_loop 的调度逻辑，同时消费本地和全局任务
    template <typename Predicate>
    void poll_until(shard_t* shard, Predicate pred);

    const std::string& get_name() const noexcept {
        return m_name;
    }

private:
    boost::asio::detail::scheduler& m_scheduler;
    std::size_t                     m_num_threads; // 由 thread_pool 创建的线程数
    std::string                     m_name;

    void     worker_loop(shard_t* shard);
    void     push_idle(shard_t* shard);
    shard_t* acquire_idle_worker();
    bool     has_work(shard_t* shard);

    // 执行本地和全局任务，返回是否执行了任务
    bool poll_tasks(shard_t* shard);

    // 尝试进入睡眠
    template <typename Predicate>
    void try_sleep_until(shard_t* shard, Predicate pred);

    // 无条件睡眠
    void try_sleep(shard_t* shard) {
        try_sleep_until(shard, []() {
            return false;
        });
    }

    std::vector<std::unique_ptr<shard_t>> m_shards;  // 所有 shard（包括外部线程的）
    std::vector<std::thread>              m_threads; // 由 thread_pool 创建的线程
    std::atomic<std::size_t>              m_wake_idx{0};
    std::atomic<uint64_t>                 m_idle_head;
    std::atomic<int64_t>                  m_pending_tasks{0};
    mc::sync::small_mutex                 m_attach_mutex; // 用于保护 attach/detach 操作
};

} // namespace mc::runtime

namespace boost::asio {
template <>
struct is_executor<mc::runtime::thread_pool::executor_type> : std::true_type {};
} // namespace boost::asio

namespace mc::runtime {

template <typename Predicate>
void thread_pool::poll_until(shard_t* shard, Predicate pred) {
    auto& ctx = *shard->ctx;

    while (!pred()) {
        if (m_scheduler.stopped()) {
            break;
        }

        bool ran_tasks = poll_tasks(shard);

        // 重启停止的上下文
        if (ctx.stopped()) {
            ctx.restart();
        }

        // 如果没有任务，尝试睡眠
        if (!ran_tasks) {
            try_sleep_until(shard, pred);
        }
    }
}

template <typename Predicate>
void thread_pool::try_sleep_until(shard_t* shard, Predicate pred) {
    // 如果谓词已满足，直接返回
    if (pred()) {
        return;
    }

    // 1. 设置状态为 Idle
    shard->state.store(State::Idle, std::memory_order_release);

    // 2. 如果不在栈中，入栈
    if (!shard->in_stack.exchange(true, std::memory_order_acq_rel)) {
        push_idle(shard);
    }

    // 3. 双重检查：是否有工作或谓词已满足？
    if (pred() || has_work(shard) || shard->state.load(std::memory_order_acquire) == State::Notified) {
        // 将状态从 Idle 恢复为 Running
        shard->state.store(State::Running, std::memory_order_relaxed);
        return;
    }

    try {
        // 阻塞直到有任务（通过 post 唤醒）或停止
        shard->ctx->run_one();
    } catch (...) {
    }

    // 恢复状态为 Running
    shard->state.store(State::Running, std::memory_order_relaxed);
}

} // namespace mc::runtime

#endif // MC_RUNTIME_THREAD_POOL_H
