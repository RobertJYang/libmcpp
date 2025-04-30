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
#include <mc/engine/base.h>
#include <mc/engine/service.h>
#include <mc/engine/timer.h>

#include <boost/asio/steady_timer.hpp>

namespace mc::engine {

struct timer::timer_impl {
    timer_impl(abstract_object& obj) : m_timer(obj.get_service()->get_strand()) {
    }

    void start(timer_type type, mc::milliseconds ms, timer_cb&& cb);

    boost::asio::steady_timer m_timer;
};

void timer::timer_impl::start(timer_type type, mc::milliseconds ms, timer_cb&& cb) {
}

timer::timer(timer_type type) : m_type(type) {
}

timer::~timer() {
}

timer::timer(timer&& other) noexcept
    : m_impl(std::move(other.m_impl)), m_type(other.m_type), m_object(other.m_object) {
    other.m_object = nullptr;
}

timer& timer::operator=(timer&& other) noexcept {
    if (this != &other) {
        m_impl   = std::move(other.m_impl);
        m_type   = other.m_type;
        m_object = other.m_object;

        other.m_object = nullptr;
    }
    return *this;
}

void timer::set_object(abstract_object& obj) {
    m_object = &obj;
}

abstract_object* timer::get_object() const {
    return m_object;
}

void timer::start(mc::milliseconds ms, timer_cb&& cb) {
    MC_ASSERT(m_object, "timer is not bound to object, cannot start");

    auto& timer = m_impl->m_timer;
    timer.expires_after(std::chrono::milliseconds(ms));
    timer.async_wait([cb = std::move(cb), this, ms](const error_code& ec) {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }

        cb();
        if (m_type == timer_type::REPEAT) {
            this->m_impl->m_timer.expires_after(std::chrono::milliseconds(ms));
        }
    });
}

void timer::stop() {
    m_state = timer_state::STOPPED;
    m_impl->m_timer.cancel();
}

void timer::restart() {
    m_state = timer_state::RUNNING;
    this->start(m_ms, std::move(m_cb));
}

void timer::cancel() {
    m_impl->m_timer.cancel();
}

basic_timer::~basic_timer() {
}

void basic_timer::start(mc::milliseconds ms) {
    timer::start(ms, [this]() {
        this->on_timeout();
    });
}

} // namespace mc::engine