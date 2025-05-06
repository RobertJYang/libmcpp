/*
 * Copyright (c) 2023, OpenUBMC Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 */

#include "task_interface.h"
#include <mc/log.h>

namespace test {
namespace errors {
REGISTER_CONST_ERROR(task_state_error, "bmc.kepler.error.TaskStateError",
                     "expect task state is ${expect}, actual state is ${current}");

} // namespace errors

task_interface::task_interface() {
}

task_interface::~task_interface() {
}

void task_interface::start() {
    if (m_state != task_state::PENDING) {
        elog("task ${name} is not pending", ("name", m_name));
        MC_REPLY_ERROR(test::errors::task_state_error,
                       ("expect", task_state::PENDING)("current", m_state));
    }
    set_state(task_state::RUNNING);

    start_timer();
}

void task_interface::stop() {
    if (m_state == task_state::COMPLETED || m_state == task_state::FAILED) {
        elog("task ${name} is already completed or failed", ("name", m_name));
        return;
    }

    set_state(task_state::FAILED);
    stop_timer();
}

void task_interface::pause() {
    if (m_state != task_state::RUNNING) {
        elog("task ${name} is not running", ("name", m_name));
        MC_REPLY_ERROR(test::errors::task_state_error,
                       ("expect", task_state::RUNNING)("current", m_state));
    }

    set_state(task_state::PAUSED);
    stop_timer();
}

void task_interface::resume() {
    if (m_state != task_state::PAUSED) {
        elog("task ${name} is not paused", ("name", m_name));
        MC_REPLY_ERROR(test::errors::task_state_error,
                       ("expect", task_state::PAUSED)("current", m_state));
    }

    set_state(task_state::RUNNING);
    start_timer();
}

void task_interface::start_timer() {
    if (!m_timer) {
        create_timer();
    } else {
        m_timer->stop();
    }

    m_timer->start(mc::milliseconds(1000));
    ilog("task ${name} start, current time: ${time}, progress: ${progress}",
         ("name", m_name)("time", mc::time_point::now())("progress", m_progress));
}

void task_interface::create_timer() {
    auto owner = get_owner();
    m_timer    = new mc::core::timer(owner);
    owner->connect(m_timer->timeout, [this]() {
        if (m_state != task_state::RUNNING) {
            return;
        }

        ilog("task ${name} progress: ${progress}", ("name", m_name)("progress", m_progress));

        m_progress++;
        if (m_progress >= 100) {
            set_state(task_state::COMPLETED);
            m_timer->stop();
        }
    });
    m_startTime = mc::time_point::now().to_string();
    m_endTime   = (mc::time_point::now() + m_timeout).to_string();
}

void task_interface::stop_timer() {
    if (m_timer) {
        m_timer->stop();
        ilog("task ${name} stop, current time: ${time}, progress: ${progress}",
             ("name", m_name)("time", mc::time_point::now())("progress", m_progress));
    }
}

uint32_t task_interface::get_progress() {
    return m_progress;
}

task_state task_interface::get_state() {
    return m_state;
}

void task_interface::set_state(task_state state) {
    ilog("task ${name} state from ${old_state} to ${new_state}",
         ("name", m_name)("old_state", m_state)("new_state", state));
    m_state = state;
}

} // namespace test
