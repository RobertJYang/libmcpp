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
#include <mc/log.h>
#include <mc/string.h>
#include <mc/time.h>

namespace test {
uint32_t my_task_object::m_next_task_id{1};

my_task_object::my_task_object(mc::core::object* parent)
    : mc::engine::object<my_task_object>(parent) {
}

mc::shared_ptr<my_task_object> my_task_object::create_task(mc::engine::service* service,
                                                           mc::milliseconds     timeout) {
    auto task = mc::make_shared<my_task_object>(service);

    auto id = m_next_task_id++;
    task->m_task.m_id.set_value(id);
    task->m_task.m_name.set_value("Task " + std::to_string(id));
    task->m_task.m_timeout = timeout;

    task->set_object_name(task->m_task.m_name.value());
    task->set_object_path(mc::string::format(object_type::path_pattern, {{"Id", id}}));

    service->register_object(task);

    ilog("task ${name} created", ("name", task->m_task.m_name.value()));
    return task;
}

} // namespace test
