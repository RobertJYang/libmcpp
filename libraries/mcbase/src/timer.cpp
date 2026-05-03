/*
 * Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <mc/runtime/steady_timer.h>
#include <mc/runtime/condition_variable.h>
#include <mc/sync/small_mutex.h>
#include <mc/timer.h>

#include <atomic>
#include <mutex>
#include <thread>

namespace mc {
struct timer::timer_impl {
    using timer_type = mc::runtime::steady_timer;

    explicit timer_impl(mc::runtime::any_executor executor) : m_timer(std::move(executor))
    {}

    ~timer_impl()
    {
        stop(false);
    }

    void start(mc::shared_ptr<timer> t);
    void stop(bool wait);
    bool begin_callback(uint64_t generation);
    void end_callback();
    bool is_cancelled(uint64_t generation) const;

    std::atomic<bool>     m_is_cancelled{true};
    std::atomic<uint64_t> m_generation{0};
    timer_type            m_timer;

    mc::small_mutex                 m_mutex;
    mc::runtime::condition_variable m_cv;
    bool                            m_callback_active{false};
    std::thread::id                 m_callback_thread;
};

void timer::timer_impl::start(mc::shared_ptr<timer> t)
{
    m_is_cancelled.store(false, std::memory_order_release);
    const auto generation = m_generation.fetch_add(1, std::memory_order_acq_rel) + 1;
    m_timer.cancel();
    m_timer.expires_after(std::chrono::milliseconds(t->m_interval));
    auto weak_t = t->weak_from_this();
    m_timer.async_wait([weak_t = std::move(weak_t), generation](const auto& ec) {
        auto object_ptr = weak_t.lock();
        if (!object_ptr) {
            return;
        }
        auto t = mc::static_pointer_cast<timer>(object_ptr);
        if (ec || !t->m_impl || t->m_impl->is_cancelled(generation)) {
            return;
        }

        if (!t->m_impl->begin_callback(generation)) {
            return;
        }

        struct callback_guard {
            timer_impl* impl;
            ~callback_guard()
            {
                impl->end_callback();
            }
        } guard{t->m_impl.get()};

        try {
            t->timeout();
        } catch (...) {
            throw;
        }

        if (t->m_single_shot || !t->m_impl || t->m_impl->is_cancelled(generation)) {
            return;
        }

        t->m_impl->start(std::move(t));
    });
}

void timer::timer_impl::stop(bool wait)
{
    m_is_cancelled.store(true, std::memory_order_release);
    m_generation.fetch_add(1, std::memory_order_acq_rel);
    m_timer.cancel();

    if (!wait) {
        return;
    }

    std::unique_lock lock(m_mutex);
    if (m_callback_active && m_callback_thread == std::this_thread::get_id()) {
        return;
    }
    m_cv.wait(lock, [this]() {
        return !m_callback_active;
    });
}

bool timer::timer_impl::begin_callback(uint64_t generation)
{
    std::lock_guard lock(m_mutex);
    if (is_cancelled(generation)) {
        return false;
    }
    m_callback_active = true;
    m_callback_thread = std::this_thread::get_id();
    return true;
}

void timer::timer_impl::end_callback()
{
    std::lock_guard lock(m_mutex);
    m_callback_active = false;
    m_callback_thread = {};
    m_cv.notify_all();
}

bool timer::timer_impl::is_cancelled(uint64_t generation) const
{
    return m_is_cancelled.load(std::memory_order_acquire) || m_generation.load(std::memory_order_acquire) != generation;
}

timer::timer(object* parent) : object(parent)
{}

timer::~timer()
{
    if (m_impl) {
        m_impl->stop(false);
    }
    m_impl.reset();
}

mc::milliseconds timer::interval() const
{
    return m_interval;
}

void timer::set_interval(mc::milliseconds msec)
{
    m_interval = msec;
    if (!check_active()) {
        return;
    }

    m_impl->start(mc::static_pointer_cast<timer>(this->shared_from_this()));
}

bool timer::is_single_shot() const
{
    return m_single_shot;
}

void timer::set_single_shot(bool single_shot)
{
    m_single_shot = single_shot;
}

bool timer::is_active() const
{
    return check_active();
}

bool timer::check_active() const
{
    if (!m_impl) {
        return false;
    }

    return !m_impl->m_is_cancelled.load(std::memory_order_acquire) &&
           m_impl->m_timer.expiry() > timer_impl::timer_type::clock_type::now();
}

void timer::start(mc::milliseconds msec)
{
    m_interval = msec;
    if (!m_impl) {
        m_impl = std::make_unique<timer_impl>(get_executor());
    }

    m_impl->start(mc::static_pointer_cast<timer>(this->shared_from_this()));
}

void timer::stop()
{
    if (!m_impl) {
        return;
    }

    m_impl->stop(true);
}

timer_ptr timer::single_shot(mc::milliseconds msec, std::function<void()> functor)
{
    if (!functor) {
        return {};
    }

    return single_shot(msec, nullptr, std::move(functor));
}

timer_ptr timer::single_shot(mc::milliseconds msec, object* context, std::function<void()> functor)
{
    if (!functor) {
        return {};
    }

    auto t = mc::make_shared<timer>(context);
    t->timeout.connect(std::move(functor));
    t->set_single_shot(true);
    t->start(msec);
    return t;
}

} // namespace mc