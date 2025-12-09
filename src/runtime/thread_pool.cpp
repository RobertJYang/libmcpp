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

#include <algorithm>
#include <boost/asio/post.hpp>
#include <boost/asio/recycling_allocator.hpp>
#include <mc/log.h>
#include <mc/runtime/thread_pool.h>

namespace mc::runtime {
thread_local thread_pool::shard_t* t_current_shard = nullptr;

struct shard_scope {
    shard_scope(thread_pool::shard_t* shard) {
        t_current_shard = shard;
    }
    ~shard_scope() {
        t_current_shard = nullptr;
    }
};

namespace {
boost::asio::detail::scheduler& get_or_create_scheduler(boost::asio::execution_context& ctx) {
    if (boost::asio::has_service<boost::asio::detail::scheduler>(ctx)) {
        return boost::asio::use_service<boost::asio::detail::scheduler>(ctx);
    }

    auto& s = boost::asio::make_service<boost::asio::detail::scheduler>(ctx, false);
    s.init_task();
    s.work_started();
    return s;
}

// 生成符合长度限制的线程名称
// 格式：name-idx，如果过长则截断为 name..-idx
std::string make_thread_name(const std::string& base_name, std::size_t index) {
    std::string suffix = "-" + std::to_string(index);
    std::string tname  = base_name;

    // 限制总长度为 15 (pthread 限制通常是 16 字节包含 null)
    if (tname.length() + suffix.length() > 15) {
        // 如果需要截断，中间添加 ".." 提示
        // 预留 2 字节给 ".."
        int keep_len = 15 - static_cast<int>(suffix.length()) - 2;
        if (keep_len > 0) {
            tname = tname.substr(0, keep_len) + "..";
        } else {
            // 极端情况：连 ID 都快放不下了，优先保证 ID
            tname = tname.substr(0, 15 - suffix.length());
        }
    }
    tname += suffix;
    return tname;
}

} // namespace

thread_pool::thread_pool(std::size_t num_threads, const std::string& name)
    : boost::asio::execution_context(), m_scheduler(get_or_create_scheduler(*this)), m_num_threads(num_threads), m_name(name) {
    MC_ASSERT_THROW(m_num_threads < NULL_INDEX, mc::runtime_exception, "线程数不能超过 {}", NULL_INDEX - 1);

    if (m_num_threads > 0) {
        m_shards.reserve(m_num_threads);
        for (std::size_t i = 0; i < m_num_threads; ++i) {
            auto shard  = std::make_unique<shard_t>();
            shard->ctx  = std::make_unique<io_context>();
            shard->work = std::make_unique<boost::asio::executor_work_guard<io_context::executor_type>>(
                boost::asio::make_work_guard(shard->ctx->get_executor()));

            shard->next.store(NULL_INDEX, std::memory_order_relaxed);
            shard->state.store(State::Running, std::memory_order_relaxed);
            shard->id = i;

            m_shards.push_back(std::move(shard));
        }
    }

    m_idle_head.store(NULL_INDEX, std::memory_order_relaxed);
}

thread_pool::~thread_pool() {
    stop();
    join();
}

thread_pool::executor_type thread_pool::get_executor() noexcept {
    return executor_type(*this);
}

void thread_pool::start() {
    if (!m_threads.empty()) {
        return;
    }

    m_threads.reserve(m_num_threads);
    for (std::size_t i = 0; i < m_num_threads; ++i) {
        m_threads.emplace_back([this, i]() {
            set_current_thread_name(make_thread_name(m_name, i));
            dlog("{name} 线程 {idx} 启动", ("name", m_name)("idx", i));
            try {
                worker_loop(i);
            } catch (const std::exception& e) {
                elog("{name} 线程 {idx} 异常: {error}", ("name", m_name)("idx", i)("error", e.what()));
            }
            dlog("{name} 线程 {idx} 结束", ("name", m_name)("idx", i));
        });
    }
}

void thread_pool::push_idle(shard_t* shard) {
    uint64_t old_head = m_idle_head.load(std::memory_order_relaxed);
    uint64_t new_head;

    do {
        uint16_t old_index = static_cast<uint16_t>(old_head & INDEX_MASK);
        shard->next.store(old_index, std::memory_order_relaxed);

        // 增加 Tag（高48位）+ 设置新 Index（低16位）
        uint64_t new_tag = (old_head & ~INDEX_MASK) + TAG_INCREMENT;
        new_head         = new_tag | shard->id;

    } while (!m_idle_head.compare_exchange_weak(old_head, new_head,
                                                std::memory_order_release,
                                                std::memory_order_relaxed));
}

bool thread_pool::has_work(shard_t* shard) {
    // 检查全局任务
    if (m_pending_tasks.load(std::memory_order_relaxed) > 0) {
        return true;
    }

    // 检查本地任务
    if (shard->ctx->poll() > 0) {
        return true;
    }

    return false;
}

void thread_pool::try_sleep(shard_t* shard) {
    // 1. 设置状态为 Idle
    shard->state.store(State::Idle, std::memory_order_release);

    // 2. 如果不在栈中，入栈
    if (!shard->in_stack.exchange(true, std::memory_order_acq_rel)) {
        push_idle(shard);
    }

    // 3. 双重检查：是否有工作？
    if (has_work(shard) || shard->state.load(std::memory_order_acquire) == State::Notified) {
        // 将状态从 Idle 恢复为 Running
        shard->state.store(State::Running, std::memory_order_relaxed);
        return;
    }

    try {
        // 阻塞直到有任务（通过 post 唤醒）或停止
        shard->ctx->run_one();
    } catch (const std::exception& e) {
        elog("${name} 线程 ${idx} run_one 异常: ${error}",
             ("name", m_name)("idx", shard->id)("error", e.what()));
    }

    // 3. 恢复状态为 Running
    shard->state.store(State::Running, std::memory_order_relaxed);
}

void thread_pool::worker_loop(std::size_t idx) {
    auto& shard = *m_shards[idx];
    auto& ctx   = *shard.ctx;

    // 设置 TLS
    shard_scope scope(&shard);
    while (true) {
        if (m_scheduler.stopped()) {
            break;
        }

        // 1. 优先执行本地任务
        bool ran_local = ctx.poll() > 0;

        // 2. 再执行全局任务
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

        // 3. 如果上下文已停止，重启上下文，避免无限循环
        if (ctx.stopped()) {
            ctx.restart();
        }

        // 4. 如果本地和全局任务均为空，尝试睡眠
        if (!ran_local && !ran_global) {
            try_sleep(&shard);
        }
    }
}

// 出栈并获取一个 Worker
thread_pool::shard_t* thread_pool::acquire_idle_worker() {
    uint64_t old_head = m_idle_head.load(std::memory_order_acquire);

    while (true) {
        uint16_t old_index = static_cast<uint16_t>(old_head & INDEX_MASK);
        if (old_index == NULL_INDEX) {
            return nullptr;
        }

        shard_t* node       = m_shards[old_index].get();
        uint16_t next_index = node->next.load(std::memory_order_relaxed);

        // 计算新栈顶：旧 Tag + 1 | 下一个索引
        uint64_t new_tag  = (old_head & ~INDEX_MASK) + TAG_INCREMENT;
        uint64_t new_head = new_tag | next_index;

        if (m_idle_head.compare_exchange_weak(old_head, new_head,
                                              std::memory_order_acquire,
                                              std::memory_order_acquire)) {
            // 标记为不在栈中
            node->in_stack.store(false, std::memory_order_release);

            // 尝试通知：Idle -> Notified
            State expected = State::Idle;
            if (node->state.compare_exchange_strong(expected, State::Notified,
                                                    std::memory_order_release,
                                                    std::memory_order_relaxed)) {
                // 成功抢占该 Worker。返回它以便调用者决定如何使用
                return node;
            }

            old_head = m_idle_head.load(std::memory_order_acquire);
        }

        // 如果 CAS 失败, old_head 自动更新为当前头. 重试
    }
}

static void empty_cb() {
}

void thread_pool::wake_up_workers() {
    if (m_shards.empty()) {
        return;
    }

    // 尝试获取一个 Idle Worker
    if (auto* shard = acquire_idle_worker()) {
        boost::asio::post(
            shard->ctx->get_executor(),
            boost::asio::bind_allocator(boost::asio::recycling_allocator<void>(), empty_cb));
        return;
    }
}

void thread_pool::stop() {
    if (stopped()) {
        return;
    }

    m_scheduler.stop();

    for (auto& shard : m_shards) {
        shard->work.reset();
        shard->ctx->stop();
    }
}

void thread_pool::join() {
    for (auto& t : m_threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    m_threads.clear();
}

bool thread_pool::stopped() const {
    return m_scheduler.stopped();
}

std::size_t thread_pool::poll_one() {
    boost::system::error_code ec;
    return m_scheduler.poll_one(ec);
}

thread_pool::io_context& thread_pool::get_shard(std::size_t idx) {
    if (m_shards.empty()) {
        throw std::runtime_error("没有可用的 Worker 分片");
    }
    return *m_shards[idx % m_shards.size()]->ctx;
}

std::size_t thread_pool::shard_count() const {
    return m_shards.size();
}

thread_pool::shard_t* thread_pool::get_current_shard() {
    return t_current_shard;
}

} // namespace mc::runtime