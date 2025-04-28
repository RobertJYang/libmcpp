/*
 * Copyright (c) 2023, OpenUBMC Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 */

#include "task_object.h"
#include <mc/log.h>
#include <mc/string.h>
#include <mc/time.h>

namespace test {
uint32_t task_object::m_next_task_id{1};

task_object_ptr task_object::create_task(mc::engine::service* service, mc::milliseconds timeout) {
    auto task = mc::im::make_ref<task_object>();

    auto id                  = m_next_task_id++;
    task->m_task.m_id        = id;
    task->m_task.m_name      = "Task " + std::to_string(id);
    task->m_task.m_startTime = mc::time_point::now().to_string();
    task->m_task.m_endTime   = (mc::time_point::now() + timeout).to_string();

    task->set_object_name(task->m_task.m_name);
    task->set_object_path(mc::string::format(object_type::path_pattern, {{"Id", id}}));

    task->m_service = service;
    service->register_object(task);

    ilog("task ${name} created", ("name", task->m_task.m_name));
    return task;
}

void task_object::start() {
    if (m_task.m_state != task_state::PENDING) {
        elog("task ${name} is not pending", ("name", m_task.m_name));
        return;
    }
    set_state(task_state::RUNNING);

    m_timer = std::make_unique<boost::asio::steady_timer>(m_service->get_strand());
    start_timer();
}

void task_object::stop() {
    if (m_task.m_state == task_state::COMPLETED || m_task.m_state == task_state::FAILED) {
        return;
    }

    set_state(task_state::FAILED);
    stop_timer();
}

void task_object::pause() {
    if (m_task.m_state != task_state::RUNNING) {
        elog("task ${name} is not running", ("name", m_task.m_name));
        return;
    }

    set_state(task_state::PAUSED);
    stop_timer();
}

void task_object::resume() {
    if (m_task.m_state != task_state::PAUSED) {
        elog("task ${name} is not paused", ("name", m_task.m_name));
        return;
    }

    set_state(task_state::RUNNING);
    start_timer();
}

void task_object::start_timer() {
    m_timer->expires_after(std::chrono::milliseconds(1000));
    m_timer->async_wait([this](const boost::system::error_code& ec) {
        if (ec || m_task.m_state != task_state::RUNNING) {
            return;
        }

        ilog("task ${name} progress: ${progress}",
             ("name", m_task.m_name)("progress", m_task.m_progress));

        m_task.m_progress++;
        if (m_task.m_progress >= 100) {
            set_state(task_state::COMPLETED);
            m_timer->cancel();
        }
    });

    ilog("task ${name} start, current time: ${time}, progress: ${progress}",
         ("name", m_task.m_name)("time", mc::time_point::now())("progress", m_task.m_progress));
}

void task_object::stop_timer() {
    m_timer->cancel();
    ilog("task ${name} stop, current time: ${time}, progress: ${progress}",
         ("name", m_task.m_name)("time", mc::time_point::now())("progress", m_task.m_progress));
}

uint32_t task_object::get_progress() {
    return m_task.m_progress;
}

task_state task_object::get_state() {
    return m_task.m_state;
}

void task_object::set_state(task_state state) {
    ilog("task ${name} state from ${old_state} to ${new_state}",
         ("name", m_task.m_name)("old_state", m_task.m_state)("new_state", state));
    m_task.m_state = state;
}
} // namespace test
