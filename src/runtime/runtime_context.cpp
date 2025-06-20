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

#include <mc/common.h>
#include <mc/exception.h>
#include <mc/log.h>
#include <mc/runtime/runtime_context.h>
#include <mc/singleton.h>
#include <mc/sync/mutex_box.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#ifndef MC_MAX_IO_THREADS
#define MC_MAX_IO_THREADS 10
#endif

#ifndef MC_MAX_WORK_THREADS
#define MC_MAX_WORK_THREADS 10
#endif

namespace mc::runtime {
using io_work_guard = boost::asio::executor_work_guard<io_context::executor_type>;
using work_guard    = boost::asio::executor_work_guard<work_context::executor_type>;

struct thread_pool {
    thread_pool() {
    }

    void start(std::size_t threads, std::function<void()> func) {
        m_threads.reserve(threads);
        for (std::size_t i = 0; i < threads; ++i) {
            m_threads.emplace_back(func);
        }
    }

    std::size_t get_thread_count() const {
        return m_threads.size();
    }

    bool empty() const {
        return m_threads.empty();
    }

    void join() {
        for (auto& thread : m_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    void clear() {
        m_threads.clear();
    }

    std::vector<std::thread> m_threads;
};

static inline std::size_t get_thread_count(std::size_t threads) {
    if (threads == 0) {
        return std::max(std::size_t{1}, std::size_t(std::thread::hardware_concurrency()));
    }
    return threads;
}

class runtime_context::impl {
public:
    impl() = default;
    ~impl();

    void set_config(const runtime_config& new_config);

    bool is_initialized() const;
    bool is_stopped() const noexcept;

    void initialize(const runtime_config& config);
    void start();
    void stop();
    void join();

    void ensure_start();
    void start_impl();

    io_context& get_io_context() noexcept {
        ensure_start();
        return m_io_context;
    }

    work_context& get_work_context() noexcept {
        ensure_start();
        return m_work_context;
    }

    // 任务分发方法
    void post_to_io_context(std::function<void()> task) {
        ensure_start();
        boost::asio::post(m_io_context, std::move(task));
    }

    void post_to_work_context(std::function<void()> task) {
        ensure_start();
        boost::asio::post(m_work_context, std::move(task));
    }

    void defer_to_io_context(std::function<void()> task) {
        ensure_start();
        boost::asio::defer(m_io_context, std::move(task));
    }

    void defer_to_work_context(std::function<void()> task) {
        ensure_start();
        boost::asio::defer(m_work_context, std::move(task));
    }

    void dispatch_to_io_context(std::function<void()> task) {
        ensure_start();
        boost::asio::dispatch(m_io_context, std::move(task));
    }

    void dispatch_to_work_context(std::function<void()> task) {
        ensure_start();
        boost::asio::dispatch(m_work_context, std::move(task));
    }

private:
    io_context                     m_io_context;
    thread_pool                    m_io_threads;
    std::unique_ptr<io_work_guard> m_io_guard;

    work_context                m_work_context;
    thread_pool                 m_work_threads;
    std::unique_ptr<work_guard> m_work_guard;

    enum class state_t : std::uint8_t {
        uninitialized = 0, // 未初始化
        initialized   = 1, // 已初始化但未启动
        running       = 2, // 运行中
        stopped       = 3  // 已停止
    };

    struct data_t {
        runtime_config config;
        state_t        state{state_t::uninitialized};
    };
    mc::mutex_box<data_t> m_data;
};

runtime_context::impl::~impl() {
    if (!is_stopped()) {
        stop();
        join();
    }
}

void runtime_context::impl::set_config(const runtime_config& new_config) {
    auto io_threads   = get_thread_count(new_config.io_threads);
    auto work_threads = get_thread_count(new_config.work_threads);

    auto& config        = m_data.unsafe_get_data().config;
    config.io_threads   = std::min(io_threads, std::size_t{MC_MAX_IO_THREADS});
    config.work_threads = std::min(work_threads, std::size_t{MC_MAX_WORK_THREADS});

    dlog("runtime_context set config, "
         "IO threads: ${count}, "
         "work threads: ${work_count}",
         ("count", io_threads)("work_count", work_threads));
}

void runtime_context::impl::ensure_start() {
    auto state = m_data.unsafe_get_data().state;
    if (MC_LIKELY(state == state_t::running)) {
        return;
    }

    m_data.with_lock([this](auto& data) {
        if (data.state == state_t::uninitialized) {
            dlog("runtime_context ensure start, state: uninitialized, use default config");
            set_config({});
            start_impl();
        } else if (data.state == state_t::initialized) {
            start_impl();
        } else if (data.state == state_t::stopped) {
            MC_THROW(mc::invalid_op_exception, "runtime_context is stopped");
        }
    });
}

bool runtime_context::impl::is_initialized() const {
    return m_data.unsafe_get_data().state != state_t::uninitialized;
}

void runtime_context::impl::initialize(const runtime_config& config) {
    if (m_data.unsafe_get_data().state != state_t::uninitialized) {
        wlog("runtime_context already initialized, ignoring duplicate initialization");
        return;
    }

    m_data.with_lock([this, config](auto& data) {
        set_config(config);
        data.state = state_t::initialized;
    });
}

void runtime_context::impl::start() {
    m_data.with_lock([this](auto&) {
        start_impl();
    });
}

void runtime_context::impl::start_impl() {
    auto& data = m_data.unsafe_get_data();
    if (!m_io_threads.empty() || !m_work_threads.empty()) {
        wlog("runtime_context already started, ignoring duplicate start");
        return;
    }

    data.state = state_t::running;

    // 确保上下文已启动
    if (m_io_context.stopped()) {
        m_io_context.restart();
    }
    if (m_work_context.stopped()) {
        m_work_context.restart();
    }

    // 创建工作守护，防止线程立即退出
    m_io_guard   = std::make_unique<io_work_guard>(boost::asio::make_work_guard(m_io_context));
    m_work_guard = std::make_unique<work_guard>(boost::asio::make_work_guard(m_work_context));

    // 启动IO线程池
    m_io_threads.start(data.config.io_threads, [this]() {
        dlog("IO thread started");
        try {
            m_io_context.run();
        } catch (const std::exception& e) {
            elog("IO thread exception: ${error}", ("error", e.what()));
        }
        dlog("IO thread ended");
    });

    // 启动系统线程池
    m_work_threads.start(data.config.work_threads, [this]() {
        dlog("work thread started");
        try {
            m_work_context.run();
        } catch (const std::exception& e) {
            elog("work thread exception: ${error}", ("error", e.what()));
        }
        dlog("work thread ended");
    });

    dlog("runtime_context started successfully, "
         "IO threads: ${count}, "
         "work threads: ${work_count}",
         ("count", data.config.io_threads)("work_count", data.config.work_threads));
}

void runtime_context::impl::stop() {
    m_data.with_lock([this](auto& data) {
        if (data.state != state_t::running) {
            return;
        }

        dlog("stopping runtime_context...");

        data.state = state_t::stopped;

        m_io_guard.reset();
        m_work_guard.reset();

        m_io_context.stop();
        m_work_context.stop();

        dlog("runtime_context stop signal sent");
    });
}

void runtime_context::impl::join() {
    if (m_io_threads.empty()) {
        return;
    }

    dlog("waiting for IO threads to finish...");

    m_io_threads.join();
    m_work_threads.join();

    m_io_threads.clear();
    m_work_threads.clear();

    dlog("runtime_context stopped completely");
}

bool runtime_context::impl::is_stopped() const noexcept {
    auto& state = m_data.unsafe_get_data().state;
    return state == state_t::stopped || state == state_t::uninitialized;
}

// runtime_context 实现
runtime_context::runtime_context() : m_impl(std::make_unique<impl>()) {
}

runtime_context::~runtime_context() = default;

void runtime_context::initialize(const runtime_config& config) {
    m_impl->initialize(config);
}

void runtime_context::start() {
    m_impl->ensure_start();
}

void runtime_context::stop() {
    m_impl->stop();
}

void runtime_context::join() {
    m_impl->join();
}

bool runtime_context::is_stopped() const noexcept {
    return m_impl->is_stopped();
}

io_context::executor_type runtime_context::get_io_executor() noexcept {
    return get_io_context().get_executor();
}

work_context::executor_type runtime_context::get_work_executor() noexcept {
    return get_work_context().get_executor();
}

io_context& runtime_context::get_io_context() noexcept {
    return m_impl->get_io_context();
}

work_context& runtime_context::get_work_context() noexcept {
    return m_impl->get_work_context();
}

io_strand runtime_context::make_io_strand() noexcept {
    return io_strand(get_io_context().get_executor());
}

work_strand runtime_context::make_work_strand() noexcept {
    return work_strand(get_work_context().get_executor());
}

} // namespace mc::runtime