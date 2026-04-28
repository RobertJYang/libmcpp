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
#include <mc/event.h>
#include <mc/exception.h>
#include <mc/log/log.h>
#include <mc/runtime/runtime_context.h>
#include <mc/runtime/runtime_executor.h>
#include <mc/runtime/runtime_strand.h>
#include <mc/runtime/thread_pool.h>
#include <mc/singleton.h>
#include <mc/sync/mutex_box.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "runtime/include/event_dispatcher.h"

#ifndef MC_MAX_IO_THREADS
#define MC_MAX_IO_THREADS 10
#endif

#ifndef MC_MAX_WORK_THREADS
#define MC_MAX_WORK_THREADS 10
#endif

namespace mc::runtime {

immediate_executor::immediate_executor()  = default;
immediate_executor::~immediate_executor() = default;

immediate_executor::immediate_executor(const immediate_executor&)            = default;
immediate_executor& immediate_executor::operator=(const immediate_executor&) = default;

bool immediate_executor::operator==(const immediate_executor&) const noexcept
{
    return true;
}

bool immediate_executor::operator!=(const immediate_executor&) const noexcept
{
    return false;
}

void immediate_executor::on_work_started() const noexcept
{}

void immediate_executor::on_work_finished() const noexcept
{}

immediate_context::immediate_context()  = default;
immediate_context::~immediate_context() = default;

immediate_context::executor_type immediate_context::get_executor() const noexcept
{
    return executor_type();
}

class runtime_context::impl {
public:
    impl() : m_event_dispatcher(std::make_unique<event_dispatcher>())
    {}
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
    void reset_after_fork();

    thread_pool& io() noexcept
    {
        ensure_start();
        return *m_io_pool;
    }

    thread_pool& work() noexcept
    {
        ensure_start();
        // work_threads=0 时共享 io 线程池
        return m_work_pool ? *m_work_pool : *m_io_pool;
    }

    event_dispatcher& dispatcher() noexcept
    {
        return *m_event_dispatcher;
    }

private:
    std::unique_ptr<thread_pool>      m_io_pool;
    std::unique_ptr<thread_pool>      m_work_pool; // 可能为 null（work_threads=0 时）
    std::unique_ptr<event_dispatcher> m_event_dispatcher;

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

runtime_context::impl::~impl()
{
    if (!is_stopped()) {
        stop();
        join();
    }
}

void runtime_context::impl::set_config(const runtime_config& new_config)
{
    // io_threads 可以为 0（外部线程通过 run() 加入）
    // work_threads 可以为 0（共享 io 池）
    auto io_threads   = new_config.io_threads;
    auto work_threads = new_config.work_threads;

    auto& config        = m_data.unsafe_get_data().config;
    config.io_threads   = std::min(io_threads, std::size_t{MC_MAX_IO_THREADS});
    config.work_threads = std::min(work_threads, std::size_t{MC_MAX_WORK_THREADS});

    dlog("runtime_context set config, IO threads: ${count}, work threads: ${work_count}",
         ("count", config.io_threads)("work_count", config.work_threads));
}

void runtime_context::impl::ensure_start()
{
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

bool runtime_context::impl::is_initialized() const
{
    return m_data.unsafe_get_data().state != state_t::uninitialized;
}

void runtime_context::impl::initialize(const runtime_config& config)
{
    if (m_data.unsafe_get_data().state != state_t::uninitialized) {
        wlog("runtime_context already initialized, ignoring duplicate initialization");
        return;
    }

    m_data.with_lock([this, config](auto& data) {
        set_config(config);
        data.state = state_t::initialized;
    });
}

void runtime_context::impl::start()
{
    m_data.with_lock([this](auto&) {
        start_impl();
    });
}

void runtime_context::impl::start_impl()
{
    auto& data = m_data.unsafe_get_data();
    if ((m_io_pool && !m_io_pool->stopped()) || (m_work_pool && !m_work_pool->stopped())) {
        wlog("runtime_context already started, ignoring duplicate start");
        return;
    }

    data.state = state_t::running;

    // 创建主线程池（io）
    m_io_pool = std::make_unique<thread_pool>(data.config.io_threads, "io");
    m_io_pool->start();

    // work_threads > 0 时创建独立的 work 线程池，否则共享 io 池
    if (data.config.work_threads > 0) {
        m_work_pool = std::make_unique<thread_pool>(data.config.work_threads, "work");
        m_work_pool->start();
        dlog("runtime_context started, IO threads: ${io}, work threads: ${work}",
             ("io", data.config.io_threads)("work", data.config.work_threads));
    } else {
        m_work_pool = nullptr; // work() 方法会返回 io 池
        dlog("runtime_context started, IO threads: ${io}, work shares IO pool", ("io", data.config.io_threads));
    }
}

void runtime_context::impl::stop()
{
    m_data.with_lock([this](auto& data) {
        if (data.state != state_t::running) {
            return;
        }

        dlog("stopping runtime_context...");
        data.state = state_t::stopped;

        if (m_io_pool) {
            m_io_pool->stop();
        }
        if (m_work_pool) {
            m_work_pool->stop();
        }

        dlog("runtime_context stop signal sent");
    });
}

void runtime_context::impl::join()
{
    dlog("waiting for threads to finish...");
    if (m_io_pool) {
        m_io_pool->join();
    }
    if (m_work_pool) {
        m_work_pool->join();
    }

    m_io_pool.reset();
    m_work_pool.reset();

    dlog("runtime_context stopped completely");
}

void runtime_context::impl::reset_after_fork()
{
    // fork 后子进程不能 join 父进程线程池。
    auto* old_io   = m_io_pool.release();
    auto* old_work = m_work_pool.release();
    (void)old_io;
    (void)old_work;

    auto& data = m_data.unsafe_get_data();
    data.state = state_t::uninitialized;

    dlog("runtime_context reset after fork: released inherited thread pools");
}

bool runtime_context::impl::is_stopped() const noexcept
{
    auto& state = m_data.unsafe_get_data().state;
    return state == state_t::stopped || state == state_t::uninitialized;
}

runtime_context::runtime_context() : m_impl(std::make_unique<impl>())
{}

runtime_context::~runtime_context()
{
    if (!is_stopped()) {
        stop();
        join();
    }
}

void runtime_context::initialize(const runtime_config& config)
{
    m_impl->initialize(config);
}

void runtime_context::start()
{
    m_impl->ensure_start();
}

void runtime_context::stop()
{
    m_impl->stop();
}

void runtime_context::join()
{
    m_impl->join();
}

void runtime_context::reset_after_fork()
{
    m_impl->reset_after_fork();
}

bool runtime_context::is_stopped() const noexcept
{
    return m_impl->is_stopped();
}

thread_pool& runtime_context::io() noexcept
{
    return m_impl->io();
}

thread_pool& runtime_context::work() noexcept
{
    return m_impl->work();
}

any_executor runtime_context::get_io_executor() noexcept
{
    return m_impl->io().get_executor();
}

any_executor runtime_context::get_work_executor() noexcept
{
    return m_impl->work().get_executor();
}

any_executor runtime_context::get_executor() noexcept
{
    return any_executor(runtime_executor(*this));
}

void runtime_context::send_event(mc::object& target, mc::event& e)
{
    m_impl->dispatcher().send_event(target, e);
}

void runtime_context::post_event(mc::object& target, std::unique_ptr<mc::event> e)
{
    post_event(target, std::move(e), 0);
}

void runtime_context::post_event(mc::object& target, std::unique_ptr<mc::event> e, int priority)
{
    m_impl->dispatcher().post_event(target, std::move(e), priority);
}

void runtime_context::send_posted_events(mc::object* target, mc::event_type_id type)
{
    m_impl->dispatcher().send_posted_events(target, type);
}

void runtime_context::remove_posted_events(mc::object* target, mc::event_type_id type)
{
    m_impl->dispatcher().remove_posted_events(target, type);
}

global_event_filter runtime_context::install_global_filter(mc::event_type_id type, global_event_filter filter)
{
    return m_impl->dispatcher().install_global_filter(type, std::move(filter));
}

void runtime_context::remove_global_filter(mc::event_type_id type)
{
    m_impl->dispatcher().remove_global_filter(type);
}

any_executor runtime_context::create_strand()
{
    return any_executor(runtime_strand());
}

any_executor runtime_context::create_io_strand()
{
    return any_executor(runtime_strand().bound_pool(&io()));
}

any_executor runtime_context::create_work_strand()
{
    return any_executor(runtime_strand().bound_pool(&work()));
}

any_executor get_default_executor()
{
    return get_io_executor();
}

} // namespace mc::runtime
