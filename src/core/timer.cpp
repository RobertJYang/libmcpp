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
#include <mc/core/application.h>
#include <mc/core/timer.h>
#include <mc/runtime/steady_timer.h>

namespace mc::core {
struct timer::timer_impl {
    using timer_type = mc::runtime::steady_timer;

    explicit timer_impl(mc::runtime::any_executor executor) : m_timer(std::move(executor))
    {}

    ~timer_impl()
    {
        m_timer.cancel();
    }

    void start(mc::shared_ptr<timer> t);

    bool       m_is_cancelled{false};
    timer_type m_timer;
};

void timer::timer_impl::start(mc::shared_ptr<timer> t)
{
    m_is_cancelled = false;
    m_timer.expires_after(std::chrono::milliseconds(t->m_interval));
    m_timer.async_wait([this, t](const auto& ec) {
        if (ec || m_is_cancelled) {
            return;
        }

        t->timeout();

        if (t->m_single_shot || m_is_cancelled) {
            return;
        }

        start(std::move(t));
    });
}

timer::timer(object* parent) : object(parent)
{}

timer::~timer()
{
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

    m_impl->m_timer.cancel();
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

    return m_impl->m_timer.expiry() > timer_impl::timer_type::clock_type::now();
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

    m_impl->m_timer.cancel();
    m_impl->m_is_cancelled = true;
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
} // namespace mc::core