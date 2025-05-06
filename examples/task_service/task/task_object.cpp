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

    auto id                = m_next_task_id++;
    task->m_task.m_id      = id;
    task->m_task.m_name    = "Task " + std::to_string(id);
    task->m_task.m_timeout = timeout;

    task->set_object_name(task->m_task.m_name);
    task->set_object_path(mc::string::format(object_type::path_pattern, {{"Id", id}}));

    task->m_service = service;
    service->register_object(task);

    ilog("task ${name} created", ("name", task->m_task.m_name));
    return task;
}

} // namespace test
