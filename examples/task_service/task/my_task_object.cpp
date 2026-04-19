/**
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

#include "my_task_object.h"
#include <mc/engine/service.h>
#include <mc/format.h>
#include <mc/log.h>
#include <mc/string.h>
#include <mc/time.h>

namespace test {
uint32_t my_task_object::m_next_task_id{1};

my_task_object::my_task_object(mc::object* parent) : mc::engine::object<my_task_object>(parent)
{}

mc::shared_ptr<my_task_object> my_task_object::create_task(mc::object* parent, mc::milliseconds timeout)
{
    auto task = mc::make_shared<my_task_object>(parent);

    auto id = m_next_task_id++;
    task->m_task.m_id.set_value(id);
    task->m_task.m_name.set_value("Task " + std::to_string(id));
    task->m_task.m_timeout = timeout;

    task->set_object_name(task->m_task.m_name.value());
    task->set_object_path(sformat(object_type::path_pattern, ("Id", id)));

    ilog("task ${name} created", ("name", task->m_task.m_name.value()));
    return task;
}

my_tasks_object::my_tasks_object(mc::object* parent) : mc::engine::object<my_tasks_object>(parent)
{}

std::string_view my_tasks_object::create_task(const std::string& name)
{
    auto task = my_task_object::create_task(this, mc::milliseconds(1000));
    m_tasks.push_back(task);
    if (auto service = this->get_service()) {
        service->register_object(task);
    }
    return task->m_task.m_name.value();
}

std::vector<std::string_view> my_tasks_object::get_tasks()
{
    std::vector<std::string_view> tasks;
    for (auto& task : m_tasks) {
        tasks.push_back(task->m_task.m_name.value());
    }
    return tasks;
}
} // namespace test

MC_REFLECT(test::my_task_object, ((m_task, "task")))
MC_REFLECT(test::my_tasks_object, (m_tasks_intf, "tasks"), (create_task, "CreateTask")(get_tasks, "GetTasks"))
